//
// Created by zr on 23-4-9.
//
#include "mqtt_broker.h"
#include "mqtt_session.h"
#include "base/mqtt_util.h"
#include "mqtt_tasks.h"
#include "db/mqtt_db.h"
#include "thrdpool/thrdpool.h"
#include <string.h>
#include <assert.h>
#include <signal.h>

static void dispatch_new_connection(tmq_socket_t conn, void* arg)
{
    tmq_broker_t* broker = (tmq_broker_t*) arg;

    /* dispatch tcp connection using round-robin */
    tmq_io_context_t* next_context = &broker->io_contexts[broker->next_io_context++];
    if(broker->next_io_context >=  broker->io_threads)
        broker->next_io_context = 0;
    tmq_mailbox_push(&next_context->pending_tcp_connections, (tmq_mail_t)(intptr_t)(conn));
}

/* Construct a result of connecting request and notify the corresponding io thread.
 * Called by the broker thread. */
static void make_connect_respond(tmq_io_context_t* context, tmq_tcp_conn_t* conn,
                                 connack_return_code code, tmq_session_t* session, int sp)
{
    session_connect_resp* resp = malloc(sizeof(session_connect_resp));
    resp->conn = conn;
    resp->return_code = code;
    resp->session = session;
    resp->session_present = sp;
    tmq_mailbox_push(&context->mqtt_connect_responses, resp);
}

static void session_close_callback(void* arg, tmq_session_t* session, int force_clean)
{
    if(!session->clean_session && !force_clean)
        return;
    tmq_broker_t* broker = arg;
    /* unsubscribe all topics */
    tmq_map_iter_t sub_it = tmq_map_iter(session->subscriptions);
    for(; tmq_map_has_next(sub_it); tmq_map_next(session->subscriptions, sub_it))
        tmq_topics_remove_subscription(&broker->topics_tree, (char*) sub_it.first, session->client_id);
    /* remove this session from the broker */
    tmq_map_erase(broker->sessions, session->client_id);
    SESSION_RELEASE(session);
}

static message_store_t* get_message_store(tmq_broker_t* broker)
{
    if(broker->mongodb_pool)
    {
        tmq_str_t mongodb_store_trigger = tmq_config_get(&broker->conf, "mongodb_store_trigger");
        int trigger = mongodb_store_trigger ? atoi(mongodb_store_trigger): DEFAULT_MONGODB_STORE_TRIGGER;
        tmq_str_free(mongodb_store_trigger);
        return tmq_message_store_mongodb_new(trigger);
    }
    return tmq_message_store_memory_new();
}

static void mqtt_publish(void* arg, tmq_session_t* session, char* topic, tmq_message* message, uint8_t retain);

