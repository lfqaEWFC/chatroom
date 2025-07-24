#include "User.hpp"

int handle_signin(json* signin)
{
    bool checkflag = true;
    char *input;
    char username[64];
    char password[64];
    char checkpw[64];
    char question[1024];
    char answer[1024];
    char show[LARGESIZE];
    
    strcpy(show,"账户名称：");
    input = readline(show);
    strcpy(username,input);
    while(true)
    {
        strcpy(show,"账户密码：");
        free(input);
        input = readline(show);
        strcpy(password,input);
        if(strlen(password) < 8){
            cout << "密码长度过短，请重新输入" << endl;
            memset(password,0,64);
            continue;
        }
        system("clear");
        break;
    }
    while(checkflag)
    {
        strcpy(show,"请确认密码：");
        free(input);
        input = readline(show);
        strcpy(checkpw,input);
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

    strcpy(show,"密保问题：");
    free(input);
    input = readline(show);
    strcpy(question,input);
    strcpy(show,"答案：");
    free(input);
    input = readline(show);
    strcpy(answer,input);
    
    *signin = {
        {"request",SIGNIN},
        {"username",username},
        {"password",password},
        {"que",question},
        {"ans",answer},
    };

    free(input);
    return 0;

}

bool handle_login(json *login)
{
    char *input;
    char show[LARGESIZE];
    char in_user[64];
    char in_password[64];

    strcpy(show,"用户名称： ");
    input = readline(show);
    strcpy(in_user,input);
    cout << "如果忘记密码,请输入Y以获取密保问题" << endl;
    strcpy(show,"用户密码： ");
    free(input);
    input = readline(show);
    strcpy(in_password,input);

    if(strcmp(in_password,"Y") == 0){
        *login = {
            {"request",FORGET_PASSWORD},
            {"username",in_user}
        };
        free(input);
        return true;
    }

    *login = {
        {"request",LOGIN},
        {"username",in_user},
        {"password",in_password}
    };

    free(input);
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

    char* input;
    char show[LARGESIZE];
    char break_username[LARGESIZE];
    char break_password[LARGESIZE];

    strcpy(show,"账号名称: ");
    input = readline(show);
    strcpy(break_username,input);
    strcpy(show,"密码: ");
    free(input);
    input = readline(show);
    strcpy(break_password,input);

    *json_break = {
        {"request",BREAK},
        {"username",break_username},
        {"password",break_password}
    };

    free(input);
    return true;
}

void handle_add_friend(json *json_friend,string username){

    char* input;
    char show[LARGESIZE];
    char friend_user[LARGESIZE];

    strcpy(show,"添加好友名称： ");
    input = readline(show);
    strcpy(friend_user,input);

    *json_friend = {
        {"request",ADD_FRIEND},
        {"username",username},
        {"fri_username",friend_user},
    };

    free(input);
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
    
    char show[LARGESIZE];
    char* input;
    char fri_user[LARGESIZE];
    
    strcpy(show,"对端好友名称： ");
    input = readline(show);
    strcpy(fri_user,input);

    *chat_name = {
        {"request",CHAT_NAME},
        {"username",username},
        {"fri_user",fri_user}
    };

    free(input);
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
                     pthread_cond_t *cond,pthread_mutex_t *mutex,string* file_path,
                     string* fri_username,string* pri_show)
{   
    cout << "进入私聊模式，对方：" << fri_user << endl;
    cout << "提示：\n"
        << "- 输入普通消息将直接发送。\n"
        << "- 输入 /exit 可退出私聊。\n" 
        << "- 输入 /file 可传输文件。\n" << endl;
        
    *pri_show = "["+username+"->"+fri_user+"]: ";

    while (true && !(*end_flag) && *pri_flag){
        
        string input;
        char show[MAX_REASONABLE_SIZE];
    
        sprintf(show,"[%s->%s]: ",username.c_str(),fri_user.c_str());
        cout << show;        
        getline(cin,input);                 
        string message = input;
    
        if (message == "/file") {
            char file_show[MAX_REASONABLE_SIZE];
            strcpy(file_show,"请输入命令：");
            *pri_show = "请输入命令：";

            cout << "进入文件传输模式：" << endl;
            cout << "提示：\n"
                << "- 输入 EXIT 退出文件传输模式。\n"
                << "- 输入 LIST + 路径名 将列出目录。\n"
                << "- 输入 STOR + 文件路径 可传输文件。\n"
                << "- 当前尚不支持文件名有特殊字符的文件传输\n" << endl;

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
                    *pri_show = "["+username+"->"+fri_user+"]: ";
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
            pri_show->clear();
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
    }
    return;
}

void handle_black(string username,int cfd) {

    char* input;
    json blacklist;
    bool in_flag = true;

    cout << "=============" << endl;
    cout << " a.加入黑名单 " << endl;
    cout << " b.移除黑名单 " << endl;
    cout << "=============" << endl;

    while(in_flag) {
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
        switch(choice) {
            case 'a': {
                char show[LARGESIZE];
                char target[LARGESIZE];
                strcpy(show,"请输入要加入黑名单的用户名： ");
                input = readline(show);
                strcpy(target,input);
                blacklist["request"] = ADD_BLACKLIST;
                blacklist["username"] = username;
                blacklist["target"] = target;
                in_flag = false;
                break;
            }
            case 'b': {
                char show[LARGESIZE];
                char target[LARGESIZE];
                strcpy(show,"请输入要移除黑名单的用户名： ");
                input = readline(show);
                strcpy(target,input);
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

    free(input);
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

    free(del_user);
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

    free(fri_user);
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
            free(filename);
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
        free(filename);
    }

    return;
}

void handle_create_group(string username,int cfd)
{
    char* input;
    char group_show[MSGBUF];
    json send_json;

    while(true)
    {    
        strcpy(group_show,"请输入创建的群聊名称： ");
        input = readline(group_show);
        if(strlen(input) > GROUP_LEN)
        {
            cout << "群聊名称过长，请重新输入...." << endl;
            free(input);
            continue;
        }
        break;
    }

    send_json = {
        {"request",CREATE_GROUP},
        {"username",username},
        {"group_name",input}
    };
    sendjson(send_json,cfd);

    free(input);
    return;
}

void handle_add_group(string username,int cfd,bool end_flag,bool* id_flag,
                          pthread_cond_t* cond,pthread_mutex_t* mutex)
{
    char *input;
    json send_json;
    char name_show[LARGESIZE];
    char id_show[LARGESIZE];
    
    strcpy(name_show,"请输入需要加入的群聊名称: ");
    input = readline(name_show);
    send_json = {
        {"request",SEL_GROUP},
        {"group_name",input}
    };
    sendjson(send_json,cfd);
    handle_pthread_wait(end_flag,cond,mutex);
    if(*id_flag)
    {
        strcpy(id_show,"请输入需要加入的群聊id: ");
        input = readline(id_show);
        send_json = {
            {"request",ADD_GROUP},
            {"group_id",atoi(input)},
            {"username",username}
        };
        sendjson(send_json,cfd);
        handle_pthread_wait(end_flag,cond,mutex);
        *id_flag = false;
    }
    
    wait_user_continue();
    free(input);
    return;
}

void deal_add_group(int cfd,string username,bool endflag,bool* group_add_flag,
                    pthread_cond_t* cond,pthread_mutex_t* mutex)
{
    json send_json;
    char id_show[MSGBUF];
    char name_show[MSGBUF];
    char* input;
    long gid;

    send_json = {
        {"request",DEAL_ADDGROUP},
        {"username",username},
    };
    sendjson(send_json,cfd);
    handle_pthread_wait(endflag,cond,mutex);

    if(*group_add_flag)
    {
        json send_json;
        json elements = json::array();
        strcpy(id_show,"请输入群聊id(输入-1退出交互): ");
        strcpy(name_show,"请输入用户名: ");

        while(true)
        {
            input = readline(id_show);
            gid = atoi(input);
            if(gid == -1)
            {
                free(input);
                break;
            }
            free(input);
            input = readline(name_show);
            json msg = {
                {"gid",gid},
                {"username",input}
            };
            elements.push_back(msg);
            free(input);
        }

        if(elements.size()!= 0)
        {
            send_json = {
                {"request",COMMIT_ADD},
                {"elements",elements}
            };
            sendjson(send_json,cfd);
            handle_pthread_wait(endflag,cond,mutex);
        }
        else cout << "未处理请求..." << endl;
        
        *group_add_flag = false;
    }
    
    wait_user_continue();
    return;
}

void handle_show_group(int cfd,string username)
{
    json send_json;

    send_json = {
        {"request",SHOW_GROUP},
        {"username",username}
    };

    sendjson(send_json,cfd);

    return;
}
