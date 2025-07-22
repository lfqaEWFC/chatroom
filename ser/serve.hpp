#include <nlohmann/json.hpp>
#include <iostream>
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/Database/Database.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"

using json = nlohmann::json;

bool handle_signin(json json_quest, unique_ptr<database> &db);
bool handle_login(json json_quest, unique_ptr<database> &db, json *reflact,unordered_map<int, string> *cfd_to_user);
bool handle_forget_password(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_check_answer(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_logout(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<int, string> *cfd_to_user);
bool handle_break(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_add_friend(json json_quest, unordered_map<int, string> *cfd_to_user, unique_ptr<database> &db, json *reflact);
bool handle_get_offline(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_get_friend(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_deal_friend(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_chat_name(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<string, string> *user_to_friend);
bool handle_history_pri(json json_quest, unique_ptr<database> &db, json *reflact, unordered_map<string, string> user_to_friend);
bool handle_private_chat(json json_quest,unique_ptr<database> &db,json *reflact, 
                         unordered_map<string, string> *user_to_friend,unordered_map<int, string> cfd_to_user);
bool handle_del_peer(json json_quest,unique_ptr<database>&db,unordered_map<string, string> *user_to_friend);
bool handle_add_black(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_rem_black(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_check_friend(json json_quest,json *reflact,unique_ptr<database>&db,unordered_map<int, string>* cfd_to_user);
bool handle_del_friend(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_add_file(json json_quest,json *reflact,unique_ptr<database>&db,
                     unordered_map<int, string> cfd_to_user,unordered_map<string,string>user_to_friend);
bool handle_show_file(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_del_retr(json json_quest,json* reflact,unique_ptr<database>&db);
