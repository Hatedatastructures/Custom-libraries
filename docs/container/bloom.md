## 🎯 布隆过滤器 `bloom_filter_container`

#### **复杂度分析**

* `push`, `pop`：`O(log n)`。
* `top`, `empty`, `size`：`O(1)`。
* 构造（初始化列表）：`O(n log n)`。
* 拷贝/移动构造：`O(n)`/`O(1)`。

#### **边界条件和错误处理**

* 操作前检查 `empty()`。
* 分配失败抛 `std::bad_alloc`。
* 确保 `compare` 满足弱排序。

#### **注意事项**

* 非线程安全。
* 无迭代接口。

#### **使用示例**

```cpp
using namespace template_container;
int main() 
{
    // 1. 构造函数示例
    std::cout << "=== 构造函数示例 ===\n";
    
    // 默认构造（大顶堆）
    queue_adapter::priority_queue<int> pq1;
    std::cout << "pq1 (默认构造，空队列): size=" << pq1.size() << std::endl;
    
    // 初始化列表构造
    queue_adapter::priority_queue<int> pq2 = {3, 1, 4, 1, 5, 9};
    std::cout << "pq2 (初始化列表构造): top=" << pq2.top() << std::endl;
    
    // 2. 优先队列操作示例
    std::cout << std::endl << "=== 优先队列操作示例 ===" <<std::endl;

    
    pq1.push(10);
    pq1.push(20);
    pq1.push(15);
    
    std::cout << "pq1 入队后: top=" << pq1.top() << std::endl;
    
    pq1.pop();
    std::cout << "pq1 出队后: top=" << pq1.top() << std::endl;
    
    // 3. 自定义比较器示例（小顶堆）
    std::cout << "\n=== 自定义比较器示例 ===\n";
    
    using MinHeap = queue_adapter::priority_queue<int, 
        template_container::imitation_functions::greater<int>>;
    
    MinHeap minHeap;
    minHeap.push(5);
    minHeap.push(3);
    minHeap.push(7);
    
    std::cout << "小顶堆: top=" << minHeap.top() << std::endl;
    
    return 0;
}
```
> **引用**：头文件 `queue_adapter` 命名空间
---
## 🎯 布隆过滤器 `bloom_filter_container`

### 🎯 布隆过滤器概览

布隆过滤器是一种**空间高效的概率性数据结构**，专门用于快速判断元素是否可能存在于集合中。它通过多个哈希函数将元素映射到位数组中，具有以下特点：

- ✅ **无假阴性**：如果返回 "不存在"，则元素一定不在集合中
- ⚠️ **可能假阳性**：如果返回 "存在"，元素可能在集合中（存在误判率）
- 🚀 **高效性能**：插入和查询都是 O(1) 时间复杂度
- 💾 **空间紧凑**：相比哈希表节省大量内存空间

### 🏗️ 类定义与结构

```cpp
namespace bloom_filter_container 
{
    template <typename bloom_filter_type_value,
              typename bloom_filter_hash_functor = template_container::algorithm::hash_algorithm::hash_function<bloom_filter_type_value>>
    class bloom_filter 
    {
    private:
        bloom_filter_hash_functor hash_functions_object;  // 多哈希函数对象
        using bit_set = template_container::base_class_container::bit_set;
        bit_set instance_bit_set;                         // 底层位集合
        size_t _capacity;                                 // 位数组容量
        
    public:
        // 构造函数
        bloom_filter();                                   // 默认容量 1000
        explicit bloom_filter(const size_t& capacity);   // 自定义容量
        
        // 核心操作
        void set(const bloom_filter_type_value& value);  // 插入元素
        bool test(const bloom_filter_type_value& value); // 测试元素
        
        // 状态查询
        [[nodiscard]] size_t size() const;               // 已置位数量
        [[nodiscard]] size_t capacity() const;           // 总容量
    };
}
```

### 🔧 核心特性

#### 📊 布隆过滤器特性表

| 特性 | 说明 | 优势 |
|------|------|------|
| **概率性判断** | 可能存在假阳性，无假阴性 | 快速排除不存在的元素 |
| **多哈希映射** | 使用 3 个独立哈希函数 | 降低冲突概率 |
| **位级存储** | 基于位数组的紧凑存储 | 极高的空间效率 |
| **无删除操作** | 不支持元素删除 | 避免误删和复杂性 |

#### 🎯 哈希函数策略

布隆过滤器使用三种独立的哈希算法：

| 哈希函数 | 算法类型 | 特点 |
|----------|----------|------|
| **hash_sdmmhash** | SDBM 哈希 | 简单快速，分布均匀 |
| **hash_djbhash** | DJB 哈希 | 经典字符串哈希算法 |
| **hash_pjwhash** | PJW 哈希 | 适合标识符哈希 |

