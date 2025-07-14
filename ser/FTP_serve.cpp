#include"FTP_serve.hpp"
#include"/home/mcy-mcy/文档/chatroom/define/define.hpp"

int main(){
    FTP f1(HANDLE_RECV_NUM,HANDLE_RECV_NUM);
    f1.init();
    f1.start();
    return 0;
}
