//
// Created by just do it on 2024/1/12.
//
#include <assert.h>
#include "mqtt_db.h"
#include "mqtt/mqtt_acl.h"
#include "store/mqtt_msg_store.h"
#include "base/mqtt_util.h"

void store_messages_to_mongodb(mongoc_client_t* mongo_client, const char* mqtt_client_id, sending_packet_t* packets,
                               int n_packets)
{
    const bson_t** documents = malloc(sizeof(bson_t*) * n_packets);
    sending_packet_t* p = packets;
    for(int i = 0; i < n_packets; i++)
    {
        tmq_publish_pkt* publish_pkt = p->packet.packet_ptr;
        documents[i] = BCON_NEW("client_id", BCON_UTF8(mqtt_client_id),
                                "timestamp", BCON_INT64(p->store_timestamp),
                                "message", "{",
                                "packet_id", BCON_INT32(p->packet_id),
                                "flags", BCON_INT32(publish_pkt->flags),
                                "topic", BCON_UTF8(publish_pkt->topic),
                                "payload", BCON_UTF8(publish_pkt->payload),
                                "}");
        sending_packet_t* next = p->next;
        tmq_any_pkt_cleanup(&p->packet);
        free(p);
        p = next;
    }
    bson_error_t error;
    mongoc_collection_t* collection = mongoc_client_get_collection(mongo_client, "tinyMQTT_message_db", "messages");
    if(!mongoc_collection_insert_many(collection, documents, n_packets, NULL, NULL, &error))
        tlog_error("error occurred when storing messages to mongodb: %s", error.message);
    mongoc_collection_destroy(collection);
}

int fetch_messages_from_mongodb(mongoc_client_t* mongo_client, const char* mqtt_client_id, int limit,
                                sending_packet_t** result_head, sending_packet_t** result_tail)
{
    bson_t* filter = BCON_NEW("client_id", BCON_UTF8(mqtt_client_id));
    bson_t* opts = BCON_NEW("limit", BCON_INT64(limit), "sort", "{", "timestamp", BCON_INT32(1), "}");
    mongoc_collection_t* collection = mongoc_client_get_collection(mongo_client, "tinyMQTT_message_db", "messages");
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, filter, opts, NULL);
    bson_destroy(filter);
    bson_destroy(opts);

    bson_t delete_filter = BSON_INITIALIZER;
    bson_t delete_object_ids;
    const bson_t* doc;
    int cnt = 0;
    sending_packet_t* head = NULL;
    sending_packet_t** p = &head;
    bson_append_array_begin(&delete_filter, "$in", strlen("$in"), &delete_object_ids);
    while (mongoc_cursor_next(cursor, &doc))
    {
        bson_iter_t iter;
        if(bson_iter_init(&iter, doc))
        {
            *p = malloc(sizeof(sending_packet_t));
            bzero(*p, sizeof(sending_packet_t));
            (*p)->packet.packet_type = MQTT_PUBLISH;
            tmq_publish_pkt* message = malloc(sizeof(tmq_publish_pkt));

            bson_iter_next(&iter);
            const bson_oid_t* oid = bson_iter_oid(&iter);
            char str[16];
            const char* key;
            bson_uint32_to_string(cnt++, &key, str, sizeof(str));
            bson_append_oid(&delete_object_ids, key, strlen(key), oid);
            bson_iter_next(&iter);
            bson_iter_next(&iter);
            bson_iter_next(&iter);
            bson_iter_t msg_iter;
            bson_iter_recurse(&iter, &msg_iter);
            bson_iter_next(&msg_iter);
            int32_t packet_id = bson_iter_int32(&msg_iter);
            (*p)->packet_id = packet_id;
            message->packet_id = packet_id;
            bson_iter_next(&msg_iter);
            message->flags = bson_iter_int32(&msg_iter);
            bson_iter_next(&msg_iter);
            message->topic = tmq_str_new(bson_iter_utf8(&msg_iter, 0));
            bson_iter_next(&msg_iter);
            message->payload = tmq_str_new(bson_iter_utf8(&msg_iter, 0));
            (*p)->packet.packet_ptr = message;
            p = &(*p)->next;
        }
    }
    mongoc_cursor_destroy (cursor);
    bson_append_array_end(&delete_filter, &delete_object_ids);
    bson_t* delete_query = BCON_NEW("_id", BCON_DOCUMENT(&delete_filter));
    mongoc_collection_delete_many(collection, delete_query, NULL, NULL, NULL);
    mongoc_collection_destroy(collection);
    bson_destroy(delete_query);
    *result_head = head;
    *result_tail = head ? (sending_packet_t*)p : NULL;
    return cnt;
}

static MYSQL_STMT* execute_statement(MYSQL* conn, const char* query, MYSQL_BIND* param_bind, MYSQL_BIND* result_bind)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
        fatal_error("mysql_stmt_init(), out of memory");
    if(mysql_stmt_prepare(stmt, query, strlen(query)))
        fatal_error("mysql_stmt_prepare() error");
    if(param_bind && mysql_stmt_bind_param(stmt, param_bind))
        fatal_error("mysql_stmt_bind_param() error");
    if(mysql_stmt_execute(stmt))
        fatal_error("mysql_stmt_execute() error");
    if(result_bind)
    {
        if(mysql_stmt_bind_result(stmt, result_bind))
            fatal_error("mysql_stmt_bind_result() error");
        if(mysql_stmt_store_result(stmt))
            fatal_error("mysql_stmt_store_result() error");
    }
    return stmt;
}

