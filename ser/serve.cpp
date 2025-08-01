#include "serve.hpp"

void select_new_owner_or_disband_group(const string& old_owner, unique_ptr<database>& db)
{
    MYSQL_RES* res = db->query_sql("SELECT group_id FROM `groups` WHERE owner_name ='"+old_owner+"'");
    if (!res) return;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) 
    {
        string gid = row[0];
        MYSQL_RES* res2 = db->query_sql(
            "SELECT username FROM group_members WHERE group_id="+gid+" AND username !='"+old_owner+"' LIMIT 1");
        if (!res2) continue;
        MYSQL_ROW row2 = mysql_fetch_row(res2);
        if (row2) {
            string new_owner = row2[0];
            db->execute_sql("UPDATE `groups` SET owner_name ='"+new_owner+"' WHERE group_id="+gid);
        } else {
            db->execute_sql("DELETE FROM `groups` WHERE group_id = " + gid);
            db->execRedis("DEL group_message:" + gid);
        }
        db->free_result(res2);
    }

    db->free_result(res);
    return;
}

string get_current_mysql_timestamp() {
    time_t now = time(nullptr);
    tm tm_struct;

    localtime_r(&now, &tm_struct);

    ostringstream oss;
    oss << put_time(&tm_struct, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool handle_signin(json json_quest, unique_ptr<database> &db) 
{
    string username = db->escape_mysql_string_full(json_quest["username"]);
    string password = db->escape_mysql_string_full(json_quest["password"]);
    string question = db->escape_mysql_string_full(json_quest["que"]);
    string answer   = db->escape_mysql_string_full(json_quest["ans"]);

    MYSQL_RES* res = db->query_sql("SHOW TABLES LIKE 'user'");
    
    if (res && mysql_num_rows(res) <= 0) {
        bool creatchk = db->execute_sql(
            "CREATE TABLE user("
            "uid INT PRIMARY KEY AUTO_INCREMENT,"
            "username VARCHAR(64) UNIQUE NOT NULL,"
            "password VARCHAR(64) NOT NULL,"
            "que VARCHAR(1024) NOT NULL,"
            "ans VARCHAR(1024) NOT NULL)"
        );
        if (!creatchk) {
            cout << "CREATE TABLE user ERROR" << endl;
            return false;
        }        
    }

    res = db->query_sql("SELECT * FROM user WHERE username = '"+username+"'");
    if(res == nullptr) return false;
    int rows = mysql_num_rows(res);
    if(rows > 0) return false;

    string sql = "INSERT INTO user(username, password, que, ans) "
                 "VALUES('" +username + "','" + password + "','" + question + "','" + answer + "')";

    bool addchk = db->execute_sql(sql);
    if (!addchk) {
        cout << "INSERT failed" << endl;
        return false;
    }

    return true;
}

bool handle_forget_password(json json_quest,unique_ptr<database> &db,json *reflact){

    string recv_username = db->escape_mysql_string_full(json_quest["username"]);

    MYSQL_RES* res = db->query_sql("SELECT que FROM user WHERE username = '"+ recv_username +"';");

    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端查询用户表异常..."}
        };
        db->free_result(res);
        return false;
    }
    else{
        MYSQL_ROW row = mysql_fetch_row(res);
        if((res && mysql_num_rows(res) <= 0)){
            *reflact = {
                {"sort",REFLACT},
                {"request",FORGET_PASSWORD},
                {"que_flag",false},
                {"reflact","不存在该用户,请重试..."}
            };
            db->free_result(res);
            return false;
        }
        else{
            string username = json_quest["username"];
            string user_question = row[0];
            *reflact = {
                {"sort",REFLACT},
                {"request",FORGET_PASSWORD},
                {"que_flag",true},
                {"username",username},
                {"reflact",user_question}
            };
            db->free_result(res);
            return true;
        }
    }

    return false;
}

bool handle_login(json json_quest,unique_ptr<database> &db,json *reflact,unordered_map<int, string> *cfd_to_user){

    string show_username = json_quest["username"];
    string recv_username = db->escape_mysql_string_full(json_quest["username"]);
    string recv_password = json_quest["password"];

    MYSQL_RES* res = db->query_sql("SELECT password FROM user WHERE username = '"+ recv_username +"';");

    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端查询用户表异常..."}
        };
        db->free_result(res);
        return false;
    }
    else{
        MYSQL_ROW row = mysql_fetch_row(res);
        if((res && mysql_num_rows(res) <= 0)){
            *reflact = {
                {"sort",REFLACT},
                {"request",LOGIN},
                {"login_flag",false},
                {"reflact","不存在该用户,请重试..."}
            };
            db->free_result(res);
            return false;
        }else{
            string user_password = row[0];
            auto it = cfd_to_user->begin();
            for(;it != cfd_to_user->end();it++){
                if(it->second == show_username){
                    *reflact = {
                        {"sort",REFLACT},
                        {"request",LOGIN},
                        {"login_flag",false},
                        {"reflact","已有终端的登录，请勿重复登录..."}
                    };
                    db->free_result(res);
                    return false;
                }
            }
            if(user_password == recv_password){
                *reflact = {
                    {"sort",REFLACT},
                    {"request",LOGIN},
                    {"login_flag",true},
                    {"username",show_username},
                    {"reflact","登录成功，欢迎进入聊天室!!!"}
                };
                db->free_result(res);
                return true;
            }
            else{
                *reflact = {
                    {"sort",REFLACT},
                    {"request",LOGIN},
                    {"login_flag",false},
                    {"reflact","密码错误,请重试...."}
                };
                db->free_result(res);
                return false;
            }
        }
    }

    db->free_result(res);
    return false;

}

bool handle_check_answer(json json_quest,unique_ptr<database> &db,json *reflact){

    string recv_answer = json_quest["answer"];
    string recv_username = db->escape_mysql_string_full(json_quest["username"]);

    MYSQL_RES* res = db->query_sql("SELECT ans FROM user WHERE username = '"+ recv_username +"';");

    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端查询用户表异常..."}
        };
        db->free_result(res);
        return false;
    }
    else{
        MYSQL_ROW row = mysql_fetch_row(res);
        if((res && mysql_num_rows(res) <= 0)){
            *reflact = {
                {"sort",REFLACT},
                {"request",CHECK_ANS},
                {"ans_flag",false},
                {"reflact","不存在该用户,请重试..."}
            };
            db->free_result(res);
            return false;
        }
        else{
            string username = json_quest["username"];
            string user_answer = row[0];
            if(user_answer == recv_answer){
                *reflact = {
                    {"sort",REFLACT},
                    {"request",CHECK_ANS},
                    {"username",username},
                    {"ans_flag",true},
                    {"reflact","密保问题回答正确，欢迎进入聊天室..."}
                };
                db->free_result(res);
                return false;
            }
            else{
                *reflact = {
                    {"sort",REFLACT},
                    {"request",CHECK_ANS},
                    {"ans_flag",false},
                    {"reflact","密保问题回答错误,请重新登录..."}
                };
                db->free_result(res);
                return true;
            }
        }
    }

    return false;
}

bool handle_logout(json json_quest, unique_ptr<database> &db, json *reflact,
                   unordered_map<int, string> *cfd_to_user, unordered_map<string, int> *user_to_cfd)
{
    string redis_key = "offline:logout_time";
    string recv_username = json_quest["username"];
    redisReply* reply;
    int redis_integer = 0;

    auto it_uc = user_to_cfd->find(recv_username);
    if(it_uc != user_to_cfd->end())
    {
        user_to_cfd->erase(it_uc);
        cout << "delete user_to_cfd" << endl;
    }

    for (auto it = cfd_to_user->begin(); it != cfd_to_user->end(); ++it) {
        if (it->second == recv_username) 
        {
            int cfd = it->first;
            cout << "delete cfd_to_user" << endl;
            cfd_to_user->erase(it);

            redisContext *redis = db->get_redis_conn();
            string timestamp = get_current_mysql_timestamp();
            const char* argv[] = {"HSET", redis_key.c_str(), recv_username.c_str(), timestamp.c_str()};
            size_t argvlen[] = {4, redis_key.size(), recv_username.size(), timestamp.size()};
            redisReply* reply = (redisReply*)redisCommandArgv(redis, 4, argv, argvlen);

            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
            {
                *reflact = {
                    {"sort", ERROR},
                    {"reflact", "redis HSET reply error or nullptr"}
                };
                if (reply) db->free_reply(reply);
                return true;
            }

            *reflact = {
                {"sort", REFLACT},
                {"request", LOGOUT},
                {"reflact", "用户成功退出"}
            };
            db->free_reply(reply);
            return true;
        }
    }

    *reflact = {
        {"sort", ERROR},
        {"reflact", "退出失败，用户不在线或未注册..."}
    };

    db->free_reply(reply);
    return false;
}

bool handle_break(json json_quest, unique_ptr<database> &db, json *reflact)
{
    string sql;
    MYSQL_RES *res;
    string username = json_quest["username"];
    const string fri_key = "offline:friend_req:" + username;
    const string pri_key = "offline:private:" + username;
    const string gad_key = "offline:group_add:" + username;
    const string time_key = "offline:logout_time";
    const string sys_key = "offline:system_notice:" + username;
    string recv_password = json_quest["password"];
    string break_username = db->escape_mysql_string_full(json_quest["username"]);
    string break_password = db->escape_mysql_string_full(json_quest["password"]);

    sql = "SELECT password FROM user WHERE username = '"+break_username+"'";
    res = db->query_sql(sql);
    if(res == nullptr)
    {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return false;
    }

    uint64_t rows = mysql_num_rows(res);
    db->free_result(res);
    if(rows == 0)
    {
        *reflact = {
            {"sort", REFLACT},
            {"request", BREAK},
            {"reflact", "用户不存在或密码输入错误，请重试..."}
        };
        return true;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    string password = row[0];
    if(password != recv_password)
    {
        *reflact = {
            {"sort", REFLACT},
            {"request", BREAK},
            {"reflact", "用户不存在或密码输入错误，请重试..."}
        };
        return true;
    }

    sql = "DELETE FROM user WHERE username = '" + break_username + "' AND password = '" + break_password + "'";
    bool res1 = db->execute_sql(sql);

    sql = "DELETE FROM friendship WHERE username = '"+break_username+"' OR friend_username = '"+break_username+"'";
    bool res2 = db->execute_sql(sql);

    sql = "DELETE FROM private_message WHERE sender = '"+break_username+"' OR receiver = '"+break_username+"'";
    bool res3 = db->execute_sql(sql);

    if(!res1 || !res2 || !res3){
        *reflact = {
            {"sort", ERROR},
            {"reflact", "服务端删除数据库数据异常..."}
        };
        return false;
    }

    redisReply* reply1 = db->execRedis("DEL " + fri_key);
    redisReply* reply2 = db->execRedis("DEL " + pri_key);
    redisReply* reply3 = db->execRedis("DEL " + gad_key);
    redisReply* reply4 = db->execRedis("DEL " + sys_key);
    redisReply* reply5 = db->execRedis("HDEL " + time_key + " " + username);

    if ((reply1 == nullptr) || (reply2 == nullptr) || (reply3 == nullptr) 
        || (reply4 == nullptr) || (reply5 == nullptr))
    {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "注销成功，但清理 Redis 缓存时发生部分错误"}
        };

        if(reply1) db->free_reply(reply1);
        if(reply2) db->free_reply(reply2);
        if(reply3) db->free_reply(reply3);
        if(reply4) db->free_reply(reply4);
        if(reply5) db->free_reply(reply5);

        return true;
    }

    select_new_owner_or_disband_group(break_username, db);

    *reflact = {
        {"sort", REFLACT},
        {"request", BREAK},
        {"reflact", "注销成功，账户及相关数据和缓存已删除"}
    };

    if(reply1) db->free_reply(reply1);
    if(reply2) db->free_reply(reply2);
    if(reply3) db->free_reply(reply3);
    if(reply4) db->free_reply(reply4);
    if(reply5) db->free_reply(reply5);

    return true;
}


