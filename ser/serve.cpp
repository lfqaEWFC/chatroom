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

bool handle_login(json json_quest,unique_ptr<database> &db,json *reflact,unordered_map<int, string> *cfd_to_user){

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
            auto it = cfd_to_user->begin();
            for(;it != cfd_to_user->end();it++){
                if(it->second == recv_username){
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

bool handle_logout(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<int, string>* cfd_to_user) {
    
    string recv_username = json_quest["username"];

    for (auto it = cfd_to_user->begin(); it != cfd_to_user->end(); ++it) {
        if (it->second == recv_username) {
            int cfd = it->first;
            cout << "delete cfd_to_user" << endl;
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
        if (it->second == fri_name) {
            int cfd = it->first;
            json send_fri;
            send_fri = {
                {"sort",MESSAGE},
                {"request", ASK_ADD_FRIEND},
                {"message", ""+username+" 向您发送了好友申请,需要处理时请选择聊天界面好友申请选项"}
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
            {"from", username},
            {"message", ""+username+" 向您发送了好友申请,需要处理时请选择聊天界面好友申请选项"}
        };

        string redis_key = "offline:friend_req:" + fri_name;
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

bool handle_get_offline(json json_quest,unique_ptr<database> &db,json *reflact){

    json arr = json::array();
    const string user = json_quest["username"];
    const string fri_key = "offline:friend_req:" + user;
    const string pri_key = "offline:private:" + user;

    redisReply *reply = db->execRedis("LRANGE "+fri_key+" 0 -1");
    if (!reply) {
        *reflact = {
            {"sort", ERROR},
            {"reflact", "服务端获取 Redis 回复失败"}
        };
        freeReplyObject(reply);
        return true;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        string err = reply->str;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "Redis 错误: " + err}
        };
        freeReplyObject(reply);
        return true;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            json req = json::parse(reply->element[i]->str);
            arr.push_back(req["message"]);
        }
    }

    freeReplyObject(reply);
    reply = db->execRedis("HGETALL "+pri_key+"");
    if (reply->type == REDIS_REPLY_ERROR) {
        string err = reply->str;
        *reflact = {
            {"sort", ERROR},
            {"reflact", "Redis 错误: " + err}
        };
        freeReplyObject(reply);
        return true;
    }
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            string sender = reply->element[i]->str;
            string count = reply->element[i + 1]->str;
            string line = "好友 [" + sender + "] 有 " + count + " 条未读私聊消息";
            arr.push_back(line);
        }
    }

    *reflact = {
        {"sort", MESSAGE},
        {"request", GET_OFFLINE_MSG},
        {"element",reply->elements},
        {"elements", arr}
    };

    redisReply* del_reply = db->execRedis("DEL "+fri_key+"");
    redisReply* del_pri = db->execRedis("DEL "+pri_key+"");
    if((del_reply == nullptr) || (del_pri == nullptr)){
        cerr << "Main Redis DB DEL "+fri_key+" Error" << endl;
        freeReplyObject(reply);
        freeReplyObject(del_reply);
        return false;
    }

    freeReplyObject(reply);
    freeReplyObject(del_reply);

    return true;
}

bool handle_get_friend(json json_quest,unique_ptr<database> &db,json *reflact){
    
    const string user = json_quest["username"];

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
            {"username",user},
            {"fri_user",fri_user}
        };
    }

    return true;
}

