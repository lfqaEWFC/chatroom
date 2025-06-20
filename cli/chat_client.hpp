#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/cli/menu.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/Threadpool.hpp"
#include <sys/epoll.h>
#include "User.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

typedef struct recv_args{
    int cfd;
    int epfd;
    string *username;
    bool *end_flag;
    bool *end_start_flag;
    bool *end_chat_flag;
    pthread_cond_t *recv_cond;    
    pthread_mutex_t *recv_lock;
}recv_args;

class client: public menu{

    public:

        client(int in_cfd):
        end_start_flag(false),end_chat_flag(true),end_flag(false),handle_login_flag(true),
        chat_choice(0),start_choice(0),cfd(in_cfd){

            epfd = epoll_create(EPSIZE);
            ev.data.fd = cfd;
            ev.events = EPOLLET | EPOLLRDHUP | EPOLLIN;
            epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);

            args = new recv_args;
            args->cfd = cfd;
            args->epfd = epfd;
            args->end_flag = &end_flag;
            args->end_start_flag = &end_start_flag;          
            args->end_chat_flag = &end_chat_flag;
            args->username = &username;
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
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
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
                        }
                        else if(start_choice == BREAK){
                            system("clear");
                            json *json_break = new json;;
                            handle_break(json_break);
                            sendjson(*json_break,cfd);
                            delete json_break;
                            pthread_cond_wait(&recv_cond,&recv_lock);
                        }
                        else{
                            cout << "请输入正确的选项..." << endl;
                            continue;
                        }
                    }

                    system("clear");

                }

                while(!end_chat_flag){

                    if(handle_login_flag){
                        handle_success_login(cfd,username);
                        handle_offline_login(cfd,username);
                        handle_login_flag = false;
                    }

                    this->chat_show();
                    if (!(cin >> chat_choice)) {
                        cout << "请输入数字选项..." << endl;
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        continue;
                    }

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
                            pthread_cond_wait(&recv_cond,&recv_lock);
                            break;
                        }
                        case 2:{

                            break;
                        }
                        case 3:{
                            system("clear");
                            json *add_friend = new json;
                            handle_add_friend(add_friend,username);
                            sendjson(*add_friend,cfd);
                            delete(add_friend);
                            pthread_cond_wait(&recv_cond,&recv_lock);
                            break;
                        }

                    }

                    system("clear");

                }
            }

            return;

        }

    private:
    
        int cfd;
        int epfd;
        bool end_flag;
        bool end_start_flag;
        bool handle_login_flag;
        bool end_chat_flag;
        string username;
        recv_args *args;
        int start_choice;
        int chat_choice;
        struct epoll_event ev;
        pthread_t recv_pthread;
        pthread_cond_t recv_cond;
        pthread_mutex_t recv_lock;

        static void* recv_thread(void *args){

            recv_args *new_args = (recv_args*)args;
            struct epoll_event evlist[1];
            char *recvbuf = new char[MAXBUF];

            while(true){

                int n = epoll_wait(new_args->epfd,evlist,1,-1);
                for(int i=0;i < n;i++){
                    if(evlist[0].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)){
                        cout << "服务器已关闭，即将退出程序...." << endl;
                        *new_args->end_flag = true;
                        *new_args->end_chat_flag = true;
                        *new_args->end_start_flag = true;
                        pthread_cond_signal(new_args->recv_cond);
                        return nullptr;
                    }
                    else if(evlist[0].events & EPOLLIN){
                        int n;
                        while((n = recv(new_args->cfd,recvbuf,MAXBUF,MSG_DONTWAIT)) != 0){
                            if(n == -1){
                                if(errno != EAGAIN && errno != EWOULDBLOCK){
                                    perror("recv");
                                    return nullptr;
                                }else break;               
                            } 
                        }
                        json recvjson = nlohmann::json::parse(recvbuf);
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
                                    char *in_ans = new char[64];
                                    cout << "密保问题: " << recvjson["reflact"] << endl;
                                    cout << "请输入答案" << endl;
                                    cin >> in_ans;
                                    json send_json = {
                                        {"request",CHECK_ANS},
                                        {"username",recvjson["username"]},
                                        {"answer",in_ans}
                                    };
                                    sendjson(send_json,new_args->cfd);
                                    continue;
                                }
                                else{
                                    cout << recvjson["reflact"] << endl;
                                }
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
                            else{
                                
                            }
                            sleep(1);
                            pthread_cond_signal(new_args->recv_cond);
                            continue;
                        }
                        else if(recvjson["sort"] == MESSAGE){
                            if(recvjson["request"] == ASK_ADD_FRIEND){
                                cout << recvjson["message"] << endl;
                            }
                            else if(recvjson["request"] == GET_OFFLINE_MSG){
                                //处理element逻辑
                            }
                            continue;
                        }
                        else if(recvjson["sort"] == ERROR){
                            cout << "发生错误 : " << recvjson["reflact"] << endl;
                            *new_args->end_flag = true;
                            *new_args->end_start_flag = true;
                            *new_args->end_chat_flag = true;
                            sleep(1);
                            pthread_cond_signal(new_args->recv_cond);
                        }
                        else{
                            cout << "接受信息类型错误" << endl;
                            *new_args->end_start_flag = true;
                            return nullptr;
                        }
                    }
                }

                memset(recvbuf,0,MAXBUF);
            }
        }

};
