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

    const string user = json_quest["username"];
    const string fri_key  = "offline:friend_req:" + user;

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
        freeReplyObject(reply);
        *reflact = {
            {"sort", ERROR},
            {"reflact", "Redis 错误: " + err}
        };
        freeReplyObject(reply);
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

    redisReply* del_reply = db->execRedis("DEL "+fri_key+"");
    if(del_reply == nullptr){
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

    MYSQL_RES *res = db->query_sql("SELECT * FROM user WHERE username = '"+fri_user+"'");
    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端查询用户表出错..."}
        };
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


    res = db->query_sql("SELECT * FROM friendship WHERE username = '"+username+"' AND friend_username = '"+fri_user+"' AND status = 1");
    if(res == nullptr){
        *reflact = {
            {"sort",ERROR},
            {"reflact","服务端验证好友关系出错..."}
        };
        return true;
    }
    rows = mysql_num_rows(res);
    if(rows == 0){
        *reflact = {
            {"sort",REFLACT},
            {"request",CHAT_NAME},
            {"chat_flag",false},
            {"reflact","您还尚未与该用户建立好友关系..."}
        };
        return true;
    }else{
        (*user_to_friend)[username] = fri_user;
        *reflact = {
            {"sort",REFLACT},
            {"request",CHAT_NAME},
            {"chat_flag",true},
            {"reflact","**********"+fri_user+"**********"}
        };
        return true;
    }

    return true;
}
