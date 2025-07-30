#include <nlohmann/json.hpp>
#include <iostream>
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/Database/Database.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include <ctime>
#include <string>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

string get_current_mysql_timestamp();
bool handle_signin(json json_quest, unique_ptr<database> &db);
bool handle_login(json json_quest, unique_ptr<database> &db, json *reflact,
                  unordered_map<int, string> *cfd_to_user);
bool handle_forget_password(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_check_answer(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_logout(json json_quest, unique_ptr<database> &db, json *reflact,
                   unordered_map<int, string> *cfd_to_user, unordered_map<string, int> *user_to_cfd);
bool handle_break(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_add_friend(json json_quest, unordered_map<int, string> *cfd_to_user, 
                      unique_ptr<database> &db, json *reflact);
bool handle_get_offline(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_get_friend(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_deal_friend(json json_quest, unique_ptr<database> &db, json *reflact);
bool handle_chat_name(json json_quest, unique_ptr<database> &db, json *reflact, 
                      unordered_map<string, string> *user_to_friend);
bool handle_history_pri(json json_quest, unique_ptr<database> &db, json *reflact,
                        unordered_map<string, string> user_to_friend);
bool handle_private_chat(json json_quest,unique_ptr<database> &db,json *reflact, 
                         unordered_map<string, string> *user_to_friend,unordered_map<int, string> cfd_to_user);
bool handle_del_peer(json json_quest,unique_ptr<database>&db,unordered_map<string, string> *user_to_friend);
bool handle_add_black(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_rem_black(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_check_friend(json json_quest,json *reflact,unique_ptr<database>&db,
                         unordered_map<int, string>* cfd_to_user);
bool handle_del_friend(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_add_file(json json_quest,json *reflact,unique_ptr<database>&db,
                     unordered_map<int, string> cfd_to_user,unordered_map<string,string>user_to_friend,
                     unordered_map<string, int> user_to_cfd,unordered_map<string,int> user_to_group);
bool handle_show_file(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_create_group(json json_quest,json* reflact,unique_ptr<database>&db);
bool handle_select_group(json json_quest,json* reflact,unique_ptr<database>&db);
bool handle_add_group(json json_quest,json* reflact,unique_ptr<database>&db,unordered_map<string,int> user_to_cfd);
bool deal_add_group(json json_quest,json* reflact,unique_ptr<database>&db);
bool handle_commit_add(json json_quest,json* reflact,unique_ptr<database>&db);
bool handle_show_group(json json_quest,json* reflact,unique_ptr<database>&db);
bool handle_group_name(json json_quest,json* reflact,unique_ptr<database>&db,
                       unordered_map<string, int> *user_to_group);
bool handle_group_history(json json_quest,json* reflact,unique_ptr<database>&db);
bool handle_group_chat(json json_quest,json* reflact,unique_ptr<database>&db,
                       unordered_map<string, int> user_to_cfd,unordered_map<string,int> user_to_group);
bool handle_group_end(json json_quest,json* reflact,unique_ptr<database>&db,
                      unordered_map<string,int>* user_to_group);
bool handle_show_member(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_add_admin(json json_quest,json *reflact,unique_ptr<database>&db,unordered_map<string,int> user_to_cfd);
bool handle_show_friend(json json_quest,json *reflact,unique_ptr<database>&db);
bool handle_add_member(json json_quest,json *reflact,unique_ptr<database>&db,unordered_map<string,int> user_to_cfd);
bool handle_break_group(json json_quest,json *reflact,unique_ptr<database>&db,
                        unordered_map<string,int> user_to_cfd,unordered_map<string,int> user_to_group);
bool handle_kill_user(json json_quest,json *reflact,unique_ptr<database>&db,
                      unordered_map<string,int> user_to_cfd,unordered_map<string,int> user_to_group);
bool handle_del_group(json json_quest,json *reflact,unique_ptr<database>&db,unordered_map<string,int> user_to_cfd);
