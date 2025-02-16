#!/usr/bin/env python3
import sys
import json
import numpy as np
from sentence_transformers import SentenceTransformer

def main():
    # 从命令行参数或标准输入读取文本
    if len(sys.argv) > 1:
        input_text = sys.argv[1]
    else:
        input_text = sys.stdin.read().strip()

    # 加载 Sentence‑BERT 模型（这里以 all‑MiniLM‑L6‑v2 为例）
    model = SentenceTransformer('./model/sentence-bert/all-MiniLM-L6-v2')
    embedding = model.encode(input_text)
    # 归一化向量（如果需要）
    norm = np.linalg.norm(embedding)
    if norm > 0:
        embedding = embedding / norm

    # 将向量转换为逗号分隔的字符串输出
    embedding_str = ",".join(f"{x:.6f}" for x in embedding)
    # 也可以输出 JSON 格式
    # print(json.dumps({"embedding": embedding_str}))
    print(embedding_str)

if __name__ == "__main__":
    main()