static void start_session(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt* connect_pkt)
{
    char* will_topic = NULL, *will_message = NULL;
    uint8_t will_qos = 0, will_retain = 0;
    if(CONNECT_WILL_FLAG(connect_pkt->flags))
    {
        will_topic = connect_pkt->will_topic;
        will_message = connect_pkt->will_message;
        will_qos = CONNECT_WILL_QOS(connect_pkt->flags);
        will_retain = (CONNECT_WILL_RETAIN(connect_pkt->flags) != 0);
    }

    /* if the client doesn't provide a client id, generate a temporary id for it. */
    if(connect_pkt->client_id == NULL || tmq_str_len(connect_pkt->client_id) == 0)
    {
        connect_pkt->client_id = tmq_str_assign(connect_pkt->client_id, "tmp_client[");
        char conn_id[50];
        tmq_tcp_conn_id(conn, conn_id, sizeof(conn_id));
        connect_pkt->client_id = tmq_str_append_str(connect_pkt->client_id, conn_id);
        connect_pkt->client_id = tmq_str_append_char(connect_pkt->client_id, ']');
    }
    tmq_session_t** session = tmq_map_get(broker->sessions, connect_pkt->client_id);
    /* if there is an active session associated with this client id */
    if(session && (*session)->state == OPEN)
        make_connect_respond(conn->io_context, conn, IDENTIFIER_REJECTED, NULL, 0);

    /* if there is a closed session associated with this client id,
     * it must not be a clean-session */
    else if(session)
    {
        assert((*session)->clean_session == 0);
        /* if the client try to start a clean-session,
         * clean up the old session's state and start a new clean-session */
        if(CONNECT_CLEAN_SESSION(connect_pkt->flags))
        {
            tmq_session_close(*session, 1);
            tmq_session_t* new_session = tmq_session_new(broker, mqtt_publish, session_close_callback, conn,
                                                         connect_pkt->client_id, connect_pkt->username, 1, connect_pkt->keep_alive,
                                                         will_topic, will_message, will_qos, will_retain,
                                                         broker->inflight_window_size, get_message_store(broker));
            tmq_map_put(broker->sessions, connect_pkt->client_id, SESSION_SHARE(new_session));
            make_connect_respond(conn->io_context, conn, CONNECTION_ACCEPTED, new_session, 1);
        }
        /* otherwise, resume the old session's state */
        else
        {
            tmq_session_resume(*session, conn, connect_pkt->keep_alive,
                               will_topic, will_message, will_qos, will_retain);
            make_connect_respond(conn->io_context, conn, CONNECTION_ACCEPTED, *session, 1);
        }
    }
    /* if no stored session in the broker, just create a new one */
    else
    {
        int clean_session = CONNECT_CLEAN_SESSION(connect_pkt->flags) != 0;
        tmq_session_t* new_session = tmq_session_new(broker, mqtt_publish, session_close_callback, conn,
                                                     connect_pkt->client_id, connect_pkt->username, clean_session, connect_pkt->keep_alive,
                                                     will_topic, will_message, will_qos, will_retain,
                                                     broker->inflight_window_size, get_message_store(broker));
        tmq_map_put(broker->sessions, connect_pkt->client_id, SESSION_SHARE(new_session));
        make_connect_respond(conn->io_context, conn, CONNECTION_ACCEPTED, new_session, 0);
    }
    tmq_connect_pkt_cleanup(connect_pkt);
    free(connect_pkt);
}

/* handle session creating and closing */
void handle_session_req(void* arg)
{
    session_operation_t* operation = arg;
    tmq_broker_t* broker = operation->broker;
    session_req* req = &operation->req;

    /* handle connect request */
    if(req->op == SESSION_CONNECT)
    {
        session_connect_req* connect_req = &req->connect_req;
        /* try to start a mqtt session */
        start_session(broker, connect_req->conn, connect_req->connect_pkt);
    }
    /* handle session disconnect or force-close */
    else
    {
        tmq_session_t* session = req->session;
        if(req->op == SESSION_FORCE_CLOSE)
        {
            /* send the will-message if session closed without receiving a disconnect packet */
            if(session->will_topic)
            {
                tmq_message will_msg = {
                        .qos = session->will_qos,
                        .message = session->will_message
                };
                tmq_topics_publish(&broker->topics_tree, session->will_topic,
                                   &will_msg, session->will_retain);
            }
            tlog_info("session[%s] closed forcely", session->client_id);
        }
        else tlog_info("session[%s] disconnected", session->client_id);
        tmq_session_close(session, 0);
    }
    free(arg);
}

