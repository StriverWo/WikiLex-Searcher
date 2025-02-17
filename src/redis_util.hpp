#ifndef REDIS_UTIL_HPP
#define REDIS_UTIL_HPP


#include <hiredis/hiredis.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <algorithm>

class RedisUtil {
public:
    RedisUtil(const std::string& host = "127.0.0.1", int port = 6379);
    ~RedisUtil();
    
    // 连接redis服务器
    bool connect();

    // 热词统计功能
    bool incrementWordCount(const std::string& word);
    int getWordCount(const std::string& word);
    std::vector<std::string> getTopWords(int limit = 10);
    std::vector<std::pair<std::string, int>> getTopWordsSorted(int limit = 10);

    // 历史记录功能
    bool logQuery(const std::string& query);
    std::vector<std::string> getQueryHistory(int limit = 10);

private:
    redisContext* context;
    std::string host;
    int port;
};


// 下面是具体的定义：

RedisUtil::RedisUtil(const std::string& host, int port)
    : host(host), port(port), context(nullptr) {}

RedisUtil::~RedisUtil() {
    if (context) {
        redisFree(context);
    }
}

bool RedisUtil::connect() {
    context = redisConnect(host.c_str(), port);
    if (context == nullptr || context->err) {
        if (context) {
            std::cerr << "Redis connection error: " << context->errstr << std::endl;
        } else {
            std::cerr << "Cannot allocate redis context" << std::endl;
        }
        return false;
    }
    return true;
}

bool RedisUtil::incrementWordCount(const std::string& word) {
    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "ZINCRBY topwords 1 %s", word.c_str()));
    if (reply == nullptr) {
        std::cerr << "Redis command error!" << std::endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}
int RedisUtil::getWordCount(const std::string& word) {
    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "ZSCORE topwords %s", word.c_str()));
    int count = 0;
    if (reply != nullptr) {
        if (reply->type == REDIS_REPLY_STRING) {
            try {
                count = std::stoi(reply->str);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Error converting word count to integer: " << word << std::endl;
            }
        } else {
            std::cerr << "Error retrieving count for word: " << word << std::endl;
        }
        freeReplyObject(reply);
    }
    return count;
}
std::vector<std::string> RedisUtil::getTopWords(int limit) {
    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "ZREVRANGE topwords 0 %d WITHSCORES", limit - 1));
    std::vector<std::string> topWords;

    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            std::string word = reply->element[i]->str;
            topWords.emplace_back(std::move(word));
            // // 将字符串形式的分数转换为整数
            // try {
            //     int count = std::stoi(reply->element[i + 1]->str);
            //     topWords[word] = count;
            // } catch (const std::invalid_argument& e) {
            //     std::cerr << "Error converting score to integer for word: " << word << std::endl;
            // }
        }
    }
    freeReplyObject(reply);
    return topWords;
}

// RedisUtil 类中添加排序方法
std::vector<std::pair<std::string, int>> RedisUtil::getTopWordsSorted(int limit) {
    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "ZREVRANGE topwords 0 %d WITHSCORES", limit - 1));
    std::vector<std::pair<std::string, int>> topWords;

    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            std::string word = reply->element[i]->str;
            try {
                int count = std::stoi(reply->element[i + 1]->str);
                topWords.emplace_back(word, count);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Error converting score to integer for word: " << word << std::endl;
            }
        }
    }
    freeReplyObject(reply);

    // 按计数从高到低排序
    std::sort(topWords.begin(), topWords.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    return topWords;
}


bool RedisUtil::logQuery(const std::string& query) {
    // 使用 LPUSH 将查询记录推入 "query_history" 列表中
    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "LPUSH query_history %s", query.c_str()));
    if (!reply) {
        std::cerr << "Redis LPUSH command error for query history!" << std::endl;
        return false;
    }
    freeReplyObject(reply);
    // 限制列表长度为100（保留最近100条记录）
    reply = static_cast<redisReply*>(redisCommand(context, "LTRIM query_history 0 99"));
    if (reply) {
        freeReplyObject(reply);
    }
    return true;
}

std::vector<std::string> RedisUtil::getQueryHistory(int limit) {
    std::vector<std::string> history;
    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "LRANGE query_history 0 %d", limit - 1));
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            history.push_back(reply->element[i]->str);
        }
    }
    freeReplyObject(reply);
    return history;
}

#endif // REDIS_UTIL_HPP