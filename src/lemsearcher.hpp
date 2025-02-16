#pragma once  

#include <algorithm>
#include <jsoncpp/json/json.h>
#include <vector>
#include <string>
#include "lemindex.hpp"
#include "lemutil.hpp"  // 用于分词

namespace ns_searcher
{
    // 将word变成数组，因为搜索的关键字中可能有多个关键字对应一个文档
    // 为了使文档只出现一次，我们将所以倒排拉链的文档都去重
    // 相同文档的将权重加起来，把映射这个文档的关键字填写到数组里面

    //该结构体是用来对重复文档去重的结点结构
    struct InvertedElemPrint
    {
        uint64_t doc_id;  //文档ID
        int weight;       //重复文档的权重之和
        std::vector<std::string> words;//关键字的集合，我们之前的倒排拉链节点只能保存一个关键字
        InvertedElemPrint():doc_id(0), weight(0){}
    };

    //定义一个用于存储向量搜索结果的结构体
    struct VectorResult {
        uint64_t doc_id;    //文档ID
        float similarity;   //向量相似度得分（转换后，数值越高表示越相似）
    };

    class Searcher
    {
    private:
        ns_index::Index *index; //供系统进行查找的索引
    public:
        Searcher(){}
        ~Searcher(){}
    public:
        void InitSearcher(const std::string &input, const std::string &vector_input)
        {
            // 获取或者创建index对象（单例）
            index = ns_index::Index::GetInstance();  
            //std::cout<< "获取index单例成功...."<<std::endl;
            LOG(NORMAL , "获取index单例成功....");
            // 根据index对象建立正排和倒排索引
            index->BuildIndex(input, vector_input);
            //std::cout<< "建立正排和倒排索引成功...."<<std::endl;
            LOG(NORMAL , "建立正排、倒排和向量索引成功....");
        }


         // 原有的倒排搜索逻辑（简化版），结果存放在 inverted_results 中
    void InvertedSearch(const std::string &query, std::vector<ns_searcher::InvertedElemPrint> &inverted_results) {
        std::vector<std::string> words;
        ns_util::JiebaUtil::CutString(query, &words);
        std::unordered_map<uint64_t, ns_searcher::InvertedElemPrint> tokens_map;
        for (std::string word : words) {
            boost::to_lower(word);
            ns_index::InvertedList* inv_list = index->GetInvertedList(word);
            if (inv_list == nullptr)
                continue;
            for (const auto &elem : *inv_list) {
                auto &item = tokens_map[elem.doc_id]; // 自动创建或更新已有项
                item.doc_id = elem.doc_id;
                item.weight += elem.weight;
                item.words.push_back(elem.word);
            }
        }
        // 合并后将结果存入 inverted_results
        for (const auto &kv : tokens_map) {
            inverted_results.push_back(kv.second);
        }
    }
    
    void VectorSearch(const std::vector<float>& query_vector, std::vector<VectorResult> &vector_results) {
        //std::vector<float> query_vec = ns_util::ComputeVector(query, 384);
        std::vector<float> query_vec = query_vector;
        if (query_vec.empty()) {
            std::cerr << "Query 向量化失败" << std::endl;
            return;
        }

        int k = 20;
        auto result = index->GetVectorIndex()->searchKnn(query_vec.data(), k);
        while (!result.empty()) {
            auto pair = result.top();
            result.pop();
            VectorResult vecRes;
            vecRes.doc_id = pair.second;
            // 如果 InnerProductSpace 返回的是内积值（相似度），则直接使用即可
            vecRes.similarity = 1 - pair.first;  // 这里 1 - pair.first 就是相似度得分
            vector_results.push_back(vecRes);
        }
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

    // 融合策略实现：（取并集）
    void SearchCombined(const std::string &query, const std::vector<float>& query_vector,std::string *json_string) {
        // 1. 分别获得倒排搜索结果和向量搜索结果
        std::vector<ns_searcher::InvertedElemPrint> inverted_results;
        InvertedSearch(query, inverted_results);
        
        std::vector<VectorResult> vector_results;
        VectorSearch(query_vector, vector_results);
        
        // 2. 分别构建 doc_id -> 得分 映射（未归一化的得分）
        std::unordered_map<uint64_t, float> inverted_score_map;
        for (const auto &item : inverted_results) {
            inverted_score_map[item.doc_id] = static_cast<float>(item.weight);
        }
        std::unordered_map<uint64_t, float> vector_score_map;
        for (const auto &item : vector_results) {
            vector_score_map[item.doc_id] = item.similarity;
        }
        // 3. 对倒排索引得分进行归一化：将所有倒排得分除以最大得分，使其归一化到 [0, 1]
        float max_inv = 0.0f;
        for (const auto &kv : inverted_score_map) {
            if (kv.second > max_inv)
                max_inv = kv.second;
        }
        if (max_inv > 0.0f) {
            for (auto &kv : inverted_score_map) {
                kv.second /= max_inv;
            }
        }
        // 3. 取并集：将两侧所有的 doc_id 加入集合
        std::unordered_set<uint64_t> all_ids;
        for (const auto &kv : inverted_score_map) {
            all_ids.insert(kv.first);
        }
        for (const auto &kv : vector_score_map) {
            all_ids.insert(kv.first);
        }
        
        // 4. 计算综合得分：对于不存在于某一侧的候选，得分默认为 0
        struct CombinedResult {
            uint64_t doc_id;
            float combined_score;
        };
        std::vector<CombinedResult> combined_results;
        float alpha = 0.5f;  // 倒排得分权重
        float beta  = 0.5f;  // 向量得分权重
        for (auto doc_id : all_ids) {
            float inv_score = 0.0f;
            if (inverted_score_map.find(doc_id) != inverted_score_map.end()) {
                inv_score = inverted_score_map[doc_id];
            }
            float vec_score = 0.0f;
            if (vector_score_map.find(doc_id) != vector_score_map.end()) {
                vec_score = vector_score_map[doc_id];
            }
            float combined = alpha * inv_score + beta * vec_score;
            combined_results.push_back({doc_id, combined});
        }
        
        // 5. 对融合结果按综合得分降序排序
        std::sort(combined_results.begin(), combined_results.end(),
                [](const CombinedResult &a, const CombinedResult &b) {
                    return a.combined_score > b.combined_score;
                });
        
        // 6. 从正排索引中获取文档信息，并构建 JSON 结果
        Json::Value root;
        for (const auto &item : combined_results) {
            ns_index::DocInfo* doc = index->GetForwardIndex(item.doc_id);
            if (doc == nullptr)
                continue;
            Json::Value elem;
            elem["title"] = doc->title;
            elem["language"] = doc->language;
            elem["forms"] = doc->forms;
            elem["senses"] = doc->senses;
            elem["url"] = doc->url;
            elem["score"] = item.combined_score;
            root.append(elem);
        }
        Json::StyledWriter writer;
        *json_string = writer.write(root);
    }
    };
}
