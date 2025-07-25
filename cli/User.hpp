#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <ctime>
#include <iomanip>
#include <readline/readline.h>
#include <readline/history.h>
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
void handle_get_friend_req(json *get_friend_req,string username );
void handle_chat_name(json *get_friend_req,string usernamevoid);
void handle_history_pri(json *offline_pri,string username);
void handle_pri_chat(string username,string fri_user,int cfd,int FTP_ctrl_cfd,bool* end_flag,
                     bool* FTP_stor_flag,bool* pri_flag,string client_num,
                     pthread_cond_t *cond,pthread_mutex_t *mutex,string* file_path,
                     string *fri_username,string *pri_show);
void handle_black(string username,int cfd);
void handle_check_friend(string username,int cfd);
void handle_delete_friend(string username,int cfd);
void handle_pthread_wait(bool endflag,pthread_cond_t *cond,pthread_mutex_t *mutex);
int handle_pasv(string reflact);
void handle_show_file(string username, int cfd,string* fri_username);
void handle_retr_file(int FTP_ctrl_cfd,bool endflag,pthread_mutex_t* mutex,string* fri_username,
                      pthread_cond_t* cond,bool* FTP_retr_flag,string* file_name);
void handle_create_group(string username,int cfd);
void handle_add_group(string username,int cfd,bool end_flag,bool* id_flag,
                      pthread_cond_t* cond,pthread_mutex_t* mutex);
void deal_add_group(int cfd,string username,bool endflag,bool* group_add_flag,
                    pthread_cond_t* cond,pthread_mutex_t* mutex);
void handle_show_group(int cfd,string username);
void handle_group_name(int cfd,string username);
void handle_history_group(int cfd,string username,long group_id);
void handle_group_chat(int cfd,string username,string group_role,long group_id,
                       string group_name,bool end_flag);
