#include "serve.hpp"

bool handle_signin(json json_quest, unique_ptr<database> &db) {

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

    string username = json_quest["username"];
    string password = json_quest["password"];
    string question = json_quest["que"];
    string answer   = json_quest["ans"];

    string sql = "INSERT INTO user(username, password, que, ans) VALUES('" +username + "','" + password + "','" + question + "','" + answer + "')";

    bool addchk = db->execute_sql(sql);
    if (!addchk) {
        cout << "INSERT failed" << endl;
        return false;
    }

    return true;
}

bool handle_forget_password(json json_quest,unique_ptr<database> &db,json *reflact){

    string recv_username = json_quest["username"];

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
            string user_question = row[0];
            *reflact = {
                {"sort",REFLACT},
                {"request",FORGET_PASSWORD},
                {"que_flag",true},
                {"username",recv_username},
                {"reflact",user_question}
            };
            db->free_result(res);
            return true;
        }
    }

    return false;
}

bool handle_login(json json_quest,unique_ptr<database> &db,json *reflact){

    string recv_username = json_quest["username"];
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
            if(user_password == recv_password){
                *reflact = {
                    {"sort",REFLACT},
                    {"request",LOGIN},
                    {"login_flag",true},
                    {"username",recv_username},
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
    string recv_username = json_quest["username"];

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
            string user_answer = row[0];
            if(user_answer == recv_answer){
                *reflact = {
                    {"sort",REFLACT},
                    {"request",CHECK_ANS},
                    {"username",recv_username},
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

bool handle_in_online(json json_quest,unique_ptr<database> &db){

    string recv_username = json_quest["username"];
    redisReply *online_reply = new redisReply;

    online_reply = db->execRedis("exists online_users");

    if(online_reply == nullptr){
        cout << "COMMAND EXISTS ERROR" << endl;
        return false;
    }else{
        redisReply *check_reply = new redisReply;
        check_reply = db->execRedis("SADD online_users '"+recv_username+"'");
        if(check_reply == nullptr){
            cout << "COMMAND SADD ERROR" << endl;
            delete online_reply;
            delete check_reply;
            return false;
        }
        delete check_reply;
    }

    delete online_reply;
    
    return true;
}

bool handle_logout(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<int, string>* cfd_to_user) {
    
    string recv_username = json_quest["username"];

    for (auto it = cfd_to_user->begin(); it != cfd_to_user->end(); ++it) {
        if (it->second == recv_username) {
            int cfd = it->first;
            db->redis_del_online_user(recv_username);
            cfd_to_user->erase(it);
            *reflact = {
                {"sort", REFLACT},
                {"request", LOGOUT},
                {"reflact", "用户成功退出"}
            };
            return true;
        }
    }

    *reflact = {
        {"sort", ERROR},
        {"reflact", "退出失败，用户不在线或未注册..."}
    };

    return false;
}

bool handle_break(json json_quest,unique_ptr<database> &db,json *reflact){

    string break_username = json_quest["username"];
    string break_password = json_quest["password"];

    string sql = "DELETE FROM user WHERE username = '" + break_username + "' AND password = '" + break_password + "'";
    bool res = db->execute_sql(sql);

    if(!res){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端查询用户表异常..."}
        };
        return false;
    }
    else{
        my_ulonglong affected = mysql_affected_rows(db->get_mysql_conn());

        if (affected == 0) {
            *reflact = {
                {"sort", REFLACT},
                {"request",BREAK},
                {"reflact", "注销失败，用户名或密码错误..."}
            };
        } 
        else {
            *reflact = {
                {"sort", REFLACT},
                {"request",BREAK},
                {"reflact", "注销成功，账户已被删除"}
            };
        }
    }

    return true;
}

bool handle_add_friend(json json_quest,unordered_map<int, string>* cfd_to_user,unique_ptr<database> &db,json *reflact){

    string fri_name = json_quest["fri_username"];
    string username = json_quest["username"];

    MYSQL_RES* res = db->query_sql("SHOW TABLES LIKE 'friendship'");

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
        return false;
    }

    if (username == fri_name) {
        *reflact = {
            {"sort",REFLACT},
            {"request",ADD_FRIEND},
            {"reflact","不能添加自己为好友"}
        };
        return false;
    }

    string check_friendship_sql = "SELECT * FROM friendship WHERE username = '" + username + "' AND friend_username = '" + fri_name + "'";
    MYSQL_RES* rel_check = db->query_sql(check_friendship_sql);
    if (rel_check && mysql_num_rows(rel_check) > 0) {
        *reflact = {
            {"sort",REFLACT},
            {"request",ADD_FRIEND},
            {"reflact","好友申请已发送，请勿重复添加"}
        };
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
        return false;
    }

    auto it = cfd_to_user->begin();
    for (; it != cfd_to_user->end(); ++it) {
        if (it->second == fri_name) {
            int cfd = it->first;
            json send_fri;
            send_fri = {
                {"sort",MESSAGE},
                {"request", ASK_ADD_FRIEND},
                {"message", "'"+username+"'向您发送了好友申请，需要处理时请选择聊天界面好友申请选项"}
            };
            sendjson(send_fri,cfd);
            return true;
        }
    }

    if(it == cfd_to_user->end()){
        json offline_msg = {
            {"type", "friend_request"},
            {"from", username},
            {"message", "'" + username + "'向您发送了好友申请"}
        };

        string redis_value = offline_msg.dump();
        string redis_cmd = "LPUSH offline:friend_req:" + fri_name + " '" + offline_msg.dump() + "'";

        bool redis_ok = db->execRedis(redis_cmd);
        if (!redis_ok) {
            cout << "Redis 离线消息写入失败" << endl;
            *reflact = {
                {"sort",ERROR},
                {"reflact","Redis 离线消息写入失败"}
            };
            return false;
        }
    }

    *reflact = {
        {"sort",REFLACT},
        {"request",ADD_FRIEND},
        {"reflact","好友申请发送成功,等待处理中..."}
    };

    return true;
}

bool handle_get_offline(json json_quest,unique_ptr<database> &db,json *reflact){

    const string user = json_quest["username"];
    const string key  = "offline:friend_req:" + user;

    redisReply *reply = db->execRedis("LRANGE "+key+" 0 -1");
    if (!reply) {
        *reflact = {
        {"sort", ERROR},
        {"reflact", "服务端获取 Redis 回复失败"}
        };
        return true;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        string err = reply->str;
        freeReplyObject(reply);
        *reflact = {
        {"sort", ERROR},
        {"reflact", "Redis 错误: " + err}
        };
        return true;
    }

    json arr = json::array();

    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            json req = json::parse(reply->element[i]->str);
            arr.push_back(req["message"]);
        }
    }

    *reflact = {
        {"sort", MESSAGE},
        {"request", GET_OFFLINE_MSG},
        {"element",reply->elements},
        {"elements", arr}
    };

    freeReplyObject(reply);

    return true;
}
