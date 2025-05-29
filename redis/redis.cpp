#include <hiredis/hiredis.h>
#include <iostream>

int main() {
    // 1. 连接到Redis服务器（默认127.0.0.1:6379）
    redisContext* conn = redisConnect("127.0.0.1", 6379);
    if (conn == nullptr || conn->err) {
        std::cerr << "连接失败: " << (conn ? conn->errstr : "无法分配连接对象") << std::endl;
        return 1;
    }

    // 2. 执行SET命令
    redisReply* reply = (redisReply*)redisCommand(conn, "SET key1 hello_redis");
    freeReplyObject(reply); // 释放回复内存

    // 3. 执行GET命令 
    reply = (redisReply*)redisCommand(conn, "GET key1");
    if (reply->type == REDIS_REPLY_STRING) {
        std::cout << "GET key1: " << reply->str << std::endl;
    }
    freeReplyObject(reply);

    // 4. 关闭连接
    redisFree(conn);
    return 0;
}
