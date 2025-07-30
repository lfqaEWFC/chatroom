#include "Database.hpp"

database::database(const char *mysql_host,int mysql_port,const char* mysql_user,const char* mysql_pass,const char* mysql_db,const char *redis_host,int redis_port):
mysql_conn(nullptr), redis_conn(nullptr), connected(false){
    mysql_conn = mysql_init(nullptr);
    if (!mysql_conn) {
        cerr << "MySQL Init Error" << endl;
        connected = false;
        return;
    }
    connected = connect_mysql(mysql_host,mysql_user,mysql_pass,mysql_db,mysql_port) && connect_redis(redis_host,redis_port);
}

bool database::connect_mysql(const char* host,const char* user,const char* pass,const char* db, int port){
    if (!mysql_real_connect(mysql_conn, host, user, pass, db, port, nullptr, 0)) {
        std::cerr << "MySQL Connect Error: " << mysql_error(mysql_conn) << std::endl;
        return false;
    }
    return true;
}

bool database::connect_redis(const char* host, int port){
    redis_conn = redisConnect(host, port);
    if (redis_conn == nullptr || redis_conn->err) {
        std::cerr << "Redis Connect Error: " << (redis_conn ? redis_conn->errstr : "null") << std::endl;
        return false;
    }
    return true;
}

database::~database(){
    if (mysql_conn) {
        mysql_close(mysql_conn);
    }
    if (redis_conn) {
        redisFree(redis_conn);
    }
}

bool database::execute_sql(const string& sql){
    if (mysql_query(mysql_conn, sql.c_str())) {
        std::cerr << "MySQL Exec Error: " << mysql_error(mysql_conn) << std::endl;
        return false;
    }
    return true;
}

MYSQL_RES* database::query_sql(const string& sql){
    if (mysql_query(mysql_conn, sql.c_str())) {
        std::cerr << "MySQL Query Error: " << mysql_error(mysql_conn) << std::endl;
        return nullptr;
    }
    return mysql_store_result(mysql_conn);
}

void database::free_reply(redisReply* reply){
    if(reply){
        freeReplyObject(reply); 
    }
}

redisReply* database::execRedis(const std::string& command){
    redisReply* reply = (redisReply*)redisCommand(redis_conn, command.c_str());
    if (!reply) {
        std::cerr << "Redis Error: " << redis_conn->errstr << std::endl;
        return nullptr;
    }

    return reply;
}

redisContext* database::get_redis_conn(){
    return redis_conn;
}

void database::free_result(MYSQL_RES* result){
    if (result) {
        mysql_free_result(result);
    }
}

bool database::is_connected() const {
    return connected;
}

bool database::redis_del_online_user(const string& username) {
    if (!redis_conn) return false;
    string new_username = "'" + username + "'";
    redisReply* reply = (redisReply*)redisCommand(redis_conn, "SREM online_users %s", new_username.c_str());
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

MYSQL* database::get_mysql_conn() {
    return mysql_conn;
}

bool database::lpushJson(const string& key, const json& json_data){
    if (!redis_conn) return false;

    string json_str = json_data.dump();

    const char* argv[3];
    size_t argvlen[3];

    argv[0] = "LPUSH";
    argvlen[0] = strlen(argv[0]);

    argv[1] = key.c_str();
    argvlen[1] = key.size();

    argv[2] = json_str.c_str();
    argvlen[2] = json_str.size();

    redisReply* reply = (redisReply*)redisCommandArgv(redis_conn, 3, argv, argvlen);
    if (!reply) {
        std::cerr << "Redis LPUSH failed: connection or command error" << std::endl;
        return false;
    }

    freeReplyObject(reply);
    return true;
}

string database::escape_mysql_string_full(const string& input)
{
    string output;
    output.resize(input.size() * 2 + 1);
    
    unsigned long len = mysql_real_escape_string(mysql_conn,
                                                 &output[0],
                                                 input.c_str(),
                                                 input.size());

    output.resize(len);

    return output;
}

