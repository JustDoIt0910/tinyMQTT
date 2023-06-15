//
// Created by zr on 23-4-9.
//
#include "mqtt_broker.h"
#include "mqtt_session.h"
#include "base/mqtt_util.h"
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

static void dispatch_new_connection(tmq_socket_t conn, void* arg)
{
    tmq_broker_t* broker = (tmq_broker_t*) arg;

    /* dispatch tcp connection using round-robin */
    tmq_io_group_t* next_group = &broker->io_groups[broker->next_io_group++];
    if(broker->next_io_group >= MQTT_IO_THREAD)
        broker->next_io_group = 0;

    pthread_mutex_lock(&next_group->pending_conns_lk);
    tmq_vec_push_back(next_group->pending_conns, conn);
    pthread_mutex_unlock(&next_group->pending_conns_lk);

    tmq_notifier_notify(&next_group->new_conn_notifier);
}

/* Construct a result of connecting request and notify the corresponding io thread.
 * Called by the broker thread. */
static void make_connect_respond(tmq_io_group_t* group, tmq_tcp_conn_t* conn,
                                 connack_return_code code, tmq_session_t* session, int sp)
{
    session_connect_resp resp = {
            .conn = get_ref(conn),
            .return_code = code,
            .session = session,
            .session_present = sp
    };
    pthread_mutex_lock(&group->connect_resp_lk);
    tmq_vec_push_back(group->connect_resp, resp);
    pthread_mutex_unlock(&group->connect_resp_lk);

    tmq_notifier_notify(&group->connect_resp_notifier);
}

static void mqtt_publish_deliver(void* arg, char* topic, tmq_message* message, uint8_t retain);

static void start_session(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt* connect_pkt)
{
    tmq_connect_pkt_print(connect_pkt);
    /* validate username and password if anonymous login is not allowed */
    int success = 1;
    tmq_str_t allow_anonymous = tmq_config_get(&broker->conf, "allow_anonymous");
    if(strcmp(allow_anonymous, "true") != 0)
    {
        if(!tmq_config_exist(&broker->pwd_conf, connect_pkt->username))
        {
            success = 0;
            make_connect_respond(conn->group, conn, NOT_AUTHORIZED, NULL, 0);
        }
        else
        {
            tmq_str_t pwd_stored = tmq_config_get(&broker->pwd_conf, connect_pkt->username);
            char* pwd_encoded = password_encode(connect_pkt->password);
            if(strcmp(pwd_encoded, pwd_stored) != 0)
            {
                success = 0;
                make_connect_respond(conn->group, conn, NOT_AUTHORIZED, NULL, 0);
            }
            tmq_str_free(pwd_stored);
            free(pwd_encoded);
        }
    }
    if(!success) goto cleanup;
    /* try to start a clean session */
    if(CONNECT_CLEAN_SESSION(connect_pkt->flags))
    {
        tmq_session_t** session = tmq_map_get(broker->sessions, connect_pkt->client_id);
        /* if there is a session in the broker associate with this client id */
        if(session)
        {
            /* if this session has already closed, just clean it up and create a new one. */
            if((*session)->state == CLOSED)
            {
                assert((*session)->clean_session == 0);
                /* todo: clear the session data */
                free(*session);
                tmq_session_t* new_session = tmq_session_new(broker, mqtt_publish_deliver,
                                                             conn, connect_pkt->client_id, 1,
                                                             connect_pkt->keep_alive, broker->inflight_window_size);
                tmq_map_put(broker->sessions, connect_pkt->client_id, new_session);
                make_connect_respond(conn->group, conn, CONNECTION_ACCEPTED, new_session, 1);
            }
            /* if this session is still active, disconnect it and create a new one in the callback. */
            else
            {

            }
        }
        /* no existing session associate with this client id, just create a new one. */
        else
        {
            tmq_session_t* new_session = tmq_session_new(broker, mqtt_publish_deliver,
                                                         conn, connect_pkt->client_id, 1,
                                                         connect_pkt->keep_alive, broker->inflight_window_size);
            tmq_map_put(broker->sessions, connect_pkt->client_id, new_session);
            make_connect_respond(conn->group, conn, CONNECTION_ACCEPTED, new_session, 0);
        }
    }
    /* try to start a session */
    else
    {
        tmq_session_t** session = tmq_map_get(broker->sessions, connect_pkt->client_id);
        /* if there is a session in the broker associate with this client id */
        if(session)
        {
            /* if the session is closed, resume the old session. */
            if((*session)->state == CLOSED)
            {
                assert((*session)->clean_session == 0);
                (*session)->conn = get_ref(conn);
                (*session)->state = OPEN;
                (*session)->last_pkt_ts = time_now();
                make_connect_respond(conn->group, conn, CONNECTION_ACCEPTED, *session, 1);
            }
            /* if this session is still active, disconnect it
             * and bind it with the new tcp connection in the callback. */
            else
            {

            }
        }
        /* no existing session associate with this client id, just create a new one. */
        else
        {
            tmq_session_t* new_session = tmq_session_new(broker, mqtt_publish_deliver,
                                                         conn, connect_pkt->client_id, 0,
                                                         connect_pkt->keep_alive, broker->inflight_window_size);
            tmq_map_put(broker->sessions, connect_pkt->client_id, new_session);
            make_connect_respond(conn->group, conn, CONNECTION_ACCEPTED, new_session, 0);
        }
    }
    cleanup:
    tmq_str_free(allow_anonymous);
    tmq_connect_pkt_cleanup(connect_pkt);
}

