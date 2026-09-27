# 容器概览

`Wan/Container/Container.hpp` 聚合项目容器头文件，公开便捷命名空间为 `StandardCon`。

| 类别 | 公开类型 |
|---|---|
| 顺序容器 | `Vector`、`List`、`String` |
| 适配器 | `Queue`、`Stack`、`PriorityQueue` |
| 有序容器 | `TreeMap`、`TreeSet`、`BinaryTree`、`BalanceTree`、`RedBlackTree` |
| 哈希 | `HashMap`、`HashSet`、`HashTable`、`HashFunction` |
| 概率结构 | `BitSet`、`BloomFilter` |
| 所有权 | `Pointer::SmartPtr`、`Pointer::UniquePtr`、`Pointer::SharedPtr`、`Pointer::WeakPtr` |

容器没有内部并发同步；并发访问须由调用方同步。
