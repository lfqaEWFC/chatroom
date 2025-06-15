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
