#include "User.hpp"

bool is_all_space(const char* str)
{
    if (str == NULL) return true;
    while (*str) {
        if (!isspace(*str)) return false;
        str++;
    }
    return true;
}

int handle_signin(json* signin)
{
    char *input;
    char username[64];
    char password[64];
    char checkpw[64];
    char question[1024];
    char answer[1024];
    char show[LARGESIZE];
    
    while(true)
    {
        system("clear");
        strcpy(show,"账户名称：");
        input = readline(show);
        if(is_all_space(input))
        {
            cout << "请勿输入全部为空的字符串..." << endl;
            wait_user_continue();
            continue;
        }
        strcpy(username,input);
        while(true)
        {
            system("clear");
            strcpy(show,"账户密码：");
            free(input);
            input = readline(show);
            strcpy(password,input);
            if(strlen(password) < 8){
                cout << "密码长度过短，请重新输入" << endl;
                wait_user_continue();
                continue;
            }
            if(is_all_space(password))
            {
                cout << "请勿输入全部为空的字符串..." << endl;
                wait_user_continue();
                continue;
            }

            system("clear");  
            strcpy(show,"请确认密码：");
            free(input);
            input = readline(show);
            strcpy(checkpw,input);
            if(strcmp(checkpw,password) == 0){
                cout << "密码设置成功" << endl;
                wait_user_continue();
                break;
            }
            else{
                cout << "确认密码与帐号密码不相同，请重新设置密码..." << endl;
                wait_user_continue();
                continue;
            }
        }
        break;
    }
    while(true)
    {
        system("clear");
        strcpy(show,"密保问题：");
        free(input);
        if(is_all_space(input))
        {
            cout << "请勿输入全部为空的字符串..." << endl;
            wait_user_continue();
            continue;
        }
        input = readline(show);
        strcpy(question,input);
        strcpy(show,"答案：");
        free(input);
        input = readline(show);
        if(is_all_space(input))
        {
            cout << "请勿输入全部为空的字符串..." << endl;
            wait_user_continue();
            continue;
        }
        strcpy(answer,input); 
        break;
    }
        
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
        << "- 输入 /file 可传输文件。\n";
    cout << "注意: 最多可以发送 4096 个字节的长文本" << endl;
    cout << endl;
        
    *pri_show = "["+username+"->"+fri_user+"]: ";

    while (true && !(*end_flag) && *pri_flag){
        if(*end_flag || !(*pri_flag)) break;
        string input;
        char show[MAX_REASONABLE_SIZE];
    
        sprintf(show,"[%s->%s]: ",username.c_str(),fri_user.c_str());
        cout << show;        
        getline(cin,input);                 
        string message = input;
    
        if (message == "/file") {
            cout << endl;
            char file_show[MAX_REASONABLE_SIZE];
            strcpy(file_show,"请输入命令：");
            *pri_show = "请输入命令：";

            cout << "进入文件传输模式：" << endl;
            cout << "提示：\n"
                << "- 输入 EXIT 退出文件传输模式。\n"
                << "- 输入 LIST + 路径名 将列出目录。\n"
                << "- 输入 STOR + 文件路径 可传输文件。\n" << endl;

            while (true && !(*end_flag) && *pri_flag){
                if(*end_flag || !(*pri_flag)) break;
                char *file_input = readline(file_show);

                if(strstr(file_input,"LIST") && !(*end_flag) && *pri_flag){
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
                else if(strstr(file_input,"STOR") && !(*end_flag) && *pri_flag){                    
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
                    if(*end_flag || !*pri_flag)
                        cout << "您不能向该好友发送文件..." << endl;
                    else
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

void handle_black(string username,int cfd,bool end_flag,pthread_cond_t* cond,pthread_mutex_t* mutex) {

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
                show_user_friend(cfd,username);
                handle_pthread_wait(end_flag, cond, mutex);
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
                show_user_friend(cfd,username);
                handle_pthread_wait(end_flag, cond, mutex);
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

void handle_show_file(string username, int cfd,bool endflag,
                      pthread_cond_t* cond,pthread_mutex_t* mutex,
                      bool* pri_chat_flag,bool* group_flag)
{
    int num;
    long gid;
    char *input;
    char show[MSGBUF];
    
    cout << "=============" << endl;
    cout << " 1.私聊模式 " << endl;
    cout << " 2.群聊模式 " << endl;
    cout << "=============" << endl;

    while(true)
    {
        strcpy(show,"请选择私聊或者群聊模式：");
        input = readline(show);
        num = atoi(input);
        if(num != 1 && num != 2){
           cout << "请您输入正确的选项..." << endl; 
           free(input);
           continue;
        }
        free(input);
        break;
    }

    if(num == 1)
    {
        *pri_chat_flag = true;
        show_user_friend(cfd,username);
        handle_pthread_wait(endflag, cond, mutex);
        strcpy(show,"请输入对端用户名称：");
        input = readline(show);
        json send_json = {
            {"request",SHOW_FILE},
            {"group_flag",false},
            {"sender",input},
            {"username",username}
        };
        sendjson(send_json,cfd);
        free(input);
    }
    if(num == 2)
    {
        *group_flag = true;
        handle_show_group(cfd,username);
        handle_pthread_wait(endflag,cond,mutex);
        strcpy(show,"请输入群聊id: ");
        while(true && *group_flag){
            input = readline(show);
            gid = atoi(input);
            if(gid == 0)
            {
                cout << "请输入正确的群聊id" << endl;
                *group_flag = false;
                free(input);
                break;
            } 
            free(input);
            break;
        }
        if(*group_flag)
        {
            json send_json = {
                {"group_flag",true},
                {"request",SHOW_FILE},
                {"group_id",gid},
                {"username",username}
            };
            sendjson(send_json,cfd);
        }
    }
    
    return;
}

void handle_retr_file(int FTP_ctrl_cfd,bool endflag,pthread_mutex_t* mutex,string* fri_username,
                      pthread_cond_t* cond,bool* FTP_retr_flag,string* file_path,bool group_flag)
{
    int cfd = FTP_ctrl_cfd;
    char fileshow[MSGBUF];

    cout << "进入文件接收模式,输入EXIT退出" << endl;

    if(group_flag)
    {
        string sender;
        string filepath;
        char *file_input;
        while(true){
            strcpy(fileshow,"请输入文件发送者：");
            file_input = readline(fileshow);
            if(strcmp(file_input,"EXIT") == 0)
            {
                cout << "退出文件接收模式..." << endl;
                wait_user_continue();
                fri_username->clear();
                free(file_input);
                return;
            }
            sender = file_input;
            *fri_username = sender;
            free(file_input);
            strcpy(fileshow,"请输入文件路径：");
            file_input = readline(fileshow);  
            filepath = file_input;
            *file_path = filepath;
            free(file_input);
            json send_json = {
                {"cmd","PASV"},
                {"filename",filepath}
            };
            sendjson(send_json,cfd);
                    *FTP_retr_flag = true;
        handle_pthread_wait(endflag,cond,mutex);
        }
    }
    else
    {
        while(true && !endflag){
            strcpy(fileshow,"请输入文件路径: ");
            char* filepath = readline(fileshow);
            if(strcmp(filepath,"EXIT") == 0)
            {
                cout << "退出文件接收模式..." << endl;
                wait_user_continue();
                fri_username->clear();
                free(filepath);
                return;
            }
            else
            {
                *file_path = filepath;
                json send_json = {
                    {"cmd","PASV"},
                    {"filename",filepath}
                };
                sendjson(send_json,cfd);
            }
            *FTP_retr_flag = true;
            handle_pthread_wait(endflag,cond,mutex);
            free(filepath);
        }
    }

    return;
}

void handle_create_group(string username,int cfd,bool& create_wait)
{
    char* input;
    char group_show[MSGBUF];
    json send_json;

    while(true)
    {    
        system("clear");
        strcpy(group_show,"请输入创建的群聊名称： ");
        input = readline(group_show);
        if(strlen(input) > GROUP_LEN)
        {
            cout << "群聊名称过长，请重新创建..." << endl;
            free(input);
            wait_user_continue();
            create_wait = false;
            return;
        }
        if(is_all_space(input))
        {
            cout << "请勿输入全部为空的字符串..." << endl;
            free(input);
            wait_user_continue();
            create_wait = false;
            return;
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

void handle_group_name(int cfd,string username,bool* group_flag)
{   
    char *input;
    char name_show[LARGESIZE];
    char id_show[LARGESIZE];
    json send_json;
    long group_id;

    strcpy(id_show,"请输入群聊id: ");

    input = readline(id_show);
    group_id = atoi(input);
    if(group_id == 0 && group_id < 0)
    {
        cout << "请输入正确的gid..." << endl;
        *group_flag = false;
        return;
    }

    send_json = {
        {"request",GROUP_NAME},
        {"username",username},
        {"gid",group_id}
    };
    sendjson(send_json,cfd);

    free(input);
    return;
}

void handle_history_group(int cfd,string username,long group_id)
{
    json send_json;

    send_json = {
        {"sort",REFLACT},
        {"request",GROUP_HISTORY},
        {"gid",group_id},
        {"username",username}
    };
    sendjson(send_json,cfd);

    return;
}

void handle_group_chat(int cfd,string username,string group_role,long group_id,
                       string group_name,bool end_flag,string* group_show,
                       pthread_cond_t *cond,pthread_mutex_t *mutex,string* file_path,
                       int FTP_ctrl_cfd,bool* FTP_stor_flag,bool* group_flag)
{
    cout << "进入群聊模式，群名：" << group_name << endl;
    cout << "提示：\n"
        << "- 输入普通消息将直接发送。\n"
        << "- 输入 /file 可传输文件。\n"
        << "- 输入 /exit 可退出群聊模式。\n"
        << "- 输入 /add 可邀请好友加入群聊。\n";
    
    if(group_role == "owner")
    {
        cout << "- 输入 /break 可解散群聊。\n"
            << "- 输入 /admin 可以添加或者删除管理员。\n";
    }
    if(group_role == "admin" || group_role == "owner")
        cout << "- 输入 /kill 可退出移除用户。\n";
    if(group_role == "admin" || group_role == "member")
        cout << "- 输入 /delete 可退出群聊。 \n";
    cout << "注意: 最多可以发送 4096 个字节的长文本" << endl;
    cout << endl;

    *group_show = "[ "+group_name+" ] "+username+": ";
    
    while(true && !end_flag && *group_flag)
    {
        json send_json;
        string message;
        char show[MAX_REASONABLE_SIZE];
    
        sprintf(show,"[ %s ] %s: ",group_name.c_str(),username.c_str());
        cout << show;        
        getline(cin,message); 
        if(end_flag || !(*group_flag)) break;
        
        if(message == "/exit")
        {
            send_json = {
              {"request",GROUP_END},
              {"username",username},
              {"gid",group_id}  
            };
            sendjson(send_json,cfd);
            handle_pthread_wait(end_flag,cond,mutex);
            break;
        }
        else if(message == "/file" && !end_flag && *group_flag)
        {
            cout << endl;
            char file_show[MAX_REASONABLE_SIZE];
            strcpy(file_show,"请输入命令：");
            *group_show = "请输入命令：";

            cout << "进入文件传输模式：" << endl;
            cout << "提示：\n"
                << "- 输入 EXIT 退出文件传输模式。\n"
                << "- 输入 LIST + 路径名 将列出目录。\n"
                << "- 输入 STOR + 文件路径 可传输文件。\n" << endl;

            while (true && !end_flag && *group_flag){
                if(end_flag || !(*group_flag)) break;

                char *file_input = readline(file_show);
                if(strstr(file_input,"LIST") && !end_flag && *group_flag){
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
                    handle_pthread_wait(end_flag,cond,mutex);
                }
                else if(strstr(file_input,"STOR") && !end_flag && *group_flag){                    
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
                    handle_pthread_wait(end_flag,cond,mutex);
                } 
                else if(strcmp(file_input,"EXIT") == 0){
                    cout << "退出文件传输模式..." << endl;
                    cout << endl;
                    *group_show = "[ "+group_name+" ] "+username+": ";
                    break;
                }
                else{
                    if(end_flag || !*group_flag)
                        cout << "您无法在这个群聊中发送文件..." << endl;
                    else
                        cout << "命令错误，请重新输入命令..." << endl;
                    continue;
                }
                free(file_input);         
            }
            continue;
        }
        else if(message == "/admin" && !end_flag && *group_flag)
        {   
            char *admin;
            char show[MSGBUF];
            json send_json;

            if(group_role != "owner")
            {
                cout << "你没有权限执行此操作..." << endl; 
                continue;               
            } else
            {
                show_group_members(cfd,username,group_id);
                handle_pthread_wait(end_flag,cond,mutex);
                strcpy(show,"请输入成员名称：");
                admin = readline(show);
                send_json = {
                    {"request",ADD_ADMIN},
                    {"addname",admin},
                    {"group_id",group_id},
                    {"username",username}
                };
                sendjson(send_json,cfd);
                free(admin);
            }
            handle_pthread_wait(end_flag,cond,mutex);
            continue;
        }
        else if(message == "/add" && !end_flag && *group_flag)
        {
            char *admem;
            char show[MSGBUF];
            json send_json;

            show_user_friend(cfd,username);
            handle_pthread_wait(end_flag,cond,mutex);
            strcpy(show,"请输入需要邀请的用户：");
            admem = readline(show);
            send_json = {
                {"request",ADD_GROUP_MEM},
                {"addname",admem},
                {"gid",group_id},
                {"username",username}
            };
            sendjson(send_json,cfd);
            handle_pthread_wait(end_flag,cond,mutex);
            free(admem);
            continue;
        }
        else if(message == "/break" && !end_flag && *group_flag)
        {
            if(group_role != "owner")
            {
                cout << "你没有权限执行这个操作..." << endl;
                continue;
            } else
            {
                json send_json = {
                    {"request",BREAK_GROUP},
                    {"username",username},
                    {"gid",group_id}
                };
                sendjson(send_json,cfd);
            }
            handle_pthread_wait(end_flag,cond,mutex);
            break;
        }
        else if(message == "/kill" && !end_flag && *group_flag)
        {
            if(group_role == "member")
            {
                cout << "你没有权限执行这个操作..." << endl;
                continue;
            } else
            {
                char *kill;
                char show[MSGBUF];
                json send_json;

                show_group_members(cfd,username,group_id);
                handle_pthread_wait(end_flag,cond,mutex);
                strcpy(show,"请输入需要移除的用户：");
                kill = readline(show);
                send_json = {
                    {"request",KILL_GROUP_USER},
                    {"username",username},
                    {"kill_user",kill},
                    {"gid",group_id}
                };
                sendjson(send_json,cfd);
                free(kill);
            }
            handle_pthread_wait(end_flag,cond,mutex);
            continue;
        }
        else if(message == "/delete" && !end_flag && *group_flag)
        {
            if(group_role == "owner")
            {
                cout << "群主只能解散群聊..." << endl;
                continue;
            }   
            json send_json = {
                {"request",DELETE_GROUP},
                {"username",username},
                {"gid",group_id}
            };
            sendjson(send_json,cfd);
            handle_pthread_wait(end_flag,cond,mutex);
            break;
        }
        if(!end_flag && *group_flag)
        {
            send_json = {
                {"request",GROUP_CHAT},
                {"username",username},
                {"gid",group_id},
                {"message",message}
            };
            sendjson(send_json,cfd);
        }
    }
    
    return;
}

void show_group_members(int cfd,string username,long gid)
{
    json send_json = {
        {"request",SHOW_MEMBER},
        {"username",username},
        {"gid",gid}
    };
    sendjson(send_json,cfd);

    return;
}

void show_user_friend(int cfd,string username)
{
    json send_json = {
        {"request",SHOW_FRIEND},
        {"username",username}
    };
    sendjson(send_json,cfd);

    return;
}
