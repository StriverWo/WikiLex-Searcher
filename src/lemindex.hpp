#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <jsoncpp/json/json.h>
#include <algorithm>
#include <cctype>
#include <cmath>

// 引入项目自定义的头文件
#include "lemutil.hpp"
#include "lemlog.hpp"

// 引入 HNSWlib 头文件（假定路径正确）
#include "hnswlib/hnswlib.h"
#include "hnswlib/space_ip.h"

namespace ns_index {

struct DocInfo {
    std::string title;       // 词条标题（使用 lemma 字段）
    std::string language;    // 词条语言
    std::string forms;       // 词形变化（多个形式以空格分隔）
    std::string senses;      // 释义（多个释义以分号分隔）
    std::string url;         // 词条对应的 URL
    uint64_t doc_id;         // 文档ID（可从 lexeme id 提取）
    std::vector<float> vec;  // 词条向量表示
};

struct InvertedElem {
    uint64_t doc_id;  // 文档ID
    std::string word; // 关键词
    int weight;       // 权重：例如按关键词在标题和词形变化中的出现次数加权
};

using InvertedList = std::vector<InvertedElem>;

class Index {
public:
    // 获取单例实例
    static Index* GetInstance();

    ~Index();

    // 构建索引：先构建正排和倒排索引，再加载向量数据，并构建向量索引
    bool BuildIndex(const std::string& simplifiedFile, const std::string& vectorFile);

    // 根据 doc_id 获取正排索引中的文档
    DocInfo* GetForwardIndex(uint64_t doc_id);

    // 根据关键词获取倒排拉链
    InvertedList* GetInvertedList(const std::string& word);

    // 获取向量索引指针
    hnswlib::HierarchicalNSW<float>* GetVectorIndex();

private:
    Index() = default;
    Index(const Index&) = delete;
    Index& operator=(const Index&) = delete;

    // 构建正排和倒排索引（从简化后的 JSON 文件中读取）
    bool BuildForwardIndex(const std::string& simplifiedFile);

    // 针对单个文档构建倒排索引
    bool BuildInvertedIndex(const DocInfo& doc);

    // 从向量数据文件加载向量，并更新正排索引中对应文档的向量字段
    bool LoadVectors(const std::string& vectorFile);

    // 构建向量索引：利用 HNSWlib 将每个文档的向量插入到索引中
    bool BuildVectorIndex();

    // 辅助：对向量归一化
    void normalizeVector(std::vector<float>& vec);

    std::unordered_map<uint64_t, DocInfo> forward_index;              // 正排索引（以 doc_id 为 key）
    std::unordered_map<std::string, InvertedList> inverted_index;       // 倒排索引（以关键词为 key）
    hnswlib::HierarchicalNSW<float>* vector_index = nullptr;            // 向量索引
    hnswlib::SpaceInterface<float>* space = nullptr;                    // 距离空间
    int dim = 384;  // 向量维度（例如 Sentence‑BERT 为384）

    static Index* instance;
    static std::mutex mtx;
};

// 单例成员初始化
Index* Index::instance = nullptr;
std::mutex Index::mtx;

Index* Index::GetInstance() {
    if (!instance) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!instance) {
            instance = new Index();
        }
    }
    return instance;
}

Index::~Index() {
    if (vector_index) {
        delete vector_index;
        vector_index = nullptr;
    }
    if (space) {
        delete space;
        space = nullptr;
    }
}

bool Index::BuildIndex(const std::string& simplifiedFile, const std::string& vectorFile) {
    if (!BuildForwardIndex(simplifiedFile)) {
        std::cerr << "构建正排索引失败" << std::endl;
        return false;
    }
    if (!LoadVectors(vectorFile)) {
        std::cerr << "加载向量数据失败" << std::endl;
        return false;
    }
    if (!BuildVectorIndex()) {
        std::cerr << "构建向量索引失败" << std::endl;
        return false;
    }
    std::cout << "索引构建完成！" << std::endl;
    return true;
}

DocInfo* Index::GetForwardIndex(uint64_t doc_id) {
    auto it = forward_index.find(doc_id);
    if (it == forward_index.end()) {
        std::cerr << "doc_id " << doc_id << " 不存在于正排索引中！" << std::endl;
        return nullptr;
    }
    return &it->second;
}

InvertedList* Index::GetInvertedList(const std::string& word) {
    auto it = inverted_index.find(word);
    if (it == inverted_index.end())
        return nullptr;
    return &it->second;
}

hnswlib::HierarchicalNSW<float>* Index::GetVectorIndex() {
    return vector_index;
}

