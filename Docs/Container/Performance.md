# 容器性能

这些容器是独立实现，不能因接口相似就假定其复杂度与标准容器相同。

| 容器 | 常见操作 | 最坏情况与取舍 |
|---|---|---|
| `Vector<T>` | 索引 O(1)、尾部追加均摊 O(1)、遍历 O(n) | 头部插入与删除 O(n)；扩容使迭代器失效 |
| `List<T>` | 两端插入/删除和取大小均为 O(1) | 查找和按索引遍历 O(n)；每个元素单独分配节点 |
| `Queue<T>` | 两端入队/出队 O(1) | 仅能访问两端 |
| `HashMap` / `HashSet` | 查找/插入/删除期望 O(1) | 冲突严重时最坏 O(n)；桶会预留空间 |
| `TreeMap` / `TreeSet` | 有序操作 O(log n) | 节点分配与指针访问开销高于连续存储 |
| `BalanceTree<K,V>` | AVL 操作 O(log n) | 节点分配；有序遍历依赖指针 |
| `RedBlackTree` | 有序操作 O(log n) | 节点分配；平衡操作有额外常数开销 |
| `BinaryTree<T>` | 仅平衡时为 O(log n) | 有序插入可退化为高度及操作均为 O(n) |

`CustomLibrariesBenchmark` 为可选目标，不属于默认构建和 CTest：

```powershell
cmake --preset default -DCustomLibrariesBuildBenchmarks=ON
cmake --build build --target CustomLibrariesBenchmark --parallel
build\Benchmarks\CustomLibrariesBenchmark.exe > build\BenchmarkResults.csv
```

程序先预热，再采样五次，并向标准输出写入以纳秒为单位的中位数 CSV。负载包含升序、降序、固定种子乱序，以及 1k 元素时的常量哈希键。对照对象是 STL、Boost 1.89 flat 容器，以及 64 KiB TCP 回环下的 `Reliable` 与直接 Asio。冲突和未排序的 `flat_map` 用例规模较小，因为它们刻意覆盖平方级最坏情况。

结果受编译器、构建模式、机器负载和依赖版本影响；比较改动时须保持这些条件一致。基准不是正确性测试，也不设通过时间阈值。
