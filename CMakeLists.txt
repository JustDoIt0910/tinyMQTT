cmake_minimum_required(VERSION 3.8)

project(tinyMQTT)

set(TINYMQTT_LIB_SRC mqtt_socket.c
        mqtt_buffer.c
        mqtt_map.c
        mqtt_loop.c
        mqtt_broker.c
        mqtt_client.c
        3rd/tinylog/tlog.c)

include_directories(3rd/tinylog)

add_library(mqtt ${TINYMQTT_LIB_SRC})

add_executable(broker broker.c)
add_executable(client client.c)
target_link_libraries(broker mqtt pthread)
target_link_libraries(client mqtt pthread)