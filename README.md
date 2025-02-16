# WikiLex-Searcher


## 一.数据准备与清洗
### 1. 数据准备
#### （1）下载Wikidata语料库
下载网址：https://www.wikidata.org/wiki/Wikidata:Database_download/zh 。
我下载的是wikidata-20250101-lexemes.json.gz，然后将其放入data目录下，进行解压： 

```
gunzip wikidata-20250101-lexemes.json.gz
```
如果是以.tar.gz或者.tar.tgz结尾的压缩文件，可以通过tar命令解压：
```
tar  xxx.tar.gz
```
这样在data目录下将生成原始的Wikidata语料库，名为wikidata-20250101-lexemes.json。它是一个JSON格式的文件，每一行代表一个JSON对象，解析时可以逐行读取。  
Wikidata原始的JSON的结构信息可以参见官网：https://doc.wikimedia.org/Wikibase/master/php/docs_topics_json.html。

至此原始的语料库准备完毕。

#### （2）数据清洗简化
因为原始的JSON内容复杂，字段众多，而且对于本项目而言只是作为一个英文的词条搜索引擎，所以里面的内容需要大幅度简化，去除不关注的信息。
简化逻辑主要是：

##### a. 打开原始物料文件，逐行读取JSON对象
##### b. 解析每个JSON对象，提取详细信息。这里需要预先安装jsoncpp：
```
sudo apt-get install -y libjsoncpp-dev
```
##### c. 保留英文的词条信息
词条太多了，本项目只针对英文词条。
##### d. 同时构建词条对应的url和combined_text信息
前者用于前端访问时可以直接跳转，后者用于后续结合深度学习模型进行文本向量化以构建向量索引。  
**这部分代码位于src/simplify_lexemes.cpp中**
构建并编译本项目后，可以直接调用相应的可执行文件来获得清洗简化后的JSON文件。执行后会在data目录下生成simplified_lexemes.json：
```
./build/simplify_lexemes
```


