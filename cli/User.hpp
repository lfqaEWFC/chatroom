#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <ctime>
#include <iomanip>
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"

using json = nlohmann::json;
using namespace std; 

int handle_signin(json *signin);
bool handle_login(json *login);
bool handle_break(json *json_break);
void handle_success_login(int cfd,string username);
void handle_add_friend(json *json_friend,string username);
void handle_offline_login(int cfd,string username);
void handle_get_friend_req(json *get_friend_req,string usernamevoid );
void handle_chat_name(json *get_friend_req,string usernamevoid);
void handle_history_pri(json *offline_pri,string username);
void handle_pri_chat(string username,string fri_user,int cfd,bool* end_flag);
