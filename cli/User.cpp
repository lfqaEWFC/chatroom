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

    delete[] username;
    delete[] password;
    delete[] checkpw;

    return 0;

}

bool handle_login(json *login){

    char *in_user = new char[64];
    string in_password;

    cout << "请输入用户名称 : " << endl;
    cin >> in_user;
    cout << "请输入用户密码 : " << endl;
    cout << "如果忘记密码,请输入Y以获取密保问题..." << endl;
    cin >> in_password;

    if(in_password == "Y"){
        *login = {
            {"request",FORGET_PASSWORD},
            {"username",in_user}
        };
        return true;
    }

    *login = {
        {"request",LOGIN},
        {"username",in_user},
        {"password",in_password}
    };

    cout << "end login" << endl;

    return true;
}

void handle_success_login(int cfd,string username){

    json send_json = {
        {"request",IN_ONLINE},
        {"username",username}
    };

    sendjson(send_json,cfd);

    return;
}

bool handle_break(json *json_break){

    string break_username;
    string break_password;

    cout << "请输入要注销的账号名称: " << endl;
    cin >> break_username;
    cout << "请输入帐号密码: " << endl;
    cin >> break_password;

    *json_break = {
        {"request",BREAK},
        {"username",break_username},
        {"password",break_password}
    };

    return true;
}

void handle_add_friend(json *json_friend,string username){

    string friend_user;

    cout << "请输入需要添加的好友名称: " << endl;
    cin >> friend_user;

    *json_friend = {
        {"request",ADD_FRIEND},
        {"username",username},
        {"fri_username",friend_user},
    };

    return;
}

void handle_offline_login(int cfd,string username){
 
    json send_json = {
        {"request",GET_OFFLINE_MSG},
        {"username",username}
    };

    sendjson(send_json,cfd);

    return;
}

void handle_get_friend_req(json *get_friend_req,string username){

    *get_friend_req = {
        {"request",GET_FRIEND_REQ},
        {"username",username},
    };

    return;
}

void handle_chat_name(json *chat_name,string username){
    
    string send_username = username;
    string fri_user;
    
    cout << "请输入好友名称" << endl;
    cin >> fri_user;

    *chat_name = {
        {"request",CHAT_NAME},
        {"username",username},
        {"fri_user",fri_user}
    };

    return;

}