bool handle_add_friend(json json_quest,unordered_map<int, string>* cfd_to_user,unique_ptr<database> &db,json *reflact){

    string fri_name = db->escape_mysql_string_full(json_quest["fri_username"]);
    string username = db->escape_mysql_string_full(json_quest["username"]);
    string show_user = json_quest["username"];
    string show_fri = json_quest["fri_username"];

    MYSQL_RES* res = db->query_sql("SHOW TABLES LIKE 'friendship'");

    if(!res){
        cout << "SHOW failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端好友关系表出错..."}
        };
        return false;
    }
    if (res && mysql_num_rows(res) <= 0) {
        bool creatchk = db->execute_sql(
            "CREATE TABLE friendship("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "friend_username VARCHAR(64) NOT NULL,"
            "username VARCHAR(64) NOT NULL,"
            "status INT NOT NULL DEFAULT 0,"
            "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
            "UNIQUE KEY unique_friendship (username, friend_username)"
            ");"
        );
        if (!creatchk) {
            cout << "CREATE TABLE friendship ERROR" << endl;
            *reflact = {
                {"sort",ERROR},
                {"reflact","创建好友关系表出错..."}
            };
            db->free_result(res);
            return false;
        }        
    }

    string check_user_sql = "SELECT username FROM user WHERE username = '" + fri_name + "'";
    MYSQL_RES* user_check = db->query_sql(check_user_sql);
    if (!user_check || mysql_num_rows(user_check) == 0) {
        *reflact = {
            {"sort",REFLACT},
            {"request",ADD_FRIEND},
            {"reflact","您要添加的用户不存在，请检查用户名是否正确"}
        };
        db->free_result(res);
        db->free_result(user_check);
        return false;
    }

    if (username == fri_name) {
        *reflact = {
            {"sort",REFLACT},
            {"request",ADD_FRIEND},
            {"reflact","不能添加自己为好友"}
        };
        db->free_result(res);
        db->free_result(user_check);
        return false;
    }

    string check_friendship_sql0 = "SELECT * FROM friendship WHERE username = '" + username + "' AND friend_username = '" + fri_name + "'";
    MYSQL_RES* rel0_check = db->query_sql(check_friendship_sql0);
    int row0 = 0; 
    if(rel0_check)
        row0 = mysql_num_rows(rel0_check);
    string check_friendship_sql1 = "SELECT * FROM friendship WHERE username = '" + fri_name + "' AND friend_username = '" + username + "'";
    MYSQL_RES* rel1_check = db->query_sql(check_friendship_sql1);
    int row1 = 0;
    if(rel1_check)
        row1 = mysql_num_rows(rel1_check);
    if(!rel1_check || !rel0_check){
        cout << "SELECT failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端好友关系表出错..."}
        };
        db->free_result(res);
        db->free_result(user_check);
        db->free_result(rel0_check);
        db->free_result(rel1_check);
        return false;
    }        
    if (row0 || row1) {
        *reflact = {
            {"sort",REFLACT},
            {"request",ADD_FRIEND},
            {"reflact","好友申请已发送或已收到对方的好友申请，请勿重复添加"}
        };
        db->free_result(res);
        db->free_result(user_check);
        db->free_result(rel0_check);
        db->free_result(rel1_check);
        return false;
    }

    string sql = "INSERT INTO friendship(username, friend_username, status) VALUES('" +username + "','" + fri_name + "',0)";
    bool addchk = db->execute_sql(sql);
    if (!addchk) {
        cout << "INSERT failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端好友关系表出错..."}
        };
        db->free_result(res);
        db->free_result(user_check);
        db->free_result(rel0_check);
        db->free_result(rel1_check);
        return false;
    }

    auto it = cfd_to_user->begin();
    for (; it != cfd_to_user->end(); ++it) {
        if (it->second == show_fri) {
            int cfd = it->first;
            json send_fri;
            send_fri = {
                {"sort",MESSAGE},
                {"request", ASK_ADD_FRIEND},
                {"message", ""+show_user+" 向您发送了好友申请,需要处理时请选择聊天界面好友申请选项"}
            };
            sendjson(send_fri,cfd);
            *reflact = {
                {"sort",REFLACT},
                {"request",ADD_FRIEND},
                {"reflact","好友申请发送成功,等待处理中..."}
            };
            db->free_result(res);
            db->free_result(user_check);
            db->free_result(rel0_check);
            db->free_result(rel1_check);
            return true;
        }
    }

    if(it == cfd_to_user->end()){
        json offline_msg = {
            {"type", "friend_request"},
            {"from", show_user},
            {"message", ""+show_user+" 向您发送了好友申请,需要处理时请选择聊天界面好友申请选项"}
        };

        string redis_key = "offline:friend_req:" + show_fri;
        bool redis_ok = db->lpushJson(redis_key,offline_msg);

        if (!redis_ok) {
            cout << "Redis 离线消息写入失败" << endl;
            *reflact = {
                {"sort",ERROR},
                {"reflact","Redis 离线消息写入失败"}
            };
            db->free_result(res);
            db->free_result(user_check);
            db->free_result(rel0_check);
            db->free_result(rel1_check);
            return false;
        }
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",ADD_FRIEND},
        {"reflact","好友申请发送成功,等待处理中..."}
    };

    db->free_result(res);
    db->free_result(user_check);
    db->free_result(rel0_check);
    db->free_result(rel1_check);

    return true;
}

bool handle_get_offline(json json_quest, unique_ptr<database> &db, json *reflact) 
{
    redisReply *reply;
    json arr = json::array();
    const string user = json_quest["username"];
    const string fri_key = "offline:friend_req:" + user;
    const string pri_key = "offline:private:" + user;
    const string gad_key = "offline:group_add:" + user;
    const string time_key = "offline:logout_time";
    const string sys_key = "offline:system_notice:" + user;

    reply = db->execRedis("HGETALL " + pri_key);
    if (reply && reply->type == REDIS_REPLY_ERROR) {
        string err = reply->str;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "Redis 错误: " + err}
        };
        freeReplyObject(reply);
        return true;
    }
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            string sender = reply->element[i]->str;
            string count = reply->element[i + 1]->str;
            string line = "好友 [" + sender + "] 有 " + count + " 条未读私聊消息";
            arr.push_back(line);
        }
    }
    if (reply) freeReplyObject(reply);

    reply = db->execRedis("HGET " + time_key + " " + user);
    if (reply && reply->type == REDIS_REPLY_ERROR) {
        string err = reply->str;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "Redis 错误: " + err}
        };
        freeReplyObject(reply);
        return true;
    }
    string logout_time;
    if (reply && reply->type == REDIS_REPLY_STRING) {
        logout_time = reply->str;
    } else {
        logout_time = "1970-01-01 00:00:00";
    }
    if (reply) freeReplyObject(reply);

    MYSQL_RES* res = db->query_sql(
        "SELECT group_id FROM group_members WHERE "
        "username = '" + db->escape_mysql_string_full(user) + "' AND status = 1"
    );
    if (!res) {
        *reflact = {{"sort", "ERROR"}, {"reflact", "MySQL 查询用户所在群失败"}};
        return true;
    }
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        int gid_offline_cnt = 0;
        long gid = atoi(row[0]);
        string redis_group_key = "group_message:" + to_string(gid);
        redisReply *group_reply = db->execRedis("LRANGE " + redis_group_key + " 0 -1");
        if (group_reply) {
            for (size_t i = 0; i < group_reply->elements; ++i) {
                try {
                    json msg_json = json::parse(group_reply->element[i]->str);
                    string timestamp = msg_json["timestamp"];
                    if (timestamp > logout_time) {
                        gid_offline_cnt++;
                    }
                } catch (...) {
                    cerr << "JSON parse error in group_message:" << gid << endl;
                }
            }
        }
        db->free_reply(group_reply);
        MYSQL_RES* res1 = db->query_sql(
            "SELECT COUNT(*) FROM group_message "
            "WHERE group_id=" + to_string(gid) + " "
            "AND created_at > '" + logout_time + "' "
        );
        if (res1 == nullptr) {
            *reflact = {{"sort", "ERROR"}, {"reflact", "MySQL 查询用户所在群消息失败"}};
            return true;
        }
        MYSQL_ROW row1 = mysql_fetch_row(res1);
        gid_offline_cnt += atoi(row1[0]);
        if (gid_offline_cnt > 0) {
            string line = "gid 为 " + to_string(gid) + " 的群组有 " + to_string(gid_offline_cnt) + " 条新消息";
            arr.push_back(line);
        }
    }
    db->free_result(res);

    reply = db->execRedis("HGETALL " + gad_key);
    if (reply && reply->type == REDIS_REPLY_ERROR) {
        string err = reply->str;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "Redis 错误: " + err}
        };
        freeReplyObject(reply);
        return true;
    }
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            string group_id = reply->element[i]->str;
            string count = reply->element[i + 1]->str;
            string line = "gid 为 " + group_id + " 的群组有 " + count + " 条加群申请";
            arr.push_back(line);
        }
    }
    if (reply) freeReplyObject(reply);

    reply = db->execRedis("LRANGE " + fri_key + " 0 -1");
    if (!reply) {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "服务端获取 Redis 回复失败"}
        };
        return true;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        string err = reply->str;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "Redis 错误: " + err}
        };
        if (reply) freeReplyObject(reply);
        return true;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            json req = json::parse(reply->element[i]->str);
            arr.push_back(req["message"]);
        }
    }
    if (reply) freeReplyObject(reply);

    reply = db->execRedis("LRANGE " + sys_key + " 0 -1");
    if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
        string merged_msg = "系统通知：";
        for (size_t i = 0; i < reply->elements; ++i) {
            try {
                json msg_json = json::parse(reply->element[i]->str);
                string timestamp = msg_json["timestamp"];
                string message = msg_json["message"];
                merged_msg += "\n[" + timestamp + "] " + message;
            } catch (...) {
                cerr << "JSON parse error in system_notice" << endl;
            }
        }
        arr.push_back(merged_msg);
    }
    if (reply) freeReplyObject(reply);

    redisReply* del_sys = db->execRedis("DEL " + sys_key);
    redisReply* del_reply = db->execRedis("DEL " + fri_key);
    redisReply* del_pri = db->execRedis("DEL " + pri_key);
    redisReply* del_gad = db->execRedis("DEL " + gad_key);
    if ((del_reply == nullptr) || (del_pri == nullptr) || (del_gad == nullptr) || (del_sys == nullptr)) {
        cerr << "Main Redis DB DEL error" << endl;
        *reflact = {{"sort", "ERROR"}, {"reflact", "redis DEL error"}};
        if (del_reply) freeReplyObject(del_reply);
        if (del_pri) freeReplyObject(del_pri);
        if (del_gad) freeReplyObject(del_gad);    
        if (del_sys) freeReplyObject(del_sys);
        return true;
    }
    freeReplyObject(del_reply);
    freeReplyObject(del_pri);
    freeReplyObject(del_gad);
    freeReplyObject(del_sys);

    *reflact = {
        {"sort", MESSAGE},
        {"request", GET_OFFLINE_MSG},
        {"elements", arr}
    };

    return true;
}

