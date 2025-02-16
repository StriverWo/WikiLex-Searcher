// lemvectorize.cpp
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>

// 读取整个文件内容为字符串（如果需要）
std::string LoadFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    // 定义输入 JSON 文件、输出向量文件和模型名称（这里使用默认模型名称）
    std::string inputFile = "./data/simplified_lexemes.json";
    std::string outputFile = "./data/lexeme_vectors.txt";
    std::string modelName = "all-MiniLM-L6-v2";

    // 构造调用命令，注意用引号包裹路径以防止空格问题
    std::string command = "python3 ./model/sentence-bert/vectorize.py --input \"" 
                            + inputFile + "\" --output \"" + outputFile 
                            + "\" --model \"" + modelName + "\"" ;
                            
    std::cout << "执行命令: " << command << std::endl;
    
    // 使用 popen 调用 Python 脚本，并读取其输出
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "popen() 调用失败！" << std::endl;
        return 1;
    }
    
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int returnCode = pclose(pipe);
    std::cout << "Python 脚本输出:" << std::endl;
    std::cout << result << std::endl;
    std::cout << "返回码: " << returnCode << std::endl;
    
    return 0;
}
