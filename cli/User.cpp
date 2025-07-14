#include "User.hpp"

int handle_signin(json* signin){

    bool checkflag = true;
    char *username = new char[64];
    char *password = new char[64];
    char *checkpw = new char[64];
    char question[1024];
    char answer[1024];
    
    cout << "请输入注册账户名称" << endl;
    cin >> username;
    while(true){
        cout << "请输入注册账户密码" << endl;
        cin >> password;
        if(strlen(password) < 8){
            cout << "密码长度过短，请重新输入" << endl;
            memset(password,0,64);
            continue;
        }
        system("clear");
        break;
    }
    while(checkflag){
        cout << "请确认密码" << endl;
        cin >> checkpw;
        if(strcmp(checkpw,password) == 0){
            cout << "密码设置成功" << endl;
            checkflag = false;
            continue;
        }
        else{
            cout << "确认密码与帐号密码不相同，请重新确认" << endl;
            continue;
        }
    }

    cout << "请输入密保问题" << endl;
    cin >> question;
    cout << "请输入答案" << endl;
    cin  >> answer;
    
    *signin = {
        {"request",SIGNIN},
        {"username",username},
        {"password",password},
        {"que",question},
        {"ans",answer},
    };

    delete[] username;
    delete[] password;
    delete[] checkpw;

    return 0;

}

bool handle_login(json *login){

    char *in_user = new char[64];
    string in_password;

    cout << "请输入用户名称 : " << endl;
    cin >> in_user;
    cout << "请输入用户密码 : " << endl;
    cout << "如果忘记密码,请输入Y以获取密保问题..." << endl;
    cin >> in_password;

    if(in_password == "Y"){
        *login = {
            {"request",FORGET_PASSWORD},
            {"username",in_user}
        };
        return true;
    }

    *login = {
        {"request",LOGIN},
        {"username",in_user},
        {"password",in_password}
    };

    cout << "end login" << endl;

    return true;
}

void handle_success_login(int cfd,string username){

    json send_json = {
        {"request",IN_ONLINE},
        {"username",username}
    };

    sendjson(send_json,cfd);

    return;
}

bool handle_break(json *json_break){

    string break_username;
    string break_password;

    cout << "请输入要注销的账号名称: " << endl;
    cin >> break_username;
    cout << "请输入帐号密码: " << endl;
    cin >> break_password;

    *json_break = {
        {"request",BREAK},
        {"username",break_username},
        {"password",break_password}
    };

    return true;
}

void handle_add_friend(json *json_friend,string username){

    string friend_user;

    cout << "请输入需要添加的好友名称: " << endl;
    cin >> friend_user;

    *json_friend = {
        {"request",ADD_FRIEND},
        {"username",username},
        {"fri_username",friend_user},
    };

    return;
}

void handle_offline_login(int cfd,string username){
 
    json send_json = {
        {"request",GET_OFFLINE_MSG},
        {"username",username}
    };

    sendjson(send_json,cfd);

    return;
}

void handle_get_friend_req(json *get_friend_req,string username){

    *get_friend_req = {
        {"request",GET_FRIEND_REQ},
        {"username",username},
    };

    return;
}

void handle_chat_name(json *chat_name,string username){
    
    string send_username = username;
    string fri_user;
    
    cout << "请输入好友名称" << endl;
    cin >> fri_user;

    *chat_name = {
        {"request",CHAT_NAME},
        {"username",username},
        {"fri_user",fri_user}
    };

    return;
}

void handle_history_pri(json *offline_pri,string username){

    *offline_pri= {
        {"username",username},
        {"request",GET_HISTORY_PRI}
    };

    return;
}

