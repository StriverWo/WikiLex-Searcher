#include "cpp-httplib/httplib.h"
#include "lemsearcher.hpp"
#include "redis_util.hpp"

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
    // 初始化 Redis 客户端
    RedisUtil redis;
    redis.connect();
    
    // 1. 初始化，构建搜索索引
    ns_searcher::Searcher *search = new ns_searcher::Searcher();    
    search->InitSearcher(input,vector_input);  //初始化search，创建单例，并构建索引  

    // 2. 搭建服务器
    httplib::Server svr;
    svr.set_base_dir(root_path.c_str());    // 访问首页

     // 用户注册接口
    svr.Post("/register", [&redis](const httplib::Request &req, httplib::Response &rsp) {
        Json::Value requestBody;
        Json::Reader reader;
        if (!reader.parse(req.body, requestBody)) {
            rsp.set_content("无效的请求数据", "text/plain");
            return;
        }

        std::string username = requestBody["username"].asString();
        std::string password = requestBody["password"].asString();

        if (redis.registerUser(username, password)) {
            rsp.set_content(R"({"success": true, "message": "注册成功"})", "application/json");
        } else {
            rsp.set_content(R"({"success": false, "message": "用户名已存在"})", "application/json");
        }
    });

    // 用户登录接口
    svr.Post("/login", [&redis](const httplib::Request &req, httplib::Response &rsp) {
        Json::Value requestBody;
        Json::Reader reader;
        if (!reader.parse(req.body, requestBody)) {
            rsp.set_content("无效的请求数据", "text/plain");
            return;
        }

        std::string username = requestBody["username"].asString();
        std::string password = requestBody["password"].asString();

        if (redis.loginUser(username, password)) {
            // 登录成功，设置会话
            // redisCommand(redis.context, "SET session:%s logged_in", username.c_str());
            rsp.set_content(R"({"success": true, "message": "登录成功"})", "application/json");
        } else {
            rsp.set_content(R"({"success": false, "message": "用户名或密码错误"})", "application/json");
        }
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