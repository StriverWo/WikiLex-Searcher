#include "cpp-httplib/httplib.h"
#include "lemsearcher.hpp"
#include "redis_util.hpp"
#include "mysql_util.hpp"

const std::string input = "./data/simplified_lexemes.json";    
const std::string vector_input = "./data/lexeme_vectors.txt";    
const std::string root_path = "./lemwwwroot";    


std::string exec_python_vectorize(const std::string& input_text) {
    // 构造调用命令，此处假设 python3 可执行文件在 PATH 中
    // 你可以将输入文本作为命令行参数传递，但要注意转义空格和特殊字符
    std::string command = "python3 ./src/lemvectorize.py \"" + input_text + "\"";

    // 打开管道读取脚本输出
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    // 去除末尾换行符
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}


// 将逗号分隔的字符串转换为 vector<float>
std::vector<float> ParseEmbeddingString(const std::string &embedding_str) {
    std::vector<float> embedding;
    std::istringstream iss(embedding_str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        try {
            // 使用 std::stof 将 token 转为 float
            float value = std::stof(token);
            embedding.push_back(value);
        } catch (const std::exception &e) {
            std::cerr << "转换向量元素失败: " << token << ", 错误: " << e.what() << std::endl;
        }
    }
    return embedding;
}


int main() {
    // 初始化 MySQL 客户端
    // 使用 tcp://127.0.0.1:3306，连接本地 MySQL ，user name为users，密码为“FangFang123!"" 数据库名为 wikilex
    // 传入用户名和用户密码
    MysqlUtil mysql("tcp://127.0.0.1:3306", "users", "FangFang123!\"", "wikilex");
    if (!mysql.connect()) {
        std::cerr << "MySQL 连接失败" << std::endl;
        return -1;
    }

    // 初始化 Redis 客户端
    RedisUtil redis;
    redis.connect();

    // 初始化布隆过滤器并从数据库中加载username构建布隆过滤器
    BloomFilter bloomFilter(1000000, 7);  // 位数组大小为1000000，7个哈希函数
    mysql.loadUsernamesForBloomFilter(bloomFilter);

    // 1. 初始化，构建搜索索引
    ns_searcher::Searcher *search = new ns_searcher::Searcher();    
    search->InitSearcher(input,vector_input);  //初始化search，创建单例，并构建索引  

    // 2. 搭建服务器
    httplib::Server svr;
    svr.set_base_dir(root_path.c_str());    // 访问首页

    // 用户注册接口
    svr.Post("/register", [&redis,&mysql, &bloomFilter](const httplib::Request &req, httplib::Response &rsp) {
        Json::Value requestBody;
        Json::Reader reader;
        if (!reader.parse(req.body, requestBody)) {
            rsp.set_content("无效的请求数据", "text/plain");
            return;
        }

        std::string username = requestBody["username"].asString();
        std::string password = requestBody["password"].asString();

        // 为了简单，这里直接使用明文作为密码哈希，实际上应进行安全性哈希处理
        std::string password_hash = password;

        // 使用布隆过滤器进行过滤，避免所有的注册请求都去数据库中查找是否存在该用户名
        // 但好像注册时布隆过滤器的用处不大，因为判断存在时也可能是假的，还需去数据库中查询
        // 使用布隆过滤器预先判断用户名是否可能存在
        if (bloomFilter.contains(username)) {
            // 布隆过滤器返回包含，可能存在，也可能是假阳性，
            // 此时必须进一步查询数据库来确认
            if (mysql.usernameExists(username)) {
                rsp.set_content(R"({"success": false, "message": "用户名已存在"})", "application/json");
                return;
            }
        }

        if (!mysql.registerUser(username, password_hash)) {
            rsp.set_content(R"({"success": false, "message": "用户名已存在或注册失败"})", "application/json");
            return;
        }

        //注册成功后，更新布隆过滤器
        bloomFilter.add(username);

        // 可选：将新注册用户的信息写入 Redis 缓存
        redis.registerUser(username, password); // 如有相应实现
        rsp.set_content(R"({"success": true, "message": "注册成功"})", "application/json");
    });

    // 用户登录接口
    svr.Post("/login", [&redis,&mysql,&bloomFilter](const httplib::Request &req, httplib::Response &rsp) {
        Json::Value requestBody;
        Json::Reader reader;
        if (!reader.parse(req.body, requestBody)) {
            rsp.set_content("无效的请求数据", "text/plain");
            return;
        }

        std::string username = requestBody["username"].asString();
        std::string password = requestBody["password"].asString();
        // 这里同样简单的将密码作为明文
        std::string password_hash = password;

        // 使用布隆过滤器判断用户名是否可能存在
        if (!bloomFilter.contains(username)) {
            rsp.set_content(R"({"success": false, "message": "用户名或密码错误"})", "application/json");
            return;
        }

        if (mysql.loginUser(username, password_hash)) {
            // 登录成功后，可在 Redis 中更新用户会话信息（例如设置一个 session key）
            // redis.setSession(username, "logged_in");
            rsp.set_content(R"({"success": true, "message": "登录成功"})", "application/json");
        } else {
            rsp.set_content(R"({"success": false, "message": "用户名或密码错误"})", "application/json");
        }
        // 将下面的原来的代码删掉，直接从mysql中更为准确的验证登录能否成功
        // if (redis.loginUser(username, password)) {
        //     // 登录成功，设置会话
        //     // redisCommand(redis.context, "SET session:%s logged_in", username.c_str());
        //     rsp.set_content(R"({"success": true, "message": "登录成功"})", "application/json");
        // } else {
        //     rsp.set_content(R"({"success": false, "message": "用户名或密码错误"})", "application/json");
        // }
    });

    // 3.构建服务端应答响应
    svr.Get("/s", [&search, &redis](const httplib::Request &req, httplib::Response &rsp){
        // has_para：这个函数用来检测用户的请求中是否有搜索关键字
        if(!req.has_param("search")){    
            rsp.set_content("必须要有搜索关键字!", "text/plain; charset=utf-8");    
            return;    
        }    

        // 获取用户输入的关键词
        std::string text = req.get_param_value("search");
        std::cout << "用户在搜索：" << text << std::endl;

        // 假设用户的用户名存储在请求的cookie或某个请求参数中（这里我们假设用户名是 "username"）
        std::string username = req.get_param_value("username");  // 这里假设请求中包含了用户名参数

        // 检查用户是否登录（此处略过详细的登录验证）
        if (username.empty()) {
            rsp.set_content("用户未登录", "text/plain; charset=utf-8");
            return;
        }

        // 将搜索记录添加到 Redis 历史记录中
        if (!redis.logQuery(username, text)) {
            rsp.set_content("记录搜索历史失败！", "text/plain; charset=utf-8");
            return;
        }

        // 热词统计
        if(!redis.incrementWordCount(text)) {
            // 注意：封闭函数局部变量不能在 lambda 体中引用，除非其位于捕获列表中
            rsp.set_content("热词统计失败！", "text/plain; charset=utf-8");
            return;
        }

        // 使用python脚本对文本进行向量化
        std::vector<float> embedding_vector;
        try {
            std::string embedding_str = exec_python_vectorize(text);
            //std::cout << "文本向量化结果: " << embedding_str << std::endl;
            embedding_vector = ParseEmbeddingString(embedding_str);
        } catch (const std::exception &e) {
            std::cerr << "错误: " << e.what() << std::endl;
        }

        // 搜索文本匹配的结果
        std::string json_results;
        search->SearchCombined(text,embedding_vector,&json_results);

        rsp.set_content(json_results,"application/json");
        std::cout << "用户搜索成功，结果已返回！" << std::endl;
    });
    
    // 增加接口用来获取热词
    svr.Get("/top-words", [&redis](const httplib::Request &req, httplib::Response &rsp) {
        auto topWords = redis.getTopWords();
        Json::Value jsonResult; // 使用 jsoncpp 的 Json::Value 来构造 JSON 数据
        
        for (size_t i = 0; i < topWords.size(); ++i) {
            Json::Value wordObj;
            wordObj["rank"] = static_cast<int>(i + 1);
            wordObj["word"] = topWords[i];
            jsonResult.append(wordObj); // 将每个热词对象添加到数组中
        }

        // 将 jsonResult 转换为字符串并设置响应内容
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, jsonResult);
        rsp.set_content(jsonString, "application/json");
    });

    // 历史记录接口：获取最近的搜索历史记录
    svr.Get("/history", [&redis](const httplib::Request &req, httplib::Response &rsp) {
        if (!req.has_param("username")) {
            rsp.set_content("缺少用户名参数", "text/plain");
            return;
        }
        std::string username = req.get_param_value("username");
        auto history = redis.getQueryHistory(username, 10);  // 获取指定用户的历史记录        
        Json::Value jsonResult;
        for (const auto &entry : history) {
            jsonResult.append(entry);
        }
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, jsonResult);
        rsp.set_content(jsonString, "application/json");
    });

    std::cout << "服务器启动成功......" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}