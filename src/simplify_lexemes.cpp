#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <jsoncpp/json/json.h>
#include <algorithm>
#include <memory>

// 解析单个 lexeme 对象，并提取详细信息，仅保留英语条目
Json::Value ProcessLexeme(const Json::Value &lex) {
    Json::Value simple;

    // 只保留包含英文 lemma 的条目
    if (!lex["lemmas"].isMember("en"))
        return Json::Value(); // 返回空对象，表示跳过

    // 词条 id
    if (lex.isMember("id"))
        simple["id"] = lex["id"];

    // 语言：这里直接标记为 "en"
    simple["language"] = "en";

    // 英文 lemma
    if (lex["lemmas"]["en"].isMember("value"))
        simple["lemma"] = lex["lemmas"]["en"]["value"];

    // 提取所有英文形式（forms）：遍历 forms 数组，提取每个 form 中 "representations" 的 "en" 的 value
    if (lex.isMember("forms") && lex["forms"].isArray()) {
        Json::Value forms(Json::arrayValue);
        for (const auto &form : lex["forms"]) {
            if (form["representations"].isMember("en") &&
                form["representations"]["en"].isMember("value")) {
                forms.append(form["representations"]["en"]["value"]);
            }
        }
        simple["forms"] = forms;
    }

    // 提取所有英文释义（senses）：遍历 senses 数组，提取每个 sense 中 "glosses" 的 "en" 的 value
    if (lex.isMember("senses") && lex["senses"].isArray()) {
        Json::Value senses(Json::arrayValue);
        for (const auto &sense : lex["senses"]) {
            if (sense["glosses"].isMember("en") &&
                sense["glosses"]["en"].isMember("value")) {
                senses.append(sense["glosses"]["en"]["value"]);
            }
        }
        simple["senses"] = senses;
    }

    // 构造综合描述字段 combined_text：将 lemma、所有 form 和所有 sense 拼接
    std::string combined_text;
    if (simple.isMember("lemma"))
        combined_text += simple["lemma"].asString();
    if (simple.isMember("forms") && simple["forms"].isArray()) {
        for (const auto &f : simple["forms"])
            combined_text += " " + f.asString();
    }
    if (simple.isMember("senses") && simple["senses"].isArray()) {
        for (const auto &s : simple["senses"])
            combined_text += " " + s.asString();
    }
    simple["combined_text"] = combined_text;

    // URL：如果原数据中没有 url 字段，则构造 Wikidata 词条页面 URL（例如：https://www.wikidata.org/wiki/L4）
    if (lex.isMember("url"))
        simple["url"] = lex["url"];
    else if (lex.isMember("id"))
        simple["url"] = "https://www.wikidata.org/wiki/Lexeme:" + lex["id"].asString();

    return simple;
}

int main() {
    // 输入文件（每行一个 JSON 对象）
    const std::string inputFile = "./data/wikidata-20250101-lexemes.json";
    // 输出文件（简化后的详细 JSON 数据）
    const std::string outputFile = "./data/simplified_lexemes.json";

    std::ifstream in(inputFile);
    if (!in.is_open()) {
        std::cerr << "无法打开输入文件: " << inputFile << std::endl;
        return 1;
    }
    std::ofstream out(outputFile);
    if (!out.is_open()) {
        std::cerr << "无法创建输出文件: " << outputFile << std::endl;
        return 1;
    }

    std::string line;
    int count = 0;
    int kept = 0;
    Json::Value simplifiedArray(Json::arrayValue); // 用于存储所有简化后的 JSON 对象

    while (std::getline(in, line)) {
        if (line.empty())
            continue;

        Json::Value lex;
        Json::CharReaderBuilder builder;
        std::istringstream iss(line);
        std::string errs;
        if (!Json::parseFromStream(builder, iss, &lex, &errs)) {
            std::cerr << "JSON 解析错误: " << errs << std::endl;
            continue;
        }
        count++;
        Json::Value simple = ProcessLexeme(lex);
        // 如果返回空对象，则说明该词条不包含英文信息，跳过
        if (simple.isNull())
            continue;

        simplifiedArray.append(simple); // 将简化后的对象添加到数组中
        kept++;
        if (count % 1000 == 0) {
            std::cout << "已处理 " << count << " 个词条，保留 " << kept << " 个英文词条" << std::endl;
        }
    }

    // 将数组以 JSON 格式写入输出文件
    Json::StreamWriterBuilder writerBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
    writer->write(simplifiedArray, &out);

    in.close();
    out.close();
    std::cout << "完成，总共处理 " << count << " 个词条，保留 " << kept << " 个英文词条，输出文件：" << outputFile << std::endl;
    return 0;
}