bool handle_get_friend(json json_quest,unique_ptr<database> &db,json *reflact){
    
    const string user = db->escape_mysql_string_full(json_quest["username"]);
    string show_user = json_quest["username"];

    MYSQL_RES* res = db->query_sql("SELECT username FROM friendship WHERE friend_username = '"+user+"' AND status = 0");

    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","好友关系相关信息出错"}
        };
        db->free_result(res);
        return true;
    }
    else{
        int rows = mysql_num_rows(res);
        if(rows == 0){
            *reflact = {
                {"sort",REFLACT},
                {"request",GET_FRIEND_REQ},
                {"reflact","暂无需要处理的好友申请..."},
                {"do_flag",false}
            };
            db->free_result(res);
            return true;
        }

        vector<string>requests;
        vector<string>fri_user;
        MYSQL_ROW row;
        while((row = mysql_fetch_row(res)) != nullptr){
            fri_user.push_back(row[0]);
            requests.push_back(row[0] + string(" 向您发送了好友请求"));
        }
        *reflact = {
            {"sort",REFLACT},
            {"request",GET_FRIEND_REQ},
            {"do_flag",true},
            {"reflact",requests},
            {"username",show_user},
            {"fri_user",fri_user}
        };
    }

    return true;
}

bool handle_deal_friend(json json_quest,unique_ptr<database> &db,json *reflact,
                        unordered_map<string, int> user_to_cfd){

    vector<string> recv_commit = json_quest["commit"];
    vector<string> recv_refuse = json_quest["refuse"];
    string fri_user = db->escape_mysql_string_full(json_quest["username"]);
    ssize_t c_size = recv_commit.size();
    ssize_t r_size = recv_refuse.size();
    bool c_chk = true;
    bool r_chk = true; 

    for(int i=0;i<c_size;i++){
        string user = db->escape_mysql_string_full(recv_commit[i]);
        c_chk = db->execute_sql("UPDATE friendship SET status = 1 WHERE "
                                "username = '"+user+"' AND friend_username = '"+fri_user+"'");
        if(c_chk == false) 
            break;
        c_chk = db->execute_sql("INSERT INTO friendship(username, friend_username, status) "
                                "VALUES ('" +fri_user+ "','" +user+ "',1)");
        if(c_chk == false) 
            break;
        auto it = user_to_cfd.find(recv_commit[i]);
        if(it != user_to_cfd.end())
        {
            int cfd = it->second;
            json send_json = {
                {"sort",MESSAGE},
                {"request",NON_PEER_CHAT},
                {"message","通知："+fri_user+" 同意了你的好友请求..."}
            };
            sendjson(send_json,cfd);
        }
        else
        {
            string redis_key = "offline:system_notice:" + recv_commit[i];
            json msg = {
                {"timestamp",get_current_mysql_timestamp()},
                {"message",""+fri_user+" 同意了你的好友请求..."}
            };
            db->lpushJson(redis_key,msg);
        }
    }

    for(int i=0;i<r_size;i++){
        string user = db->escape_mysql_string_full(recv_refuse[i]);
        r_chk = db->execute_sql("DELETE FROM friendship WHERE username = '"+user+"' AND friend_username = '"+fri_user+"'");
        if(r_chk == false)
            break;
        auto it = user_to_cfd.find(user);
        if(it != user_to_cfd.end())
        {
            int cfd = it->second;
            json send_json = {
                {"sort",MESSAGE},
                {"request",NON_PEER_CHAT},
                {"message","通知："+fri_user+" 拒绝了你的好友请求..."}
            };
            sendjson(send_json,cfd);
        }
        else
        {
            string redis_key = "offline:system_notice:" + recv_refuse[i];
            json msg = {
                {"timestamp",get_current_mysql_timestamp()},
                {"message",""+fri_user+" 拒绝了你的好友请求..."}
            };
            db->lpushJson(redis_key,msg);
        }
    }

    if(r_chk == false || c_chk == false){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端处理好友关系出错..."}
        };
        return true;
    }else{
        *reflact = {
            {"sort",REFLACT},
            {"request",DEAL_FRI_REQ},
            {"reflact","处理成功，请进行下一步操作..."}
        };
    }

    return true;

}

bool handle_chat_name(json json_quest,unique_ptr<database> &db,json *reflact,unordered_map<string,string>* user_to_friend){

    string show_username = json_quest["username"];
    string show_fri_user = json_quest["fri_user"];
    string fri_user = db->escape_mysql_string_full(json_quest["fri_user"]);
    string username = db->escape_mysql_string_full(json_quest["username"]);

    if(fri_user == username){
        *reflact = {
            {"sort",REFLACT},
            {"request",CHAT_NAME},
            {"chat_flag",false},
            {"reflact","你不能和自己聊天..."}
        };
        return true;
    }

    MYSQL_RES *res = db->query_sql("SELECT * FROM user WHERE username = '"+fri_user+"'");
    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端查询用户表出错..."}
        };
        mysql_free_result(res);
        return true;
    }
    ssize_t rows = mysql_num_rows(res);
    if(rows == 0){
        *reflact = {
            {"sort",REFLACT},
            {"request",CHAT_NAME},
            {"chat_flag",false},
            {"reflact","输入错误，该用户不存在..."}
        };
    }


    res = db->query_sql("SELECT status FROM friendship WHERE username = '"+username+"' AND friend_username = '"+fri_user+"'");
    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端验证好友关系出错..."}
        };
        mysql_free_result(res);
        return true;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if(!row){
        *reflact = {
            {"sort",REFLACT},
            {"request",CHAT_NAME},
            {"chat_flag",false},
            {"reflact","您还尚未与该用户建立好友关系..."}
        };
        mysql_free_result(res);
        return true;
    }else{
        string status_str = row[0];
        int status = stoi(status_str);
        if(status == 1){
            cout << "add user_to_friend: "+show_username+"" << endl;
            (*user_to_friend)[show_username] = show_fri_user;
            *reflact = {
                {"sort",REFLACT},
                {"request",CHAT_NAME},
                {"chat_flag",true},
                {"pri_username",show_fri_user},
                {"reflact","验证成功，可进入私聊模式..."}
            };
        }
        else if(status == 2){
            *reflact = {
                {"sort",REFLACT},
                {"request",CHAT_NAME},
                {"chat_flag",false},
                {"reflact","你已将"+show_fri_user+"加入黑名单...."}
            };
        }
        else if(status == 3){
            *reflact = {
                {"sort",REFLACT},
                {"request",CHAT_NAME},
                {"chat_flag",false},
                {"reflact","你已被"+fri_user+"加入黑名单...."}
            };
        }
        mysql_free_result(res);
        return true;
    }

    return true;
}

bool handle_history_pri(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<string, string> user_to_friend){
    
    string show_username = json_quest["username"];
    string username = db->escape_mysql_string_full(json_quest["username"]);
    string fri_user = db->escape_mysql_string_full(user_to_friend[show_username]);
    
    MYSQL_RES *res = db->query_sql("SHOW TABLES LIKE 'private_message'");
    if(res == nullptr){
        cout << "SHOW failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SHOW ERROR..."}
        };
        return true; 
    }

    if(mysql_num_rows(res)<=0){
        bool creatchk = db->execute_sql(
            "CREATE TABLE private_message("
            "id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '消息唯一 ID',"
            "sender VARCHAR(64) NOT NULL COMMENT '发送方用户名',"
            "receiver VARCHAR(64) NOT NULL COMMENT '接收方用户名',"
            "content TEXT COMMENT '消息内容',"
            "timestamp DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '发送时间'"
            ");"
        );
        if(creatchk == false){
            cout << "CREAT ERROR" << endl;
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL CREAT ERROR"}
            };
            return true;
        }
    }

    res = db->query_sql(
        "SELECT * FROM ("
            "SELECT * FROM private_message "
            "WHERE (sender = '"+username+"' AND receiver = '"+fri_user+"') "
               "OR (sender = '"+fri_user+"' AND receiver = '"+username+"') "
            "ORDER BY timestamp DESC "
            "LIMIT 50 "
        ")AS recent_message "
        "ORDER BY timestamp ASC;"
    );
    if(res == nullptr){
        cout << "SELECT ERROR" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR"}
        };
        return true;
    }

    if(mysql_num_rows(res) == 0){
        *reflact = {
            {"sort",REFLACT},
            {"request",GET_HISTORY_PRI},
            {"ht_flag",false},
            {"reflact","暂无历史消息..."} 
        };
    }
    else
    {
        json history_msgs = json::array();
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res)) != nullptr) {
            json msg;
            msg["id"] = row[0];
            msg["sender"] = row[1];
            msg["receiver"] = row[2];
            msg["content"] = row[3];
            msg["timestamp"] = row[4];
            history_msgs.push_back(msg);
        }

        *reflact = {
            {"sort",REFLACT},
            {"request",GET_HISTORY_PRI},
            {"ht_flag",true},
            {"reflact",history_msgs}
        };
    }

    return true;
}