/* handle session creating and closing */
static void handle_session_ctl(void* arg)
{
    tmq_broker_t* broker = arg;

    session_ctl_list ctls = tmq_vec_make(session_ctl);
    pthread_mutex_lock(&broker->session_ctl_lk);
    tmq_vec_swap(ctls, broker->session_ctl_reqs);
    pthread_mutex_unlock(&broker->session_ctl_lk);

    for(session_ctl* ctl = tmq_vec_begin(ctls); ctl != tmq_vec_end(ctls); ctl++)
    {
        /* handle connect request */
        if(ctl->op == SESSION_CONNECT)
        {
            session_connect_req *connect_req = &ctl->context.start_req;
            /* try to start a mqtt session */
            start_session(broker, connect_req->conn, &connect_req->connect_pkt);
            release_ref(connect_req->conn);
        }
        /* handle disconnect request */
        else if(ctl->op == SESSION_DISCONNECT)
        {
            tmq_session_t* session = ctl->context.session;
            session->state = CLOSED;
            release_ref(session->conn);
            session->conn = NULL;
            /* todo: clean up the session context and remove the session if it is a clean session */
            tlog_info("session %p closed", session);
        }
        else
        {
            tmq_session_t* session = ctl->context.session;
            session->state = CLOSED;
            release_ref(session->conn);
            session->conn = NULL;
            /* todo: clean up the session context and remove the session if it is a clean session */
            tlog_info("session %p force closed", session);
        }
    }
    tmq_vec_free(ctls);
}

