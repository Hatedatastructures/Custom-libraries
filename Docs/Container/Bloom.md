# BloomFilter

`StandardCon::BloomFilter<T, Hash>` 是固定 bit 容量的概率成员过滤器，适合先快速排除“不可能存在”的查询。它不保存原始元素，只记录元素映射到的位，因此不能枚举元素，也不支持删除。

## 如何映射元素

默认哈希器依次计算 SDMM、DJB 和 PJW 三个哈希结果，并对 bit 容量取模得到三个索引。`Set(value)` 将这三位全部置为 1；`Test(value)` 只有在三位都为 1 时才返回可能存在。若哈希器对同一元素始终返回相同结果，则已插入元素不会被判为不存在；未插入元素可能因位碰撞产生误报。

```cpp
#include <Wan.hpp>

int main()
{
    StandardCon::BloomFilter<StandardCon::String> Filter{10'000};
    Filter.Set(StandardCon::String{"key"});
    const bool MaybePresent = Filter.Test(StandardCon::String{"key"});
    return MaybePresent ? 0 : 1;
}
```

默认哈希支持可转为 `uint64_t` 的标量和 `StandardCon::String`。其他类型须提供默认构造的哈希器，并实现 `HashSdmmhash`、`HashDjbhash`、`HashPjwhash` 三个返回可取模整数的成员函数。标准库 `std::string` 不属于默认哈希器支持的类型，须传入自定义哈希器。

构造容量必须大于零，创建后固定；增加元素数会逐渐增加误报机会。`Capacity()` 是 bit 容量；`Size()` 是当前置位数，不是插入元素数。位集合由 `BitSet` 管理，过滤器本身不提供并发同步。
