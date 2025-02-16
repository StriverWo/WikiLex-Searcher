#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>

// 引入cppjieba头文件
#include "cppjieba/Jieba.hpp"  //引入头文件（确保你建立的没有错误才可以使用）
// 引入s
//#include "sentencepiece_processor.h"


namespace ns_util
{

    class StringUtil
    {
        public:
        //切分字符串
        static void Splist(const std::string &target, std::vector<std::string> *out, const std::string &sep)
        {
            //boost库中的split函数
            boost::split(*out, target, boost::is_any_of(sep), boost::token_compress_on);
            //第一个参数：表示你要将切分的字符串放到哪里
            //第二个参数：表示你要切分的字符串
            //第三个参数：表示分割符是什么，不管是多个还是一个
            //第四个参数：它是默认可以不传，即切分的时候不压缩，不压缩就是保留空格
            //如：字符串为aaaa\3\3bbbb\3\3cccc\3\3d
                //如果不传第四个参数 结果为aaaa  bbbb  cccc  d
                //如果传第四个参数为boost::token_compress_on 结果为aaaabbbbccccd
                //如果传第四个参数为boost::token_compress_off 结果为aaaa  bbbb  cccc  d
        }
    };

    //下面这5个是分词时所需要的词库路径
    const char* const DICT_PATH = "./src/dict/jieba.dict.utf8";    
    const char* const HMM_PATH = "./src/dict/hmm_model.utf8";    
    const char* const USER_DICT_PATH = "./src/dict/user.dict.utf8";    
    const char* const IDF_PATH = "./src/dict/idf.utf8";    
    const char* const STOP_WORD_PATH = "./src/dict/stop_words.utf8";  

    class JiebaUtil    
    {    
    private:    
        static cppjieba::Jieba jieba; //定义静态的成员变量（需要在类外初始化）   
    public:    
        static void CutString(const std::string &src, std::vector<std::string> *out)    
        {   
            //调用CutForSearch函数，第一个参数就是你要对谁进行分词，第二个参数就是分词后的结果存放到哪里
            jieba.CutForSearch(src, *out);    
        }     
    };

    //类外初始化，就是将上面的路径传进去，具体和它的构造函数是相关的，具体可以去看一下源代码
    cppjieba::Jieba JiebaUtil::jieba(DICT_PATH, HMM_PATH, USER_DICT_PATH, IDF_PATH, STOP_WORD_PATH);

    // ------------------------------
    // 新增：基于 SentencePiece 的分词工具类
    // ------------------------------

    // class SentencePieceUtil 
    // {
    // public:
    //     // 使用 SentencePiece 对输入文本进行分词，并返回 token id 序列
    //     // 注意：为了避免每次调用重复加载模型，使用了static 变量控制加载
    //     static void Encode(const std::string &src, std::vector<int64_t> *out)
    //     {
    //         // 声明静态的 SentencePieceProcessor 对象和加载标志
    //         static sentencepiece::SentencePieceProcessor sp;
    //         static bool is_model_loaded = false;
    //         if (!is_model_loaded)
    //         {
    //             // 请根据实际情况修改模型文件路径
    //             const std::string sp_model_path = "./t5-small/spiece.model";
    //             auto status = sp.Load(sp_model_path);
    //             if (!status.ok())
    //             {
    //                 std::cerr << "加载 SentencePiece 模型失败: " << status.ToString() << std::endl;
    //                 return;
    //             }
    //             is_model_loaded = true;
    //         }
    //         // 调用 SentencePiece 的 EncodeAsIds 方法生成 token id 序列
    //         std::vector<int> temp_out;
    //         sp.Encode(src, &temp_out);
    //         // 将 std::vector<int> 转换为 std::vector<int64_t>
    //         out->resize(temp_out.size());
    //         std::transform(temp_out.begin(), temp_out.end(), out->begin(), [](int val) { return static_cast<int64_t>(val); })
    //     }
    // };

    // =========================
    // WordPiece 分词（基于 BERT 的词汇表）相关功能
    // =========================

    // 加载 BERT 词汇表。假设词汇文件 vocab.txt 中每一行一个词，行号即为 token id。
    inline std::unordered_map<std::string, int> LoadVocab(const std::string &vocab_file) {
        std::unordered_map<std::string, int> vocab;
        std::ifstream in(vocab_file);
        if (!in.is_open()) {
            std::cerr << "无法打开词汇文件: " << vocab_file << std::endl;
            return vocab;
        }
        std::string line;
        int index = 0;
        while (std::getline(in, line)) {
            if (!line.empty()) {
                vocab[line] = index;
                ++index;
            }
        }
        return vocab;
    }

    // 对单个单词进行 WordPiece 分词，返回该单词分词后的 token id 序列
    inline std::vector<int> WordPieceTokenizeWord(const std::string &word,
                                                   const std::unordered_map<std::string, int> &vocab,
                                                   int unk_token) {
        std::vector<int> output_tokens;
        int start = 0;
        int word_len = word.size();
        bool is_bad = false;
        while (start < word_len) {
            int end = word_len;
            std::string cur_substr;
            bool found = false;
            // 贪心匹配，尝试匹配最长子串
            while (start < end) {
                std::string substr = word.substr(start, end - start);
                if (start > 0) {
                    substr = "##" + substr;  // 非首个子词添加 "##" 前缀
                }
                if (vocab.find(substr) != vocab.end()) {
                    cur_substr = substr;
                    found = true;
                    break;
                }
                --end;
            }
            if (!found) {
                is_bad = true;
                break;
            }
            output_tokens.push_back(vocab.at(cur_substr));
            start = end;
        }
        if (is_bad) {
            output_tokens.clear();
            output_tokens.push_back(unk_token);
        }
        return output_tokens;
    }

    // 对整段文本进行 WordPiece 分词：先按空格拆分，然后对每个单词调用 WordPieceTokenizeWord，
    // 将所有 token id 拼接起来返回
    inline std::vector<int> WordPieceTokenize(const std::string &text,
                                               const std::unordered_map<std::string, int> &vocab,
                                               int unk_token) {
        std::vector<int> token_ids;
        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            // 可根据需要对 word 进行预处理，例如转为小写
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            std::vector<int> wp_tokens = WordPieceTokenizeWord(word, vocab, unk_token);
            token_ids.insert(token_ids.end(), wp_tokens.begin(), wp_tokens.end());
        }
        return token_ids;
    }

}