/* handle subscribe/unsubscribe/publish requests */
static void handle_message_ctl(void* arg)
{
    tmq_broker_t* broker = arg;

    message_ctl_list ctls = tmq_vec_make(message_ctl);
    pthread_mutex_lock(&broker->message_ctl_lk);
    tmq_vec_swap(ctls, broker->message_ctl_reqs);
    pthread_mutex_unlock(&broker->message_ctl_lk);

    for(message_ctl * ctl = tmq_vec_begin(ctls); ctl != tmq_vec_end(ctls); ctl++)
    {
        if(ctl->op == SUBSCRIBE || ctl->op == UNSUBSCRIBE)
        {
            subscribe_unsubscribe_req req = ctl->context.sub_unsub_req;
            tmq_session_t** session = tmq_map_get(broker->sessions, req.client_id);
            if(!session || (*session)->state == CLOSED)
                continue;

            tmq_any_packet_t ack;
            if(ctl->op == SUBSCRIBE)
            {
                /* the subscription will always success. */
                tmq_suback_pkt* sub_ack = malloc(sizeof(tmq_suback_pkt));
                sub_ack->packet_id = req.sub_unsub_pkt.subscribe_pkt.packet_id;

                /* add all the topic filters into the topic tree. */
                topic_filter_qos* tf = tmq_vec_begin(req.sub_unsub_pkt.subscribe_pkt.topics);
                for(; tf != tmq_vec_end(req.sub_unsub_pkt.subscribe_pkt.topics); tf++)
                {
                    tlog_info("subscribe{client=%s, topic=%s, qos=%u}", req.client_id, tf->topic_filter, tf->qos);
                    retain_message_list retain = tmq_topics_add_subscription(&broker->topics_tree, tf->topic_filter,
                                                                             req.client_id, tf->qos);
                    /* send the retained messages that match the subscription */
                    for(retain_message_t** it = tmq_vec_begin(retain); it != tmq_vec_end(retain); it++)
                    {
                        retain_message_t* retain_msg = *it;
                        uint8_t final_qos = tf->qos < retain_msg->retain_msg.qos ? tf->qos :
                                retain_msg->retain_msg.qos;
                        tmq_session_publish(*session, retain_msg->retain_topic,
                                            retain_msg->retain_msg.message, final_qos, 1);
                    }
                    tmq_vec_free(retain);
                    tmq_topics_info(&broker->topics_tree);
                    tmq_vec_push_back(sub_ack->return_codes, tf->qos);
                }
                tmq_subscribe_pkt_cleanup(&req.sub_unsub_pkt.subscribe_pkt);
                ack.packet_type = MQTT_SUBACK;
                ack.packet = sub_ack;
                tmq_session_send_packet(*session, &ack, 0);
                /* todo: send retain messages */
            }
            else
            {
                tmq_unsuback_pkt* unsub_ack = malloc(sizeof(tmq_unsuback_pkt));
                unsub_ack->packet_id = req.sub_unsub_pkt.unsubscribe_pkt.packet_id;

                tmq_str_t* tf = tmq_vec_begin(req.sub_unsub_pkt.unsubscribe_pkt.topics);
                for(; tf != tmq_vec_end(req.sub_unsub_pkt.unsubscribe_pkt.topics); tf++)
                {
                    tlog_info("unsubscribe{client=%s, topic=%s}", req.client_id, *tf);
                    tmq_topics_remove_subscription(&broker->topics_tree, *tf, req.client_id);
                    tmq_topics_info(&broker->topics_tree);
                }
                tmq_unsubscribe_pkt_cleanup(&req.sub_unsub_pkt.unsubscribe_pkt);
                ack.packet_type = MQTT_UNSUBACK;
                ack.packet = unsub_ack;
                tmq_session_send_packet(*session, &ack, 0);
            }
            tmq_str_free(req.client_id);
        }
        else
        {
            publish_req req = ctl->context.pub_req;
            tmq_topics_publish(&broker->topics_tree, 0, req.topic, &req.message, req.retain);
            tmq_str_free(req.topic);
            tmq_str_free(req.message.message);
        }
    }
}

void mqtt_connect_request(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt* connect_pkt)
{
    session_connect_req req = {
            .conn = get_ref(conn),
            .connect_pkt = *connect_pkt
    };
    session_ctl ctl = {
            .op = SESSION_CONNECT,
            .context.start_req = req
    };
    pthread_mutex_lock(&broker->session_ctl_lk);
    tmq_vec_push_back(broker->session_ctl_reqs, ctl);
    pthread_mutex_unlock(&broker->session_ctl_lk);

    tmq_notifier_notify(&broker->session_ctl_notifier);
}

void mqtt_disconnect_request(tmq_broker_t* broker, tmq_session_t* session)
{
    session_ctl ctl = {
            .op = SESSION_DISCONNECT,
            .context.session = session
    };
    pthread_mutex_lock(&broker->session_ctl_lk);
    tmq_vec_push_back(broker->session_ctl_reqs, ctl);
    pthread_mutex_unlock(&broker->session_ctl_lk);

    tmq_notifier_notify(&broker->session_ctl_notifier);
}