int mysql_add_user(MYSQL* mysql_conn, const char* username, const char* password)
{
    char find_user_query[100] = {0};
    sprintf(find_user_query, "SELECT id from tinymqtt_user_table WHERE username = '%s'", username);
    if(mysql_real_query(mysql_conn, find_user_query, strlen(find_user_query)) != 0)
    {
        tlog_error("mysql_real_query() error");
        return -1;
    }
    MYSQL_RES* res = mysql_store_result(mysql_conn);
    if(!res) return -1;
    // user already exists
    if(mysql_fetch_row(res))
        return -1;
    static const char* insert_user = "INSERT INTO tinymqtt_user_table(username, password) VALUES(?, ?)";
    MYSQL_BIND param_bind[2];
    unsigned long username_len = strlen(username);
    unsigned long password_len = strlen(password);
    bzero(param_bind, sizeof(param_bind));
    param_bind[0].buffer_type = param_bind[1].buffer_type = MYSQL_TYPE_STRING;
    param_bind[0].buffer = (char*)username;
    param_bind[0].buffer_length = username_len;
    param_bind[0].length = &username_len;
    param_bind[0].is_null = (bool*)0;
    param_bind[1].buffer = (char*)password;
    param_bind[1].buffer_length = password_len;
    param_bind[1].length = &password_len;
    param_bind[1].is_null = (bool*)0;

    MYSQL_STMT* stmt = execute_statement(mysql_conn, insert_user, param_bind, NULL);
    mysql_stmt_close(stmt);
    return 0;
}

int mysql_validate_connect_password(MYSQL* mysql_conn, const char* username, const char* password)
{
    static const char* pwd_query = "SELECT password from tinymqtt_user_table WHERE username = ? LIMIT 1";
    unsigned long username_len = strlen(username);
    MYSQL_BIND param_bind;
    bzero(&param_bind, sizeof(MYSQL_BIND));
    param_bind.buffer_type = MYSQL_TYPE_STRING;
    param_bind.buffer = (char*)username;
    param_bind.buffer_length = username_len;
    param_bind.length = &username_len;
    param_bind.is_null = (bool*)0;

    char stored_password[100] = {0};
    unsigned long password_len;
    bool password_null;
    MYSQL_BIND result_bind;
    bzero(&result_bind, sizeof(MYSQL_BIND));
    result_bind.buffer_type = MYSQL_TYPE_STRING;
    result_bind.buffer = stored_password;
    result_bind.buffer_length = 100;
    result_bind.length = &password_len;
    result_bind.is_null = &password_null;

    MYSQL_STMT* stmt = execute_statement(mysql_conn, pwd_query, &param_bind, &result_bind);
    int success = 0;
    if(!mysql_stmt_fetch(stmt))
    {
        char* password_encoded = password_encode((char*)password);
        if(strcmp(stored_password, password_encoded) == 0)
            success = 1;
        free(password_encoded);
    }
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return success;
}

void mysql_load_acl(MYSQL* mysql_conn, tmq_acl_t* acl)
{
    static const char* acl_query = "SELECT * FROM tinymqtt_acl_table";
    if(mysql_real_query(mysql_conn, acl_query, strlen(acl_query)) != 0)
    {
        tlog_error("mysql_real_query() error");
        return;
    }
    MYSQL_RES* res = mysql_store_result(mysql_conn);
    if(!res) return;
    MYSQL_ROW row = NULL;
    while((row = mysql_fetch_row(res)) != NULL)
    {
        unsigned int num_fields = mysql_num_fields(res);
        unsigned long* lengths = mysql_fetch_lengths(res);
        assert(num_fields == 7);
        tmq_permission_e permission = atoi(row[1]);
        tmq_access_e access = atoi(row[5]);
        if(lengths[2] > 0)
        {
            tlog_info("load acl rule: %s ip [%s] %s %s",
                      permission_str[permission], row[2], access_str[access], row[6]);
            tmq_acl_add_rule(acl, row[6], acl_ip_rule_new(permission, row[2], access));

        }
        else if(lengths[3] > 0)
        {
            tlog_info("load acl rule: %s user [%s] %s %s",
                      permission_str[permission], row[3], access_str[access], row[6]);
            tmq_acl_add_rule(acl, row[6], acl_username_rule_new(permission, row[3], access));
        }
        else if(lengths[4] > 0)
        {
            tlog_info("load acl rule: %s client_id [%s] %s %s",
                      permission_str[permission], row[4], access_str[access], row[6]);
            tmq_acl_add_rule(acl, row[6], acl_client_id_rule_new(permission, row[4], access));
        }
        else
        {
            tlog_info("load acl rule: %s all %s %s",
                      permission_str[permission], access_str[access], row[6]);
            tmq_acl_add_rule_for_all(acl, row[6], acl_all_rule_new(permission,  access));
        }
    }
    mysql_free_result(res);
}