/* handle subscribe/unsubscribe */
static void handle_topic_req(void* arg)
{
    topic_operation_t* operation = arg;
    tmq_broker_t* broker = operation->broker;
    topic_req * req = &operation->req;

    tmq_session_t** session = tmq_map_get(broker->sessions, req->client_id);
    if(!session || (*session)->state == CLOSED)
        return;

    tmq_any_packet_t ack;
    /* handle subscribe request */
    if(req->op == TOPIC_SUBSCRIBE)
    {
        /* the subscription will always success. */
        tmq_suback_pkt* sub_ack = malloc(sizeof(tmq_suback_pkt));
        sub_ack->packet_id = req->subscribe_pkt.packet_id;
        tmq_vec_init(&sub_ack->return_codes, uint8_t);

        retain_message_list_t all_retain = tmq_vec_make(retain_message_t*);

        /* add all the topic filters into the topic tree. */
        topic_filter_qos* tf = tmq_vec_begin(req->subscribe_pkt.topics);
        for(; tf != tmq_vec_end(req->subscribe_pkt.topics); tf++)
        {
            if(broker->acl_enabled && tmq_acl_auth(&broker->acl, *session, tf->topic_filter, SUB) == DENY)
            {
                tlog_info("subscribe{client=%s, topic=%s, qos=%u} denied", req->client_id, tf->topic_filter, tf->qos);
                continue;
            }
            retain_message_list_t retain = tmq_topics_add_subscription(&broker->topics_tree, tf->topic_filter,
                                                                       *session, tf->qos);
            tlog_info("subscribe{client=%s, topic=%s, qos=%u} success", req->client_id, tf->topic_filter, tf->qos);
            tmq_vec_extend(all_retain, retain);
            tmq_vec_free(retain);

            tmq_vec_push_back(sub_ack->return_codes, tf->qos);
        }
        tmq_subscribe_pkt_cleanup(&req->subscribe_pkt);
        ack.packet_type = MQTT_SUBACK;
        ack.packet_ptr = sub_ack;
        tmq_session_send_packet(*session, &ack, 1);
        /* send all the retained messages that match the subscription */
        for(retain_message_t** it = tmq_vec_begin(all_retain); it != tmq_vec_end(all_retain); it++)
        {
            retain_message_t* retain_msg = *it;
            broadcast_ctx_t* broadcast_task = malloc(sizeof(broadcast_ctx_t));
            broadcast_task->topic = tmq_str_new(retain_msg->retain_topic);
            broadcast_task->message.message = tmq_str_new(retain_msg->retain_msg.message);
            broadcast_task->message.qos = retain_msg->retain_msg.qos;
            broadcast_task->retain = 1;
            subscribe_info_t info = {
                    .session = SESSION_SHARE(*session),
                    .qos = tf->qos
            };
            tmq_vec_push_back(broadcast_task->subscribers, info);
            tmq_io_context_t* context = (*session)->conn->io_context;
            tmq_mailbox_push(&context->broadcast_tasks, broadcast_task);
        }
        tmq_vec_free(all_retain);
    }
    /* handle unsubscribe request */
    else
    {
        tmq_unsuback_pkt* unsub_ack = malloc(sizeof(tmq_unsuback_pkt));
        unsub_ack->packet_id = req->unsubscribe_pkt.packet_id;

        tmq_str_t* tf = tmq_vec_begin(req->unsubscribe_pkt.topics);
        for(; tf != tmq_vec_end(req->unsubscribe_pkt.topics); tf++)
        {
            tlog_info("unsubscribe{client=%s, topic=%s}", req->client_id, *tf);
            tmq_topics_remove_subscription(&broker->topics_tree, *tf, req->client_id);
        }
        tmq_unsubscribe_pkt_cleanup(&req->unsubscribe_pkt);
        ack.packet_type = MQTT_UNSUBACK;
        ack.packet_ptr = unsub_ack;
        tmq_session_send_packet(*session, &ack, 1);
    }
    free(arg);
}

/* handle publish */
static void handle_publish_req(void* arg)
{
    publish_ctx_t* operation = arg;

    tmq_broker_t* broker = operation->broker;
    publish_req* req = &operation->req;

    tmq_topics_publish(&broker->topics_tree, req->topic, &req->message, req->retain);
    tmq_str_free(req->topic);
    tmq_str_free(req->message.message);
    free(arg);
}

typedef struct password_validate_context_s
{
    tmq_broker_t* broker;
    tmq_tcp_conn_t* conn;
    tmq_connect_pkt* connect_pkt;
} password_validate_context_t;

static void validate_password_in_thread_pool(void* arg)
{
    password_validate_context_t* ctx = arg;
    MYSQL* mysql_client = tmq_mysql_conn_pool_pop(&ctx->broker->mysql_pool);
    if(ctx->connect_pkt->username &&
    ctx->connect_pkt->password &&
    validate_connect_password(mysql_client, ctx->connect_pkt->username, ctx->connect_pkt->password))
    {
        session_req req = {
                .op = SESSION_CONNECT,
                .connect_req = {
                        .conn = ctx->conn,
                        .connect_pkt = ctx->connect_pkt
                }
        };
        session_operation_t* operation = malloc(sizeof(session_operation_t));
        operation->broker = ctx->broker;
        operation->req = req;
        tmq_executor_post(&ctx->broker->executor, handle_session_req, operation, 1);
    }
    else
    {
        tmq_connect_pkt_cleanup(ctx->connect_pkt);
        free(ctx->connect_pkt);
        make_connect_respond(ctx->conn->io_context, ctx->conn, NOT_AUTHORIZED, NULL, 0);
    }
    tmq_mysql_conn_pool_push(&ctx->broker->mysql_pool, mysql_client);
    free(ctx);
}

