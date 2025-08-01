#include "chat_client.hpp"
#include "define.hpp"

using namespace std;

int main(int argc,char** argv){

    char *serve_ip = new char[MSGBUF];
    char *serve_port = new char[MSGBUF];
    int cfd = 0;
    int FTP_ctrl_cfd = 0;
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
    FTP_ctrl_cfd = inetconnect(serve_ip,FTP_PORTNUM);
    if(cfd == -1 || FTP_ctrl_cfd == -1){
        perror("inetconnect");
        return 0;
    }
    char in_num[10] = {0};
    string client_num;
    
    ssize_t n = recv(FTP_ctrl_cfd, in_num, sizeof(in_num) - 1, 0);
    if (n <= 0) {
        perror("recv clientnum");
        return 0;
    }
    
    in_num[n] = '\0';
    client_num = in_num;
    
    cout << "Received client_num: " << client_num << endl;
    
    client run_cli(cfd,FTP_ctrl_cfd,client_num);
    run_cli.start();

    return 0;
}
