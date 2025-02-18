# WikiLex-Searcher

## 构建和编译
```Bash
cd WikiLex-Searcher
mkdir build && cd build
cmake ..
make
```
## 运行前准备
请先阅读后续的步骤，完整数据预处理和相关库安装后再执行。
## 测试和运行
编译后会自动在build目录下生成可执行文件，在build下执行下述命令即可建立搜素引擎服务端：
```Bash
./lemserver
```
这样服务端就建立了，打开浏览器输入：127.0.0.1:8080 即可连接该搜索引擎。  
此外经过构建编译后，还会在build下生成系列有关数据预处理和测试的程序，这些源代码都可以在scr目录下找到。

## 一. 数据预处理
### 1. 数据准备
#### （1）下载Wikidata语料库
下载网址：https://www.wikidata.org/wiki/Wikidata:Database_download/zh 。
我下载的是wikidata-20250101-lexemes.json.gz，然后将其放入data目录下，进行解压： 

```Bash
gunzip wikidata-20250101-lexemes.json.gz
```
如果是以.tar.gz或者.tar.tgz结尾的压缩文件，可以通过tar命令解压：
```Bash
tar -xzvf xxx.tar.gz
```
这样在data目录下将生成原始的Wikidata语料库，名为wikidata-20250101-lexemes.json。它是一个JSON格式的文件，每一行代表一个JSON对象，解析时可以逐行读取。  
Wikidata原始的JSON的结构信息可以参见官网：https://doc.wikimedia.org/Wikibase/master/php/docs_topics_json.html。

至此原始的语料库准备完毕。

#### （2）数据清洗简化
因为原始的JSON内容复杂，字段众多，而且对于本项目而言只是作为一个英文的词条搜索引擎，所以里面的内容需要大幅度简化，去除不关注的信息。
简化逻辑主要是：

##### a. 打开原始物料文件，逐行读取JSON对象
##### b. 解析每个JSON对象，提取详细信息。这里需要预先安装jsoncpp：
```Bash
sudo apt-get install -y libjsoncpp-dev
```
##### c. 保留英文的词条信息
词条太多了，本项目只针对英文词条。
##### d. 同时构建词条对应的url和combined_text信息
前者用于前端访问时可以直接跳转，后者用于后续结合深度学习模型进行文本向量化以构建向量索引。  
**这部分代码位于src/simplify_lexemes.cpp中**  
构建并编译本项目后，可以直接调用相应的可执行文件来获得清洗简化后的JSON文件。执行后会在data目录下生成simplified_lexemes.json：
```Bash
./build/simplify_lexemes
```
### 2. 文本向量化
#### (1) 下载模型
**本项目以sentence-Bert模型为例：**   
下载方法有多种：
a. 本项目中的 model/sentence-bert/ 路径下含有加载sentence-transformers/all-MiniLM-L6-v2模型的Python脚本文件： loadmodel.py。通过如下命令可以自动在该目录下下载该模型:
```Bash
cd model/sentence-bert/
python3 loadmodel.py    // 如果执行过程出错，根据提示信息pip install对应的包即可。
```
b. 如果a提供的脚本执行过程中在连接远程服务器的时候，执行出错，比如说无法连接到 huggingface.co 该网址，可以尝试 ping 一下该网址。然后再次尝试执行脚本。  
c. 如果该脚本始终无法执行（执行出错），可以直接从 https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/tree/main 官方网址下载该模型。  
这里是解决服务器无法连接huggingface下载模型的博客，可以参考：https://blog.csdn.net/a61022706/article/details/134887159 。
#### (2) 预先向量化
鉴于实时向量化及其缓慢，本项目采用事先对简化后的JSON文件选择相应的combined_text字段进行向量化。 以此来为后续构建向量索引进行预备向量数据。  
在该项目中在的 model/sentence-bert/ 路径下仍然提供对简化后的 simplified_lexemes.json 进行向量化的Python脚本：vectorize.py. 通过执行如下命令便可以在data目录下生成向量化后的文本文件lexeme_vectors.txt。  
```Bash
cd model/sentence-bert/
python3 vectorize.py    // 如果执行过程出错，根据提示信息pip install对应的包即可。
```

**至此，数据预处理工作已完成，data目录下将会有后续构建正排、倒排和向量索引的数据文件：lexeme_vectors.txt  simplified_lexemes.json。**

## 二. 索引构建
### 1. 正排索引构建
以哈希表 std::unordered_map<uint64_t, DocInfo> 作为正排索引存储介质：每个id对应一条词条文本信息。
### 2. 倒排索引构建
仍然以哈希表std::unordered_map<std::string, InvertedList> 作为倒排索引存储介质：每个字符串对应一条倒排拉链，拉链上可以有多条词条文本信息。
我们使用cppjieba分词，对词条标题和forms进行分词，然后构建倒排索引。需要注意的是，对于jieba分词而言，可能会不恰当的包含空格或者标点符号，这点需要额外处理。
### 3. 向量索引构建
事实上，完成正排、倒排索引的构建后，就已经可以进行文本匹配了，但是很多时候，我们搜索时并不一定是想获得确切的词条信息，比如我们搜索文本 "for what reason?" 这个文本搜索可能得不到我们预想的词条，那么此时构建向量索引重要性就体现出来了，根据**语义相似度**来进行搜索，恰好能满足我们预期的结果。
### 4. 附注
在大多数场景的使用中，当我们搜索单个词的时候事实上我们更关注文本匹配搜索，而如果是搜索某个句子的时候更关注句意与词条的匹配度。这点我们在后续搜索时介绍。

