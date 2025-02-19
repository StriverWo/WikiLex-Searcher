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
#include "bloomfilter.hpp"

class MysqlUtil {
public:
    MysqlUtil(const std::string& host, const std::string& user, const std::string& password, const std::string& db);
    ~MysqlUtil();

    // 链接数据库
    bool connect();
    
    // 在名为 db（此处就是"wikilex"）的数据库中建立保存用户信息的users表
    bool createUsersTable();
    // 注册用户信息至users表中
    bool registerUser(const std::string& username, const std::string& password_hash);
    // 根据用户名查询用户记录，对比密码是否一致
    bool loginUser(const std::string& username, const std::string& password_hash);
    bool usernameExists(const::std::string& username);
    
    // 初始化布隆过滤器
    bool loadUsernamesForBloomFilter(BloomFilter &bloomFilter);

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
    bool isConnected;
};


MysqlUtil::MysqlUtil(const std::string& host, const std::string& user,
                     const std::string& password, const std::string& db)
    : host(host), user(user), password(password), db(db), driver(nullptr), isConnected(false) {}

MysqlUtil::~MysqlUtil() {
    // unique_ptr 自动释放连接资源
}


bool MysqlUtil::loadUsernamesForBloomFilter(BloomFilter &bloomFilter) {
    // 如果没有连接
    if(!isConnected) {
        std::cerr << "数据库未连接" <<std::endl;
        return false;
    }
    
    try {
        // 创建一个 Statement 对象
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        // 执行查询，获取所有用户名
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT username FROM users"));
        while (res->next()) {
            std::string username = res->getString("username");
            // 将用户名加入 BloomFilter
            bloomFilter.add(username);
        }
    } catch (sql::SQLException &e) {
        std::cerr << "SQL Exception in loadUsernamesForBloomFilter: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

bool MysqlUtil::connect() {
    try {
        driver = get_driver_instance();
        if(!driver) {
            std::cerr << "无法获取 MySQL 驱动实例" << std::endl;
        }
        conn.reset(driver->connect(host, user, password));
        conn->setSchema(db);
        isConnected = true;
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "MySQL连接错误: " << e.what() << std::endl;
        isConnected = false;
        return false;
    }
}

bool MysqlUtil::createUsersTable() {
    if(!isConnected) {
        std::cerr << "数据库未连接" <<std::endl;
        return false;
    }
    try {
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::string createTableQuery = "CREATE TABLE IF NOT EXISTS users ("
                                        "id INT AUTO_INCREMENT PRIMARY KEY,"
                                        "username VARCHAR(255) NOT NULL UNIQUE,"
                                        "password_hash VARCHAR(255) NOT NULL"
                                        ")";
        stmt->execute(createTableQuery);
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "创建 users 表失败: " << e.what() << std::endl;
        return false;
    }
}


bool MysqlUtil::registerUser(const std::string& username, const std::string& password_hash) {
    if(!isConnected) {
        std::cerr << "数据库未连接" <<std::endl;
        return false;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("INSERT INTO users(username, password_hash) VALUES (?, ?)"));
        pstmt->setString(1, username);
        pstmt->setString(2, password_hash);
        int affectedRows = pstmt->executeUpdate();
        return affectedRows > 0;
    } catch (sql::SQLException &e) {
        // 检查是否是users表不存在的错误（SQLSTATE 42S02）
        if (std::string(e.getSQLState()).substr(0, 5) == "42S02") {
            if (createUsersTable()) {
                try {
                    std::unique_ptr<sql::PreparedStatement> pstmt(
                        conn->prepareStatement("INSERT INTO users(username, password_hash) VALUES (?, ?)"));
                    pstmt->setString(1, username);
                    pstmt->setString(2, password_hash);
                    int affectedRows = pstmt->executeUpdate();
                    return affectedRows > 0;
                } catch (sql::SQLException &e2) {
                    std::cerr << "再次尝试注册用户失败: " << e2.what() << std::endl;
                    return false;
                }
            } else {
                std::cerr << "创建 users 表失败，无法注册用户" << std::endl;
                return false;
            }
        } else{
            std::cerr << "注册用户失败: " << e.what() << std::endl;
            return false;
        }
    }
}


bool MysqlUtil::usernameExists(const std::string &username) {
    // 如果没有连接
    if(!isConnected) {
        std::cerr << "数据库未连接" <<std::endl;
        return false;
    }
    try {
        // 预编译 SQL 语句，查询 users 表中指定用户名的记录数
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("SELECT COUNT(*) AS count FROM users WHERE username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            int count = res->getInt("count");
            return (count > 0);
        }
    } catch (sql::SQLException &e) {
        std::cerr << "SQL Exception in usernameExists: " << e.what() << std::endl;
        return false;
    }
    return false;
}



bool MysqlUtil::loginUser(const std::string& username, const std::string& password_hash) {
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("SELECT password_hash FROM users WHERE username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string storedHash = res->getString("password_hash");
            return storedHash == password_hash;
            // 使用crypto_memcmp时需要系统中事先安装 OpenSSL 开发库：sudo apt-get install libssl-dev
            // return crypto_memcmp(storedHash.c_str(), password_hash.c_str(), storedHash.length()) == 0;
        } else {
            return false;
        }
    } catch (sql::SQLException &e) {
        std::cerr << "登录查询失败: " << e.what() << std::endl;
        return false;
    }
}

#endif