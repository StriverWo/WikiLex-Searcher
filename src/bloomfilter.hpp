#include <vector>
#include <functional>
#include <bitset>
#include <iostream>

class BloomFilter {
public:
    BloomFilter(size_t size, size_t hash_count) 
        : bitset(size), hash_count(hash_count) {}

    // 添加元素到布隆过滤器
    void add(const std::string &element) {
        for (size_t i = 0; i < hash_count; ++i) {
            size_t hash = hash_function(i, element);
            bitset[hash % bitset.size()] = true;
        }
    }

    // 检查元素是否在布隆过滤器中
    bool contains(const std::string &element) {
        for (size_t i = 0; i < hash_count; ++i) {
            size_t hash = hash_function(i, element);
            if (!bitset[hash % bitset.size()]) {
                return false;
            }
        }
        return true;
    }

private:
    // 自定义哈希函数，使用不同的种子来生成多个哈希值
    size_t hash_function(size_t i, const std::string &element) {
        std::hash<std::string> hash_fn;
        return hash_fn(element) ^ (i + 0x9e3779b9 + (hash_fn(element) >> 6));
    }

    std::bitset<1000000> bitset; // 位数组，大小可以调整
    size_t hash_count; // 哈希函数的数量
};
