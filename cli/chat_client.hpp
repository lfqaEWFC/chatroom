#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/cli/menu.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/Threadpool.hpp"
#include <sys/epoll.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class client: public menu{

    public:

        client(int in_cfd):
        endflag(false),start_choice(0),cfd(in_cfd){
            spc_pool = new pool(48);
        }

        void start(){

            this->start_show();
            cin >> start_choice;

            while(!endflag){

                if(start_choice == LOGIN){
                    
                }
                else if(start_choice == LOGOUT){

                }
                else if(start_choice == SIGNIN){
                    json signin = {
                        {"request",SIGNIN},
                        {"message","this is signin"}
                    };
                    std::string json_str = signin.dump();
                    const char* data = json_str.c_str(); 
                    size_t data_len = json_str.size();
                    char *recvbuf = new char[MAXBUF]; 
                    send(cfd,data,data_len,0);
                    recv(cfd,recvbuf,MAXBUF,0);
                    cout << recvbuf << endl;
                }
                else if(start_choice == BREAK){

                }
                else{

                }
                
                endflag = true ;//在没有实现任何功能前防止死循环

            }
            
        }

    private:
    
        int cfd;
        bool endflag;
        pool *spc_pool;
        int start_choice;

};