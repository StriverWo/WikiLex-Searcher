#ifndef MYSQL_UTIL_HPP
#define MYSQL_UTIL_HPP
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <memory>
#include <string>
#include <iostream>

class MysqlUtil {
public:
    MysqlUtil(const std::string& host, const std::string& user, const std::string& password, const std::string& db);
    ~MysqlUtil();
    bool connect();
    // 注册用户信息至users表中
    bool registerUser(const std::string& username, const std::string& password_hash);
    // 根据用户名查询用户记录，对比密码是否一致
    bool loginUser(const std::string& username, const std::string& password_hash);
    
    
    bool logQuery(int user_id, const std::string& query);
    std::vector<std::string> getUserHistory(int user_id, int limit);
    // ...其他接口

private:
    std::string host;
    std::string user;
    std::string password;
    std::string db;
    sql::Driver* driver;
    std::unique_ptr<sql::Connection> conn;
};


MysqlUtil::MysqlUtil(const std::string& host, const std::string& user,
                     const std::string& password, const std::string& db)
    : host(host), user(user), password(password), db(db), driver(nullptr) {}

MysqlUtil::~MysqlUtil() {
    // unique_ptr 自动释放连接资源
}

bool MysqlUtil::connect() {
    try {
        driver = get_driver_instance();
        conn.reset(driver->connect(host, user, password));
        conn->setSchema(db);
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "MySQL连接错误: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlUtil::registerUser(const std::string& username, const std::string& password_hash) {
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("INSERT INTO users(username, password_hash) VALUES (?, ?)"));
        pstmt->setString(1, username);
        pstmt->setString(2, password_hash);
        int affectedRows = pstmt->executeUpdate();
        return affectedRows > 0;
    } catch (sql::SQLException &e) {
        std::cerr << "注册用户失败: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlUtil::loginUser(const std::string& username, const std::string& password_hash) {
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("SELECT password_hash FROM users WHERE username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string storedHash = res->getString("password_hash");
            return storedHash == password_hash; // 这里建议使用安全的哈希比较方法
        } else {
            return false;
        }
    } catch (sql::SQLException &e) {
        std::cerr << "登录查询失败: " << e.what() << std::endl;
        return false;
    }
}

#endif