bool handle_private_chat(json json_quest,unique_ptr<database> &db,json *reflact, 
    unordered_map<string, string> *user_to_friend,unordered_map<int, string> cfd_to_user)
{
    string sender = json_quest["from"];
    string receiver = json_quest["to"];
    string message = json_quest["message"];
    string safe_sender = db->escape_mysql_string_full(json_quest["from"]);
    string safe_receiver = db->escape_mysql_string_full(json_quest["to"]);
    string safe_message = db->escape_mysql_string_full(json_quest["message"]);
    bool file_flag = json_quest["file_flag"];

    if(file_flag){

    }
    else{
        MYSQL_RES *res = db->query_sql("SELECT status FROM friendship WHERE username = '"+safe_sender+"' "
                                       "AND friend_username = '"+safe_receiver+"'");
        if(res == nullptr){
            cout << "SELECT failed" << endl;
            *reflact = {
                {"sort",ERROR}, 
                {"reflact","MYSQL SELECT ERROR"}
            };
            return true;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if(row){
            string status_str = row[0];
            int status = stoi(status_str);
            if(status == 3){
                *reflact = {
                    {"sort",MESSAGE},
                    {"request",ADD_BLACKLIST},
                    {"reflact","你已被"+receiver+"加入黑名单..."}                
                };
                cout << "delete user_to_friend: "+sender+"" << endl;
                (*user_to_friend).erase(sender);
                return true;
            }  
        }
        else{
            *reflact = {
                {"sort",MESSAGE},
                {"request",ADD_BLACKLIST},
                {"reflact","你已被"+receiver+"删除..."}                
            };
            cout << "delete user_to_friend: "+sender+"" << endl;
            (*user_to_friend).erase(sender);
            return true;
        }

        bool sql_chk = db->execute_sql("INSERT INTO private_message(sender, receiver, content) "
                                       "VALUES('" +safe_sender+ "','" +safe_receiver+ "','"+safe_message+"')");
        if(sql_chk == false){
            cout << "INSERT failed" << endl;
            *reflact = {
                {"sort",ERROR}, 
                {"reflact","MYSQL INSERT ERROR"}
            };
            return true;
        }

        auto it = cfd_to_user.begin();
        for (; it != cfd_to_user.end(); ++it) {
            if (it->second == receiver) {
                int cfd = it->first;
                if((*user_to_friend)[receiver] == sender){                   
                    json send_msg = {
                        {"sort",MESSAGE},
                        {"request",PEER_CHAT},
                        {"sender",sender},
                        {"receiver",receiver},
                        {"message",message}
                    };
                    sendjson(send_msg,cfd);
                    return false;
                }
                else{
                    json send_msg = {
                        {"sort",MESSAGE},
                        {"request",NON_PEER_CHAT},
                        {"message","通知: 好友"+sender+"向您发送了一条新消息"}
                    };
                    sendjson(send_msg,cfd);
                    return false;
                }
            }
        }
        if(it == cfd_to_user.end()){
            bool redis_chk = true;
            string redis_key = "offline:private:" + receiver;
            redisReply *reply = db->execRedis("HEXISTS "+redis_key+" "+sender+"");
            if(reply == nullptr) redis_chk = false;
            else if(reply->type == REDIS_REPLY_INTEGER){
                if(reply->integer == 1){
                    freeReplyObject(reply);
                    cout << "increasing..." << endl;
                    reply = db->execRedis("HINCRBY "+redis_key+" "+sender+" 1");
                    if(reply == nullptr) redis_chk = false;
                }
                else{
                    freeReplyObject(reply);
                    cout << "setting..." << endl;
                    reply = db->execRedis("HSET "+redis_key+" "+sender+" 1");
                    if(reply == nullptr) redis_chk = false;
                }
            }
            else{
                freeReplyObject(reply);
                cout << "redis reply type is wrong" << endl;
                *reflact = {
                    {"sort",ERROR}, 
                    {"reflact","redis reply type is wrong"}
                };
                return true;
            }
            if(!redis_chk){
                freeReplyObject(reply);
                cout << "redis hash wrong" << endl;
                *reflact = {
                    {"sort",ERROR}, 
                    {"reflact","redis hash wrong"}
                };
                return true;
            }
            if (reply) freeReplyObject(reply);
        }
    }

    return false;
}

bool handle_del_peer(json json_quest,unique_ptr<database>&db,unordered_map<string, string> *user_to_friend){

    string sender = json_quest["from"];
    string receiver = json_quest["to"];

    cout << "delete user_to_friend: "+sender+"" << endl;
    (*user_to_friend).erase(sender);

    return true;
}

bool handle_add_black(json json_quest, json *reflact, unique_ptr<database>& db)
{
    string show_target = json_quest["target"];
    string username = db->escape_mysql_string_full(json_quest["username"]);
    string target = db->escape_mysql_string_full(json_quest["target"]);

    if(target == username){
        *reflact = {
            {"sort", REFLACT},
            {"request", REMOVE_BLACKLIST},
            {"reflact", "您不能将自己添加到黑名单..."}
        };
        return true;
    }

    string sql_check = "SELECT status FROM friendship "
                       "WHERE username = '"+username+"' AND friend_username = '"+target+"';";
    MYSQL_RES *res = db->query_sql(sql_check);
    if (res == nullptr) {
        cout << "SELECT ERROR" << endl;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return true;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        *reflact = {
            {"sort", REFLACT},
            {"request", ADD_BLACKLIST},
            {"reflact", "您还尚未与该用户建立好友关系..."}
        };
        return true;
    }
    else{
        string status_str = row[0];
        int status = stoi(status_str);
        if(status == 2){
            *reflact = {
                {"sort", REFLACT},
                {"request", ADD_BLACKLIST},
                {"reflact", "已经添加到黑名单，请勿重复添加..."}
            };
            return true;
        }
    }
    mysql_free_result(res);

    string sql_update1 = "UPDATE friendship SET status = 2 WHERE "
                         "username = '"+username+"' AND friend_username = '"+target+"';";
    string sql_update2 = "UPDATE friendship SET status = 3 WHERE "
                         "username = '"+target+"' AND friend_username = '"+username+"' AND status = 1;";
    bool chk1 = db->execute_sql(sql_update1);
    bool chk2 = db->execute_sql(sql_update2);
    if (chk1 && chk2) {
        *reflact = {
            {"sort", REFLACT},
            {"request", ADD_BLACKLIST},
            {"reflact", "成功将用户"+show_target+"添加至黑名单..."}
        };
    } 
    else {
        *reflact = {
            {"sort", REFLACT},
            {"request", ADD_BLACKLIST},
            {"reflact", "添加黑名单操作失败..."}
        };
    }

    return true;
}

bool handle_rem_black(json json_quest,json *reflact,unique_ptr<database>&db){
    
    int status;
    int per_status;
    string sql_update;
    string show_target = json_quest["target"];
    string username = db->escape_mysql_string_full(json_quest["username"]);
    string target = db->escape_mysql_string_full(json_quest["target"]);

    if(target == username){
        *reflact = {
            {"sort", REFLACT},
            {"request", REMOVE_BLACKLIST},
            {"reflact", "您不能将自己移出黑名单..."}
        };
        return true;
    }

    string sql_check1 = "SELECT status FROM friendship "
                        "WHERE username = '"+username+"' AND friend_username = '"+target+"';";
    string sql_check2 = "SELECT status FROM friendship "
                        "WHERE username = '"+target+"' AND friend_username = '"+username+"';";
    MYSQL_RES *res1 = db->query_sql(sql_check1);
    MYSQL_RES *res2 = db->query_sql(sql_check2);

    if (res1 == nullptr || res2 == nullptr) {
        cout << "SELECT ERROR" << endl;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return true;
    }
    MYSQL_ROW row1 = mysql_fetch_row(res1);
    MYSQL_ROW row2 = mysql_fetch_row(res2);
    if (!row1 || !row2) {
        mysql_free_result(res1);
        *reflact = {
            {"sort", REFLACT},
            {"request", REMOVE_BLACKLIST},
            {"reflact", "您还尚未与该用户建立好友关系..."}
        };
        return true;
    }
    else{
        string status_str1 = row1[0];
        string status_str2 = row2[0];
        status = stoi(status_str1);
        per_status = stoi(status_str2);

        if(status == 1){
            *reflact = {
                {"sort", REFLACT},
                {"request", REMOVE_BLACKLIST},
                {"reflact", "未在黑名单中..."}
            };
            return true;
        }
    }
    mysql_free_result(res1);
    mysql_free_result(res2);
    
    if(per_status == 3){
        sql_update = "UPDATE friendship SET status = 1 WHERE "
                            "(username = '"+username+"' AND friend_username = '"+target+"' AND status = 2) OR "
                            "(username = '"+target+"' AND friend_username = '"+username+"' AND status = 3);";
    }
    if(per_status == 2){
        sql_update = "UPDATE friendship SET status = 3 WHERE "
                            "(username = '"+username+"' AND friend_username = '"+target+"' AND status = 2) OR "
                            "(username = '"+target+"' AND friend_username = '"+username+"' AND status = 3);";
    }
    bool chk = db->execute_sql(sql_update);
    if(chk){
        *reflact = {
            {"sort", REFLACT},
            {"request", REMOVE_BLACKLIST},
            {"reflact", "成功将用户"+show_target+"移出黑名单..."}
        };
    }
    else{
        *reflact = {
            {"sort", REFLACT},
            {"request", REMOVE_BLACKLIST},
            {"reflact", "移出黑名单操作失败..."}
        };
    }

    return true;
}

bool handle_check_friend(json json_quest,json *reflact,unique_ptr<database>&db,unordered_map<int, string>* cfd_to_user){
    
    MYSQL_ROW row;
    string username = db->escape_mysql_string_full(json_quest["username"]);
    MYSQL_RES *res = db->query_sql("SELECT friend_username FROM friendship WHERE username = '"+username+"'");

    if(res == nullptr){
        cout << "SELECT ERROR" << endl;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        db->free_result(res);
        return true;
    }
    json friends = json::array();
    while((row = mysql_fetch_row(res)) != nullptr){
        if(row[0] != nullptr){
            string fri_name = row[0];
            string status = "[离线]";
            auto it = (*cfd_to_user).begin();
            for(;it != (*cfd_to_user).end();it++){
                if(fri_name == it->second)
                    status = "[在线]";
            }
            friends.push_back(fri_name);
            friends.push_back(status);
        }
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",CHECK_FRIEND},
        {"friends",friends}
    };

    db->free_result(res);
    return true;
}

bool handle_del_friend(json json_quest,json *reflact,unique_ptr<database>&db,unordered_map<string,int> user_to_cfd) 
{
    string username = db->escape_mysql_string_full(json_quest["username"]);
    string del_user = db->escape_mysql_string_full(json_quest["del_user"]);

    bool mysql_chk1 = db->execute_sql(
        "DELETE FROM friendship WHERE "
        "(username = '"+username+"' AND friend_username = '"+del_user+"') OR "
        "(username = '"+del_user+"' AND friend_username = '"+username+"');"
    );

    MYSQL* affect = db->get_mysql_conn();
    my_ulonglong affected1 = mysql_affected_rows(affect);

    if (!mysql_chk1) {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL DELETE ERROR (friendship)"}
        };
        return true;
    }

    bool mysql_chk2 = db->execute_sql(
        "DELETE FROM private_message WHERE "
        "(sender = '"+username+"' AND receiver = '"+del_user+"') OR "
        "(sender = '"+del_user+"' AND receiver = '"+username+"');"
    );

    if (!mysql_chk2) {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL DELETE ERROR (private_message)"}
        };
        return true;
    }

    if (affected1 > 0) {
        *reflact = {
            {"sort", REFLACT},
            {"request", DELETE_FRIEND},
            {"reflact", "删除成功"}
        };
    } 
    else {
        *reflact = {
            {"sort", REFLACT},
            {"request", DELETE_FRIEND},
            {"reflact", "没有找到要删除的好友关系"}
        };
    }

    string nor_del_user = json_quest["del_user"];
    auto it = user_to_cfd.find(nor_del_user);
    if(it != user_to_cfd.end())
    {
        int cfd = it->second;
        json send_json = {
            {"sort",MESSAGE},
            {"request",NON_PEER_CHAT},
            {"message","通知：你已被 "+username+" 删除..."}
        };
        sendjson(send_json,cfd);
    }
    else
    {
        string redis_key = "offline:system_notice:" + nor_del_user;
        json msg = {
            {"timestamp",get_current_mysql_timestamp()},
            {"message","你已被 "+username+" 删除..."}
        };
        db->lpushJson(redis_key,msg);
    }

    return true;
}

