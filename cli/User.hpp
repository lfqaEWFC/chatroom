#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"

using json = nlohmann::json;
using namespace std; 

int handle_signin(json *signin);
bool handle_login(json *login);
bool handle_break(json *json_break);
void handle_success_login(int cfd,string username);
