#include "serve.hpp"

bool handle_signin(json json_quest, database *db) {

    MYSQL_RES* res = db->query_sql("SHOW TABLES LIKE 'user'");
    
    if (res && mysql_num_rows(res) <= 0) {
        bool creatchk = db->execute_sql(
            "CREATE TABLE user("
            "uid INT PRIMARY KEY AUTO_INCREMENT,"
            "username VARCHAR(64) UNIQUE NOT NULL,"
            "password VARCHAR(64) NOT NULL,"
            "que VARCHAR(1024) NOT NULL,"
            "ans VARCHAR(1024) NOT NULL)"
        );
        if (!creatchk) {
            std::cout << "CREATE TABLE user ERROR" << std::endl;
            return false;
        }        
    }

    string username = json_quest["username"];
    string password = json_quest["password"];
    string question = json_quest["que"];
    string answer   = json_quest["ans"];

    string sql = "INSERT INTO user(username, password, que, ans) VALUES('" +username + "','" + password + "','" + question + "','" + answer + "')";

    bool addchk = db->execute_sql(sql);
    if (!addchk) {
        cout << "INSERT failed" << endl;
        return false;
    }

    return true;
}