bool handle_add_file(json json_quest,json *reflact,unique_ptr<database>&db,
                     unordered_map<int, string> cfd_to_user,unordered_map<string,string> user_to_friend,
                     unordered_map<string, int> user_to_cfd,unordered_map<string,int> user_to_group)
{
    string sender = json_quest["sender"];
    bool group_flag = json_quest["group_flag"];
    string filename = json_quest["filename"];
    string receiver = json_quest["receiver"];
    string safe_sender = db->escape_mysql_string_full(json_quest["sender"]);
    string safe_filename = db->escape_mysql_string_full(json_quest["filename"]);
    string safe_receiver = db->escape_mysql_string_full(json_quest["receiver"]);

    if(!group_flag){
        MYSQL_RES *res = db->query_sql("SHOW TABLES LIKE 'private_file'");
        if(res == nullptr){
            cout << "SHOW failed" << endl;
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL SHOW ERROR..."}
            };
            return true; 
        }
        if(mysql_num_rows(res)<=0){
            db->free_result(res);
            bool execchk = db->execute_sql(
                "CREATE TABLE private_file ("
                "    id INT AUTO_INCREMENT PRIMARY KEY,"
                "    filename VARCHAR(64) NOT NULL,"
                "    sender VARCHAR(64) NOT NULL,"
                "    receiver VARCHAR(64) NOT NULL,"
                "    CONSTRAINT fk_private_file_sender "
                "        FOREIGN KEY (sender) "
                "        REFERENCES user(username) "
                "        ON DELETE CASCADE "
                "        ON UPDATE CASCADE,"
                "    CONSTRAINT fk_private_file_receiver "
                "        FOREIGN KEY (receiver) "
                "        REFERENCES user(username) "
                "        ON DELETE CASCADE "
                "        ON UPDATE CASCADE,"
                "    UNIQUE KEY uk_sender_receiver_filename (sender, receiver, filename)"
                ");"
            );            
            if(execchk == false){
                cout << "CREAT ERROR" << endl;
                *reflact = {
                    {"sort",ERROR},
                    {"reflact","MYSQL CREAT ERROR"}
                };
                return true;
            }
        }
        bool exechk = db->execute_sql("INSERT INTO private_file(sender, receiver, filename) "
                                      "VALUES('" +safe_sender+ "','" +safe_receiver+ "','"+safe_filename+"')");
        if(exechk == false){
            *reflact = {
                {"sort",MESSAGE},
                {"request",ADD_FILE},
                {"message","请勿上传重复的文件..."}
            };
            return true;
        }        
        auto it = cfd_to_user.begin();
        for(;it != cfd_to_user.end();it++){
            if(it->second == receiver){
                int cfd = it->first;
                json send_json;
                if(user_to_friend[receiver] == sender){
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",PEER_CHAT},
                        {"sender",sender},
                        {"receiver",receiver},
                        {"message",""+sender+" 发送了一个文件"} 
                    };
                } else {
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",NON_PEER_CHAT},
                        {"message","通知: 好友"+sender+"发送了一个文件"}
                    };
                }
                sendjson(send_json,cfd);
                break;
            }
        }
        if(it == cfd_to_user.end()){
            bool redis_chk = true;
            string redis_key = "offline:private:" + receiver;
            redisReply *reply = db->execRedis("HEXISTS "+redis_key+" "+sender+"");
            if(reply == nullptr) redis_chk = false;
            else if(reply->type == REDIS_REPLY_INTEGER){
                if(reply->integer == 1){
                    freeReplyObject(reply);
                    cout << "increasing..." << endl;
                    reply = db->execRedis("HINCRBY "+redis_key+" "+sender+" 1");
                    if(reply == nullptr) redis_chk = false;
                }
                else{
                    freeReplyObject(reply);
                    cout << "setting..." << endl;
                    reply = db->execRedis("HSET "+redis_key+" "+sender+" 1");
                    if(reply == nullptr) redis_chk = false;
                }
            }
            else{
                freeReplyObject(reply);
                cout << "redis reply type is wrong" << endl;
                *reflact = {
                    {"sort",ERROR}, 
                    {"reflact","redis reply type is wrong"}
                };
                return true;
            }
            if(!redis_chk){
                freeReplyObject(reply);
                cout << "redis hash wrong" << endl;
                *reflact = {
                    {"sort",ERROR}, 
                    {"reflact","redis hash wrong"}
                };
                return true;
            }
            if (reply) freeReplyObject(reply);
        }
        bool sql_chk = db->execute_sql("INSERT INTO private_message(sender, receiver, content) "
                                       "VALUES('" +safe_sender+ "','" +safe_receiver+ "','"+safe_sender+"发送了一个文件')");
        if(sql_chk == false){
            cout << "INSERT failed" << endl;
            *reflact = {
                {"sort",ERROR}, 
                {"reflact","MYSQL INSERT ERROR"}
            };
            return true;
        }
    }
    else
    {
        MYSQL_RES *res = db->query_sql("SHOW TABLES LIKE 'group_file'");
        string gid = safe_receiver;
        if(res == nullptr)
        {
            cout << "SHOW failed" << endl;
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL SHOW ERROR..."}
            };
            return true; 
        }
        if(mysql_num_rows(res) <= 0)
        {
            db->free_result(res);
            bool execchk = db->execute_sql(
                "CREATE TABLE group_file ("
                "    id INT AUTO_INCREMENT PRIMARY KEY,"
                "    filename VARCHAR(64) NOT NULL,"
                "    sender VARCHAR(64) NOT NULL,"
                "    group_id BIGINT NOT NULL,"
                "    CONSTRAINT fk_group_file_sender "
                "        FOREIGN KEY (sender) "
                "        REFERENCES user(username) "
                "        ON DELETE CASCADE "
                "        ON UPDATE CASCADE,"
                "    CONSTRAINT fk_group_file_group "
                "        FOREIGN KEY (group_id) "
                "        REFERENCES `groups`(group_id) "
                "        ON DELETE CASCADE "
                "        ON UPDATE CASCADE,"
                "    UNIQUE KEY uk_group_sender_filename (group_id, sender, filename)"
                ");"
            );
            if(execchk == false){
                cout << "CREAT ERROR" << endl;
                *reflact = {
                    {"sort",ERROR},
                    {"reflact","MYSQL CREAT ERROR"}
                };
                return true;
            }
        }
        bool execchk = db->execute_sql("INSERT INTO group_file(sender, group_id, filename) "
                                       "VALUES('" +safe_sender+ "','" +gid+ "','"+safe_filename+"')");
        if(execchk == false){
            *reflact = {
                {"sort",MESSAGE},
                {"request",ADD_FILE},
                {"message","请勿上传重复的文件..."}
            };
            return true;
        }
        res = db->query_sql("SELECT username FROM group_members WHERE group_id = "+gid+"");
        if(res == nullptr)
        {
            *reflact = {
                {"sort",ERROR}, 
                {"reflact","MYSQL SELECT ERROR"}
            };
            return true;
        }
        MYSQL_ROW row;
        while((row = mysql_fetch_row(res)) != nullptr)
        {
            string username = row[0];
            auto it = user_to_cfd.find(username);
            if(username == sender) continue; 
            if(it != user_to_cfd.end())
            {
                json send_json;
                int cfd = it->second;
                auto it_gid = user_to_group.find(username);
                if(it_gid != user_to_group.end())
                {
                    if(atoi(gid.c_str()) == it_gid->second){
                        send_json = {
                            {"sort",MESSAGE},
                            {"request",PEER_GROUP},
                            {"sender",sender},
                            {"message",""+sender+" 发送了一个文件"}
                        };
                        sendjson(send_json,cfd);
                        continue;
                    }
                    else
                    {
                        send_json = {
                            {"sort",MESSAGE},
                            {"request",NOT_PEER_GROUP},
                            {"message","通知: gid为 "+gid+" 的群组中成员 "+sender+" 发送了一个文件"}
                        };
                        sendjson(send_json,cfd);
                        continue;
                    }
                }
                send_json = {
                    {"sort",MESSAGE},
                    {"request",NOT_PEER_GROUP},
                    {"message","通知: gid为 "+gid+" 的群组中成员 "+sender+" 发送了一个文件"}
                };
                sendjson(send_json,cfd);
                continue;
            }
        }
        db->free_result(res);
        json redis_msg = {
            {"group_id",gid},
            {"sender",sender},
            {"timestamp",get_current_mysql_timestamp()},
            {"content",""+sender+" 发送了一个文件"} 
        };
        execchk = db->lpushJson("group_message:"+gid+"",redis_msg);
        if(execchk == false){
            *reflact = {
                {"sort",ERROR}, 
                {"reflact","redis lpush json error"}
            };
            return true;
        }
    }

    *reflact = {
        {"sort",MESSAGE},
        {"request",ADD_FILE},
        {"message","文件 "+filename+" 上传成功..."}
    };

    return true;

}

bool handle_show_file(json json_quest,json *reflact,unique_ptr<database>&db){

    MYSQL_ROW row;
    bool group_flag = json_quest["group_flag"];
    if(group_flag == false)
    {
        string sender = json_quest["sender"];
        string username = db->escape_mysql_string_full(json_quest["username"]);
        string fri_user = db->escape_mysql_string_full(json_quest["sender"]);
    
        MYSQL_RES* res = db->query_sql("SELECT filename FROM private_file "
                                       "WHERE sender = '"+fri_user+"' AND receiver = '"+username+"'");
        if(res == nullptr){
            cout << "SELECT failed" << endl;
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL SELECT ERROR..."}
            };
            return true; 
        }
    
        int rows = mysql_num_rows(res);
        if(rows == 0){
            *reflact = {
                {"sort",REFLACT},
                {"request",SHOW_FILE},
                {"get_flag",false},
                {"group_flag",false},
                {"reflact",""+sender+" 不存在或并未向您发送文件..."}
            };
            return true; 
        }
        json filenames = json::array();
        while((row = mysql_fetch_row(res)) != nullptr){
            if(row[0] != nullptr){
                string filename = row[0];
                filenames.push_back(filename);
            }
        }
        *reflact = {
            {"sort",REFLACT},
            {"request",SHOW_FILE},
            {"file_user",sender},
            {"get_flag",true},
            {"group_flag",false},
            {"reflact",filenames}
        };
        return true;
    }
    else
    {
        MYSQL_RES* res;
        MYSQL_ROW row;
        long gid = json_quest["group_id"];
        res = db->query_sql("SELECT sender,filename FROM group_file WHERE group_id = "+to_string(gid)+"");
        if(res == nullptr)
        {
            *reflact = {
                {"sort",ERROR}, 
                {"reflact","MYSQL SELECT ERROR"}
            };
            return true;
        }
        uint64_t rows = mysql_num_rows(res);
        if(rows <= 0)
        {
            *reflact = {
                {"sort",REFLACT},
                {"request",SHOW_FILE},
                {"get_flag",false},
                {"group_flag",false},
                {"reflact","gid 为 "+to_string(gid)+" 的群组没有需要下载的文件..."}
            };
            return true;
        }
        json array = json::array();
        while((row = mysql_fetch_row(res)) != nullptr)
        {
            string sender = row[0];
            string filename = row[1];
            json msg = {
                {"sender",sender},
                {"filename",filename}
            };
            array.push_back(msg);
        }
        *reflact = {
            {"sort",REFLACT},
            {"request",SHOW_FILE},
            {"get_flag",true},
            {"group_flag",true},
            {"group_id",gid},
            {"reflact",array}
        };
    }

    return true;

}

bool handle_create_group(json json_quest,json* reflact,unique_ptr<database>&db)
{
    long group_id = 0;
    bool sqlchk1 = true;
    bool sqlchk2 = true;
    MYSQL_ROW row;
    MYSQL_RES *res = nullptr;
    string show_group = json_quest["group_name"];
    string username = db->escape_mysql_string_full(json_quest["username"]);
    string group_name = db->escape_mysql_string_full(json_quest["group_name"]);

    res = db->query_sql("SHOW TABLES LIKE 'groups'");
    if(res == nullptr){
        cout << "SHOW failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SHOW ERROR..."}
        };
        db->free_result(res);
        return true; 
    }

    if(mysql_num_rows(res) <= 0)
    {
        sqlchk1 = db->execute_sql(
            "CREATE TABLE `groups` ("
            "group_id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '群组唯一ID', "
            "name VARCHAR(64) NOT NULL COMMENT '群名', "
            "owner_name VARCHAR(64) DEFAULT NULL COMMENT '群主用户ID', "
            "created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间', "
            "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最后更新时间', "
            "CONSTRAINT fk_owner "
            "   FOREIGN KEY (owner_name) "
            "   REFERENCES user(username) "
            "   ON DELETE SET NULL "
            "   ON UPDATE CASCADE"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='群组信息表';"
        );
    }

    res = db->query_sql("SHOW TABLES LIKE 'group_members'");
    if(res == nullptr){
        cout << "SHOW failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SHOW ERROR..."}
        };
        db->free_result(res);
        return true; 
    }

    if(mysql_num_rows(res) <= 0)
    {
        sqlchk2 = db->execute_sql(
            "CREATE TABLE group_members ("
            "id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键ID',"
            "group_id BIGINT NOT NULL COMMENT '群组ID',"
            "username VARCHAR(64) NOT NULL COMMENT '用户名称',"
            "status BOOL NOT NULL COMMENT '添加状态',"
            "role ENUM('owner','admin','member') DEFAULT 'member' COMMENT '群内角色',"
            "joined_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',"
        
            "CONSTRAINT fk_group_members_group "
            "   FOREIGN KEY (group_id) "
            "   REFERENCES `groups`(group_id) "
            "   ON DELETE CASCADE "
            "   ON UPDATE CASCADE,"
        
            "CONSTRAINT fk_group_members_user "
            "   FOREIGN KEY (username) "
            "   REFERENCES user(username) "
            "   ON DELETE CASCADE "
            "   ON UPDATE CASCADE,"
        
            "UNIQUE KEY uk_group_user (group_id, username)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='群成员表';"
        );
    }
    
    if(sqlchk1 == false || sqlchk2 == false)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL CREATE ERROR..."}
        };
        db->free_result(res);
        return true;
    }

    res =  db->query_sql("SELECT * FROM `groups` WHERE name = '"+group_name+"' AND owner_name = '"+username+"'");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        db->free_result(res);
        return true;
    }
    uint64_t rows = mysql_num_rows(res);
    if(rows > 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",CREATE_GROUP},
            {"reflact","创建群名重复，请重新命名创建..."}
        };
        db->free_result(res);
        return true;
    }
    sqlchk1 = db->execute_sql("INSERT INTO `groups`(name, owner_name) "
                              "VALUES('"+group_name+"','"+username+"')");
    if(sqlchk1 == false){
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL INSERT ERROR..."}
        };
        db->free_result(res);
        return true;
    }

    res = db->query_sql("SELECT LAST_INSERT_ID();");

    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        db->free_result(res);
        return true;
    }

    row = mysql_fetch_row(res);

    if(row == nullptr)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",CREATE_GROUP},
            {"reflact","group not found..."}
        };
        db->free_result(res);
        return true;
    }
    group_id = atoi(row[0]);

    sqlchk1 = db->execute_sql("INSERT INTO group_members(group_id,username,role,status) "
                              "VALUES("+to_string(group_id)+",'"+username+"','owner',true)");
    if(sqlchk1 == false){
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL INSERT ERROR..."}
        };
        db->free_result(res);
        return true;
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",CREATE_GROUP},
        {"reflact","群聊 "+show_group+" 创建成功..."}
    } ;

    db->free_result(res);
    return true;
}
 
