import subprocess 
from sentence_transformers import SentenceTransformer
import numpy as np
import json

def main():
    # 加载 Sentence - BERT 模型
    model = SentenceTransformer('./model/sentence-bert/all-MiniLM-L6-v2')
    
    # 启动 C++ 程序（请确保路径正确）
    cpp_process = subprocess.Popen(
        ['./lemdebug'], 
        stdin=subprocess.PIPE, 
        stdout=subprocess.PIPE, 
        stderr=subprocess.PIPE,
        text=True
    )
    
     # 读取并显示 C++ 程序的初始输出信息
    print("初始化输出信息：")
    while True:
        line = cpp_process.stdout.readline()
        if not line or line.strip() == "###INITEND###":
            break
        print(line, end='')

    while True:
        input_text = input("请输入搜索文本: ")
        # 生成句子向量
        embedding = model.encode(input_text)
        # 对向量归一化（若模型输出未归一化）
        norm = np.linalg.norm(embedding)
        if norm > 0:
            embedding = embedding / norm
        
        # 将向量转换为逗号分隔的字符串
        embedding_str = ','.join(f"{x:.6f}" for x in embedding)
        
        # 将输入文本和向量打包为 JSON 对象
        data = {
            "input_text": input_text,
            "embedding": embedding_str
        }
        data_str = json.dumps(data)
        
        try:
            # 传递 JSON 字符串给 C++ 程序（每次传一行）
            cpp_process.stdin.write(data_str + "\n")
            cpp_process.stdin.flush()

            full_output = ""
            end_delimiter = "###END###"
            while True:
                line = cpp_process.stdout.readline()
                if end_delimiter in line:
                    break
                full_output += line

            print("搜索结果:")
            print(full_output)
        except Exception as e:
            print(f"发生错误: {e}")
            break

    # 关闭 C++ 进程
    cpp_process.stdin.close()
    cpp_process.wait()

if __name__ == "__main__":
    main()