bool handle_deal_friend(json json_quest,unique_ptr<database> &db,json *reflact){

    vector<string> recv_commit = json_quest["commit"];
    vector<string> recv_refuse = json_quest["refuse"];
    string fri_user = json_quest["username"];
    ssize_t c_size = recv_commit.size();
    ssize_t r_size = recv_refuse.size();
    bool c_chk = true;
    bool r_chk = true; 

    for(int i=0;i<c_size;i++){
        string user = recv_commit[i];
        c_chk = db->execute_sql("UPDATE friendship SET status = 1 WHERE username = '"+user+"' AND friend_username = '"+fri_user+"'");
        if(c_chk == false) 
            break;
        c_chk = db->execute_sql("INSERT INTO friendship(username, friend_username, status) VALUES ('" +fri_user+ "','" +user+ "',1)");
        if(c_chk == false) 
            break;
    }

    for(int i=0;i<r_size;i++){
        string user = recv_refuse[i];
        r_chk = db->execute_sql("DELETE FROM friendship WHERE username = '"+user+"' AND friend_username = '"+fri_user+"'");
        if(r_chk == false)
            break;
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

    string fri_user = json_quest["fri_user"];
    string username = json_quest["username"];

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
            cout << "add user_to_friend: "+username+"" << endl;
            (*user_to_friend)[username] = fri_user;
            *reflact = {
                {"sort",REFLACT},
                {"request",CHAT_NAME},
                {"chat_flag",true},
                {"pri_username",fri_user},
                {"reflact","验证成功，可进入私聊模式..."}
            };
        }
        else if(status == 2){
            *reflact = {
                {"sort",REFLACT},
                {"request",CHAT_NAME},
                {"chat_flag",false},
                {"reflact","你已将"+fri_user+"加入黑名单...."}
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
    
    string username = json_quest["username"];
    string fri_user = user_to_friend[username];
    
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
    bool file_flag = json_quest["file_flag"];

    if(file_flag){

    }
    else{
        MYSQL_RES *res = db->query_sql("SELECT status FROM friendship WHERE username = '"+sender+"' AND friend_username = '"+receiver+"'");
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

        bool sql_chk = db->execute_sql("INSERT INTO private_message(sender, receiver, content) VALUES('" +sender+ "','" +receiver+ "','"+message+"')");
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

bool handle_add_black(json json_quest, json *reflact, unique_ptr<database>& db){

    string username = json_quest["username"];
    string target = json_quest["target"];

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
            {"reflact", "成功将用户"+target+"添加至黑名单..."}
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
    string username = json_quest["username"];
    string target = json_quest["target"];

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
            {"reflact", "成功将用户"+target+"移出黑名单..."}
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
    string username = json_quest["username"];
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

bool handle_del_friend(json json_quest,json *reflact,unique_ptr<database>&db) {
    string username = json_quest["username"];
    string del_user = json_quest["del_user"];

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

    return true;
}

bool handle_add_file(json json_quest,json *reflact,unique_ptr<database>&db,unordered_map<int, string> cfd_to_user){

    string sender = json_quest["sender"];
    string filename = json_quest["filename"];
    string receiver = json_quest["receiver"];

    MYSQL_RES *res = db->query_sql("SHOW TABLEs LIKE 'private_file'");
    if(res == nullptr){
        cout << "SHOW failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL SHOW ERROR..."}
        };
        return true; 
    }
    if(mysql_num_rows(res)<=0){
        bool execchk = db->execute_sql(
            "CREATE TABLE private_file("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "filename VARCHAR(64) NOT NULL,"
            "sender VARCHAR(64) NOT NULL,"
            "receiver VARCHAR(64) NOT NULL"
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
                                  "VALUES('" +sender+ "','" +receiver+ "','"+filename+"')");
    if(exechk == false){
        cout << "INSERT failed" << endl;
        *reflact = {
            {"sort",ERROR},
            {"reflact","MYSQL INSERT ERROR..."}
        };
        return true; 
    }
    auto it = cfd_to_user.begin();
    for(;it != cfd_to_user.end();it++){
        if(it->second == receiver){
            int cfd = it->first;
            json send_json = {
                {"sort",MESSAGE},
                {"request",NON_PEER_CHAT},
                {"message","通知: 好友"+sender+"发送了一个文件"}
            };
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
                                   "VALUES('" +sender+ "','" +receiver+ "','"+sender+"发送了一个文件')");
    if(sql_chk == false){
        cout << "INSERT failed" << endl;
        *reflact = {
            {"sort",ERROR}, 
            {"reflact","MYSQL INSERT ERROR"}
        };
        return true;
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
    string username = json_quest["username"];
    string fri_user = json_quest["fri_user"];

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
            {"reflact","用户 "+fri_user+" 不存在或并未向您发送文件..."}
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
        {"file_user",fri_user},
        {"get_flag",true},
        {"reflact",filenames}
    };

    return true;

}

bool handle_del_retr(json json_quest,json* reflact,unique_ptr<database>&db){

    char cur_path[LARGESIZE];
    char del_filename[LARGESIZE];
    char file_path[MAXBUF];
    string filename = json_quest["filename"];
    string sender = json_quest["sender"];
    string receiver = json_quest["receiver"];
    
    getcwd(cur_path,sizeof(cur_path));

    sprintf(del_filename,"%s_to_%s-%s",sender.c_str(),receiver.c_str(),filename.c_str());
    sprintf(file_path,"%s/%s/%s",cur_path,"file_tmp",del_filename);
    remove(file_path);

    bool sql_chk = db->execute_sql("DELETE FROM private_file WHERE sender = '"+sender+"' "
                                   "AND receiver = '"+receiver+"' AND filename = '"+filename+"'");
    if(sql_chk == false){
        *reflact = {
            {"sort",ERROR},
            {"message","MYSQL DELETE ERROR"}
        };
        return true;
    }

    *reflact = {
        {"sort",MESSAGE},
        {"request",END_RETR},
        {"message","文件 "+filename+" 下载完成..."}
    };

    return true;
}
