# 树

当前树类型都是按键比较的二叉搜索树，区别在于是否自动维持平衡，以及节点中保存的平衡状态。

## 三种实现

- `StandardCon::BinaryTree<T>` 只维护二叉搜索树的左右子链接，不旋转或重平衡。操作成本为 O(h)，其中 `h` 是树高；顺序插入可形成高度为 n 的链，查找、插入和删除最坏退化为 O(n)。
- `StandardCon::BalanceTree<K, V>` 是 AVL 树。节点保存父子链接、高度和平衡因子；插入和删除后沿祖先路径更新高度，并按需要执行单旋或双旋。树高保持 O(log n)，查找、插入和删除为 O(log n)。
- `StandardCon::RedBlackTree<K, V, Extractor, Compare>` 为节点保存红/黑颜色、父子链接；插入和删除后通过重新着色与旋转恢复红黑性质。节点高度维持 O(log n)，有序操作为 O(log n)。

```cpp
#include <Wan.hpp>
#include <string>

int main()
{
    StandardCon::BinaryTree<int> Plain;
    Plain.Push(5);
    Plain.Push(3);
    Plain.Push(7);

    StandardCon::BalanceTree<int, std::string> Avl;
    Avl.Push(StandardCon::Pair<int, std::string>{5, "five"});

    StandardCon::TreeSet<int> Ordered;
    Ordered.Push(3);
    Ordered.Push(1);
    return 0;
}
```

## 遍历、删除与失效

树迭代器沿节点链接执行中序或反向中序遍历，因此结果按比较器排序。AVL 旋转和红黑树旋转只改节点链接，不移动节点对象；插入与重平衡不会使其他节点迭代器失效。删除只使被删节点上的迭代器和引用失效。复制会逐节点建立独立结构；移动或交换会转移根节点和比较器状态。

`BinaryTree` 不保证平衡，不能把其复杂度按 AVL 或红黑树估算。三种树都不提供内部并发同步；并发修改或读写须由调用方同步。
