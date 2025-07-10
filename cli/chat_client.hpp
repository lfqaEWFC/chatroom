#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/cli/menu.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/Threadpool.hpp"
#include <sys/epoll.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "User.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

typedef struct recv_args{
    int cfd;
    int epfd;
    string *username;
    string *fog_username;
    bool *end_flag;
    bool *end_start_flag;
    bool *fog_que_flag;
    bool *pri_chat_flag;
    bool *chat_name_flag;
    bool *add_friend_req_flag;
    bool *end_chat_flag;
    vector<string> *add_friend_fri_user;
    vector<string> *add_friend_requests;
    pthread_cond_t *recv_cond;    
    pthread_mutex_t *recv_lock;
}recv_args;

class client: public menu{

    public:

        client(int in_cfd):
        end_start_flag(false),end_chat_flag(true),end_flag(false),handle_login_flag(true),
        fog_que_flag(false),add_friend_req_flag(false),chat_name_flag(false),pri_chat_flag(false),
        chat_choice(0),start_choice(0),cfd(in_cfd){

            epfd = epoll_create(EPSIZE);
            ev.data.fd = cfd;
            ev.events = EPOLLET | EPOLLRDHUP | EPOLLIN;
            epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);

            args = new recv_args;
            args->cfd = cfd;
            args->epfd = epfd;
            args->end_flag = &end_flag;
            args->username = &username;
            args->fog_username = &fog_username;
            args->fog_que_flag = &fog_que_flag;
            args->pri_chat_flag = &pri_chat_flag;
            args->chat_name_flag = &chat_name_flag;         
            args->end_chat_flag = &end_chat_flag;
            args->end_start_flag = &end_start_flag; 
            args->add_friend_req_flag = &add_friend_req_flag;
            args->add_friend_requests = &add_friend_requests;
            args->add_friend_fri_user = &add_friend_fri_user;
            pthread_cond_init(&recv_cond,nullptr);
            pthread_mutex_init(&recv_lock,nullptr);
            args->recv_cond = &recv_cond;
            args->recv_lock = &recv_lock;
            pthread_create(&recv_pthread,nullptr,recv_thread,args);
        }

        void start(){

            while(!end_flag){

                while(!end_start_flag){
                    
                    this->start_show();
               
                    if (!(cin >> start_choice)) {
                        cout << "请输入数字选项..." << endl;
                        cin.clear();
                        wait_user_continue();
                        system("clear");
                        continue;
                    }

                    if(!end_start_flag){
                        if(start_choice == LOGIN){
                            system("clear");
                            json *login = new json;
                            handle_login(login);
                            sendjson(*login,cfd);
                            delete login;
                            pthread_cond_wait(&recv_cond,&recv_lock);
                            if(fog_que_flag && !end_flag){
                                cout << "请输入答案" << endl;
                                char *in_ans = new char[64];
                                cin >> in_ans;
                                json send_json = {
                                    {"request",CHECK_ANS},
                                    {"username",fog_username},
                                    {"answer",in_ans}
                                };
                                sendjson(send_json,cfd);
                                pthread_cond_wait(&recv_cond,&recv_lock);
                                wait_user_continue();
                            }else wait_user_continue();
                        }
                        else if(start_choice == EXIT){
                            system("clear");
                            cout << "感谢使用聊天室，再见!" << endl;
                            end_start_flag = true;
                            end_flag = true;
                            sleep(1);
                        }
                        else if(start_choice == SIGNIN){
                            system("clear");
                            json *signin = new json;
                            handle_signin(signin);
                            sendjson(*signin,cfd);
                            delete signin;
                            pthread_cond_wait(&recv_cond,&recv_lock);
                            wait_user_continue();
                        }
                        else if(start_choice == BREAK){
                            system("clear");
                            json *json_break = new json;;
                            handle_break(json_break);
                            sendjson(*json_break,cfd);
                            delete json_break;
                            pthread_cond_wait(&recv_cond,&recv_lock);
                            wait_user_continue();
                        }
                        else{
                            cout << "请输入正确的选项..." << endl;
                            wait_user_continue();
                            system("clear");
                            continue;
                        }
                    }

                    system("clear");

                }

                while(!end_chat_flag){      
                    
                    if(handle_login_flag && !end_chat_flag){
                        handle_offline_login(cfd,username);
                        handle_success_login(cfd,username);
                        handle_login_flag = false;
                    }

                    this->chat_show();
                    if (!(cin >> chat_choice)) {
                        cout << "请输入数字选项..." << endl;
                        cin.clear();
                        wait_user_continue();
                        system("clear");
                        continue;
                    }

                    if(!end_chat_flag){
                        switch(chat_choice){
                            case 1:{
                                system("clear");
                                json logout = {
                                    {"request",LOGOUT},
                                    {"username",username}
                                }; 
                                sendjson(logout,cfd);   
                                end_chat_flag = true;
                                end_start_flag = false;
                                handle_login_flag = true;
                                pthread_cond_wait(&recv_cond,&recv_lock);
                                wait_user_continue();
                                break;
                            }
                            case 2:{
                                system("clear");
                                json *chat_name = new json;
                                handle_chat_name(chat_name,username);
                                string fri_user = (*chat_name)["fri_user"];
                                sendjson(*chat_name,cfd);
                                delete chat_name;
                                pthread_cond_wait(&recv_cond,&recv_lock);
                                if(chat_name_flag && !end_flag){
                                    json offline_pri;
                                    handle_history_pri(&offline_pri,username);    
                                    sendjson(offline_pri,cfd);
                                    pthread_cond_wait(&recv_cond,&recv_lock);
                                    if(pri_chat_flag && !end_flag)
                                        handle_pri_chat(username,fri_user,cfd,&end_flag,&pri_chat_flag);
                                    cout << "=============================================" << endl;                                 
                                    wait_user_continue();
                                    chat_name_flag = false;                                                                  
                                }else wait_user_continue();
                                break;
                            }
                            case 3:{
                                system("clear");
                                json *add_friend = new json;
                                handle_add_friend(add_friend,username);
                                sendjson(*add_friend,cfd);                              
                                pthread_cond_wait(&recv_cond,&recv_lock);
                                wait_user_continue();
                                break;
                            }
                            case 4:{
                                system("clear");
                                json *get_fri_req = new json;
                                handle_get_friend_req(get_fri_req,username);
                                sendjson(*get_fri_req,cfd);
                                delete(get_fri_req);
                                pthread_cond_wait(&recv_cond,&recv_lock);
                                if(add_friend_req_flag && !end_flag){
                                    bool add_flag = true;
                                    vector<string> commit;
                                    vector<string> refuse;
                                    while(add_flag){
                                        int num;
                                        char chk;
                                        cout << "请输入需要处理的好友申请的选项(输入-1退出交互): " << endl;
                                        cin >> num;
                                        if (num == 0) {
                                            add_flag = false;
                                            break;
                                        }
                                        if ((num < 1 || num > add_friend_requests.size()) && num != -1) {
                                            cout << "编号超出范围，请重新输入。" << endl;
                                            continue;
                                        }
                                        if(num == -1){
                                            add_flag = false;
                                            break;
                                        }                                    
                                        cout << "请输入是否同意(y/n)" << endl;
                                        cin >> chk;
                                        if(chk == 'y'){
                                            commit.push_back(add_friend_fri_user[num-1]);
                                        }else if(chk == 'n'){
                                            refuse.push_back(add_friend_fri_user[num-1]);
                                        }else{
                                            cout << "请勿输入无关选项..." << endl;
                                        }
                                    }
                                    if(commit.size() == 0 && refuse.size() == 0){
                                        cout << "未处理好友关系..." << endl;
                                        sleep(1);
                                        system("clear");
                                        continue;
                                    }
                                    json send_json = {
                                        {"request",DEAL_FRI_REQ},
                                        {"commit",commit},
                                        {"refuse",refuse},
                                        {"username",username}
                                    };
                                    sendjson(send_json,cfd);  
                                    pthread_cond_wait(&recv_cond,&recv_lock);
                                    wait_user_continue();
                                }else wait_user_continue();
                                break;
                            }
                            case 6:{
                                system("clear");
                                handle_black(username,cfd);
                                pthread_cond_wait(&recv_cond,&recv_lock);
                                wait_user_continue();
                                break;
                            }
                            case 7:{

                            }
                            case 8:{

                            }
                            default:{
                                cout << "请输入正确的选项..." << endl;
                                wait_user_continue();
                                system("clear");
                                continue;
                            }
                        }

                        system("clear");

                    }
                }
            }

            return;

        }

    private:
    
        int cfd;
        int epfd;
        bool end_flag;
        bool fog_que_flag;
        bool end_chat_flag;
        bool chat_name_flag;
        bool end_start_flag;
        bool pri_chat_flag;
        bool handle_login_flag;  
        bool add_friend_req_flag;
        string username;
        string fog_username;
        recv_args *args;
        int start_choice;
        int chat_choice;
        struct epoll_event ev;
        vector<string> add_friend_fri_user;
        vector<string> add_friend_requests;
        pthread_t recv_pthread;
        pthread_cond_t recv_cond;
        pthread_mutex_t recv_lock;

        static void* recv_thread(void *args){

            string buffer;
            char recvbuf[MAXBUF] = {0};
            recv_args *new_args = (recv_args*)args;
            struct epoll_event evlist[1];
                       
            while(true && !(*new_args->end_flag)){
                
                int n = epoll_wait(new_args->epfd,evlist,1,-1);
                for(int i=0;i < n;i++){
                    if(evlist[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)){
                        cout << "服务器已关闭，即将退出程序...." << endl;
                        *new_args->end_flag = true;
                        *new_args->end_chat_flag = true;
                        *new_args->end_start_flag = true;
                        pthread_cond_signal(new_args->recv_cond);
                        return nullptr;
                    }
                    else if(evlist[i].events & EPOLLIN){
                        ssize_t n;
                        while((n = recv(new_args->cfd,recvbuf,MAXBUF,MSG_DONTWAIT)) > 0){
                            buffer.append(recvbuf, n);
                        }
                        if(n == -1){
                            if(errno != EAGAIN && errno != EWOULDBLOCK){
                                perror("recv");
                                return nullptr;
                            }              
                        }  

                        while (buffer.size() >= 4){

                            uint32_t net_len;                           
                            memcpy(&net_len, buffer.data(), 4);
                            uint32_t msg_len = ntohl(net_len);
                            if(msg_len == 0 || msg_len > MAX_REASONABLE_SIZE) {
                                cerr << "异常消息长度: " << msg_len << endl;
                                buffer.clear();
                                break;
                            }
                            if (buffer.size() < 4 + msg_len) break;
                            string json_str = buffer.substr(4, msg_len);
                            buffer.erase(0, 4 + msg_len);
                            
                            json recvjson;
                            try{
                                recvjson = json::parse(json_str);
                            }catch(...){
                                cerr << "JSON parse error\n";
                                continue;
                            }
                            if(recvjson["sort"] == REFLACT){
                                if(recvjson["request"] == SIGNIN){
                                    cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == LOGIN){
                                    if(recvjson["login_flag"]){
                                        *new_args->end_start_flag = true;
                                        *new_args->end_chat_flag = false;
                                        *new_args->username = recvjson["username"];
                                    }
                                    cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == FORGET_PASSWORD){
                                    if(recvjson["que_flag"]){
                                        *new_args->fog_que_flag = true;
                                        *new_args->fog_username = recvjson["username"];
                                        cout << "密保问题: " << recvjson["reflact"] << endl;
                                    }
                                    else
                                        cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == CHECK_ANS){
                                    if(recvjson["ans_flag"]){
                                        *new_args->end_start_flag = true;
                                        *new_args->end_chat_flag = false;
                                        *new_args->username = recvjson["username"];
                                    }
                                    cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == LOGOUT){
                                    cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == BREAK){
                                    cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == ADD_FRIEND){
                                    cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == GET_FRIEND_REQ){
                                    if(recvjson["do_flag"] == false){
                                        cout << recvjson["reflact"] << endl;
                                    }
                                    else{
                                        *new_args->add_friend_req_flag = true;
                                        *new_args->add_friend_fri_user = recvjson["fri_user"];
                                        *new_args->add_friend_requests = recvjson["reflact"];
                                        cout << "你有 " << (*new_args->add_friend_requests).size() 
                                            << " 条好友申请：" << endl;
                                        for (size_t i = 0; i < (*new_args->add_friend_requests).size(); ++i) {
                                            cout << i + 1 << ". " << (*new_args->add_friend_requests)[i] << endl;
                                        }
                                    }
                                }
                                else if(recvjson["request"] == DEAL_FRI_REQ){
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                                else if(recvjson["request"] == CHAT_NAME){
                                    string reflact = recvjson["reflact"];
                                    if(recvjson["chat_flag"] == false)
                                        cout << reflact << endl;
                                    else{                             
                                        cout << reflact << endl;
                                        cout << "=============================================" << endl;
                                        *new_args->chat_name_flag = true;
                                    }
                                }
                                else if(recvjson["request"] == GET_HISTORY_PRI){
                                    if(recvjson["ht_flag"] == false){
                                        string reflact = recvjson["reflact"];
                                        cout << reflact << '\n' << endl;
                                    }else{
                                        json history = recvjson["reflact"];
                                        for (auto& msg : history) {
                                            string sender = msg["sender"];
                                            string content = msg["content"];
                                            string timestamp = msg["timestamp"];
                                            cout << "[" << timestamp << "] " << sender << ": " << content << endl;
                                        }
                                        cout << '\n';
                                    }
                                    *new_args->pri_chat_flag = true;
                                }
                                else if(recvjson["request"] == ADD_BLACKLIST){
                                    cout << recvjson["reflact"] << endl;
                                }
                                else if(recvjson["request"] == REMOVE_BLACKLIST){
                                    cout << recvjson["reflact"] << endl;
                                }
                                else{
                                    
                                }

                                pthread_cond_signal(new_args->recv_cond);
                                
                                continue;
                            }
                            else if(recvjson["sort"] == MESSAGE){
                                if(recvjson["request"] == ASK_ADD_FRIEND){
                                    cout << "\r\033[K" << flush;
                                    cout << recvjson["message"] << endl;
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else if(recvjson["request"] == ADD_BLACKLIST){
                                    *new_args->pri_chat_flag = false;
                                    cout << "\r\033[K" << flush;
                                    cout << recvjson["reflact"] << endl;
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else if(recvjson["request"] == GET_OFFLINE_MSG){
                                    json elements = json::array();
                                    elements = recvjson["elements"];
                                    int count = elements.size();
                                    if(count > 0){ 
                                        cout << "以下是新消息: " << endl;
                                        for(int i=0;i<count;i++){
                                            cout << elements[i] << endl;
                                        }
                                    }
                                }
                                else if(recvjson["request"] == PEER_CHAT){
                                    string sender = recvjson["sender"];
                                    string receiver = recvjson["receiver"];
                                    string message = recvjson["message"];
                                    cout << "\r\033[K" << flush;
                                    cout << "[" << sender << "->" << receiver << "]: ";
                                    cout << message << endl;
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else if(recvjson["request"] == NON_PEER_CHAT){
                                    string message = recvjson["message"];
                                    cout << "\r\033[K" << flush;
                                    cout << message << endl;                                  
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                continue;
                            }
                            else if(recvjson["sort"] == ERROR){
                                cout << "\r\033[K" << flush;
                                cout << "发生错误 : " << recvjson["reflact"] << endl;
                                rl_on_new_line();
                                rl_redisplay();
                                *new_args->end_flag = true;
                                *new_args->end_start_flag = true;
                                *new_args->end_chat_flag = true;
                                pthread_cond_signal(new_args->recv_cond);
                                return nullptr;
                            }
                            else{
                                cout << "\r\033[K" << flush;
                                cout << "接受信息类型错误: " << recvjson["sort"] << endl;
                                rl_on_new_line();
                                rl_redisplay();
                                *new_args->end_flag = true;
                                *new_args->end_start_flag = true;
                                *new_args->end_chat_flag = true;
                                pthread_cond_signal(new_args->recv_cond);
                                return nullptr;
                            }
                        }
                    }
                }

                memset(recvbuf,0,MAXBUF);
            }
            
            return nullptr;
        }

};
