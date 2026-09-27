# 队列与栈适配器

这些类型限制访问顺序，并把存储工作委托给底层序列容器。

## Queue 与 Stack

`StandardCon::Queue<T>` 默认用 `StandardCon::List<T>` 保存元素。`Push` 在队尾追加，`Pop` 从队首删除，因此遵循先进先出。`Front`、`Back` 和 `Size` 直接转发到底层链表；空队列的 `Pop` 不执行操作，访问首尾元素则抛出 `std::out_of_range`。

`StandardCon::Stack<T>` 默认用 `StandardCon::Vector<T>` 保存元素。`Push` 和 `Pop` 都操作尾部，`Top` 返回尾元素，因此遵循后进先出。空栈的 `Pop` 不执行操作，`Top` 抛出 `std::out_of_range`。

```cpp
#include <Wan.hpp>

int main()
{
    StandardCon::Queue<int> Queue;
    Queue.Push(1);
    Queue.Push(2);
    const int First = Queue.Front(); // 1
    Queue.Pop();

    StandardCon::Stack<int> Stack;
    Stack.Push(3);
    Stack.Push(4);
    const int Last = Stack.Top();   // 4
    Stack.Pop();
    return First == 1 && Last == 4 ? 0 : 1;
}
```

Queue 两端操作为 O(1)，但每个元素单独分配链表节点；Stack 的 `Push` 均摊 O(1)，扩容时 O(n)，`Pop` 为 O(1)。两个适配器允许通过模板参数替换底层序列，但替代类型必须提供它们所调用的接口。

## PriorityQueue

`StandardCon::PriorityQueue<T, Compare>` 用 `StandardCon::Vector<T>` 保存完全二叉堆。数组索引 `i` 的父节点为 `(i-1)/2`，左右子节点为 `2i+1` 和 `2i+2`。插入先追加到数组末尾，再向上交换以恢复堆序；删除堆顶时把末元素移到根，再向下交换。

默认比较器 `StandardCon::Less<T>` 使较大的元素位于 `Top()`；可通过模板参数提供默认构造的比较器类型来改变顺序。当前 `Push()` 接受 `const T&` 并复制元素。`Top()` 为 O(1)，堆调整使 `Pop()` 为 O(log n)，`Push()` 摊还 O(log n)；底层向量扩容时单次追加为 O(n)，并使其元素引用失效。空队列 `Top()` 抛出 `std::out_of_range`，`Pop()` 不执行操作。

```cpp
#include <Wan.hpp>

int main()
{
    StandardCon::PriorityQueue<int> Priorities;
    Priorities.Push(3);
    Priorities.Push(8);
    Priorities.Push(5);
    const int Highest = Priorities.Top(); // 默认比较器下为 8
    return Highest == 8 ? 0 : 1;
}
```