void mqtt_connect(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt* connect_pkt)
{
    /* validate username and password if anonymous login is not allowed */
    int validate_success = 1;
    int validate_in_progress = 0;
    tmq_str_t allow_anonymous = tmq_config_get(&broker->conf, "allow_anonymous");
    if(!allow_anonymous || !tmq_str_equal(allow_anonymous, "true"))
    {
        if(broker->mysql_enabled)
        {
            password_validate_context_t* ctx = malloc(sizeof(password_validate_context_t));
            bzero(ctx, sizeof(password_validate_context_t));
            ctx->broker = broker;
            ctx->conn = TCP_CONN_SHARE(conn),
            ctx->connect_pkt = connect_pkt;
            struct thrdpool_task task = {
                    .routine = validate_password_in_thread_pool,
                    .context = ctx
            };
            thrdpool_schedule(&task, broker->thread_pool);
            validate_in_progress = 1;
        }
        else
        {
            if(!tmq_config_exist(&broker->pwd_conf, connect_pkt->username))
                validate_success = 0;
            else
            {
                tmq_str_t pwd_stored = tmq_config_get(&broker->pwd_conf, connect_pkt->username);
                char* pwd_encoded = password_encode(connect_pkt->password);
                if(strcmp(pwd_encoded, pwd_stored) != 0)
                    validate_success = 0;
                tmq_str_free(pwd_stored);
                free(pwd_encoded);
            }
        }
    }
    tmq_str_free(allow_anonymous);
    /* validate_in_progress=1 means validation is performed in thread pool */
    if(validate_in_progress)
        return;
    if(!validate_success)
    {
        tmq_connack_pkt pkt = {
                .return_code = NOT_AUTHORIZED,
                .ack_flags = 0
        };
        send_conn_ack_packet(conn, &pkt);
        tmq_connect_pkt_cleanup(connect_pkt);
        free(connect_pkt);
        return;
    }
    session_req req = {
            .op = SESSION_CONNECT,
            .connect_req = {
                    .conn = TCP_CONN_SHARE(conn),
                    .connect_pkt = connect_pkt
            }
    };
    session_operation_t* operation = malloc(sizeof(session_operation_t));
    operation->broker = broker;
    operation->req = req;
    tmq_executor_post(&broker->executor, handle_session_req, operation, 1);
}

void mqtt_disconnect(tmq_broker_t* broker, tmq_session_t* session)
{
    session_req req = {
            .op = SESSION_DISCONNECT,
            .session = session
    };
    session_operation_t* ctx = malloc(sizeof(session_operation_t));
    ctx->broker = broker;
    ctx->req = req;
    tmq_executor_post(&broker->executor, handle_session_req, ctx, 1);
}

void mqtt_subscribe(tmq_broker_t* broker, tmq_str_t client_id, tmq_subscribe_pkt* subscribe_pkt)
{
    topic_req req = {
            .op = TOPIC_SUBSCRIBE,
            .client_id = client_id,
            .subscribe_pkt = *subscribe_pkt
    };
    topic_operation_t* operation = malloc(sizeof(topic_operation_t));
    operation->broker = broker;
    operation->req = req;
    tmq_executor_post(&broker->executor, handle_topic_req, operation, 1);
}

void mqtt_unsubscribe(tmq_broker_t* broker, tmq_str_t client_id,
                              tmq_unsubscribe_pkt* unsubscribe_pkt)
{
    topic_req req = {
            .op = TOPIC_UNSUBSCRIBE,
            .client_id = client_id,
            .unsubscribe_pkt = *unsubscribe_pkt
    };
    topic_operation_t* ctx = malloc(sizeof(topic_operation_t));
    ctx->broker = broker;
    ctx->req = req;
    tmq_executor_post(&broker->executor, handle_topic_req, ctx,1);
}

