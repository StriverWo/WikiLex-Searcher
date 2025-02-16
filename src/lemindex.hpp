#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <jsoncpp/json/json.h>
#include "lemutil.hpp"
#include "lemlog.hpp"
#include "hnswlib/hnswlib.h"
#include "hnswlib/space_ip.h"


namespace ns_index {
    
    // 词条索引信息
    struct DocInfo {
        std::string title;            // 词条标题，使用 lemma 字段
        std::string language;         // 词条语言
        std::string forms;
        std::string senses;
        std::string url;              // 词条对应的 URL
        uint64_t doc_id;              // 文档ID（可从 lexeme id 提取）
        std::vector<float> vec;       // 词条向量表示
    };

    // 倒排索引中每个关键字对应的节点
    struct InvertedElem {
        uint64_t doc_id;      // 文档ID
        std::string word;     // 关键字
        int weight;           // 权重（例如关键词在文档中的出现次数加权）
    };

    // 一个关键字对应多个文档的倒排拉链
    typedef std::vector<InvertedElem> InvertedList;

    class Index {
    private:
        std::unordered_map<uint64_t, DocInfo> forward_index;  // 正排索引，存储所有词条的文档信息
        std::unordered_map<std::string, InvertedList> inverted_index;  // 倒排索引
        hnswlib::HierarchicalNSW<float>* vector_index = nullptr; // 向量索引（基于 HNSWlib）
        hnswlib::SpaceInterface<float>* space = nullptr;         // 距离空间
        int dim = 384;  // 向量维度（例如 sentence-bert 为384）

        // 单例模式
        Index() {}
        Index(const Index&) = delete;
        Index& operator=(const Index&) = delete;
        static Index* instance;
        static std::mutex mtx;

    public:
        ~Index() {
            if (vector_index) {
                delete vector_index;
                vector_index = nullptr;
            }
            if (space) {
                delete space;
                space = nullptr;
            }
        }

        // 获取 index 单例
        static Index* GetInstance() {
            if (instance == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instance == nullptr) {
                    instance = new Index();
                }
            }
            return instance;
        }

        // 构建索引接口
        bool BuildIndex(const std::string &simplifiedFile, const std::string &vectorFile) {
            // 使用简化后的json文件构建正排索引和倒排索引
            if(!BuildForwardIndex(simplifiedFile)) {
                std::cerr << "构建正排索引失败" << std::endl;
                return false;
            }
            // 加载预计算好的向量数据文件（格式id<TAB>lemma<TAB>向量）
            if(!LoadVectors(vectorFile)){
                std::cerr << "加载向量数据失败" << std::endl;
                return false;
            }
            // 构建向量索引
            if(!BuildVectorIndex()) {
                std::cerr << "构建向量索引失败" << std::endl;
                return false;
            }
            std::cout << "索引构建完成！" << std::endl;
            return true;
        }

        // 根据 doc_id 获取正排索引中的文档
        DocInfo* GetForwardIndex(uint64_t doc_id) {
            auto it = forward_index.find(doc_id);
            if (it == forward_index.end()) {
                std::cerr << "doc_id " << doc_id << " 不存在于正排索引中！" << std::endl;
                return nullptr;
            }
            return &(it->second);
        }

