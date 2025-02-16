#include "cpp-httplib/httplib.h"
#include "lemsearcher.hpp"
    
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
    // 1. 初始化，构建搜索索引
    ns_searcher::Searcher *search = new ns_searcher::Searcher();    
    search->InitSearcher(input,vector_input);  //初始化search，创建单例，并构建索引  

    // 2. 搭建服务器
    httplib::Server svr;
    svr.set_base_dir(root_path.c_str());    // 访问首页

    // 3.构建服务端应答响应
    svr.Get("/s", [&search](const httplib::Request &req, httplib::Response &rsp){
        // has_para：这个函数用来检测用户的请求中是否有搜索关键字
        if(!req.has_param("search")){    
            rsp.set_content("必须要有搜索关键字!", "text/plain; charset=utf-8");    
            return;    
        }    

        // 获取用户输入的关键词
        std::string text = req.get_param_value("search");
        std::cout << "用户在搜索：" << text << std::endl;

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


    std::cout << "服务器启动成功......" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}