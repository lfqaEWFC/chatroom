#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include <sys/epoll.h>

class serve{

    public:

        serve(const char *portnum):
        lefd(0),lfd(0),
        startflag(true){

            lfd = inetlisten(portnum);
            if(set_nonblocking(lfd) == -1){
                perror("set_nonblocking");
                startflag = false;
                return;
            }

            lefd = epoll_create(EPSIZE);
            if(lefd == -1){
                perror("epoll_create");
                startflag = false;
                return;
            }
            lev.data.fd = lfd;
            lev.events = EPOLLET;
            if(epoll_ctl(lefd,EPOLL_CTL_ADD,lfd,&lev) == -1){
                perror("epoll_ctl");
                startflag = false;
                return;
            }

            return;

        }

    void start(){
        
        while(startflag){

            int lep_cnt = epoll_wait(lefd,levlist,EPSIZE,-1);
            for(int i=0;i < lep_cnt;i++){
                accept(levlist[i].data.fd,nullptr,nullptr);
            }

        }

        return;
    }

    private:

        int lefd;
        int lfd;
        bool startflag;
        struct epoll_event lev;
        struct epoll_event levlist[EPSIZE];

};