## 三. HNSWLib库
本项目中所需的库都在lib文件夹下。

### 1. HNSW 算法
HNSW（Hierarchical Navigable Small World）是一种**基于图的近似最近邻搜索算法（Approximate Nearest Neighbor，ANN）**。它具有超快的搜索速度和出色的召回率。
#### （1）NSW算法
可导航小世界（Navigable Small World，NSW）将候选集合C构建成可导航小世界图，利用基于贪婪搜索的kNN算法从图中查找与查询点query距离最近的k个顶点。  
![image](https://github.com/user-attachments/assets/c48599f7-1d61-465c-885c-025cd8da3a09)  
**朴素贪婪搜索：** 
朴素贪婪搜索算法 给定查询点 query 和搜索开始点 entry-point ，在图G(V,E)中查找与q最近的点的贪婪搜索算法如下：

    从搜索开始点进行查找，即当前顶点为搜索开始点；
    计算查询点q与当前顶点邻节点的距离，从中选择距离最小的节点；
    如果查询点q与所选结点之间的距离小于查询点q与当前顶点之间的距离，则将所选结点作为新的顶点，递归查找；
    否则，当前顶点即为与查询点q最近的点。
**但是！！！** 通过贪婪的朴素搜索得到的最近点时局部最优点，并不是全局最优点。  

**近似kNN搜索：** 

    执行 m 次贪婪搜索，在每次搜索过程中随机选择一个进入点遍历，最后从 m 词搜索中选出距离查询点最近的 k 个结果；
    随着搜索次数的增加，找不到最近点的概率指数衰减，因此在不改变图结构的情况下能提高搜索准确性。
**数据插入算法：** 通过逐个插入元素方式进行构图，对于每个待插入元素，通过近似kNN算法查找与其最近的f个元素，然后与其相连。
```
# 数据插入算法 
# new_object -> 待插入元素 
# f: 待插入元素连接的最近邻个数 
# w: 搜索次数 
Nearest_Neighbor_Insert(object: new_object, integer: f, integer: w)  
# 找到当前图中与待插入元素new_object最近的f个元素 
SET[object]: neighbors←k-NNSearch (new_object, w, f);  
# 与待插入元素连接 
for (I←0; i<f; i++) do  
    neighbors [i].connect(new_object);  
    new_object.connect(neighbors [i]); 
```
#### （2）概率跳表
参考博客：https://www.jianshu.com/p/9d8296562806 。  
简单来说跳表通过构建多个有序链表和分层指针来提高查询和插入效率，时间复杂度可达对数级别。
#### （3）HNSW
参考文章：https://zhuanlan.zhihu.com/p/673027535 。  
分层的可导航小世界（Hierarchical Navigable Small World，HNSW）是一种基于图的数据结构，借鉴跳表的分层思想，它将节点划分成不同层级，贪婪的遍历来自上层的元素，直到达到局部最小值，然后切换到下一层，以上一层中的局部最小值作为新元素重新开始遍历，直到遍历完最底一层。  
下图简单展示了通过多层结构的HNSW图的搜索过程：  
![image](https://github.com/user-attachments/assets/c4449225-ebc5-4e26-83b7-e0a4854afb70)

***插入算法*** 
INSERT(hnsw, q, M, Mmax, efConstruction, mL) ：将新元素 q 插入hnsw图中，M是每个元素需要与其他元素建立的连接数，Mmax是每个元素的最大连接数，efConstruction是动态候选集合的大小，mL是选择q层数的标准化因子。
```
INSERT(hnsw, q, M, Mmax, efConstruction, mL) 
/** 
 * 输入 
 * hnsw：q插入的目标图 
 * q：插入的新元素 
 * M：每个点需要与图中其他的点建立的连接数 
 * Mmax：最大的连接数，超过则需要进行缩减（shrink） 
 * efConstruction：动态候选元素集合大小 
 * mL：选择q的层数时用到的标准化因子 
 */ 
Input:  
multilayer graph hnsw,  
new element q,  
number of established connections M,  
maximum number of connections for each element per layer Mmax,  
size of the dynamic candidate list efConstruction,  
normalization factor for level generation mL 
/** 
 * 输出：新的hnsw图 
 */ 
Output: update hnsw inserting element q 
 
W ← ∅  // W：现在发现的最近邻元素集合 
// 查询进入点 
ep ← get enter point for hnsw 
L ← level of ep 
/** 
 * unif(0..1)是取0到1之中的随机数 
 * 根据mL获取新元素q的层数l 
 */ 
l ← ⌊-ln(unif(0..1))∙mL⌋ 
/** 
 * 自顶层向q的层数l逼近搜索，一直到l+1,每层寻找当前层与q最近邻的1个点 
 */ 
for lc ← L … l+1 
    // 以ep为入点，在lc层查找距离ep最近的1个元素 
    W ← SEARCH_LAYER(q, ep, ef=1, lc) 
    ep ← get the nearest element from W to q 
 
// 自l层向底层逼近搜索,每层寻找当前层q最近邻的efConstruction个点赋值到集合W 
for lc ← min(L, l) … 0 
    // 以ep为入点，在lc层查找距离q最近的efConstruction个元素 
    W ← SEARCH_LAYER(q, ep, efConstruction, lc) 
    // 在W中选择q最近邻的M个点作为neighbors双向连接起来 
    neighbors ← SELECT_NEIGHBORS(q, W, M, lc) 
    add bidirectional connectionts from neighbors to q at layer lc 
    // 检查每个neighbors的连接数，如果大于Mmax，则需要缩减连接到最近邻的Mmax个 
    for each e ∈ neighbors 
        eConn ← neighbourhood(e) at layer lc 
        if │eConn│ > Mmax 
            eNewConn ← SELECT_NEIGHBORS(e, eConn, Mmax, lc) 
            set neighbourhood(e) at layer lc to eNewConn 
    ep ← W 
if l > L 
    set enter point for hnsw to q 
```

插入时类似于跳表插入那样需要确定冲哪个层级开始往下插入。通过：l ← ⌊-ln(unif(0..1))∙mL 来进行确定。其中ml是工程实验确定的经验值，通常取值为 1/ln(M) ，其中 M 是每个节点在每一层的平均连接数。  
有了起始的插入层级l，那么就可以往下层开始逐层插入该节点。但在此之前，需要确定每层距离该插入节点最近的节点集合。这个工作由SEARCH LAYER(q, ep, ef, lc)完成。

***搜素当前层最近邻*** 
SEARCH LAYER(q, ep, ef, lc) ：以ep为进入点，在第 lc 层查找距离 q 最近邻的 ef 个元素。
```
 SEARCH_LAYER(q, ep, ef, lc) 
/** 
 * 输入 
 * q：插入的新元素 
 * ep：进入点 enter point 
 * ef：需要返回的近邻数量 
 * lc：层数 
 */ 
Input:  
query element q,  
enter point ep,  
number of nearest to q elements to return ef,  
layer number lc 
/** 
 * 输出：q的ef个最近邻 
 */ 
Output: ef closest neighbors to q 
 
// 将进入点放入候选集合 
v ← ep  // v：设置访问过的元素 visited elements 
C ← ep  // C：设置候选元素 candidates 
W ← ep  // W：现在发现的最近邻元素集合 
// 遍历每一个候选元素，包括遍历过程中不断加入的元素 
while │C│ > 0 
    // 取出候选集中q的最近邻c 
    c ← extract nearest element from C to q 
    // 取出W中q的最远点f 
    f ← get furthest element from W to q 
    // 当前次迭代不改变 近邻查找结果 
    if distance(c, q) > distance(f, q)  
        break 
    /** 
     * 当c比f距离q更近时，则将c的每一个邻居e都进行遍历 
     * 如果邻居e比w中距离q最远的f要更接近q，那就把e加入到W和候选元素C中 
     * 由此会不断地遍历图，直至达到局部最佳状态，c的所有邻居没有距离更近的了或者所有邻居都已经被遍历了 
     */ 
    for each e ∈ neighbourhood(c) at layer lc 
        if e ∉ v 
            v ← v ⋃ e 
            f ← get furthest element from W to q 
            // 判断邻节点是否会改变 近邻查找结果 
            if distance(e, q) < distance(f, q) or │W│ < ef 
                C ← C ⋃ e 
                W ← W ⋃ e 
                // 保证返回的数目不大于ef 
                if │W│ > ef 
                    remove furthest element from W to q 
return W 
```
***截取集合最近邻*** 
在 HNSW 中，SEARCH-LAYER(q, ep, ef, lc) 返回 efConstruction 个最近邻点，我们知道 efConstruction 的值是大于 M （每个节点相邻节点个数）的，那么怎么在efConstruction个点中选择 M 个来进行双向连接呢？有两种算法可供选择：  
**简单选择算法 SELECT-NEIGHBORS-SIMPLE(q, C, M)**
```
SELECT_NEIGHBORS_SIMPLE(q, C, M) 
/** 
 * 输入 
 * q：查询的点 
 * C：候选元素集合 
 * M：需要返回的数目 
 */ 
Input:  
base element q,  
candidate elements C,  
number of neighbors to return M 
/** 
 * 输出：M个q的最近邻 
 */ 
Output: M nearest elements to q 
 
return M nearest elements from C to q 
```
**启发式选择算法 SELECT-NEIGHBORS-HEURISTIC(q, C, M, lc,...)**
```
SELECT_NEIGHBORS_HEURISTIC(q, C, M, lc, extendCandidates, keepPrunedConnections) 
/** 
 * 输入 
 * q：查询的点 
 * C：候选元素集合，集合内元素数量大于M 
 * M：需要返回的数目 
 * lc：层数 
 * extendCandidates：指示是否扩展候选列表的标志 
 * keepPrunedConnections：指示是否添加丢弃元素的标志 
 */ 
Input:  
base element q,  
candidate elements C,  
number of neighbors to return M,  
layer number lc,  
flag indicating whether or not to extend candidate list extendCandidates,  
flag indicating whether or not to add discarded elements keepPrunedConnections 
/** 
 * 输出：探索得到M个元素 
 */ 
Output: M elements selected by the heuristic 
 
R ← ∅ // 记录结果 
W ← C  // W：候选元素的队列 
if extendCandidates  // 通过邻居来扩充候选元素 
    for each e ∈ C 
        for each e_adj ∈ neighbourhood(e) at layer lc 
            if e_adj ∉ W 
                W ← W ⋃ e_adj 
Wd ← ∅  // 丢弃的候选元素的队列 
/** 
 * 遍历候选集合队列 
 * 候选元素队列不为空且结果数量少于M时，在W中选择q最近邻e 
 * 如果e和q的距离比e和R（已建立连接点的集合）中的元素的距离更小，就把e加入到R中，否则就把e加入Wd（丢弃） 
 * 可以理解成：如果R中存在点r，使distance(q,e)<distance(q,r)，则加入点e到R 
 */ 
while │W│ > 0 and │R│ < M 
    // 在W中选择q最近邻e 
    e ← extract nearest element from W to q 
    // 如果e和q的距离比e和R中的元素的距离更小，就把e加入到R中，否则就把e加入Wd（丢弃） 
    if e is closer to q compared to any element from R 
        R ← R ⋃ e 
    else 
        Wd ← Wd ⋃ e 
/** 
 * 如果设置keepPrunedConnections为true，且R不满足M个，那就在丢弃队列中挑选最近邻填满R为M个 
 */ 
if keepPrunedConnections 
    while │Wd│ > 0 and │R│ < M 
        R ← R ⋃ extract nearest element from Wd to q 
return R 
```

***KNN查询算法*** 
KNN SEARCH(hnsw,q,K,ef) : 在 hnsw 索引中查询距离 q 最近邻的 K 个元素。
```
K-NN-SEARCH(hnsw, q, K, ef) 
/** 
 * 输入 
 * hnsw：q插入的目标图 
 * q：查询元素 
 * K：返回的近邻数量 
 * ef：动态候选元素集合大小 
 */ 
Input:  
multilayer graph hnsw, query element q,  
number of nearest neighbors to return K,  
size of the dynamic candidate list ef 
/** 
 * 输出：q的K个最近邻元素 
 */ 
Output: K nearest elements to q 
 
W ← ∅  // W：现在发现的最近邻元素集合 
ep ← get enter point for hnsw 
L ← level of ep 
/** 
 * 自顶层向倒数第2层逼近搜索,每层寻找当前层q最近邻的1个点赋值到集合W 
 * 取W中最接近q的点作为底层的入口点，以便时搜索的时间成本最低 
 */ 
for lc ← L … 1 
    W ← SEARCH_LAYER(q, ep, ef=1, lc) 
    ep ← get nearest element from W to q 
// 从上一层得到的ep点开始搜索底层获得ef个q的最近邻 
W ← SEARCH_LAYER(q, ep, ef, lc=0) 
// 从ef个选择K个最近邻 
return K nearest elements from W to q 
```
### 2. HNSWLib库
在lib下：https://github.com/nmslib/hnswlib 。

## 四. 文本向量化
### 1. Transformer
参考链接：https://blog.csdn.net/weixin_42475060/article/details/121101749 ， https://www.zhihu.com/tardis/zm/art/600773858 。  
这里只是我的学习记录笔记。  
#### （1）Transformer整体结构
机器翻译中，Transformer可以将一种语言翻译成另一种语言：  
![image](https://github.com/user-attachments/assets/0b73e641-ee32-4c10-81e4-f1719a6999d4)

事实上，它内部由若干个编码器和解码器组成：
![image](https://github.com/user-attachments/assets/0dcb48d1-ee43-43d8-8c51-9ebe10258be6)

继续将Encoder和Decoder拆开，得到完整的结构：  
![image](https://github.com/user-attachments/assets/c7339cc4-51fe-45ff-b3e3-32110d42d3b7)

输入包含两个单词时，Transformer的整体结构如下：
![image](https://github.com/user-attachments/assets/c687eff0-5753-4c9f-a748-c143a3a0131d)

#### （2）流程示例
##### a. 获取输入句子的每个单词的向量表示x，x由单词的Embedding和单词位置的Embedding相加得到：  
![image](https://github.com/user-attachments/assets/5365e1e7-0ed8-49ee-82e8-a52270668044)
##### b. 单词向量矩阵传入Encoder模块，经过N个后得到句子所有单词的编码信息矩阵C：  
![image](https://github.com/user-attachments/assets/7b1c759e-aa86-4beb-949c-06bc28e77833)  
##### c. Encoder输出的编码信息矩阵C传递到Decoder中，Decoder会根据当前翻译过的词汇 1~i 翻译下一个单词 i+1 ：  
![image](https://github.com/user-attachments/assets/0dedc6d3-3510-4b4f-bf3a-fad605dda43f)  
如上图所示，首个 Decoder 输入一个开始符 "<Begin>" ，预测第一个单词输出为 "I"；然后然后输入翻译开始符 "<Begin>" 和单词 "I"，预测第二个单词，输出为"am"，以此类推。

#### （3）输入表示
Transformer中单词的输入表示由**单词Embedding**和**位置Embedding**相加得到。  
![image](https://github.com/user-attachments/assets/cad5a526-4aee-4991-b140-d215badf65d9)  
##### a. 单词Embedding
可由Word2vec等模型预训练得到，可以在Transformer中加入Embedding层。
##### b. 位置Embedding
Transformer 中除了单词的Embedding，还需要使用位置Embedding 表示单词出现在句子中的位置。**因为 Transformer不采用RNN结构，而是使用全局信息，不能利用单词的顺序信息，而这部分信息对于NLP来说非常重要。**（这句话不太明白？）所以Transformer中使用位置Embedding保存单词在序列中的相对或绝对位置。

#### （3）Multi-Head Attention（多头注意力机制）
![image](https://github.com/user-attachments/assets/ff9faba9-64fa-4029-8d4e-c63ddfcf180b)  
由多个**Self-Attention**组成，结构如下：  
![image](https://github.com/user-attachments/assets/f1c7dd0b-d54d-41d7-8587-10a7b91f6397)  

##### a. Self-Attention结构
![image](https://github.com/user-attachments/assets/f98d57f9-87cb-4b0a-aea1-da828ca40fe7)  
上图是Self-Attention结构，最下面是 Q (查询)、K (键值)、V (值)矩阵，是通过输入矩阵 X 和权重矩阵 WQ, WK, WV相乘得到的。  
![image](https://github.com/user-attachments/assets/a8bdd746-8570-4a10-ba70-cf86f869da0f)  
得到Q、K、V之后就可以计算得出Self-Attention的输出，如下图所示：  
![image](https://github.com/user-attachments/assets/4c6929ae-b9f2-411f-8970-07ffa9e4730a)

##### b.Multi-Head Attention输出
![image](https://github.com/user-attachments/assets/e0857866-6327-4922-b21c-5610a1313392)  
Multi-Head Attention包含多个Self-Attention层，输入X传入h各不同的Self-Attention中，计算得到h个输出矩阵：  
![image](https://github.com/user-attachments/assets/36b9acf3-9133-4c13-b203-e8401fc17fde)  
得到h
个输出矩阵Z1-Zh后，Multi-Atttention将他们拼接到一起得到 Multi-Head Attention最终的输出矩阵Z。

#### （4）编码器Encoder结构
![image](https://github.com/user-attachments/assets/355096fc-c7b4-42ca-b85a-c3663c0f9a0c)  
上图中N表示Encoder的个数，其中还有 Add & Norm 和 Feed Forward模块：
##### a. Add & Norm（残差连接和层归一化）：
参考博客：https://blog.csdn.net/2401_85377976/article/details/141423008 。

**Add（残差连接）**

残差网络： 残差网络（ResNet）通过残差连接，使得输入信息可以直接跨越一层或多层，与后续层的输出相加，从而缓解了深层网络中的梯度消失和梯度爆炸问题，使得网络可以扩展到更深的层数。
梯度消失：在深层网络中，梯度需要通过多个层次进行反向传播。根据链式法则，梯度在传播过程中会不断相乘，当层数较多时，梯度值可能会以指数形式衰减并趋近于零，导致梯度消失。  
梯度爆炸：深层网络中的梯度在传播过程中也可能因链式法则的连乘效应而迅速增长，甚至呈指数级增长，导致网络参数更新过大，网络不稳定。

**Norm（层归一化）**

归一化（Normalization）： 一种数据预处理技术，旨在通过线性或非线性变换，将输入数据或神经网络层的输出数据映射到一个特定的数值范围或分布之中。这一处理过程对于提升神经网络训练过程的稳定性、加速收敛速度以及最终提高模型性能至关重要。  
在神经网络中，常见的归一化方法包括 ：  
批归一化（Batch Normalization）： 它通过在每个批次中对输入数据进行规范化，使其均值为0、方差为1，从而加速网络的收敛过程，降低网络对初始化和学习率的敏感性，同时也有一定的正则化效果。  
层归一化（Layer Normalization）： 与批归一化不同，它在每层中对所有样本的输出进行规范化，而不是对每个批次进行规范化。层归一化在处理序列数据等不适合批处理的情况下，可以作为替代方案使用。  
组归一化（Group Normalization）： 组归一化是一种介于批归一化和层归一化之间的方法，它将输入数据分成多个小组，然后对每个小组内的样本进行归一化，从而减小小组之间的相关性，提高网络的学习能力。

##### b. Feed Forward：


#### （5）解码器Decoder结构
![image](https://github.com/user-attachments/assets/e2ba47fc-7b85-44a3-a318-f8a55050ad9a)  
##### a. 第一个Multi-Head Attention
前面已经说过，Decoder的时候，需要根据之前翻译的单词，预测当前最有可能翻译的单词，那么此时就需要**Decoder在预测第 i 个输出的时候，需要将第 i+1 之后的单词掩盖住，Mask的作用正在于此：  
![image](https://github.com/user-attachments/assets/00867131-cf1b-4875-81f0-a98d19e14b8a)  
这样就得到一个Mask Self-Attention的输出矩阵Zi，然后和Encoder类似，通过Multi-Head Attention拼接多个输出Zi然后计算得到第一个Multi-Head Attention的输出Z，其维度与输入 X 的一样。

##### b. 第二个Multi-Head Attention
Decoder的第二个Multi-Head Attention变化不大， 主要的区别在于其中Self-Attention的 K, V 矩阵不是使用上一个Multi-Head Attention的输出，而是使用Encoder的编码信息矩阵 C 计算的。根据Encoder的输出 C 计算得到 K, V ，根据上一个Multi-Head Attention的输出 Z 计算 Q。

#### （6）Linear & Softmax
待深入研究。。。  
![image](https://github.com/user-attachments/assets/541f9d67-8d24-4493-8a32-9aa6496cbefb)  
Output如图中所示，首先经过一次线性变换（线性变换层是一个简单的全连接神经网络，它可以把解码组件产生的向量投射到一个比它大得多的，被称为对数几率的向量里），然后Softmax得到输出的概率分布（softmax层会把向量变成概率），然后通过词典，输出概率最大的对应的单词作为我们的预测输出。

### 2. BERT模型
参考博客：https://blog.csdn.net/star_nwe/article/details/143227601 和 https://blog.51cto.com/u_16163510/12673828 。  
**BERT（Bidirectional Encoder Representations from Transformers**是由 Google 于2018年提出的一种预训练语言模型。其核心特点有：  
#### a. 双向上下文编码
BERT 利用 Transformer 结构的自注意力机制对文本进行双向编码，这意味着在生成每个词的表示时，会同时考虑该词左右两侧的上下文信息，从而捕捉更丰富的语义。  
#### b. 预训练任务
**Masked Language Model (MLM)**：在输入文本中随机遮蔽一些词，然后让模型预测被遮蔽词的原始值，从而让模型学习上下文关联。
**Next Sentence Prediction (NSP)**：训练模型判断两个句子是否连续，这有助于理解句子间的关系。

**不过，BERT 的输出是针对每个词的上下文嵌入，对于直接计算句子间相似度时，需要额外的处理（比如取 [CLS] 表示或平均池化）来获得句子级向量；而这种方式在语义相似度计算上可能并不是最优的。**

### 3. Sentence-BERT模型
Sentence‑BERT（SBERT） 是对原始 BERT 模型的一种改进，主要目的是生成高质量的句子级嵌入。其主要特点包括：  
#### a. 专为句子嵌入设计
SBERT 在 BERT 的基础上添加了 siamese 网络结构或 triplet 网络，通过微调使模型能够直接输出语义上聚合的句子向量。这使得生成的句子向量在比较相似度、检索任务等方面表现更好。  
#### b. 高效计算相似度
由于 SBERT 生成的是固定维度的句子级向量，可以直接用余弦相似度或欧氏距离来度量句子之间的语义相似性，不需要复杂的后处理。  
#### c. 广泛应用于语义搜索和聚类
SBERT 常用于文本相似度计算、语义搜索、聚类等任务，可以在大规模语料中快速检索出语义相似的句子。

***在WikiLex-Searcher中就使用了 Sentence-BERT 模型将文本数据转换为语义向量，进而实现了高效的语义搜索。***

## 五. 文本相似性度量
有很多比如说欧式距离、余弦相似度、Jacard相似度等。WikiLex-Searcher中使用余弦相似度来确定语义相似程度。  
**注意**：HNSWLib中好像并没有直接计算余弦相似度的，但是我们使用的文本向量都是经过归一化后的，所以直接可以利用内积来计算余弦相似度。

## 六. 搜索设计
源代码主要在src/lemsearcher.hpp中。  
### (1)查询预处理与向量化
当用户提交查询时，系统会：使用分词工具 jieba 对查询文本进行分词，从而提取出查询关键词；同时将整个查询文本输入 Sentence‑BERT生成查询向量。
***C++中并没有提供很好的接口加载模以进行文本向量化操作，所以该项目中使用 popen 函数来调用一个预先准备好的 python 脚本来进行对查询文本的向量化操作。***  
#### popen详解：
popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行指定的命令来开启一个进程。  
**工作原理**  

    FILE* popen(const char *command, const char *mode);
    1. 创建子进程：调用 popen 时，当前进程会启动一个子进程，该子进程通常通过调用 fork（在 UNIX-like 系统中）实现，然后在子进程中调用 execl 或类似的函数来启动 shell 并执行指定的命令。
    2. 建立管道：父进程和子进程之间会建立一个单向通信管道：
            如果模式为 "r"，管道的写端连接到子进程的标准输出，父进程可以读取子进程产生的数据。
            如果模式为 "w"，管道的读端连接到子进程的标准输入，父进程可以向子进程写入数据。
            返回文件流：父进程通过返回的 FILE* 流可以使用标准的输入/输出函数（如 fgets, fread, fprintf 等）与子进程进行通信。
    3. 关闭管道：通信结束后，父进程应该调用 pclose 函数来关闭文件流，同时等待子进程结束，并返回子进程的退出状态。

如下部分代码就是本项目中在 C++ 代码中调用 python 脚本的方法:  
```cpp
std::string exec_python_vectorize(const std::string& input_text) {
    // 构造调用命令，此处假设 python3 可执行文件在 PATH 中
    // 你可以将输入文本作为命令行参数传递，但要注意转义空格和特殊字符
    std::string command = "python3 /path/to/lemvectorize.py \"" + input_text + "\"";

    // 打开管道读取脚本输出
    FILE* pipe = popen(command.c_str(), "r");    // 参数"r"表示读取
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    // 去除末尾换行符
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}
```


### (2) 倒排搜索
根据查询关键词，在倒排索引中查找对应的词条，并根据关键词权重计算每个词条的倒排得分。该过程侧重于文本匹配，能捕捉用户输入与词条中显式出现的词语之间的联系。

### (3) 向量搜索
利用 HNSWlib 的向量索引，根据查询向量寻找语义上最相似的词条。该方法可以发现即使文本表述不同，但语义相近的词条，从而提升搜索的智能性。

### (4) 搜索结果融合
将倒排搜索和向量搜索得到的结果进行融合。融合策略可以采用并集、加权平均等方式，将两个渠道的得分合并，得到一个综合得分，进而对结果排序并返回。这样既兼顾了关键词匹配的精度，又利用了语义向量搜索的鲁棒性。

## 七. 服务接口
使用 cpp-httplib 构建 C++ 服务端。

## 八. 热词 & 搜索记录 & 用户注册登录
在本项目通过 Redis 保存热词和搜索的历史记录。  
**需要预先安装 Redis 和 hiredis**（这里只是个示例安装步骤）：  
```bash
# 安装 Redis
sudo apt update
sudo apt install redis-server
# 安装 hiredis（Redis C 客户端库）
git clone https://github.com/redis/hiredis.git
cd hiredis
make
sudo make install
```
这里可能会出现问题，在于使用 sudo make install 安装库文件后，需要更新动态链接器的缓存，让系统知道新库文件的位置：
```bash
sudo ldconfig
```
这样可以通过 hiredis 提供的相关接口去连接并使用 Redis。

### 1. 热词统计

统计搜索过程中对某个词条的搜索次数，将其作为score，然后与该词条信息一起组成 <score，Lex> 插入到 zset 中。  
zset 是有序的根据 score 进行的排名，那么通过 ZREVRANGE 命令便可以从zset中得到指定区间的成员，通过成员分数从高到低。  

底层使用 Redis 的 ZSet （有序集合）数据结构，在Redis中 zset 使用了两种不同的存储结构，分别是**ziplist（压缩列表）和skiplist（跳表），当zset 满足以下条件时使用压缩列表：
* 成员的数量小于128个；
* 每个member成员的字符串长度都小于64个字节。

当不满足压缩列表保存数据时，就会使用 skipList 结构来存储数据。  
**不论是使用跳表或者压缩列表，对于得到热词的这个场景，他们的搜索时间复杂度基本常熟级别的，因为他们都是有序的，而且热词一般是返回score最高的前面一些热词。**

Redis 这里有关的内容参考了：https://www.cnblogs.com/wan-ming-zhu/p/18079378 和 https://blog.csdn.net/qq_36608921/article/details/105551601。

#### (1) zipLists
![image](https://github.com/user-attachments/assets/698bf8d1-6949-4791-af5b-08ca5cc7becb)  
插入的时间复杂度是O(n)，但相比跳表节省空间。
#### (2) skipLists
插入时间复杂度是O(logn)，但相比压缩链表更占空间。

### 2. 搜索记录统计
本项目中每个用户在服务端拥有着不同的搜索记录，每个用户对应的搜索记录都使用Redis中的 List 数据结构进行存储。  
![image](https://github.com/user-attachments/assets/8e653a7b-07ee-4aa1-83f2-c504ed643661)  
![image](https://github.com/user-attachments/assets/2c15abcd-b136-442e-a831-da205ab84067)

### 3. 用户注册登录
提供用户注册登录功能，每个用户有自己的用户名和密码，在访问搜索引擎主页时需要事先登录，如果没有账号需要提前注册。

底层使用的是 Redis 中的 Hash 结构来存储用户信息（username和password）。  
Redis 6 中Hash内部有两种编码方式，Redis会根据实际情况自动选择合适的编码：  
* ziplist:
当 Hash 中的字段数量较少且每个字段和值的长度较短时，Redis 会使用 ziplist 作为内部编码。  
* hashtable:
 Hash 中的字段数量较多或者字段和值的长度较长时，Redis 会使用 hashtable 作为内部编码。  
* 性能特点：  
查找、插入和删除操作的时间复杂度：在使用 hashtable 编码时，Hash 的查找、插入和删除操作的平均时间复杂度为 ，这使得 Redis Hash 在处理大量数据时具有很高的性能。即使在使用 ziplist 编码时，对于小规模的数据，这些操作的性能也比较可观。  
内存使用效率：由于 ziplist 的紧凑结构，当 Hash 数据较小时，使用 ziplist 编码可以显著节省内存空间。但随着数据量的增加，切换到 hashtable 编码可以保证操作的性能。

##九. 集成 MySQL 与布隆过滤机制
首先需要事先安装MySQL，和 MySQL C++ 连接器。  
MySQL 的安装这里省略。  
Ubuntu 上MySQL 连接器的安装，执行：
```bash
sudo apt install libmysqlclient-dev
```
或者我的项目中安装 MySQL Connectot/C++
```bash
sudo apt install libmysqlcppconn-dev
```

* 在C++中使用MySQL示例：
```cpp
#include <iostream>
#include <mysql/mysql.h>

int main() {
    MYSQL *conn;
    conn = mysql_init(NULL);

    if (!mysql_real_connect(conn, "localhost", "myuser", "mypassword", "mydb", 0, NULL, 0)) {
        std::cerr << "MySQL Connection Failed: " << mysql_error(conn) << std::endl;
        return 1;
    }

    std::cout << "Connected to MySQL successfully!" << std::endl;
    mysql_close(conn);
    return 0;
}
```

本项目中，服务器端运行前需要事先在 MySQL 中创建一个用户名为user、密码为 FangFang123! 的用户，同时在此用户中，创建一个名为 wikilex 的数据库，如果需要自定义名字，则需要在 src/lemserver.cpp 中更改初始化mysql的一行代码，如下所示：
```cpp
MysqlUtil mysql("tcp://127.0.0.1:3306", "users", "FangFang", "wikilex");
```
下面是如何在 MySQL 中中创建一个名为 wikilex 的数据库的具体操作：
* 登录 MySQL
```Bash
mysql -u root -p
```
* 创建名为wikilex的数据库：
```sql
CREATE DATABASE wikilex;
```
* 创建用户名为：users， 密码为：'FangFang123!' 的用户，其中的 'localhost'表示用户只能从本地连接连接到Mysql服务器：
```sql
CREATE USER 'users'@'localhost' IDENTIFIED BY 'FangFang';
```
* 如果希望该用户可以从任何主机连接，可以将 'localhost' 替换为 '%'：
```sql
CREATE USER 'users'@'%' IDENTIFIED BY 'FangFang123!';
```
* 授予users用户对wikilex数据库的所有权限：
```sql
GRANT ALL PRIVILEGES ON wikilex.* TO 'users'@'localhost';
```
或者：
```sql
GRANT ALL PRIVILEGES ON wikilex.* TO 'users'@'%';
```
* 刷新权限：
```sql
FLUSH PRIVILEGES;
```
* 退出MySQL：完成上述操作后，便可以使用下述命令退出MySQL命令行了：
```sql
EXIT;
```
* 最后，检查是否创建成功：
```Bash
sudo mysql -u users -p -D wikilex
```
在输入密码 FangFang123! 之后进入到 wikilex 数据库中，说明用户创建和权限授予都成功。

### 1. 注册时用户信息持久化到 MySQL 中：

***注册新用户时同步该用户信息到 MySQL 中，并缓存到 Redis 中，并在 Redis 缓存中设置该用户信息过期时间***

### 2. 登录时用户信息首先从：
原先设计登录时是从redis中匹配用户信息密码，然后根据匹配结果来确定登录是否成功。**但是，对于用户信息而言这是很重要的信息，不容有失，所以最好的办法是以 MySQL 中数据为准，所以在登录时需要从 MySQL 中匹配用户信息。  
那么如果有不法分子拿着未知账号持续登录，那么很容易造成 MySQL 崩溃。  
有什么好的办法呢？ **布隆过滤器！！！**














