#include "chat_client.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"

using namespace std;

int main(int argc,char** argv){

    char *serve_ip = new char[MSGBUF];
    char *serve_port = new char[MSGBUF];
    int cfd = 0;
    char test[100];
    
    sprintf(serve_port,"%d",SERVE_PORT);
    if(argc == 1){
        get_local_ip(serve_ip);
    }
    if(argc >= 2){
        strcpy(serve_ip,argv[1]);
    }
    if(argc >= 3){
        sprintf(serve_port,"%s",argv[2]);
    }

    cout << serve_ip << "," << serve_port << endl;
    cfd  = inetconnect(serve_ip,(const char*)serve_port);
    if(cfd == -1){
        perror("inetconnect");
        return 0;
    }
    sleep(1);
    send(cfd,"hello",sizeof("hello"),0);
    perror("send");
    recv(cfd,test,sizeof(test),0);
    cout << test << endl;
    client run_cli;
    run_cli.start();

    return 0;
}
