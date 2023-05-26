cmake_minimum_required(VERSION 3.8)

project(tinyMQTT)

set(TINYMQTT_LIB_SRC
        base/mqtt_socket.c
        net/mqtt_buffer.c
        base/mqtt_map.c
        base/mqtt_vec.c
        base/mqtt_str.c
        base/mqtt_config.c
        base/mqtt_cmd.c
        event/mqtt_event.c
        event/mqtt_timer.c
        net/mqtt_acceptor.c
        net/mqtt_tcp_conn.c
        mqtt/mqtt_packet.c
        mqtt/mqtt_session.c
        mqtt/mqtt_codec.c
        mqtt/mqtt_broker.c
        mqtt/mqtt_client.c
        3rd/tinylog/tlog.c
        3rd/md5-c/md5.c)
set(CMAKE_BUILD_TYPE "Debug")

add_definitions(-D_GNU_SOURCE)

include_directories(${CMAKE_SOURCE_DIR} 3rd/tinylog 3rd/md5-c)

add_library(mqtt ${TINYMQTT_LIB_SRC})

link_directories(${CMAKE_BINARY_DIR})
link_libraries(mqtt pthread)

add_subdirectory(test)
add_executable(broker broker.c)
add_executable(tinymqtt_ctl mqtt_ctl.c)
add_executable(client client.c)

