# WikiLex-Searcher


## 一. 数据预处理
### 1. 数据准备
#### （1）下载Wikidata语料库
下载网址：https://www.wikidata.org/wiki/Wikidata:Database_download/zh 。
我下载的是wikidata-20250101-lexemes.json.gz，然后将其放入data目录下，进行解压： 

```
gunzip wikidata-20250101-lexemes.json.gz
```
如果是以.tar.gz或者.tar.tgz结尾的压缩文件，可以通过tar命令解压：
```
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
### 2. 文本向量化
#### (1) 下载模型
**本项目以sentence-Bert模型为例：**   
下载方法有多种：
a. 本项目中的 model/sentence-bert/ 路径下含有加载sentence-transformers/all-MiniLM-L6-v2模型的Python脚本文件： loadmodel.py。通过如下命令可以自动在该目录下下载该模型:
```
cd model/sentence-bert/
python3 loadmodel.py    // 如果执行过程出错，根据提示信息pip install对应的包即可。
```
b. 如果a提供的脚本执行过程中在连接远程服务器的时候，执行出错，比如说无法连接到 huggingface.co 该网址，可以尝试 ping 一下该网址。然后再次尝试执行脚本。  
c. 如果该脚本始终无法执行（执行出错），可以直接从 https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/tree/main 官方网址下载该模型。  
这里是解决服务器无法连接huggingface下载模型的博客，可以参考：https://blog.csdn.net/a61022706/article/details/134887159 。
#### (2) 预先向量化
鉴于实时向量化及其缓慢，本项目采用事先对简化后的JSON文件选择相应的combined_text字段进行向量化。 以此来为后续构建向量索引进行预备向量数据。  
在该项目中在的 model/sentence-bert/ 路径下仍然提供对简化后的 simplified_lexemes.json 进行向量化的Python脚本：vectorize.py. 通过执行如下命令便可以在data目录下生成向量化后的文本文件lexeme_vectors.txt。  
```
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

