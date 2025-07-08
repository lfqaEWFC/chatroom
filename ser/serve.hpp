#include <nlohmann/json.hpp>
#include <iostream>
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/Database/Database.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"

using json = nlohmann::json;

bool handle_signin(json json_quest, unique_ptr<database> &db);
bool handle_login(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_forget_password(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_check_answer(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_in_online(json json_quest, unique_ptr<database> &db);
bool handle_logout(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<int, string> *cfd_to_user);
bool handle_break(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_add_friend(json json_quest, unordered_map<int, string> *cfd_to_user, unique_ptr<database> &db, json *reflact);
bool handle_get_offline(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_get_friend(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_deal_friend(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_chat_name(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<string, string> *user_to_friend);
bool handle_history_pri(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<string, string> user_to_friend);