bool handle_select_group(json json_quest,json* reflact,unique_ptr<database>&db)
{
    MYSQL_ROW row;
    string group_name = db->escape_mysql_string_full(json_quest["group_name"]);

    MYSQL_RES *res = db->query_sql("SELECT group_id,owner_name FROM `groups` WHERE name = '"+group_name+"'");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        db->free_result(res);
        return true;
    }
    uint64_t rows = mysql_num_rows(res);
    if(rows<=0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",SEL_GROUP},
            {"id_flag",false},
            {"reflact","未搜索到相关群聊，请输入正确的群聊名称..."}
        };
        db->free_result(res);
        return true;
    }

    json sel_groups = json::array();
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        int group_id = atoi(row[0]);
        string owner_name = row[1];
        json msg = {
            {"group_id",group_id},
            {"owner_name",owner_name}
        };
        sel_groups.push_back(msg);
    }
    *reflact = {
        {"sort",REFLACT},
        {"request",SEL_GROUP},
        {"id_flag",true},
        {"sel_groups",sel_groups}
    };

    db->free_result(res);
    return true;
}

bool handle_add_group(json json_quest,json* reflact,unique_ptr<database>&db,
                      unordered_map<string,int> user_to_cfd)
{
    int group_id = json_quest["group_id"];
    string username = db->escape_mysql_string_full(json_quest["username"]);
    bool sqlchk = true;
    MYSQL_RES* res;
    MYSQL_ROW row;
    uint64_t rows;

    res = db->query_sql("SELECT * FROM  group_members "
                        "WHERE group_id = "+to_string(group_id)+" AND username = '"+username+"';");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        db->free_result(res);
        return true;
    }
    rows = mysql_num_rows(res);
    if(rows > 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",ADD_GROUP},
            {"reflact","您已经加入了群聊，请勿重复添加..."}
        };
        db->free_result(res);
        return true;
    }

    res = db->query_sql("SELECT username FROM group_members WHERE"
                        "(group_id = "+to_string(group_id)+" AND role = 'owner') OR "
                        "(group_id = "+to_string(group_id)+" AND role = 'admin')");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        db->free_result(res);
        return true;
    }
    rows = mysql_num_rows(res);
    if(rows <= 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",ADD_GROUP},
            {"reflact","查询群信息结果为空,群id有误..."}
        };
        db->free_result(res);
        return true;
    }
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        string admin = row[0];
        auto it = user_to_cfd.find(admin);
        if(it == user_to_cfd.end())
        {
            bool redis_chk = true;
            string redis_key = "offline:group_add:" + admin;
            redisReply *reply = db->execRedis("HEXISTS "+redis_key+" "+to_string(group_id)+"");
            if(reply == nullptr) redis_chk = false;
            else if(reply->type == REDIS_REPLY_INTEGER){
                if(reply->integer == 1){
                    freeReplyObject(reply);
                    cout << "increasing..." << endl;
                    reply = db->execRedis("HINCRBY "+redis_key+" "+to_string(group_id)+" 1");
                    if(reply == nullptr) redis_chk = false;
                }
                else{
                    freeReplyObject(reply);
                    cout << "setting..." << endl;
                    reply = db->execRedis("HSET "+redis_key+" "+to_string(group_id)+" 1");
                    if(reply == nullptr) redis_chk = false;
                }
            }
            else{
                freeReplyObject(reply);
                cout << "redis reply type is wrong" << endl;
                *reflact = {
                    {"sort",ERROR}, 
                    {"reflact","redis reply type is wrong"}
                };
                db->free_result(res);
                return true;
            }
            if(!redis_chk){
                cout << "redis hash wrong" << endl;
                *reflact = {
                    {"sort",ERROR}, 
                    {"reflact","redis hash wrong"}
                };
                db->free_result(res);
                return true;
            }
            if (reply) freeReplyObject(reply); 
        }
        else
        {
            int cfd = it->second;
            json send_json;
            send_json = {
                {"sort",MESSAGE},
                {"request",ADD_GROUP},
                {"message","你有一条新的加群申请等待处理...."}
            };
            sendjson(send_json,cfd);
        }
    }

    sqlchk = db->execute_sql("INSERT INTO group_members(group_id,username,role,status) "
                             "VALUES("+to_string(group_id)+",'"+username+"','member',false)");
    if (!sqlchk) {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL INSERT ERROR..."}
        };
        return true;
    }
                            
    *reflact = {    
        {"sort",REFLACT},
        {"request",ADD_GROUP},
        {"reflact"," 加群申请发送成功，等待处理中...."}
    };

    db->free_result(res);
    return true;
}

bool deal_add_group(json json_quest,json* reflact,unique_ptr<database>&db)
{
    string username = db->escape_mysql_string_full(json_quest["username"]);
    MYSQL_RES *res;
    MYSQL_RES *res1;
    MYSQL_ROW row;
    uint64_t rows;

    res = db->query_sql("SELECT group_id FROM group_members WHERE "
                        "(role = 'owner' AND username = '"+username+"') OR "
                        "(role = 'admin' AND username = '"+username+"')");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        db->free_result(res);
        return true;
    }
    rows = mysql_num_rows(res);
    if(rows <= 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",DEAL_ADDGROUP},
            {"group_add_flag",false},
            {"reflact","暂无需要处理的加群申请..."}
        };
        db->free_result(res);
        return true;
    }
    json elements = json::array();
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        long gid = atoi(row[0]);
        res1 = db->query_sql("SELECT username FROM group_members WHERE "
                            "group_id = "+to_string(gid)+" AND status = false");
        if(res == nullptr)
        {
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL SELECT ERROR..."}
            };
            db->free_result(res);
            db->free_result(res1);
            return true;
        }
        while((row = mysql_fetch_row(res1)) != nullptr)
        {
            json msg = {
                {"gid",gid},
                {"username",row[0]}
            };
            elements.push_back(msg);
        }
        db->free_result(res1);
    }
    if(elements.size() == 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",DEAL_ADDGROUP},
            {"group_add_flag",false},
            {"reflact","暂无需要处理的加群申请..."}
        };
        db->free_result(res);
        return true;
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",DEAL_ADDGROUP},
        {"group_add_flag",true},
        {"reflact",elements}
    };

    db->free_result(res);
    return true;
}

bool handle_commit_add(json json_quest,json* reflact,unique_ptr<database>&db,unordered_map<string,int> user_to_cfd)
{
    MYSQL* sql = db->get_mysql_conn();
    json elements = json_quest["elements"];
    int success_cnt = 0;
    int fail_cnt = 0;
    bool sql_chk = true;

    for(long i=0;i<elements.size();i++)
    {
        json msg = elements[i];
        long gid = msg["gid"];
        string username = db->escape_mysql_string_full(msg["username"]);
        sql_chk = db->execute_sql("UPDATE group_members SET status = 1 WHERE "
                                  "username = '"+username+"' AND group_id = "+to_string(gid)+" AND status = 0");
        if(sql_chk == false)
        {
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL UPDATE ERROR..."}
            };
            return true;
        }
        uint64_t affacted_rows = mysql_affected_rows(sql);
        if(affacted_rows > 0) 
        {
            success_cnt++;
            string nor_user = msg["username"];
            auto it = user_to_cfd.find(nor_user);
            if(it != user_to_cfd.end())
            {
                int cfd = it->second;
                json send_json = {
                    {"sort",MESSAGE},
                    {"request",NON_PEER_CHAT},
                    {"message","通知：成功加入 gid 为 "+to_string(gid)+" 的群聊"}
                };
                sendjson(send_json,cfd);
            }
            else
            {
                string redis_key = "offline:system_notice:" + nor_user;
                json msg = {
                    {"timestamp",get_current_mysql_timestamp()},
                    {"message","成功加入 gid 为 "+to_string(gid)+" 的群聊"}
                };
                db->lpushJson(redis_key,msg);
            }
        }
        else fail_cnt++;
    }

    if(success_cnt == 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",COMMIT_ADD},
            {"reflact","操作失败，请输入正确信息"}
        };
        return true;
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",COMMIT_ADD},
        {"reflact","有 "+to_string(success_cnt)+" 次操作处理成功"}
    };

    return true;
}

bool handle_show_group(json json_quest,json* reflact,unique_ptr<database>&db)
{
    string username = db->escape_mysql_string_full(json_quest["username"]);
    json elements = json::array();
    MYSQL_RES *res;
    MYSQL_RES *res1;
    MYSQL_ROW row;
    MYSQL_ROW row1;
    uint64_t rows;
    
    res = db->query_sql("SELECT group_id,role FROM group_members "
                        "WHERE username = '"+username+"' and status = 1");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        return true;
    }
    rows = mysql_num_rows(res);
    if(rows <= 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",SHOW_GROUP},
            {"show_flag",false},
            {"reflact","当前尚未加入群聊，请先加入群聊之后再查询..."}
        };
        db->free_result(res);
        return true;
    }
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        json msg;
        long gid = atoi(row[0]);
        string role = row[1];

        res1 = db->query_sql("SELECT name,owner_name FROM `groups` "
                             "WHERE group_id = "+to_string(gid)+"");
        if(res1 == nullptr)
        {
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL SELECT ERROR..."}
            };
            db->free_result(res);
            return true;
        }
        row1 = mysql_fetch_row(res1);
        if(row1 == nullptr)
        {
            *reflact = {
                {"sort",REFLACT},
                {"request",SHOW_GROUP},
                {"show_flag",false},
                {"reflact","mysql select error (row == nullptr)..."}
            };
            db->free_result(res1);
            db->free_result(res);
            return true;
        }
        string group_name = row1[0];
        string owner_name = row1[1];

        msg = {
            {"gid",gid},
            {"role",role},
            {"group_name",group_name},
            {"owner_name",owner_name}
        };
        elements.push_back(msg);
        
        db->free_result(res1);
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",SHOW_GROUP},
        {"show_flag",true},
        {"elements",elements}
    };

    db->free_result(res);
    return true;
}

