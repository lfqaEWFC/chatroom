#include "User.hpp"

int handle_signin(json* signin){

    bool checkflag = true;
    char *username = new char[64];
    char *password = new char[64];
    char *checkpw = new char[64];
    char question[1024];
    char answer[1024];
    
    cout << "请输入注册账户名称" << endl;
    cin >> username;
    while(true){
        cout << "请输入注册账户密码" << endl;
        cin >> password;
        if(strlen(password) < 8){
            cout << "密码长度过短，请重新输入" << endl;
            memset(password,0,64);
            continue;
        }
        system("clear");
        break;
    }
    while(checkflag){
        cout << "请确认密码" << endl;
        cin >> checkpw;
        if(strcmp(checkpw,password) == 0){
            cout << "密码设置成功" << endl;
            checkflag = false;
            continue;
        }
        else{
            cout << "确认密码与帐号密码不相同，请重新确认" << endl;
            continue;
        }
    }

    cout << "请输入密保问题" << endl;
    cin >> question;
    cout << "请输入答案" << endl;
    cin  >> answer;
    
    *signin = {
        {"request",SIGNIN},
        {"username",username},
        {"password",password},
        {"que",question},
        {"ans",answer},
    };

    return 0;

}
