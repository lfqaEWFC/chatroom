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
                     bool* FTP_stor_flag,bool* pri_flag,string client_num,
                     pthread_cond_t *cond,pthread_mutex_t *mutex,string* file_path,string* fri_username)
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
                    << "- 输入 STOR + 文件路径 可传输文件。\n" << endl;

                while (true && !(*end_flag) && *pri_flag){
                    char *file_input = readline(file_show);

                    if(strstr(file_input,"LIST")){
                        char *cmd = NULL;
                        char *path = NULL;
                        char *saveptr = NULL;
                        
                        cmd = strtok_r(file_input, " \n", &saveptr);
                        path = strtok_r(NULL, " \n", &saveptr);

                        json send_json;
                        if(path != NULL){
                            send_json = {
                                {"cmd","LIST"},
                                {"path",path},
                                {"run_flag",true}
                            };
                        }
                        else{
                            send_json = {
                                {"cmd","LIST"},
                                {"run_flag",false}
                            };
                        }
                        sendjson(send_json,FTP_ctrl_cfd);
                        handle_pthread_wait(*end_flag,cond,mutex);
                    }
                    else if(strstr(file_input,"STOR")){                    
                        string input_str(file_input);
                        size_t pos = input_str.find(' ');
                        string filepath;
                        if (pos != string::npos) {
                            filepath = input_str.substr(pos + 1);
                            if (filepath.empty()) {
                                cout << "命令格式错误，应为: STOR <文件路径>" << endl;
                                continue;
                            }
                            int fd = open(filepath.c_str(), O_RDONLY);
                            if (fd == -1) {
                                cout << "文件不存在或无法读取: " << filepath << endl;
                                continue;
                            }
                            close(fd);                
                        }
                        else {
                            cout << "命令格式错误，应为: STOR <文件路径>" << endl;
                            continue;
                        }
                        *FTP_stor_flag = true;

                        json send_json = {
                            {"cmd","PASV"},
                            {"filepath",filepath}
                        };
                        sendjson(send_json,FTP_ctrl_cfd);
                        *file_path = filepath;
                        handle_pthread_wait(*end_flag,cond,mutex);
                    } 
                    else if(strcmp(file_input,"EXIT") == 0){
                        cout << "退出文件传输模式..." << endl;
                        break;
                    }
                    else{
                        cout << "命令错误，请重新输入命令..." << endl;
                        continue;
                    }
                    free(file_input);
                }         
            }
            else if (message == "/exit") {
                json end = {
                    {"request",DEL_PEER},
                    {"from",username},
                    {"to",fri_user}
                };
                sendjson(end, cfd);
                fri_username->clear();
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
            free(input);
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

int handle_pasv(string reflact)
{
    char* buffer = new char[reflact.size() + 1];
    strcpy(buffer, reflact.c_str());

    char delimiter[4] = "(),";
    int cnt = 0;
    const char* in_token[10];
    char *token = strtok(buffer, delimiter);
    while(token != NULL && cnt < 10){
        in_token[cnt] = token;
        cnt++; 
        token = strtok(NULL, delimiter);
    }

    char data_ip[MAXBUF];
    snprintf(data_ip, sizeof(data_ip), "%s.%s.%s.%s",
             in_token[1], in_token[2], in_token[3], in_token[4]);
    int port = atoi(in_token[5])*256 + atoi(in_token[6]);
    char data_port[MAXBUF];
    snprintf(data_port, sizeof(data_port), "%d", port);

    int data_cfd = inetconnect(data_ip, data_port);

    delete[] buffer;

    if(data_cfd == -1){
        perror("inetconnect");
        return -1;
    }

    return data_cfd;
}

void handle_show_file(string username, int cfd, string* fri_username)
{
    char *fri_user;
    char retr_show[MSGBUF];

    strcpy(retr_show,"请输入对端用户或群组名称: ");
    fri_user = readline(retr_show);

    json send_json = {
        {"request",SHOW_FILE},
        {"fri_user",fri_user},
        {"username",username}
    };
    sendjson(send_json,cfd);

    *fri_username = fri_user;

    return;
}

void handle_retr_file(int FTP_ctrl_cfd,bool endflag,pthread_mutex_t* mutex,string* fri_username,
                      pthread_cond_t* cond,bool* FTP_retr_flag,string* file_name)
{
    int cfd = FTP_ctrl_cfd;
    char fileshow[MSGBUF];

    cout << "进入文件接收模式,输入EXIT退出" << endl;
    while(true && !endflag){
        strcpy(fileshow,"请输入文件名称: ");
        char* filename = readline(fileshow);
        if(strcmp(filename,"EXIT") == 0)
        {
            cout << "退出文件接收模式..." << endl;
            wait_user_continue();
            fri_username->clear();
            return;
        }
        else
        {
            *file_name = filename;
            json send_json = {
                {"cmd","PASV"},
                {"filename",filename}
            };
            sendjson(send_json,cfd);
        }
        *FTP_retr_flag = true;
        handle_pthread_wait(endflag,cond,mutex);
    }

    return;
}


