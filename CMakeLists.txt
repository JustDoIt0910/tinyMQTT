cmake_minimum_required(VERSION 3.8)

project(tinyMQTT)

find_package(PkgConfig)
pkg_search_module(LIBMONGOC REQUIRED libmongoc-1.0)
MESSAGE(STATUS "libmongoc dirs:" ${LIBMONGOC_INCLUDE_DIRS})
MESSAGE(STATUS "libmongoc lib:" ${LIBMONGOC_LIBRARIES})

pkg_search_module(LIBMYSQLCLIENT REQUIRED mysqlclient)
MESSAGE(STATUS "libmysqlclient dirs:" ${LIBMYSQLCLIENT_INCLUDE_DIRS})
MESSAGE(STATUS "libmysqlclient lib:" ${LIBMYSQLCLIENT_LIBRARIES})

pkg_search_module(HIREDIS REQUIRED hiredis)
MESSAGE(STATUS "hiredis dirs:" ${HIREDIS_INCLUDE_DIRS})
MESSAGE(STATUS "hiredis lib:" ${HIREDIS_LIBRARIES})

include_directories(${LIBMONGOC_INCLUDE_DIRS})
include_directories(${LIBMYSQLCLIENT_INCLUDE_DIRS})
include_directories(${HIREDIS_INCLUDE_DIRS})

set(TINYMQTT_LIB_SRC
        net/mqtt_socket.c
        base/mqtt_util.c
        base/mqtt_map.c
        base/mqtt_vec.c
        base/mqtt_str.c
        base/mqtt_config.c
        base/mqtt_cmd.c
        ev/mqtt_event.c
        ev/mqtt_timer.c
        net/mqtt_acceptor.c
        net/mqtt_connector.c
        net/mqtt_tcp_conn.c
        net/mqtt_buffer.c
        mqtt/mqtt_packet.c
        mqtt/mqtt_session.c
        mqtt/mqtt_topic.c
        codec/mqtt_codec.c
        codec/mqtt_proto_codec.c
        codec/mqtt_console_codec.c
        codec/mqtt_cluster_codec.c
        mqtt/mqtt_io_context.c
        mqtt/mqtt_executor.c
        mqtt/mqtt_broker.c
        mqtt/mqtt_client.c
        mqtt/mqtt_acl.c
        store/mqtt_msg_store.c
        cluster/mqtt_discovery.c
        cluster/mqtt_cluster.c
        db/mqtt_db.c
        db/mqtt_conn_pool.c
        forward/mqtt_rule_parser.c
        forward/mqtt_events.c
        forward/mqtt_event_source.c
        forward/adaptors/mqtt_rabbitmq_adaptor.c
        thrdpool/msgqueue.c
        thrdpool/thrdpool.c
        3rd/tinylog/tlog.c
        3rd/md5-c/md5.c
        3rd/b64.c/encode.c
        3rd/b64.c/buffer.c)
set(CMAKE_BUILD_TYPE "Debug")

add_definitions(-D_GNU_SOURCE)

include_directories(${CMAKE_SOURCE_DIR} 3rd/tinylog 3rd/md5-c 3rd/b64.c)

add_library(mqtt ${TINYMQTT_LIB_SRC})
target_link_libraries (mqtt PRIVATE
        ${LIBMONGOC_LIBRARIES}
        ${LIBMYSQLCLIENT_LIBRARIES}
        ${HIREDIS_LIBRARIES}
        uuid
        cjson)

link_directories(${CMAKE_BINARY_DIR})
link_libraries(mqtt pthread)

add_subdirectory(test)
add_subdirectory(example)
add_executable(broker broker.c)
add_executable(tinymqtt_pwd tools/tinymqtt_pwd.c)
add_executable(tinymqtt_pub tools/tinymqtt_pub.c)
add_executable(tinymqtt_sub tools/tinymqtt_sub.c)
add_executable(tinymqtt_shell tools/tinymqtt_shell.c tools/mqtt_console_cmd.c)

