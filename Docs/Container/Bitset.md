# BitSet

`StandardCon::BitSet` 用连续的 32 位无符号整数保存位状态。索引 `i` 映射到第 `i / 32` 个字和其中第 `i % 32` 位；设置、清除和检查单个位均为 O(1)。

```cpp
#include <Wan.hpp>

int main()
{
    StandardCon::BitSet Bits{100};
    Bits.Set(7);
    if (Bits.Test(7))
    {
        Bits.Reset(7);
    }
    return 0;
}
```

`Size()` 维护当前置位数，而不是容量或最近一次索引。重复 `Set` 不重复计数，清除已清零位不使计数下溢。`Set`、`Reset` 对越界索引抛出 `std::out_of_range`；`Test` 对越界索引返回 `false`。

`Resize(n)` 改变可访问的位数并清空全部位，同时将置位计数归零。位存储由 `StandardCon::Vector<uint32_t>` 管理；当前实现分配 `floor(n / 32) + 1` 个字，因此恰好 32 位边界也会多保留一个字。位图不提供内部并发同步。
