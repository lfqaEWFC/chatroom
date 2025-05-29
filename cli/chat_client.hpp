#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/cli/menu.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"

class client: public menu{

    public:

        void start(){

            this->start_show();
            cin >> start_choice;

            while(!endflag){
                if(start_choice == LOGIN){

                }
                else if(start_choice == LOGOUT){

                }
                else if(start_choice == SIGNIN){
                    
                }
                else if(start_choice == BREAK){

                }
                else{

                }
                
                endflag = true ;//在没有实现任何功能前防止死循环

            }
            
        }

    private:
    
        bool endflag = false;
        int start_choice;

};