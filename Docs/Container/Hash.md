# 哈希

哈希函数把键映射为 `uint64_t`，哈希容器再用桶数取模得到桶索引。哈希相同不代表键相等：容器仍会在对应桶中逐节点比较键，因此冲突会增加查找工作量。

## 默认哈希器

`StandardCon::HashImitationFunctions` 对可转换为 `uint64_t` 的标量直接转换；`StandardCon::String` 则逐字节计算 64 位 FNV-1a。默认哈希器不支持 `std::string`，需为它提供自定义哈希器。字符串算法只用于容器索引，不提供抗碰撞或密码学安全保证。

`StandardCon::HashFunction<T>` 包装基础哈希器，提供 `HashSdmmhash`、`HashBkdrhash`、`HashDjbhash`、`HashAphash` 和 `HashPjwhash`。这些变体分别对基础值乘以常数或作简单移位组合；它们是容器分布用的轻量变换，不是独立的密码学哈希。`BloomFilter` 使用 SDMM、DJB 和 PJW 三种结果生成位索引。

```cpp
#include <Wan.hpp>

int main()
{
    StandardCon::HashImitationFunctions BaseHash;
    const auto H = BaseHash(StandardCon::String{"key"});

    StandardCon::HashFunction<StandardCon::String> Hashes;
    const auto A = Hashes.HashSdmmhash(StandardCon::String{"key"});
    const auto B = Hashes.HashDjbhash(StandardCon::String{"key"});
    (void)H;
    (void)A;
    (void)B;
    return 0;
}
```

## HashTable 的碰撞处理

`HashTable<K, V, Extractor, Hash>` 用桶头指针数组和单向桶链处理碰撞。每个节点还链接到独立的插入顺序链，所以遍历按插入顺序进行，而不按桶顺序进行。负载系数以十分之一为单位，默认 `7` 表示最多约 0.7 个元素/桶；达到阈值时桶数从 10 开始按倍数增长。

重哈希先为所有现有节点计算新桶索引，再重接桶链；节点本身及插入顺序链不重建，因此节点地址、元素引用和既有迭代器保持有效。查找、插入和删除平均为 O(1)，严重冲突时最坏为 O(n)；遍历为 O(n)。完整关联容器接口见[关联容器说明](Associative.md)。

哈希值可用于索引或非密码学完整性检查，不能用于密码认证、签名或安全令牌。
