#include <iostream>
#include <mysql/mysql.h>
#include <nlohmann/json.hpp>
using namespace std;

using json = nlohmann::json;

int main() {

    // 创建 JSON 数据
    json data = {
        {"Id_P", 2},
        {"LastName","Tom"},
        {"FirstName", "Jason"},
        {"Address","xupt"},
        {"City","XiAn"}
    };
    //序列化
    std::string json_str = data.dump();
    //反序列化
    nlohmann::json json = nlohmann::json::parse(json_str);
    ssize_t Id_P = json["Id_P"];
    cout << Id_P << endl;

    /*
    JSON的每个大括号中都是一个键值对集合（对象 Object）
    - 键（Key）必须是字符串类型
    - 值（Value）可以是任意 JSON 支持的类型：
      - 基本类型：数字、字符串、布尔值、null
      - 复合类型：对象（大括号 {}）、数组（方括号 []）
    - 访问方式：
      - 对象用键名：json["key"]
      - 数组用索引：json[0]
    - 类型转换：
      - 自动转换：json["key"].get<int>()
      - 安全访问：json.contains("key") ? json["key"] : 默认值
    */

    // 初始化 MySQL 连接
    MYSQL *conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "MySQL 初始化失败" << std::endl;
        return 1;
    }

    // 连接到 MySQL
    if (!mysql_real_connect(conn, "localhost", "root", "password","mydatabase", 0, nullptr, 0)) {
        std::cerr << "连接失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return 1;
    }
    
    //创建person表
    string crt = "create table person(Id_P int,lastname varchar(255),firstname varchar(255),address varchar(255),city varchar(255));";
    if (mysql_query(conn, crt.c_str())) {
        std::cerr << "创建失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return 1;
    }

    // 执行 SQL 插入
    std::string sql = "INSERT INTO person (Id_P, LastName, FirstName, Address, City) VALUES (2,'Tom','Jason','xupt','XiAn')";
    if (mysql_query(conn, sql.c_str())) {
        std::cerr << "插入失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return 1;
    }

    std::cout << "插入成功，影响行数: " << mysql_affected_rows(conn) << std::endl;

    // 清理资源
    mysql_close(conn);
    return 0;
}
