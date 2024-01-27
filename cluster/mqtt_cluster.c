//
// Created by just do it on 2024/1/23.
//
#include <strings.h>
#include "codec/mqtt_cluster_codec.h"
#include "mqtt_cluster.h"
#include "mqtt/mqtt_broker.h"
#include "base/mqtt_util.h"
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_contexts.h"

void route_table_add_item(route_table_t* table, char* member_addr, const char* topic_filter, topic_tree_node_t* topic_tree_node)
{
    size_t real_size = topic_filter ? sizeof(route_table_item_t) + strlen(topic_filter) + 1: sizeof(route_table_item_t);
    route_table_item_t* item = malloc(real_size);
    bzero(item, real_size);
    item->pos_in_topic_tree = topic_tree_node;
    if(topic_filter)
        strcpy(item->topic_filter_name, topic_filter);
    route_table_item_t** items = tmq_map_get(*table, member_addr);
    if(!items)
    {
        tmq_map_put(*table, member_addr, item);
        topic_tree_node->route_table_item = tmq_map_get(*table, member_addr);
    }
    else
    {
        route_table_item_t* head = *items;
        if(head)
            head->pos_in_topic_tree->route_table_item = &item->next;
        topic_tree_node->route_table_item = items;
        item->next = head;
        *items = item;
    }
}

typedef struct cluster_route_sync_msg
{
    /* conn of cluster member to synchronize with */
    tmq_tcp_conn_t* conn;
    /* topic filter strings seperated by '\0' */
    tmq_str_t topic_filters;
} cluster_route_sync_msg;

static void sync_msg_add_topic(cluster_route_sync_msg* msg, const char* topic_filter)
{
    if(!msg->topic_filters)
        msg->topic_filters = tmq_str_new(topic_filter);
    else
        msg->topic_filters = tmq_str_append_str(msg->topic_filters, topic_filter);
    msg->topic_filters = tmq_str_append_char(msg->topic_filters, 0);
}

typedef struct cluster_publish_msg
{
    /* conn of cluster member to synchronize with */
    tmq_tcp_conn_t* conn;
    tmq_str_t topic;
    tmq_message message;
} cluster_publish_msg;

static void handle_cluster_member_operation(void* arg);
static void handle_cluster_route_operation(void* arg);
/*********************************************************************************************/
/* Handlers below are called in broker's main IO thread */
static void new_conn_handler(void* owner, tmq_mail_t mail)
{
    tmq_cluster_t* cluster = owner;
    tmq_socket_t sock = (tmq_socket_t)(intptr_t)(mail);
    tmq_tcp_conn_t* conn = tmq_tcp_conn_new(&cluster->broker->loop, NULL, sock, (tmq_codec_t*)&cluster->codec);
    tcp_conn_simple_ctx_t* ctx = malloc(sizeof(tcp_conn_simple_ctx_t));
    ctx->broker = cluster->broker;
    ctx->parsing_ctx.state = PARSING_HEADER;
    tmq_tcp_conn_set_context(conn, ctx, NULL);
    conn->state = CONNECTED;
    member_operation_t* member_op = malloc(sizeof(member_operation_t));
    member_op->op = MEMBER_ADD;
    member_op->cluster = cluster;
    member_op->member_conn = TCP_CONN_SHARE(conn);
    tmq_executor_post(&cluster->broker->executor, handle_cluster_member_operation, member_op, 0);
}

void add_route_message_handler(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_str_t topic_filters)
{
    char id[INET_ADDRSTRLEN + 7] = {0};
    tmq_tcp_conn_id(conn, id, sizeof(id));
    member_route_operation_t* route_op = malloc(sizeof(member_route_operation_t));
    route_op->op = ROUTE_ADD;
    route_op->cluster = &broker->cluster;
    route_op->member_addr = tmq_str_new(id);
    route_op->topic_filters = topic_filters;
    tmq_executor_post(&broker->executor, handle_cluster_route_operation, route_op, 0);
}

void send_route_sync_del_message(tmq_tcp_conn_t* conn, cluster_message_type type, tmq_str_t payload);
static void send_message_handler(void* owner, tmq_mail_t mail)
{
    tmq_cluster_t* cluster = owner;
    cluster_message_type message_type = (uintptr_t)(mail) & 0x07;
    struct {tmq_tcp_conn_t* conn;}* message = (void*)((uintptr_t)(mail) & ~0x07);
    if(message_type == CLUSTER_PUBLISH)
    {
        cluster_publish_msg* cluster_publish = (cluster_publish_msg*)message;
    }
    else
    {
        cluster_route_sync_msg* cluster_route_sync = (cluster_route_sync_msg*)message;
        send_route_sync_del_message(message->conn, message_type, cluster_route_sync->topic_filters);
    }
    TCP_CONN_RELEASE(message->conn);
    free(message);
}

/*********************************************************************************************/
/* Functions below are called in broker's logic thread */

/** @brief  Add a new member to cluster then synchronize local route table,
 *          or remove a member from cluster, it is called by broker's logic thread */
