# 关联容器

关联容器按键保存唯一元素，提供有序或哈希查找。键相等/排序由比较器或哈希适配器决定；修改已存入容器的键会破坏索引不变量，因此映射迭代器暴露的 `First` 为只读，映射值 `Second` 仍可修改。

## 有序容器

`TreeMap<K, V>` 和 `TreeSet<T>` 底层使用 `RedBlackTree`。每个元素单独保存在带父、左、右链接及颜色的节点中；插入和删除通过旋转与颜色调整维持红黑平衡。中序迭代得到比较器定义的顺序，查找、插入、删除为 O(log n)。

有序映射的插入返回 `StandardCon::Pair<Iterator, bool>`：迭代器指向现有或新元素，布尔值表示是否插入成功。有序集合也用这个结果报告重复键。

## 哈希容器

`HashMap<K, V>` 和 `HashSet<T>` 底层使用 `HashTable`。哈希值对桶数取模定位桶，冲突节点在桶内单向链接；节点另有一组前后链接，按插入顺序遍历。表达到负载阈值时桶数扩为两倍，重哈希只重算桶并重接节点，不重建节点。

哈希容器的 `Push` 返回 `bool`，键已存在时不插入。查找、插入、删除期望 O(1)，严重冲突时最坏 O(n)；遍历为 O(n)。`ChangeLoadFactor(n)` 接受 1 到 10 的整数，例如 `7` 表示最大装载比例 0.7。

```cpp
#include <Wan.hpp>

int main()
{
    StandardCon::TreeMap<int, int> Ordered;
    auto [TreeIt, WasInserted] = Ordered.Push(
        StandardCon::Pair<int, int>{1, 20});
    TreeIt->Second = 21;

    StandardCon::HashMap<int, int> Hashed;
    const bool Added = Hashed.Push(StandardCon::Pair<int, int>{1, 20});
    return WasInserted && Added ? 0 : 1;
}
```

## 迭代器与失效

- 红黑树旋转只改节点链接，不移动节点；插入不会使其他节点迭代器失效，删除只使被删节点的迭代器和引用失效。
- 哈希插入和重哈希保留既有节点地址及插入顺序链，因此既有迭代器和引用继续指向原节点；删除只使被删节点的迭代器和引用失效。
- 复制会创建独立节点；移动和交换转移容器存储，原迭代器随节点转移到接收该存储的对象。

哈希迭代顺序是插入顺序，不是键排序。两个容器都不提供内部并发同步；并发访问须由调用方同步。
