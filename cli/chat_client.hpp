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
    bool *endflag;
    pthread_cond_t *recv_cond;    
    pthread_mutex_t *recv_lock;
}recv_args;

class client: public menu{

    public:

        client(int in_cfd):
        endflag(false),start_choice(0),cfd(in_cfd){

            epfd = epoll_create(EPSIZE);
            ev.data.fd = cfd;
            ev.events = EPOLLET | EPOLLRDHUP | EPOLLIN;
            epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);

            args = new recv_args;
            args->cfd = cfd;
            args->endflag = &endflag;
            args->epfd = epfd;
            pthread_cond_init(&recv_cond,nullptr);
            pthread_mutex_init(&recv_lock,nullptr);
            args->recv_cond = &recv_cond;
            args->recv_lock = &recv_lock;
            pthread_create(&recv_pthread,nullptr,recv_thread,args);
        }

        void start(){

            while(!endflag){

                this->start_show();
                cin >> start_choice;

                if(start_choice == LOGIN){
                    
                }
                else if(start_choice == LOGOUT){
                    
                }
                else if(start_choice == SIGNIN){
                    system("clear");
                    json *signin;
                    handle_signin(signin);
                    string json_str = signin->dump();
                    const char* data = json_str.c_str(); 
                    size_t data_len = json_str.size();
                    char *recvbuf = new char[MAXBUF]; 
                    send(cfd,data,data_len,0);
                    pthread_cond_wait(&recv_cond,&recv_lock);
                }
                else if(start_choice == BREAK){
                    
                }
                else{
                    cout << "请输入正确的选项..." << endl;
                    continue;
                }

                system("clear");

            }
            
        }

    private:
    
        int cfd;
        int epfd;
        bool endflag;
        recv_args *args;
        int start_choice;
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
                        pthread_cond_signal(new_args->recv_cond);
                        *new_args->endflag = true;
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
                            cout << recvjson["reflact"] << endl;
                            sleep(1);
                            pthread_cond_signal(new_args->recv_cond);
                            continue;
                        }
                        else if(recvjson["sort"] == MESSAGE){
                            continue;
                        }
                        else if(recvjson["sort"] == ERROR){
                            cout << "发生错误 : " << recvjson["reflact"] << endl;
                            *new_args->endflag = true;
                            sleep(1);
                            pthread_cond_signal(new_args->recv_cond);
                        }
                        else{
                            cout << "接受信息类型错误" << endl;
                            *new_args->endflag = true;
                            return nullptr;
                        }
                    }
                }

                memset(recvbuf,0,MAXBUF);
            }
        }

};