void mqtt_publish(void* arg, tmq_session_t* session, char* topic, tmq_message* message, uint8_t retain)
{
    tmq_broker_t* broker = arg;
    if(broker->acl_enabled && tmq_acl_auth(&broker->acl, session, topic, PUB) == DENY)
        return;
    publish_req req = {
            .topic = topic,
            .message = *message,
            .retain = retain
    };
    publish_ctx_t* ctx = malloc(sizeof(publish_ctx_t));
    ctx->broker = broker;
    ctx->req = req;
    tmq_executor_post(&broker->executor, handle_publish_req, ctx, 0);
}

static void mqtt_broadcast(tmq_broker_t* broker, char* topic, tmq_message* message, subscribe_map_t* subscribers)
{
    broadcast_ctx_t** broadcast_tasks = malloc(sizeof(broadcast_ctx_t*) * broker->io_threads);
    for(int i = 0; i < broker->io_threads; i++)
    {
        broadcast_tasks[i] = malloc(sizeof(broadcast_ctx_t));
        broadcast_tasks[i]->topic = tmq_str_new(topic);
        broadcast_tasks[i]->message.message = tmq_str_new(message->message);
        broadcast_tasks[i]->message.qos = message->qos;
        broadcast_tasks[i]->retain = 0;
        tmq_vec_init(&broadcast_tasks[i]->subscribers, subscribe_info_t);
    }
    tmq_map_iter_t iter = tmq_map_iter(*subscribers);
    for(; tmq_map_has_next(iter); tmq_map_next(*subscribers, iter))
    {
        subscribe_info_t* info = iter.second;
        SESSION_SHARE(info->session);
        int io_context_idx = info->session->conn->io_context->index;
        tmq_vec_push_back(broadcast_tasks[io_context_idx]->subscribers, *info);
    }
    for(int i = 0; i < broker->io_threads; i++)
        tmq_mailbox_push(&broker->io_contexts[i].broadcast_tasks, broadcast_tasks[i]);
    free(broadcast_tasks);
}

