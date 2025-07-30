#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include <nlohmann/json.hpp>
#include <string.h>
#include <iostream>
#include <string>
#include <sstream> 

using namespace std;
using json = nlohmann::json;

class database{

    public:

    database(const char *mysql_host,int mysql_port,const char* mysql_user,const char* mysql_pass,const char* mysql_db,const char *redis_host,int redis_port);

    ~database();

    bool execute_sql(const string& sql); //执行mysql命令，改变mysql库中的数据并通过返回bool来反馈是否成功执行
    MYSQL_RES* query_sql(const string& sql); //执行mysql命令，向mysql库中获取数据并返回
    void free_result(MYSQL_RES* result); //释放result的空间
    MYSQL* get_mysql_conn();//获取mysql的连接句柄
    string escape_mysql_string_full(const std::string& input);

    redisReply* execRedis(const string& command);
    redisContext* get_redis_conn();
    bool lpushJson(const string& key, const json& json_data);
    bool redis_del_online_user(const string& username);
    void free_reply(redisReply* reply);
    
    bool is_connected() const;

    private:

    MYSQL* mysql_conn; //mysql的连接句柄
    redisContext* redis_conn; //redis的连接句柄
    bool connected; //检查是否成功连接
    
    bool connect_mysql(const char* host,const char* user,const char* pass,const char* db, int port);
    bool connect_redis(const char* host, int port);

};

#endif
