# tinyMQTT(重构中...)

## MQTT v3.1.1协议的消息broker和client

编译
```shell
git clone https://github.com/JustDoIt0910/tinyMQTT.git
cd tinyMQTT
mkdir build && cd build
cmake ..
make
```

编译生成
- libmqtt.a --------- 静态库
- broker ------------- MQTT服务器
- tinymqtt_pwd----用于添加、修改、删除用户密码的工具
- tinymqtt_pub---- 发布命令行工具
- tinymqtt_sub-----订阅命令行工具

#### 启动broker
在broker可执行文件目录下添加 tinymqtt.conf 配置文件:
```nginx configuration
# server port
port=1883
# location of the password file
password_file=pwd.conf
# if allow anonymous login, the broker won't check the username and password
allow_anonymous=true
# the maximum number of unacknowalegded publish packet
inflight_window=1
# the number of io thread
io_threads=4

```

可以使用 tinymqtt_pwd 工具添加用户：

```shell
./tinymqtt_pwd -c ./pwd.conf -A new_user
```

如果设置了allow_anonymous 为true, broker将不再检查用户密码。

![p1](https://github.com/JustDoIt0910/MarkDownPictures/blob/main/tinyMQTT/p1.png)

#### 使用客户端库

客户端提供了同步和异步两种使用方式，详见example中的client_sync.c和client_async.c,，默认是采用同步模式的，所有操作(connect/subscribe/unsubscribe/publish)都是同步阻塞的，如果要持续监听消息，可以在程序最后调用tinymqtt_loop()，这会阻塞当前线程，收到消息会调用on_message回调。

如果要使用异步模式，需要先调用tinymqtt_loop_threaded()，这会单独创建一个io线程，此后所有操作都是非阻塞的，操作请求将会被内部队列转移到io线程执行，结果都以回调函数形式进行通知。

client_sync.c

```c
void on_message(char* topic, char* message, uint8_t qos, uint8_t retain)
{
    tlog_info("received message [%s] topic=%s, qos=%u, retain=%u", message, topic, qos, retain);
}

int main()
{
    tiny_mqtt* mqtt = tinymqtt_new("192.168.3.7", 1883);
    connect_options ops = {
        "username",
        "password",
        "tmq_client_test_client",
        1,
        60,
        NULL
    };
    int res = tinymqtt_connect(mqtt, &ops);
    if(res == CONNECTION_ACCEPTED)
    {
        tinymqtt_subscribe(mqtt, "test/sub", 1);
        //tinymqtt_unsubscribe(mqtt, "test/sub");

        tinymqtt_set_message_callback(mqtt, on_message);

        tinymqtt_publish(mqtt, "test/pub", "hello!", 1, 0);
    }

    tinymqtt_loop(mqtt);
    tinymqtt_destroy(mqtt);
    return 0;
}
```



client_async.c

```c
void on_message(char* topic, char* message, uint8_t qos, uint8_t retain)
{
    tlog_info("received message [%s] topic=%s, qos=%u, retain=%u", message, topic, qos, retain);
}

void on_connect(tiny_mqtt* mqtt, int return_code)
{
    if(return_code == CONNECTION_ACCEPTED)
    {
        tlog_info("CONNECTION_ACCEPTED");
        tinymqtt_subscribe(mqtt, "test/sub", 2);
    }
    else
        tlog_error("connect falied. return code=%d", return_code);
}

void on_disconnect(tiny_mqtt* mqtt)
{
    tlog_info("disconnected");
    tinymqtt_quit(mqtt);
}

int main()
{
    tiny_mqtt* mqtt = tinymqtt_new("192.168.3.7", 1883);
    connect_options ops = {
            "username",
            "password",
            "tmq_client_test_client",
            1,
            60,
            NULL
    };

    tinymqtt_set_message_callback(mqtt, on_message);
    tinymqtt_set_connect_callback(mqtt, on_connect);
    tinymqtt_set_disconnect_callback(mqtt, on_disconnect);

    tinymqtt_loop_threaded(mqtt);

    tinymqtt_connect(mqtt, &ops);

    tinymqtt_async_wait(mqtt);
    tinymqtt_destroy(mqtt);

    return 0;
}
```

### TODO
- [x] 处理client id为空的情况，broker生成默认client id
- [ ] 修复超时重传定时器的bug
- [ ] 添加SSL支持
- [ ] 支持集群部署
