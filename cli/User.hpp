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
void handle_get_friend_req(json *get_friend_req,string usernamevoid );
void handle_chat_name(json *get_friend_req,string usernamevoid);
void handle_history_pri(json *offline_pri,string username);
void handle_pri_chat(string username,string fri_user,int cfd,int FTP_ctrl_cfd,bool* end_flag,
                     bool* FTP_stor_flag,bool* pri_flag,string client_num,
                     pthread_cond_t *cond,pthread_mutex_t *mutex,string* file_path);
void handle_black(string username,int cfd,bool *rl_display_flag);
void handle_check_friend(string username,int cfd);
void handle_delete_friend(string username,int cfd);
void handle_pthread_wait(bool endflag,pthread_cond_t *cond,pthread_mutex_t *mutex);
int handle_pasv(string reflact);