### ⚡ 性能分析

#### 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| **插入 (set)** | O(1) | 3 次哈希 + 3 次位操作 |
| **查询 (test)** | O(1) | 3 次哈希 + 3 次位检查 |
| **构造** | O(capacity/32) | 位数组初始化 |

#### 空间复杂度

| 存储类型 | 空间复杂度 | 说明 |
|----------|------------|------|
| **位数组** | O(capacity/32) | 每 32 位使用一个 int |
| **哈希对象** | O(1) | 常量空间开销 |

### 🔍 误判率分析

布隆过滤器的误判率可以通过以下公式估算：

```
误判率 ≈ (1 - e^(-k*n/m))^k

其中：
- k = 哈希函数数量 (本实现中 k=3)
- n = 插入元素数量
- m = 位数组大小 (capacity)
```

#### 📈 误判率对比表

| 容量/元素比 | 误判率 (k=3) | 适用场景 |
|-------------|-------------|----------|
| **10:1** | ~2.5% | 高精度要求 |
| **8:1** | ~5% | 一般应用 |
| **5:1** | ~15% | 粗略过滤 |

### 💡 完整使用示例

```cpp
#include "Foundation.hpp"
using namespace template_container;

int main() 
{
    // 1. 创建布隆过滤器
    std::cout << "=== 布隆过滤器示例 ===\n";
    
    // 创建容量为 10000 的布隆过滤器
    bloom_filter_container::bloom_filter<std::string> bf(10000);
    
    // 2. 插入元素
    std::cout << "\n插入元素...\n";
    std::vector<std::string> fruits = {"apple", "banana", "cherry", "date", "elderberry"};
    
    for (const auto& fruit : fruits) 
    {
        bf.set(fruit);
        std::cout << "插入: " << fruit << std::endl;
    }
    
    // 3. 测试存在的元素
    std::cout << "\n=== 测试已插入元素 ===\n";
    for (const auto& fruit : fruits) 
    {
        bool exists = bf.test(fruit);
        std::cout << "测试 '" << fruit << "': " 
                  << (exists ? "可能存在 ✓" : "一定不存在 ✗") << std::endl;
    }
    
    // 4. 测试不存在的元素
    std::cout << "\n=== 测试未插入元素 ===\n";
    std::vector<std::string> test_fruits = {"grape", "kiwi", "mango", "orange"};
    
    for (const auto& fruit : test_fruits) 
    {
        bool exists = bf.test(fruit);
        std::cout << "测试 '" << fruit << "': " 
                  << (exists ? "可能存在 (误判)" : "一定不存在 ✓") << std::endl;
    }
    
    // 5. 显示统计信息
    std::cout << "\n=== 统计信息 ===\n";
    std::cout << "布隆过滤器容量: " << bf.capacity() << " 位\n";
    std::cout << "已使用位数: " << bf.size() << " 位\n";
    std::cout << "位使用率: " << (double)bf.size() / bf.capacity() * 100 << "%\n";
    
    // 6. 性能测试示例
    std::cout << "\n=== 性能测试 ===\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    // 大量插入测试
    for (int i = 0; i < 10000; ++i) 
    {
        bf.set("test_" + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "插入 10000 个元素耗时: " << duration.count() << " 微秒\n";
    std::cout << "平均每次插入: " << (double)duration.count() / 10000 << " 微秒\n";
    
    return 0;
}
```

### 🎯 应用场景

#### ✅ 适用场景

| 场景 | 说明 | 优势 |
|------|------|------|
| **缓存预过滤** | 避免无效的缓存查询 | 减少 I/O 操作 |
| **数据库查询优化** | 快速排除不存在的记录 | 提升查询性能 |
| **网络爬虫去重** | 判断 URL 是否已访问 | 节省存储空间 |
| **垃圾邮件过滤** | 快速识别已知垃圾邮件 | 高效预筛选 |
| **分布式系统** | 减少网络通信开销 | 降低系统负载 |

#### ❌ 不适用场景

| 场景 | 原因 | 替代方案 |
|------|------|----------|
| **需要精确判断** | 存在误判率 | 使用哈希表 |
| **需要删除操作** | 不支持删除 | 使用计数布隆过滤器 |
| **小数据集** | 空间优势不明显 | 直接使用 set |

### 🚨 注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **线程安全** | 非线程安全实现 | 多线程环境需外部同步 |
| **容量规划** | 容量影响误判率 | 根据预期元素数量合理设置 |
| **哈希质量** | 哈希函数质量影响性能 | 使用提供的默认哈希函数 |
| **误判处理** | 需要处理假阳性情况 | 在应用层进行二次验证 |
| **内存占用** | 位数组占用连续内存 | 大容量时注意内存分配 |

> **引用**：头文件 `template_container::bloom_filter_container` 命名空间

---