bool handle_group_name(json json_quest,json* reflact,unique_ptr<database>&db,
                       unordered_map<string, int> *user_to_group)
{
    MYSQL_RES *res;
    MYSQL_ROW row;
    uint64_t rows;
    string role;
    string group_name;
    string username = db->escape_mysql_string_full(json_quest["username"]);
    long gid = json_quest["gid"];

    res = db->query_sql("SELECT role FROM group_members WHERE "
                        "group_id = '"+to_string(gid)+"' AND username = '"+username+"' AND status = 1");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        return true;
    }
    rows = mysql_num_rows(res);
    if(rows <= 0)
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",GROUP_NAME},
            {"group_flag",false},
            {"reflact","输入的gid有误..."}
        };
        db->free_result(res);
        return true;
    }
    row = mysql_fetch_row(res);
    role = row[0];
    db->free_result(res);
    res = db->query_sql("SELECT name FROM `groups` WHERE group_id = '"+to_string(gid)+"'");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT ERROR..."}
        };
        return true;
    }
    row = mysql_fetch_row(res);
    group_name = row[0];
    
    *reflact = {
        {"sort",REFLACT},
        {"request",GROUP_NAME},
        {"group_flag",true},
        {"group_name",group_name},
        {"gid",gid},
        {"role",role}
    };
    (*user_to_group)[json_quest["username"]] = gid;
    
    db->free_result(res);
    return true;
}

bool handle_group_history(json json_quest,json* reflact,unique_ptr<database>&db)
{
    string username = db->escape_mysql_string_full(json_quest["username"]);
    int redis_cnt = 0;
    int mysql_cnt = 50;
    long gid = json_quest["gid"];
    bool create_chk;
    MYSQL_ROW row;
    MYSQL_RES* res;
    uint64_t rows;
    redisReply *reply;
    string redis_key;
    json history_msgs = json::array();
    
    res = db->query_sql("SHOW TABLES LIKE 'group_message'");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SHOW ERROR..."}
        };
        return true;
    }
    if(mysql_num_rows(res) <= 0)
    {
        create_chk = db->execute_sql(
            "CREATE TABLE group_message ("
            "  id BIGINT AUTO_INCREMENT PRIMARY KEY, "
            "  group_id BIGINT NOT NULL, "
            "  username VARCHAR(64) NOT NULL, "
            "  message TEXT NOT NULL, "
            "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
            "  INDEX idx_group_id (group_id), "
            "  INDEX idx_sender (username), "

            "  CONSTRAINT fk_group "
            "    FOREIGN KEY (group_id) "
            "    REFERENCES `groups`(group_id) "
            "    ON DELETE CASCADE, "

            "  CONSTRAINT fk_sender "
            "    FOREIGN KEY (username) "
            "    REFERENCES user(username) "
            "    ON DELETE CASCADE "

            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"
        );
        if (!create_chk) {
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL CREATE TABLE ERROR..."}
            };
            return true;
        }
    }
    redis_key = "group_message:"+to_string(gid);
    reply = db->execRedis("LRANGE "+redis_key+" 0 49");
    if(reply == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","redis LRANGE reply is nullptr"}
        };
        return true;
    }
    else if(reply->type == REDIS_REPLY_ERROR)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","redis LRANGE reply error"}
        };
        db->free_reply(reply);
        return true;
    }
    else if(reply->type == REDIS_REPLY_ARRAY)
    {
        for(int i=0;i<reply->elements;++i)
        {
            const char* msg_str = reply->element[i]->str;
            try {
                json msg_json = json::parse(msg_str);
                history_msgs.push_back(msg_json);
                redis_cnt++;
            } catch (...) {
                cerr << "[Redis] JSON parse error on message, skipping." << endl;
            }
        }
    }
    else
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","redis LRANGE reply's type error"}
        };
        db->free_reply(reply);
        return true;
    }
    db->free_reply(reply);
    cout << "redis_cnt: " << redis_cnt << endl;
    mysql_cnt = mysql_cnt - redis_cnt;
    if(mysql_cnt > 0)
    {
        string sql = "SELECT username, message, created_at "
                     "FROM group_message "
                     "WHERE group_id = '" + to_string(gid) + "' "
                     "ORDER BY created_at DESC "
                     "LIMIT "+to_string(mysql_cnt)+"";
        db->free_result(res);
        res = db->query_sql(sql);
        if(res == nullptr)
        {
            *reflact = {
                {"sort",ERROR},
                {"reflact","MYSQL SELECT ERROR..."}
            };
            return true;
        }
        rows = mysql_num_rows(res);
        if(rows <= 0 && redis_cnt <= 0)
        {
            *reflact = {
                {"sort",REFLACT},
                {"request",GROUP_HISTORY},
                {"his_flag",false},
                {"reflact","当前暂无历史消息..."}
            };
            db->free_result(res);
            return true;
        }
        while(((row = mysql_fetch_row(res)) != nullptr) && rows > 0 )
        {  
            json msg;
            msg["sender"] = row[0];
            msg["content"] = row[1];
            msg["timestamp"] = row[2];
            history_msgs.push_back(msg);
        }
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",GROUP_HISTORY},
        {"his_flag",true},
        {"reflact",history_msgs}
    };
    db->free_result(res);
    return true;
}

bool handle_group_chat(json json_quest,json* reflact,unique_ptr<database>&db,
                       unordered_map<string, int> user_to_cfd,unordered_map<string,int> user_to_group)
{
    string message = json_quest["message"];
    string username = json_quest["username"];
    string safe_message = db->escape_mysql_string_full(json_quest["message"]);
    string safe_username = db->escape_mysql_string_full(json_quest["username"]);
    long gid = json_quest["gid"];
    json redis_msg;
    long redis_len = 0;
    bool redis_ok = true;
    redisReply *reply;
    MYSQL_RES* res;
    uint64_t rows;
    MYSQL_ROW row;
    string redis_key = "group_message:"+to_string(gid);

    redis_msg = {
        {"group_id",gid},
        {"sender",username},
        {"timestamp",get_current_mysql_timestamp()},
        {"content",message}
    };
    redis_ok = db->lpushJson(redis_key,redis_msg);
    if(redis_ok == false)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","redis 群聊消息写入错误..."}
        };
        return true;
    }
    reply = db->execRedis("LLEN "+redis_key+"");
    if(reply == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","redis LRANGE reply is nullptr"}
        };
        return true;
    }
    if(reply->type == REDIS_REPLY_INTEGER)
        redis_len = reply->integer;
    else
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","redis llen type error"}
        };
        db->free_reply(reply);
        return true;
    }
    db->free_reply(reply);
    if (redis_len >= 1000)
    {
        reply = db->execRedis("LPOP " + redis_key + " 1000");
        if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY)
        {
            *reflact = {
                {"sort", ERROR},
                {"reflact", "redis LRANGE reply error or nullptr"}
            };
            if (reply) db->free_reply(reply);
            return true;
        }
    
        string sql = "INSERT INTO group_message (group_id, username, message, created_at) VALUES ";
        for (size_t i = 0; i < reply->elements; ++i)
        {
            try
            {
                json msg_json = json::parse(reply->element[i]->str);
                string group_id_str = to_string(msg_json["group_id"].get<long>());
                string username_str = db->escape_mysql_string_full(msg_json["sender"].get<string>());
                string message_str = db->escape_mysql_string_full(msg_json["content"].get<string>());
                string created_at_str = db->escape_mysql_string_full(msg_json["timestamp"].get<string>());
    
                sql += "(" + group_id_str + ", '" + username_str + "', '" 
                        + message_str + "', '" + created_at_str + "')";
                if (i != reply->elements - 1)
                    sql += ",";
            }
            catch (...)
            {
                cerr << "[Redis] JSON parse error during batch insert, skipping." << endl;
            }
        }
        sql += ";";
    
        db->free_reply(reply);
    
        bool insert_ok = db->execute_sql(sql);
        if (!insert_ok)
        {
            *reflact = {
                {"sort", ERROR},
                {"reflact", "MySQL 批量插入群消息失败"}
            };
            return true;
        }
    }
    res = db->query_sql("SELECT username FROM group_members WHERE "
                        "group_id = "+to_string(gid)+" AND username <> '"+safe_username+"';");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT username ERROR..."}
        };
        return true;
    }
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        string receiver = row[0];
        auto it = user_to_cfd.find(receiver);
        int cfd = 0;
        if (it != user_to_cfd.end())
            cfd = user_to_cfd[receiver];
        if(cfd == 0)
            continue;
        else 
        {
            auto it_group = user_to_group.find(receiver);
            json send_json;
            if(it_group == user_to_group.end())
            {
                send_json = {
                    {"sort",MESSAGE},
                    {"request",NOT_PEER_GROUP},
                    {"message","通知: gid为 "+to_string(gid)+" 的群组有一条新消息"}
                };
            } else {

                if(it_group->second == gid){
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",PEER_GROUP},
                        {"sender",json_quest["username"]},
                        {"message",message}
                    };
                } else{
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",NOT_PEER_GROUP},
                        {"message","通知: gid为 "+to_string(gid)+" 的群组有一条新消息"}
                    };
                }
            }
            sendjson(send_json,cfd);
        }
    }

    db->free_result(res);
    return false;
}

bool handle_group_end(json json_quest,json* reflact,unique_ptr<database>&db,
     unordered_map<string,int>* user_to_group)
{
    string username = json_quest["username"];
    long gid = json_quest["gid"];

    auto it = user_to_group->find(username);
    if(it != user_to_group->end())
    {
        if(gid == it->second)
            user_to_group->erase(it);
    }
    else
    {
        *reflact = {
            {"sort",REFLACT},
            {"request",GROUP_END},
            {"reflact","寻找 peer group 失败..."}
        };
        return true;
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",GROUP_END},
        {"reflact","成功退出群聊模式..."}
    };

    return true;
}

bool handle_show_member(json json_quest,json *reflact,unique_ptr<database>&db)
{
    long gid = json_quest["gid"];
    string safe_username = db->escape_mysql_string_full(json_quest["username"]);
    MYSQL_RES *res;
    MYSQL_ROW row;

    res = db->query_sql("SELECT username,role from group_members WHERE group_id = "+to_string(gid)+"");
    if(res == nullptr)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SELECT username ERROR..."}
        };
        return true;
    }
    json members = json :: array();
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        string username = row[0];
        string role = row[1];
        json msg = {
            {"username",username},
            {"role",role}
        };
        members.push_back(msg);
    }
    *reflact = {
        {"sort",REFLACT},
        {"request",SHOW_MEMBER},
        {"members",members}
    };
    return true;
}