void handle_cluster_member_operation(void* arg)
{
    member_operation_t* member_op = arg;
    tmq_cluster_t* cluster = member_op->cluster;
    tmq_tcp_conn_t* conn = member_op->member_conn;
    if(member_op->op == MEMBER_ADD)
    {
        char id[INET_ADDRSTRLEN + 7] = {0};
        tmq_tcp_conn_id(conn, id, sizeof(id));
        printf("new member %s\n", id);
        tmq_map_put(cluster->members, id, TCP_CONN_SHARE(conn));
        /* synchronize local route table to new cluster member */
        route_table_item_t** items = tmq_map_get(cluster->route_table, "local");
        if(items)
        {
            cluster_route_sync_msg* sync_msg = malloc(sizeof(cluster_route_sync_msg));
            sync_msg->conn = TCP_CONN_SHARE(conn);
            sync_msg->topic_filters = NULL;
            for(route_table_item_t* item = *items; item; item = item->next)
                sync_msg_add_topic(sync_msg, item->topic_filter_name);
            sync_msg = (cluster_route_sync_msg*)((uintptr_t)(sync_msg) | CLUSTER_ROUTE_SYNC);
            tmq_mailbox_push(&cluster->message_send_box, sync_msg);
        }
    }
    TCP_CONN_RELEASE(conn);
    free(member_op);
}

/** @brief  Add new routes to the topic tree and route table, or remove routes from them,
 *          called by broker's logic thread */
void handle_cluster_route_operation(void* arg)
{
    member_route_operation_t* route_op = arg;
    tmq_cluster_t* cluster = route_op->cluster;
    const char* topic_filter = route_op->topic_filters;
    while(*topic_filter)
    {
        topic_tree_node_t* topic_node;
        tmq_topics_add_route(&cluster->broker->topics_tree, route_op->topic_filters, route_op->member_addr, &topic_node);
        route_table_add_item(&cluster->route_table, route_op->member_addr, topic_filter, topic_node);
        topic_filter += strlen(topic_filter) + 1;
    }
    tmq_str_free(route_op->topic_filters);
    tmq_str_free(route_op->member_addr);
    free(route_op);
}

void mqtt_route_publish(tmq_broker_t* broker, char* topic, tmq_message* message, member_addr_set* matched_members)
{
    for(tmq_map_iter_t it = tmq_map_iter(*matched_members); tmq_map_has_next(it); tmq_map_next(*matched_members, it))
        printf("{%s, %s} ==> %s\n", message->message, topic, (char*)it.first);
}

/*********************************************************************************************/

/** @brief This function is called when a new cluster member is discovered,
 *         it's called in the thread used to listen redis publish event */
static void on_new_member_discovered(void* arg, const char* ip, uint16_t port)
{
    tmq_cluster_t* cluster = arg;
    tmq_socket_t sock = tmq_tcp_socket(0);
    tmq_socket_addr_t peer_addr = tmq_addr_from_ip_port(ip, port);
    if(tmq_socket_connect(sock, peer_addr) < 0)
        fatal_error("connect to peer %s error: %s", ip, strerror(errno));
    tmq_socket_nonblocking(sock);
    tmq_mailbox_push(&cluster->new_conn_box, (tmq_mail_t)(intptr_t)(sock));
}

static void on_member_connect(tmq_socket_t conn, void* arg)
{
    new_conn_handler(arg, (tmq_mail_t)(intptr_t)conn);
}

void tmq_cluster_init(tmq_broker_t* broker, tmq_cluster_t* cluster, const char* redis_ip, uint16_t redis_port,
                           const char* node_ip, uint16_t node_port)
{
    bzero(cluster, sizeof(tmq_cluster_t));
    cluster->broker = broker;
    tmq_map_str_init(&cluster->route_table, route_table_item_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_map_str_init(&cluster->members, tmq_tcp_conn_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_mailbox_init(&cluster->new_conn_box, &broker->loop, cluster, new_conn_handler);
    tmq_mailbox_init(&cluster->message_send_box, &broker->loop, cluster, send_message_handler);
    tmq_cluster_codec_init(&cluster->codec);

    tmq_redis_discovery_set_context(&cluster->discovery, cluster);
    tmq_redis_discovery_init(&broker->loop, &cluster->discovery, redis_ip, redis_port, on_new_member_discovered, NULL);
    tmq_redis_discovery_set_context(&cluster->discovery, cluster);

    tmq_acceptor_init(&cluster->acceptor, &broker->loop, node_port);
    tmq_acceptor_set_cb(&cluster->acceptor, on_member_connect, cluster);
    tmq_acceptor_listen(&cluster->acceptor);

    // TODO use distributed lock to make "register" and "listen" operations atomic
    tmq_redis_discovery_register(&cluster->discovery, node_ip, node_port);
    tmq_redis_discovery_listen(&cluster->discovery);
}

/** @brief  add a new route item to local route table and synchronize this route to cluster members,
 *          it is called by broker's logic thread */
void tmq_cluster_add_route(tmq_cluster_t* cluster, const char* topic_filter, topic_tree_node_t* topic_node)
{
    /* first, add a new route item to local route table */
    route_table_add_item(&cluster->route_table, "local", topic_filter, topic_node);
    /* second, send "route sync" message to other cluster members  */
    tmq_map_iter_t iter = tmq_map_iter(cluster->members);
    for(; tmq_map_has_next(iter); tmq_map_next(cluster->members, iter))
    {
        cluster_route_sync_msg* sync_msg = malloc(sizeof(cluster_route_sync_msg));
        sync_msg->conn = TCP_CONN_SHARE(*(tmq_tcp_conn_t**)iter.second);
        sync_msg->topic_filters = tmq_str_new(topic_filter);
        sync_msg = (cluster_route_sync_msg*)((uintptr_t)(sync_msg) | CLUSTER_ROUTE_SYNC);
        tmq_mailbox_push(&cluster->message_send_box, sync_msg);
    }
}