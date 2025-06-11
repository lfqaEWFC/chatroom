#include <nlohmann/json.hpp>
#include <iostream>
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/Database/Database.hpp"

using json = nlohmann::json;

bool handle_signin(json json_quest,database *db);