int tmq_broker_init(tmq_broker_t* broker, const char* cfg)
{
    if(!broker) return -1;
    bzero(broker, sizeof(tmq_broker_t));
    /* read tinymqtt configure file */
    if(tmq_config_init(&broker->conf, cfg, "=") == 0)
        tlog_info("read config file %s ok", cfg);
    else
    {
        tlog_error("read config file error");
        return -1;
    }
    broker->acl_enabled = 0;
    tmq_str_t acl_enable = tmq_config_get(&broker->conf, "acl_enable");
    if(acl_enable && tmq_str_equal(acl_enable, "true"))
        broker->acl_enabled = 1;
    tmq_str_free(acl_enable);
    /* if mysql is configured, initialize mysql connection pool and load acl rules */
    tmq_str_t mysql_enable = tmq_config_get(&broker->conf, "mysql_enable");
    if(mysql_enable && tmq_str_equal(mysql_enable, "true"))
    {
        broker->mysql_enabled = 1;
        tmq_str_t mysql_host = tmq_config_get(&broker->conf, "mysql_host");
        tmq_str_t mysql_pt = tmq_config_get(&broker->conf, "mysql_port");
        tmq_str_t mysql_user = tmq_config_get(&broker->conf, "mysql_username");
        tmq_str_t mysql_pwd = tmq_config_get(&broker->conf, "mysql_password");
        tmq_str_t mysql_db = tmq_config_get(&broker->conf, "mysql_db");
        tmq_str_t mysql_pool_size = tmq_config_get(&broker->conf, "mysql_pool_size");
        const char* host = mysql_host ? mysql_host: DEFAULT_MYSQL_HOST;
        int port = mysql_pt ? atoi(mysql_pt): DEFAULT_MYSQL_PORT;
        if(!mysql_user)
        {
            tlog_error("mysql username isn't specified");
            return -1;
        }
        if(!mysql_pwd)
        {
            tlog_error("mysql password isn't specified");
            return -1;
        }
        const char* db_name = mysql_db ? mysql_db: DEFAULT_MYSQL_DB;
        int pool_size = mysql_pool_size ? atoi(mysql_pool_size): DEFAULT_MYSQL_POOL_SIZE;
        tmq_mysql_conn_pool_init(&broker->mysql_pool, host, port, mysql_user, mysql_pwd,db_name, pool_size);
        tmq_acl_init(&broker->acl, DENY);
        load_acl_from_mysql(tmq_mysql_conn_pool_pop(&broker->mysql_pool), &broker->acl);
        tmq_str_free(mysql_host);
        tmq_str_free(mysql_pt);
        tmq_str_free(mysql_user);
        tmq_str_free(mysql_pwd);
        tmq_str_free(mysql_db);
        tmq_str_free(mysql_pool_size);
    }
    /* otherwise, use password file and disable acl */
    else
    {
        tmq_str_t pwd_file_path = tmq_config_get(&broker->conf, "password_file");
        if(!pwd_file_path)
            pwd_file_path = tmq_str_new("pwd.conf");
        if(tmq_config_init(&broker->pwd_conf, pwd_file_path, ":") == 0)
            tlog_info("read password file %s ok", pwd_file_path);
        else
        {
            tlog_error("read password file error");
            tmq_str_free(pwd_file_path);
            return -1;
        }
        tmq_str_free(pwd_file_path);
        broker->acl_enabled = 0;
    }
    tmq_str_free(mysql_enable);

    tmq_str_t mongodb_uri = tmq_config_get(&broker->conf, "mongodb_uri");
    if(mongodb_uri)
    {
        mongoc_init();
        bson_error_t error;
        mongoc_uri_t* uri = mongoc_uri_new_with_error(mongodb_uri, &error);
        if(!uri)
            fatal_error("mongodb uri error: %s", error.message);
        broker->mongodb_pool = mongoc_client_pool_new(uri);
        mongoc_client_pool_set_error_api(broker->mongodb_pool, 2);
    }
    tmq_str_free(mongodb_uri);

    tmq_event_loop_init(&broker->loop);
    tmq_codec_init(&broker->codec, SERVER_CODEC);

    tmq_str_t port_str = tmq_config_get(&broker->conf, "port");
    unsigned int port = port_str ? strtoul(port_str, NULL, 10): 1883;
    tmq_str_free(port_str);
    tlog_info("listening on port %u", port);

    tmq_str_t inflight_window_str = tmq_config_get(&broker->conf, "inflight_window");
    broker->inflight_window_size = inflight_window_str ? strtoul(inflight_window_str, NULL, 10): 1;
    tmq_str_free(inflight_window_str);

    tmq_acceptor_init(&broker->acceptor, &broker->loop, port);
    tmq_acceptor_set_cb(&broker->acceptor, dispatch_new_connection, broker);

    /* initialize io contexts */
    tmq_str_t io_group_num_str = tmq_config_get(&broker->conf, "io_threads");
    broker->io_threads = io_group_num_str ? strtoul(io_group_num_str, NULL, 10): DEFAULT_IO_THREADS;
    tmq_str_free(io_group_num_str);
    tlog_info("start with %d io threads", broker->io_threads);
    broker->io_contexts = malloc(sizeof(tmq_io_context_t) * broker->io_threads);
    for(int i = 0; i <  broker->io_threads; i++)
        tmq_io_context_init(&broker->io_contexts[i], broker, i);
    broker->next_io_context = 0;

    tmq_executor_init(&broker->executor, 2);
    broker->thread_pool = thrdpool_create(10, 0);

    tmq_map_str_init(&broker->sessions, tmq_session_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_topics_init(&broker->topics_tree, broker, mqtt_broadcast);

    /* ignore SIGPIPE signal */
    signal(SIGPIPE, SIG_IGN);
    return 0;
}

void tmq_broker_run(tmq_broker_t* broker)
{
    if(!broker) return;
    tmq_executor_run(&broker->executor);

    for(int i = 0; i <  broker->io_threads; i++)
        tmq_io_context_run(&broker->io_contexts[i]);

    tmq_acceptor_listen(&broker->acceptor);
    tmq_event_loop_run(&broker->loop);

    /* clean up */
    tmq_acceptor_destroy(&broker->acceptor);
    tmq_config_destroy(&broker->conf);
    tmq_config_destroy(&broker->pwd_conf);
    for(int i = 0; i <  broker->io_threads; i++)
    {
        tmq_io_context_stop(&broker->io_contexts[i]);
        tmq_io_context_destroy(&broker->io_contexts[i]);
    }
    tmq_executor_stop(&broker->executor);
    tmq_event_loop_destroy(&broker->loop);
}