        // 根据关键字获取倒排拉链
        InvertedList* GetInvertedList(const std::string &word) {
            auto iter = inverted_index.find(word);
            if (iter == inverted_index.end())
                return nullptr;
            return &(iter->second);
        }
        // 得到向量索引
        hnswlib::HierarchicalNSW<float>* GetVectorIndex() {
            return vector_index;
        }

private:
        // -------------------------------
        // 1. 构建正排索引：从简化后的 JSON 文件中读取，每行一个 JSON 对象
        // -------------------------------
        bool BuildForwardIndex(const std::string &simplifiedFile) {
            // 1. 读取整个文件内容
            std::ifstream in(simplifiedFile);
            if (!in.is_open()) {
                std::cerr << "无法打开简化文件: " << simplifiedFile << std::endl;
                return false;
            }
            std::stringstream buffer;
            buffer << in.rdbuf();
            std::string fileContent = buffer.str();
            in.close();

            // 2. 将整个文件解析为 JSON 值（预期为数组）
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
            // 3. 遍历数组中每个词条对象
            for (const auto &lex : root) {
                // 构造 DocInfo 对象
                DocInfo doc;
                // 使用 lemma 作为标题
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
                                doc.forms += ";";
                            }
                            doc.senses += lex["senses"][i].asString();
                        }
                    }
                }
                //doc.senses = lex.get("senses","").asString();
                // 使用 combined_text 作为内容
                // doc.content = lex.get("combined_text", "").asString();
                // URL 字段
                doc.url = lex.get("url", "").asString();
                // 解析词条 id，例如 "L314" 转为数值 314；如果转换失败，使用当前 count
                std::string lexId = lex.get("id", "").asString();
                if (!lexId.empty() && lexId[0] == 'L') {
                    try {
                        doc.doc_id = std::stoull(lexId.substr(1));
                    } catch (...) {
                        doc.doc_id = count;
                    }
                } else {
                    doc.doc_id = count;
                }
                // 将 DocInfo 插入正排索引（unordered_map，以 doc_id 为 key）
                forward_index[doc.doc_id] = std::move(doc);
                // 构建倒排索引：调用 BuildInvertedIndex 对当前文档进行分词统计
                BuildInvertedIndex(forward_index[doc.doc_id]);
                count++;
            }
            std::cout << "正排和倒排索引构建完毕，总共加载 " << count << " 个词条。" << std::endl;
            return true;
        }
        
        // -------------------------------
        // 2. 构建倒排索引：针对单个文档（DocInfo），从其 title 和 content 中提取关键词
        // -------------------------------
        bool BuildInvertedIndex(const DocInfo &doc) {
            struct word_cnt {
                int title_cnt;
                int content_cnt;
                word_cnt() : title_cnt(0), content_cnt(0) {}
            };
            std::unordered_map<std::string, word_cnt> word_map;
            // 分词：使用 Jieba 分词
            std::vector<std::string> title_words;
            ns_util::JiebaUtil::CutString(doc.title, &title_words);
            for (auto s : title_words) {
                boost::to_lower(s);
                word_map[s].title_cnt++;
            }
            std::vector<std::string> content_words;
            ns_util::JiebaUtil::CutString(doc.forms, &content_words);
            for (auto s : content_words) {
                boost::to_lower(s);
                word_map[s].content_cnt++;
            }
            #define X 10
            #define Y 1
            for (auto &pair : word_map) {
                InvertedElem item;
                item.doc_id = doc.doc_id;
                item.word = pair.first;
                item.weight = X * pair.second.title_cnt + Y * pair.second.content_cnt;
                inverted_index[pair.first].push_back(std::move(item));
            }
            return true;
        }
        //
        // -------------------------------
        // 3. 从向量文件加载向量数据，更新正排索引中的 vec 字段
        // 向量文件格式： id<TAB>lemma<TAB>向量（空格分隔）
        // -------------------------------
        bool LoadVectors(const std::string &vectorFile) {
            std::ifstream in(vectorFile);
            if (!in.is_open()) {
                std::cerr << "无法打开向量文件: " << vectorFile << std::endl;
                return false;
            }
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty()) continue;
                std::istringstream iss(line);
                std::string idStr, lemma, vecStr;
                if (!std::getline(iss, idStr, '\t')) continue;
                if (!std::getline(iss, lemma, '\t')) continue;
                if (!std::getline(iss, vecStr)) continue;
                uint64_t id = 0;
                try {
                    if (!idStr.empty() && idStr[0] == 'L')
                        id = std::stoull(idStr.substr(1));
                    else
                        id = std::stoull(idStr);
                } catch (const std::exception &e) {
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
                it->second.vec = vec;
            }
            in.close();
            std::cout << "加载向量文件成功: " << vectorFile << std::endl;
            return true;
        }

        // -------------------------------
        // 4. 构建向量索引：利用 HNSWlib，将每个文档的向量加入向量索引中
        // -------------------------------
        bool BuildVectorIndex() {
            if (forward_index.empty()) {
                std::cerr << "正排索引为空，无法构建向量索引。" << std::endl;
                return false;
            }

            // space = new hnswlib::L2Space(dim);
            int count = 0;
            space = new hnswlib::InnerProductSpace(dim);
            size_t max_elements = forward_index.size();
            vector_index = new hnswlib::HierarchicalNSW<float>(space, max_elements, 16, 200);
            for (auto &pair : forward_index) {
                DocInfo &doc = pair.second;
                if (doc.vec.size() != static_cast<size_t>(dim)) {
                    std::cerr << "文档 " << doc.doc_id << " 向量维度不匹配。" << std::endl;
                    continue;
                }
                // 做每个向量归一化后再插入向量空间中
                //normalizeVector(doc.vec);
                vector_index->addPoint(doc.vec.data(), doc.doc_id);

                if((++count)%5000==0) {
                    std::cout << "向量索引构建中：第" << count << "个向量索引构建完成。" << std::endl;
                }
            }
            return true;
        }

        void normalizeVector(std::vector<float> &vec) {
            float norm = 0.0f;
            for (float v : vec)
                norm += v * v;
            norm = std::sqrt(norm);
            if (norm > 0) {
                for (float &v : vec)
                    v /= norm;
            }
        }        
    };
    // 单例成员初始化
    Index* Index::instance = nullptr;
    std::mutex Index::mtx;
}