void mqtt_subscribe_unsubscribe_request(tmq_broker_t* broker, subscribe_unsubscribe_req* sub_unsub_req,
                                        message_ctl_op op)
{
    message_ctl ctl = {
            .op = op,
            .context.sub_unsub_req = *sub_unsub_req
    };
    pthread_mutex_lock(&broker->message_ctl_lk);
    tmq_vec_push_back(broker->message_ctl_reqs, ctl);
    pthread_mutex_unlock(&broker->message_ctl_lk);

    tmq_notifier_notify(&broker->message_ctl_notifier);
}

void mqtt_publish_deliver(void* arg, char* topic, tmq_message* message, uint8_t retain)
{
    tmq_broker_t* broker = arg;

    message_ctl ctl = {
            .op = PUBLISH,
            .context.pub_req ={
                    .topic = tmq_str_new(topic),
                    .message = *message,
                    .retain = retain
            }
    };

    pthread_mutex_lock(&broker->message_ctl_lk);
    tmq_vec_push_back(broker->message_ctl_reqs, ctl);
    pthread_mutex_unlock(&broker->message_ctl_lk);

    tmq_notifier_notify(&broker->message_ctl_notifier);
}

static void mqtt_publish_forward(tmq_broker_t* broker, char* client_id,
                                 char* topic, uint8_t required_qos, tmq_message* message)
{
    tmq_session_t** session = tmq_map_get(broker->sessions, client_id);
    if(!session) return;
    /* if this session isn't active, save this message in its context */
    if((*session)->state == CLOSED)
    {

    }
    else
    {
        uint8_t final_qos = required_qos < message->qos ? required_qos : message->qos;
        tmq_session_publish(*session, topic, message->message, final_qos, 0);
    }
}

int tmq_broker_init(tmq_broker_t* broker, const char* cfg)
{
    if(!broker) return -1;
    if(tmq_config_init(&broker->conf, cfg, "=") == 0)
        tlog_info("read config file %s ok", cfg);
    else
    {
        tlog_error("read config file error");
        return -1;
    }
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

    for(int i = 0; i < MQTT_IO_THREAD; i++)
        tmq_io_group_init(&broker->io_groups[i], broker);
    broker->next_io_group = 0;

    if(pthread_mutex_init(&broker->session_ctl_lk, NULL))
        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));
    if(pthread_mutex_init(&broker->message_ctl_lk, NULL))
        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));

    tmq_vec_init(&broker->session_ctl_reqs, session_ctl);
    tmq_vec_init(&broker->message_ctl_reqs, message_ctl);

    tmq_notifier_init(&broker->session_ctl_notifier, &broker->loop, handle_session_ctl, broker);
    tmq_notifier_init(&broker->message_ctl_notifier, &broker->loop, handle_message_ctl, broker);

    tmq_map_str_init(&broker->sessions, tmq_session_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_topics_init(&broker->topics_tree, broker, mqtt_publish_forward);

    /* ignore SIGPIPE signal */
    signal(SIGPIPE, SIG_IGN);
    return 0;
}

void tmq_broker_run(tmq_broker_t* broker)
{
    if(!broker) return;
    for(int i = 0; i < MQTT_IO_THREAD; i++)
        tmq_io_group_run(&broker->io_groups[i]);
    tmq_acceptor_listen(&broker->acceptor);
    tmq_event_loop_run(&broker->loop);

    /* clean up */
    tmq_acceptor_destroy(&broker->acceptor);
    tmq_config_destroy(&broker->conf);
    tmq_config_destroy(&broker->pwd_conf);
    for(int i = 0; i < MQTT_IO_THREAD; i++)
    {
        tmq_io_group_stop(&broker->io_groups[i]);
        pthread_join(broker->io_groups[i].io_thread, NULL);
    }
    tmq_event_loop_destroy(&broker->loop);
}