#!/usr/bin/env python3
import argparse
from sentence_transformers import SentenceTransformer
import numpy as np
import ijson

def main():
    parser = argparse.ArgumentParser(description="将简化后的 JSON 文件中每个词条的 combined_text 向量化，并输出到文本文件")
    parser.add_argument("--input", type=str, required=True, help="输入的 JSON 文件路径")
    parser.add_argument("--output", type=str, required=True, help="输出向量文件路径")
    parser.add_argument("--model", type=str, default="all-MiniLM-L6-v2", help="Sentence‑BERT 模型名称或路径")
    args = parser.parse_args()

    # 1. 加载 Sentence‑BERT 模型
    print("正在加载模型：", args.model)
    model = SentenceTransformer(args.model)

    # 2. 打开输出文件准备写入
    print("正在读取 JSON 文件：", args.input)
    processed_count = 0  # 用于记录已处理的词条数量
    with open(args.output, 'w', encoding='utf-8') as fout:
        # 使用 ijson 逐行解析 JSON 文件
        with open(args.input, 'r', encoding='utf-8') as fin:
            for entry in ijson.items(fin, 'item'):
                if entry.get("language", "").lower() != "en":
                    continue
                lexeme_id = entry.get("id", "")
                lemma = entry.get("lemma", "")
                combined_text = entry.get("combined_text", "")
                if not combined_text:
                    continue

                # 4. 使用模型生成向量
                embedding = model.encode(combined_text)
                # 将向量转换为字符串，保留6位小数，各维度以空格分隔
                embedding_str = " ".join(f"{x:.6f}" for x in embedding)
                # 构造输出行：id<TAB>lemma<TAB>embedding_str
                line = f"{lexeme_id}\t{lemma}\t{embedding_str}"

                # 5. 及时将向量化结果写入输出文件
                fout.write(line + '\n')

                processed_count += 1
                if processed_count % 100 == 0:  # 每处理 100 个词条输出一次进度信息
                    print(f"已处理 {processed_count} 个词条")

    print("向量化完成，结果已保存到：", args.output)

if __name__ == "__main__":
    main()