bool handle_add_admin(json json_quest, json *reflact, unique_ptr<database> &db, 
                      unordered_map<string, int> user_to_cfd) 
{
    MYSQL_RES* res;
    MYSQL_ROW row;
    long gid = json_quest["group_id"];
    string addname = json_quest["addname"];
    string username  = json_quest["username"];
    string safe_addname = db->escape_mysql_string_full(json_quest["addname"]);
    string redis_key = "offline:system_notice:" + addname;

    if(addname == username)
    {
        *reflact = {
            {"sort", REFLACT},
            {"request",ADD_ADMIN},
            {"reflact", "您不能将自己添加为管理员..."}
        };
        return true;
    }

    res = db->query_sql("SELECT role FROM group_members WHERE "
                        "group_id = "+to_string(gid)+" AND username = '"+safe_addname+"'");
    if(res == nullptr)
    {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return true;
    }
    row = mysql_fetch_row(res);
    if(row == nullptr)
    {
        db->free_result(res);
        *reflact = {
            {"sort", REFLACT},
            {"request",ADD_ADMIN},
            {"reflact", "该用户不存在..."}
        };
        return true;
    }
    string role = row[0];
    if(role == "owner")
    {
        db->free_result(res);
        *reflact = {
            {"sort", REFLACT},
            {"request",ADD_ADMIN},
            {"reflact", "您不能将群主变成管理员..."}
        };
        return true;
    }
    db->free_result(res);

    string sql = 
        "UPDATE group_members "
        "SET role = 'admin' "
        "WHERE group_id = "+ to_string(gid) +" "
        "AND username = '" + safe_addname + "'";

    bool execchk = db->execute_sql(sql);
    if (!execchk) {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL UPDATE ERROR"}
        };
        return true;
    }

    my_ulonglong affected = mysql_affected_rows(db->get_mysql_conn());
    if (affected == 0) {
        *reflact = {
            {"sort", REFLACT},
            {"request",ADD_ADMIN},
            {"reflact", "未找到该成员或已是管理员"}
        };
        return true;
    }

    auto it = user_to_cfd.find(addname);
    if(it != user_to_cfd.end())
    {
        int cfd = it->second;
        json send_json = {
            {"sort",MESSAGE},
            {"request",NOT_PEER_GROUP},
            {"message","通知：你在 gid 为 "+to_string(gid)+" 的群组，被 "+username+" 添加为管理员"}
        };
        sendjson(send_json,cfd);
    }
    else
    {
        json msg = {
            {"timestamp",get_current_mysql_timestamp()},
            {"message","你在 gid 为 "+to_string(gid)+" 的群组，被 "+username+" 添加为管理员"}
        };
        db->lpushJson(redis_key,msg);
    }

    *reflact = {
        {"sort", REFLACT},
        {"request",ADD_ADMIN},
        {"reflact", "已成功将 " + json_quest["addname"].get<string>() + " 设为管理员"}
    };

    return true;
}

bool handle_show_friend(json json_quest,json *reflact,unique_ptr<database>&db)
{
    string safe_username = db->escape_mysql_string_full(json_quest["username"]);
    MYSQL_RES *res;
    MYSQL_ROW row;

    res = db->query_sql("SELECT friend_username FROM friendship WHERE "
                        "username = '"+safe_username+"' AND status = 1");
    if(res == nullptr)
    {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return true;
    }
    json members = json :: array();
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        string fri_user = row[0];
        members.push_back(fri_user);
    }
    *reflact = {
        {"sort",REFLACT},
        {"request",SHOW_FRIEND},
        {"members",members}
    };
    
    db->free_result(res);
    return true;
}

bool handle_add_member(json json_quest, json* reflact, unique_ptr<database>& db, 
                       unordered_map<string,int> user_to_cfd) 
{
    long gid = json_quest["gid"];
    string safe_gid = to_string(json_quest["gid"].get<long>());
    string safe_addname = db->escape_mysql_string_full(json_quest["addname"]);
    string username = json_quest["username"];
    string addname = json_quest["addname"];
    string redis_key = "offline:system_notice:" + addname;

    if(username == addname)
    {
        *reflact = { 
            {"sort", REFLACT},
            {"request",ADD_GROUP_MEM},
            {"reflact", "您不能将自己添加进群聊..."}
        };
        return true;
    }
    string sql = "INSERT INTO group_members (group_id, username, status, role) VALUES "
                 "(" + safe_gid + ", '" + safe_addname + "', 1, 'member')";
    bool execchk = db->execute_sql(sql);
    if (!execchk) {
        *reflact = { 
            {"sort", REFLACT},
            {"request",ADD_GROUP_MEM},
            {"reflact", "添加成员失败，可能群组/用户不存在或重复"}
        };
        return true;
    }
    auto it = user_to_cfd.find(addname);
    if(it != user_to_cfd.end())
    {
        int cfd = it->second;
        json send_json = {
            {"sort",MESSAGE},
            {"request",NOT_PEER_GROUP},
            {"message","通知：你已被 "+username+"  添加到 gid 为 "+to_string(gid)+" 的群聊"}
        };
        sendjson(send_json,cfd);
    }
    else
    {
        json msg = {
            {"timestamp",get_current_mysql_timestamp()},
            {"message","你已被 "+username+"  添加到 gid 为 "+to_string(gid)+" 的群聊"}
        };
        db->lpushJson(redis_key,msg);
    }
    *reflact = { 
        {"sort", REFLACT}, 
        {"request",ADD_GROUP_MEM},
        {"reflact", "成员添加成功"} 
    };
    return true;
}

bool handle_break_group(json json_quest, json* reflact, unique_ptr<database>& db, 
                        unordered_map<string,int> user_to_cfd,unordered_map<string,int> user_to_group) 
{
    MYSQL_RES* res;
    MYSQL_ROW row;
    long gid = json_quest["gid"];
    string redis_key = "group_message:"+to_string(gid);
    string safe_gid = to_string(gid);
    string username = json_quest["username"];

    res = db->query_sql("SELECT * FROM `groups` WHERE group_id = "+safe_gid+"");
    if(res == nullptr)
    {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return true;
    }
    db->free_result(res);
    res = db->query_sql("SELECT username FROM group_members WHERE "
                        "group_id = "+to_string(gid)+"");
    if(res == nullptr)
    {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return true;
    }
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        json send_json;
        string deluser = row[0];
        string redis_key = "offline:system_notice:" + deluser;
        if(deluser == username)
            continue;
        auto it = user_to_cfd.find(deluser);
        if(it != user_to_cfd.end())
        {
            int cfd = it->second;
            if(user_to_group[deluser] == gid)
            {
                send_json = {
                    {"sort",MESSAGE},
                    {"request",BREAK_GROUP},
                    {"outflag",true},
                    {"message","通知: gid 为 "+to_string(gid)+" 的群聊已经被解散"}
                };
            }
            else
            {
                send_json = {
                    {"sort",MESSAGE},
                    {"request",BREAK_GROUP},
                    {"outflag",false},
                    {"message","通知: gid 为 "+to_string(gid)+" 的群聊已经被解散"}
                };
            }
            sendjson(send_json,cfd);
        }
        else
        {
            json msg = {
                {"timestamp",get_current_mysql_timestamp()},
                {"message","gid 为 "+to_string(gid)+" 的群聊已经被解散"}
            };
            db->lpushJson(redis_key,msg);
        }
    }
    db->free_result(res);
    string sql = "DELETE FROM `groups` WHERE group_id = " + safe_gid;
    bool execchk = db->execute_sql(sql);
    if (!execchk) {
        *reflact = { 
            {"sort", ERROR},
            {"reflact", "MYSQL DELETE ERROR"}
        };
        return true;
    }
    execchk = db->execRedis("DEL " + redis_key);
    if(execchk == false)
    {
        *reflact = {
            {"sort",ERROR},
            {"reflact","redis 缓存清除出错..."}
        };
        return true;
    }
    *reflact = { 
        {"sort", REFLACT},
        {"request",BREAK_GROUP}, 
        {"reflact", "群聊解散成功"} 
    };
    
    return true;
}

bool handle_kill_user(json json_quest, json* reflact, unique_ptr<database>& db, 
                      unordered_map<string,int> user_to_cfd,unordered_map<string,int> user_to_group) 
{
    MYSQL_RES* res;
    MYSQL_ROW row;
    string kill_user = json_quest["kill_user"];
    string username = json_quest["username"];
    string safe_kill_user = db->escape_mysql_string_full(json_quest["kill_user"]);
    long gid = json_quest["gid"];
    string safe_gid = to_string(gid);
    string redis_key = "offline:system_notice:" + kill_user;
    if(kill_user == username)
    {
        *reflact = { 
            {"sort",REFLACT}, 
            {"request",KILL_GROUP_USER},
            {"reflact", "您不能将自己移出群聊"} 
        };
        return true;
    }
    res = db->query_sql("SELECT role FROM group_members WHERE group_id = " + safe_gid + " "
                        "AND username = '" + safe_kill_user + "'");
    if(res == nullptr)
    {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "MYSQL SELECT ERROR"}
        };
        return true;
    }
    row = mysql_fetch_row(res);
    if (row == nullptr) {
        db->free_result(res);
        *reflact = { 
            {"sort", REFLACT}, 
            {"request", KILL_GROUP_USER},
            {"reflact", "未找到该成员，无法移除"} 
        };
        return true;
    }
    string role = row[0];
    db->free_result(res);
    if(role == "owner")
    {
        *reflact = { 
            {"sort",REFLACT}, 
            {"request",KILL_GROUP_USER},
            {"reflact", "请不要将群主移出群聊..."} 
        };
        return true;
    }
    string sql = "DELETE FROM group_members WHERE group_id = " + safe_gid + " "
                 "AND username = '" + safe_kill_user + "'";
    bool execchk = db->execute_sql(sql);
    if (!execchk) {
        *reflact = { 
            {"sort", ERROR},
            {"reflact", "MYSQL DELETE ERROR"}
        };
        return true;
    }
    my_ulonglong affected = mysql_affected_rows(db->get_mysql_conn());
    if (affected == 0) {
        *reflact = {
            {"sort", REFLACT},
            {"request",KILL_GROUP_USER},
            {"reflact","成员移除失败"}
        };
        return true;
    }
    auto it = user_to_cfd.find(kill_user);
    if(it != user_to_cfd.end())
    {
        int cfd = it->second;
        json send_json;
        if(user_to_group[username] == gid)
        {
            send_json = {
                {"sort",MESSAGE},
                {"request",KILL_GROUP_USER},
                {"outflag",true},
                {"message","通知: 你已被 "+username+"  移出 gid 为 "+to_string(gid)+" 的群聊"}
            };
        }
        else
        {
            send_json = {
                {"sort",MESSAGE},
                {"request",KILL_GROUP_USER},
                {"outflag",false},
                {"message","通知: 你已被 "+username+"  移出 gid 为 "+to_string(gid)+" 的群聊"}
            };
        }

        sendjson(send_json,cfd);
    }
    else
    {
        json msg = {
            {"timestamp",get_current_mysql_timestamp()},
            {"message","你已被 "+username+"  移出 gid 为 "+to_string(gid)+" 的群聊"}
        };
        db->lpushJson(redis_key,msg);
    }
    *reflact = { 
        {"sort",REFLACT}, 
        {"request",KILL_GROUP_USER},
        {"reflact", "成员移除成功"} 
    };
    return true;
}

bool handle_del_group(json json_quest, json* reflact, unique_ptr<database>& db, 
                      unordered_map<string,int> user_to_cfd) 
{
    long gid = json_quest["gid"];
    string safe_gid = to_string(gid);
    string safe_username = db->escape_mysql_string_full(json_quest["username"]);

    string sql = "DELETE FROM group_members WHERE group_id = "+safe_gid+" AND username = '"+safe_username+"'";
    bool execchk = db->execute_sql(sql);
    if (!execchk) {
        *reflact = { 
            {"sort", ERROR},
            {"reflact", "MYSQL DELETE ERROR"}
        };
        return true;
    }
    my_ulonglong affected = mysql_affected_rows(db->get_mysql_conn());
    if (affected == 0) {
        *reflact = {
            {"sort", REFLACT},
            {"request",DELETE_GROUP},
            {"reflact", "群聊退出失败,可能用户不在该群"}
        };
        return true;
    }
    *reflact = { 
        {"sort", REFLACT},
        {"request",DELETE_GROUP}, 
        {"reflact", "群聊退出成功"} 
    };
    return true;
}