bool Index::BuildForwardIndex(const std::string& simplifiedFile) {
    std::ifstream in(simplifiedFile);
    if (!in.is_open()) {
        std::cerr << "无法打开简化文件: " << simplifiedFile << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string fileContent = buffer.str();
    in.close();

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(fileContent);
    if (!Json::parseFromStream(builder, iss, &root, &errs)) {
        std::cerr << "JSON解析错误: " << errs << std::endl;
        return false;
    }
    if (!root.isArray()) {
        std::cerr << "错误：预期 JSON 根元素为数组。" << std::endl;
        return false;
    }

    int count = 0;
    for (const auto& lex : root) {
        DocInfo doc;
        doc.title = lex.get("lemma", "").asString();
        doc.language = lex.get("language", "").asString();
        if (lex["forms"].isArray()) {
            for (Json::Value::ArrayIndex i = 0; i < lex["forms"].size(); ++i) {
                if (lex["forms"][i].isString()) {
                    if (i > 0) {
                        doc.forms += " ";
                    }
                    doc.forms += lex["forms"][i].asString();
                }
            }
        }
        if (lex["senses"].isArray()) {
            for (Json::Value::ArrayIndex i = 0; i < lex["senses"].size(); ++i) {
                if (lex["senses"][i].isString()) {
                    if (i > 0) {
                        doc.senses += ";";
                    }
                    doc.senses += lex["senses"][i].asString();
                }
            }
        }
        doc.url = lex.get("url", "").asString();
        std::string lexId = lex.get("id", "").asString();
        try {
            if (!lexId.empty() && lexId.front() == 'L')
                doc.doc_id = std::stoull(lexId.substr(1));
            else
                doc.doc_id = std::stoull(lexId);
        } catch (...) {
            doc.doc_id = count;
        }
        forward_index[doc.doc_id] = std::move(doc);
        BuildInvertedIndex(forward_index[doc.doc_id]);
        count++;
    }
    std::cout << "正排和倒排索引构建完毕，总共加载 " << count << " 个词条。" << std::endl;
    return true;
}

bool Index::BuildInvertedIndex(const DocInfo& doc) {
    struct word_cnt {
        int title_cnt = 0;
        int content_cnt = 0;
    };
    std::unordered_map<std::string, word_cnt> word_map;
    std::vector<std::string> title_words;
    ns_util::JiebaUtil::CutString(doc.title, &title_words);
    ns_util::removeSpacesAndPunctuationFromVector(title_words);
    for (auto& s : title_words) {
        if(s == "") continue;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        word_map[s].title_cnt++;
    }
    std::vector<std::string> content_words;
    ns_util::JiebaUtil::CutString(doc.forms, &content_words);
    ns_util::removeSpacesAndPunctuationFromVector(content_words);
    for (auto& s : content_words) {
        if(s == "") continue;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        word_map[s].content_cnt++;
    }
    const int X = 10;
    const int Y = 1;
    for (auto& pair : word_map) {
        InvertedElem item;
        item.doc_id = doc.doc_id;
        item.word = pair.first;
        item.weight = X * pair.second.title_cnt + Y * pair.second.content_cnt;
        inverted_index[pair.first].push_back(std::move(item));
    }
    return true;
}

bool Index::LoadVectors(const std::string& vectorFile) {
    std::ifstream in(vectorFile);
    if (!in.is_open()) {
        std::cerr << "无法打开向量文件: " << vectorFile << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        std::istringstream iss(line);
        std::string idStr, lemma, vecStr;
        if (!std::getline(iss, idStr, '\t'))
            continue;
        if (!std::getline(iss, lemma, '\t'))
            continue;
        if (!std::getline(iss, vecStr))
            continue;
        uint64_t id = 0;
        try {
            if (!idStr.empty() && idStr.front() == 'L')
                id = std::stoull(idStr.substr(1));
            else
                id = std::stoull(idStr);
        } catch (const std::exception& e) {
            std::cerr << "转换 docID 失败: " << idStr << ", 错误: " << e.what() << std::endl;
            continue;
        }
        std::istringstream vecStream(vecStr);
        std::vector<float> vec;
        float val;
        while (vecStream >> val) {
            vec.push_back(val);
        }
        auto it = forward_index.find(id);
        if (it == forward_index.end()) {
            std::cerr << "正排索引中未找到 docID: " << id << std::endl;
            continue;
        }
        it->second.vec = std::move(vec);
    }
    in.close();
    std::cout << "加载向量文件成功: " << vectorFile << std::endl;
    return true;
}

bool Index::BuildVectorIndex() {
    if (forward_index.empty()) {
        std::cerr << "正排索引为空，无法构建向量索引。" << std::endl;
        return false;
    }
    int count = 0;
    // 使用 InnerProductSpace，假设向量已归一化，则内积即为余弦相似度
    space = new hnswlib::InnerProductSpace(dim);
    size_t max_elements = forward_index.size();
    vector_index = new hnswlib::HierarchicalNSW<float>(space, max_elements, 16, 200);
    for (auto& pair : forward_index) {
        DocInfo& doc = pair.second;
        if (doc.vec.size() != static_cast<size_t>(dim)) {
            std::cerr << "文档 " << doc.doc_id << " 向量维度不匹配。" << std::endl;
            continue;
        }
        // 若需要归一化，请取消下行注释
        // normalizeVector(doc.vec);
        vector_index->addPoint(doc.vec.data(), doc.doc_id);
        if ((++count) % 5000 == 0) {
            std::cout << "向量索引构建中：第 " << count << " 个向量构建完成。" << std::endl;
        }
    }
    return true;
}

void Index::normalizeVector(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float v : vec) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 0) {
        for (float& v : vec) {
            v /= norm;
        }
    }
}

} // namespace ns_index
