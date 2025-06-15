#include <nlohmann/json.hpp>
#include <iostream>
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/Database/Database.hpp"

using json = nlohmann::json;

bool handle_signin(json json_quest,unique_ptr<database> &db);
bool handle_login(json json_quest,unique_ptr<database> &db,json *reflact);
bool handle_forget_password(json json_quest,unique_ptr<database> &db,json *reflact);
bool handle_check_answer(json json_quest,unique_ptr<database> &db,json *reflact);
bool handle_in_online(json json_quest,unique_ptr<database> &db);