void handle_pri_chat(string username,string fri_user,int cfd,int FTP_ctrl_cfd,bool* end_flag,
                     bool* pri_flag,string client_num,pthread_cond_t *cond,pthread_mutex_t *mutex)
{   
    cout << "进入私聊模式，对方：" << fri_user << endl;
    cout << "提示：\n"
        << "- 输入普通消息将直接发送。\n"
        << "- 输入 /exit 可退出私聊。\n" 
        << "- 输入 /file 可传输文件。\n" << endl;
        
        while (true && !(*end_flag) && *pri_flag){
           
            char show[MAX_REASONABLE_SIZE];
      
            sprintf(show,"[%s->%s]: ",username.c_str(),fri_user.c_str());        
            char *input = readline(show);
            if (!input) break;                   
            string message = string(input);
        
            if (message == "/file") {
               char file_show[MAX_REASONABLE_SIZE];
               strcpy(file_show,"请输入命令：");

               cout << "进入文件传输模式：" << endl;
               cout << "提示：\n"
                    << "- 输入 EXIT 退出文件传输模式。\n"
                    << "- 输入 LIST + 路径名 将列出目录。\n"
                    << "- 输入 RETR + 文件路径 可传输文件。\n" << endl;

                while (true && !(*end_flag) && *pri_flag){
                    char *file_input = readline(file_show);

                    if(strstr(file_input,"LIST")){
                        send(FTP_ctrl_cfd,file_input,sizeof(file_input),0);
                        handle_pthread_wait(*end_flag,cond,mutex);
                    }
                    else if(strstr(file_input,"RETR")){

                    } 
                    else if(strcpy(file_input,"EXIT") == 0){

                    }
                    else{
                        cout << "命令错误，请重新输入命令..." << endl;
                        continue;
                    }
                }
                    
            }
            else if (message == "/exit") {
                json end = {
                    {"request",DEL_PEER},
                    {"from",username},
                    {"to",fri_user}
                };
                sendjson(end, cfd);
                break;
            }
            else {
                if(!(*end_flag) && *pri_flag){
                    json msg = {
                        {"request", PRIVATE_CHAT},
                        {"from", username},
                        {"to", fri_user},
                        {"file_flag", false},
                        {"message", message}
                    };
                    sendjson(msg, cfd);
                }
            }
        }
        
    return;
}

void handle_black(string username,int cfd,bool *rl_display_flag) {

    char* input;
    json blacklist;
    bool in_flag = true;

    cout << "=============" << endl;
    cout << " a.加入黑名单 " << endl;
    cout << " b.移除黑名单 " << endl;
    cout << "=============" << endl;

    while(in_flag) {
        *rl_display_flag = true; 
        input = readline("请输入选项: ");
        if (input == nullptr) {
            cout << "输入不能为空，请重新输入..." << endl;
            continue;
        }
        if(strlen(input) > 1){
            cout << "请输入一个字符...." << endl;
            continue;
        }
        char choice = input[0];
        free(input);
        *rl_display_flag = false; 
        switch(choice) {
            case 'a': {
                cout << "请输入要加入黑名单的用户名:" << endl;
                string target;
                cin >> target;
                blacklist["request"] = ADD_BLACKLIST;
                blacklist["username"] = username;
                blacklist["target"] = target;
                in_flag = false;
                break;
            }
            case 'b': {
                cout << "请输入要移除黑名单的用户名:" << endl;
                string target;
                cin >> target;
                blacklist["request"] = REMOVE_BLACKLIST;
                blacklist["username"] = username;
                blacklist["target"] = target;
                in_flag = false;
                break;
            }
            default: {
                cout << "请输入选项 a 或 b..." << endl;
                break;
            }
        }
    }

    sendjson(blacklist,cfd);

    return;
}

void handle_check_friend(string username,int cfd){

    json check = {
        {"username",username},
        {"request",CHECK_FRIEND},
    };
    sendjson(check,cfd);

    return;

}

void handle_delete_friend(string username,int cfd){

    char show[NOMSIZE] = "请输入需要删除的用户: ";
    char *del_user = readline(show);

    json delete_json{
        {"username",username},
        {"del_user",del_user},
        {"request",DELETE_FRIEND}
    };

    sendjson(delete_json,cfd);

    return;
}

void handle_pthread_wait(bool endflag,pthread_cond_t *cond,pthread_mutex_t *mutex){
    if(!endflag)
        pthread_cond_wait(cond,mutex);
        
    return;
}
