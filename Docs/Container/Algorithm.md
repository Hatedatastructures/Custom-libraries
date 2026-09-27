# 算法

`StandardCon::Algorithm` 提供少量基于迭代器的通用算法，不拥有或分配元素存储。

## Copy、Find 与 Swap

- `Copy(begin, end, out)` 按顺序读取源范围，并用赋值写入目标范围，返回写入后的目标尾后迭代器。复杂度 O(n)；目标必须已有至少 n 个可写位置，源目标区间不得重叠。
- `Find(begin, end, value)` 逐项使用 `operator==` 比较，返回第一个匹配迭代器；未找到时返回 `end`。复杂度最坏 O(n)，不要求排序。
- `Swap(a, b)` 转发到 `std::swap`；异常规格与所选 `std::swap` 重载一致。

```cpp
#include <Wan.hpp>

int main()
{
    int Source[] = {1, 2, 3};
    int Target[] = {0, 0, 0};
    auto OutputEnd = StandardCon::Algorithm::Copy(
        Source, Source + 3, Target);
    auto Found = StandardCon::Algorithm::Find(
        Target, OutputEnd, 2);
    return Found != OutputEnd ? 0 : 1;
}
```

`Copy` 不会扩展目标容器，也不会检测迭代器是否来自有效范围；调用方负责提供合法的半开区间和足够的目标空间。
