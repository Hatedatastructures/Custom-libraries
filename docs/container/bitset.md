## 位图
## `bit_set`
### 类概述
* `bit_set` 是一个高效的位集合实现，使用整数数组存储二进制位，支持位的设置、重置和查询操作。该类通过位运算实现紧凑的数据存储，
每个 `int` 可存储 32 位，适合处理大量布尔值的场景（如位图索引、数据标记等）。
#### **类及其函数定义**

```cpp
class bit_set 
{
    template_container::vector_container::vector<int> vector_bit_set;
    size_t _size;
public:
    bit_set();
    explicit bit_set(const size_t& new_capacity);
    void resize(const size_t& new_capacity);
    bit_set(const bit_set& other);
    bit_set& operator=(const bit_set& other);
    void set(const size_t& value);
    void reset(const size_t& value);
    [[nodiscard]] size_t size() const;
    bool test(const size_t& value);
};
```

#### **作用描述**\*\*

1. **构造与析构**

   * `bit_set()`：创建空位集，内部不分配额外空间。
   * `bit_set(new_capacity)`：根据 `new_capacity`（可表示的最大值），分配 `((new_capacity/32)+1)` 个 `int` 元素，每个 `int` 管理 32 位。
   * `resize(new_capacity)`：同构造体，根据新容量重置内部存储并清空已设置位。
   * 默认拷贝/赋值：深复制底层数组和已设置计数。

2. **设置位 (`set`)**

   * 计算索引：`block = value/32`，`bit = value%32`。
   * 在对应 `vector_bit_set[block]` 上做按位或：`|= (1 << bit)`，将该位置 `1`。
   * 更新 `_size++`，代表总设置位数加一。

3. **重置位 (`reset`)**

   * 同理计算 `block` 和 `bit`。
   * 在对应整型上做按位与取反：`&= ~(1 << bit)`，将该位清 `0`。
   * 更新 `_size--`，代表总设置位数减一。

4. **测试位 (`test`)**

   * 若 `_size==0`，快速返回 `false`（无任何位被设置）。
   * 计算 `block` 和 `bit` 后，返回 `vector_bit_set[block] & (1 << bit) != 0`。

5. **查询**

   * `size()`：返回当前被设置的位数。

#### **返回值说明**

* `set(value)` → `void`：将指定 `value` 的位标记为 1，并增加总计数。
* `reset(value)` → `void`：将指定 `value` 的位清零，并减少总计数。
* `test(value)` → `bool`：检查指定 `value` 的位是否被设置。
* `size()` → `size_t`：返回已设置位的数量。
* 构造、赋值 → 无返回。

#### **内部原理剖析**

* **底层存储**：
  使用 `vector<int>` 将位划分为若干个 `int`（32 位）块。索引计算为整除 32 得到块下标，取模 32 得到块内位偏移。

* **位操作**：

  * `|`：位或，将目标位设置为 1，不影响其他位。
  * `& ~`：先对掩码取反生成全 1 除目标位为 0，然后与原值按位与，清除目标位，不影响其他位。

* **大小维护**：
  `_size` 追踪目前被设置的位总数，仅在 `set` 与 `reset` 中更新；不检查重复 `set` 可能导致计数不准。

#### **复杂度分析**

* **时间**：

  * `set`/`reset`/`test`：`O(1)`。
  * `resize`：O(n)（`n` 为新数组长度）。

* **空间**：
  `O(cap/32)` 个 `int`，其中 `cap` 为可表示的最大值。

#### **边界条件和错误处理**

* **越界访问**：
  未对 `value` 范围检查，用户须确保 `value < capacity*32`。
* **重复 `set`**：
  未检查是否已设置，相同 `value` 重复调用会导致 `_size` 溢出计数不准确。
* **`reset` 未测试**：
  若位未被设置即调用 `reset`，`_size` 会错误减少。

#### **注意事项**

* **线程安全**：
  非线程安全，需外部同步。
* **计数一致性**：
  建议在 `set` 前 `test`，在 `reset` 前检查 `test`，以保证 `_size` 正确。
* **容量管理**：
  `resize` 会清空所有设置，慎用。
> **引用** 头文件 `base_class_container` 命名空间
---
