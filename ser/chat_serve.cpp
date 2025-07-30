#include "include/inetsockets_tcp.hpp"
#include "ser/chat_serve.hpp"


int main(int argc,char **argv){

    char *portnum = new char[MSGBUF];

    if(argc == 1){
        sprintf(portnum,"%d",SERVE_PORT);
    }
    else if(argc == 2){
        strcpy(portnum,argv[1]);
    }
    else{
        cout << "error args" << endl;
        return 0;
    }

    serve start_serve((const char*)portnum);
    start_serve.start();

    return 0;
}
