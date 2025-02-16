from transformers import AutoTokenizer, AutoModel
from sentence_transformers import SentenceTransformer
import os

# 模型名称
model_name = "sentence-transformers/all-MiniLM-L6-v2"

# 定义保存模型的目录
save_directory = "./all-MiniLM-L6-v2"

# 创建保存目录（如果不存在）
if not os.path.exists(save_directory):
    os.makedirs(save_directory)

try:
    # 使用 transformers 库下载分词器和模型
    tokenizer = AutoTokenizer.from_pretrained(model_name)
    model = AutoModel.from_pretrained(model_name)

    # 保存分词器和模型到指定目录
    tokenizer.save_pretrained(save_directory)
    model.save_pretrained(save_directory)

    print(f"模型 {model_name} 已成功下载并保存到 {save_directory}。")

except Exception as e:
    print(f"下载模型时出现错误: {e}")

# 如果你还想使用 sentence-transformers 库的方式下载和保存
try:
    st_model = SentenceTransformer(model_name)
    st_model.save(save_directory)
    print(f"使用 sentence-transformers 库，模型 {model_name} 已成功下载并保存到 {save_directory}。")
except Exception as e:
    print(f"使用 sentence-transformers 库下载模型时出现错误: {e}")