cmake_minimum_required(VERSION 3.8)

project(tinyMQTT)

set(TINYMQTT_LIB_SRC
        mqtt_socket.c
        mqtt_buffer.c
        mqtt_map.c
        mqtt_vec.c
        mqtt_event.c
        mqtt_acceptor.c
        mqtt_tcp_conn.c
        mqtt_timer.c
        mqtt_broker.c
        mqtt_client.c
        3rd/tinylog/tlog.c)
set(CMAKE_BUILD_TYPE "Debug")

add_definitions(-D_GNU_SOURCE)

include_directories(${CMAKE_SOURCE_DIR} 3rd/tinylog)

add_library(mqtt ${TINYMQTT_LIB_SRC})

link_directories(${CMAKE_BINARY_DIR})
link_libraries(mqtt pthread)

add_subdirectory(test)
add_executable(broker broker.c)
add_executable(client client.c)

