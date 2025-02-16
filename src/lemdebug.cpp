#include "lemsearcher.hpp"    
#include <cstdio>    
#include <iostream>
#include <cstring>    
#include <string>    
    
const std::string input = "./data/simplified_lexemes.json";    
const std::string vector_input = "./data/lexeme_vectors.txt";    

int main()    
{    
    ns_searcher::Searcher *search = new ns_searcher::Searcher();    
    search->InitSearcher(input,vector_input);  //初始化search，创建单例，并构建索引  
    
    // 初始化结束标志：###INITEND###
    std::cout << "###INITEND###" << std::endl;
    
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
        // 解析 JSON
        Json::CharReaderBuilder readerBuilder;
        Json::Value root;
        std::string errs;
        std::istringstream iss(line);
        if (!Json::parseFromStream(readerBuilder, iss, &root, &errs)) {
            std::cerr << "JSON解析失败: " << errs << std::endl;
            continue;
        }
        
        std::string input_text = root.get("input_text", "").asString();
        std::string embedding_str = root.get("embedding", "").asString();
        
        // 将 embedding_str 解析为 vector<float>
        std::vector<float> query_embedding;
        std::istringstream embeddingStream(embedding_str);
        std::string token;
        while (std::getline(embeddingStream, token, ',')) {
            try {
                query_embedding.push_back(std::stof(token));
            } catch (...) {
                std::cerr << "解析向量失败: " << token << std::endl;
            }
        }
        
        // 这里调用你的搜索函数，比如 SearchCombinedWithEmbedding(query_embedding, &json_result)
        std::string json_result;
        search->SearchCombined(input_text,query_embedding,&json_result);
        
        // 这里简单输出，实际应输出搜索结果
        std::cout << "收到查询文本: " << input_text << std::endl;
        std::cout << "搜索结果: " << std::endl;
        std::cout << json_result << std::endl;//输出打印 

        // 添加结束分隔符
        std::cout << "###END###" << std::endl;
    }  

    delete search;
    return 0;    
}

