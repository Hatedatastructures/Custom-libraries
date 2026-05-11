## 📊 算法细节与性能分析

### 🌳 红黑树 vs AVL 树深度对比

#### 📈 性能特征对比

| 特性 | 红黑树 (RB Tree) | AVL 树 | 推荐场景 |
|------|------------------|--------|----------|
| **平衡严格度** | 松散平衡 | 严格平衡 | RB: 频繁插入删除 |
| **树高度** | ≤ 2log₂(n+1) | ≤ 1.44log₂(n+2) | AVL: 频繁查询 |
| **插入复杂度** | O(log n) | O(log n) | - |
| **删除复杂度** | O(log n) | O(log n) | - |
| **查询复杂度** | O(log n) | O(log n) | AVL 略优 |
| **旋转次数** | 插入≤2次，删除≤3次 | 插入≤2次，删除≤log n次 | RB 更稳定 |
| **内存开销** | 1 bit (颜色) | 2 bits (平衡因子) | RB 更节省 |

#### 🎯 选择策略

```cpp
// 场景分析
if (读操作 >> 写操作) {
    // 选择 AVL 树 - 查询性能更优
    // 适用：数据库索引、字典查找
    tree_container::avl_tree<Key, Value> container;
} else if (写操作频繁) {
    // 选择红黑树 - 插入删除更稳定
    // 适用：STL map/set、内存管理
    tree_container::rb_tree<Key, Value> container;
}
```

### 🔗 哈希表设计原理

#### ⚖️ 负载因子优化

| 负载因子 | 冲突概率 | 空间利用率 | 性能表现 | 适用场景 |
|----------|----------|------------|----------|----------|
| **0.5** | 很低 | 50% | 极快查询 | 高性能要求 |
| **0.7** | 低 | 70% | 快速查询 | **推荐默认值** |
| **0.9** | 中等 | 90% | 中等查询 | 内存敏感 |
| **1.0+** | 高 | >90% | 慢查询 | 不推荐 |

```cpp
// 动态调整负载因子
hash_map<std::string, int> map;
map.change_load_factor(0.6);  // 降低冲突率
map.change_load_factor(0.8);  // 提高空间利用率
```

#### 🪣 桶数量策略

| 桶数量类型 | 优势 | 劣势 | 使用场景 |
|------------|------|------|----------|
| **素数** | 哈希分布均匀 | 计算复杂 | 通用场景 |
| **2的幂** | 位运算快速 | 可能聚集 | 性能敏感 |

#### 🔄 冲突解决机制

```cpp
// 链式哈希 (Chaining) - 本库采用
struct HashNode {
    Key key;
    Value value;
    HashNode* next;  // 链表解决冲突
};

// 极端情况优化：链表转红黑树
if (chain_length > TREE_THRESHOLD) {
    convert_to_red_black_tree();  // 保证 O(log n) 性能
}
```

#### 🔄 再哈希 (Rehashing) 策略

| 触发条件 | 扩容倍数 | 时间复杂度 | 注意事项 |
|----------|----------|------------|----------|
| **负载因子 > 0.7** | 2倍 | O(n) | 可能引起卡顿 |
| **手动调用** | 自定义 | O(n) | 可控制时机 |

### 🔢 位集与布隆过滤器优化

#### 💾 位运算效率分析

| 操作 | 位集 bit_set | 布隆过滤器 | 传统容器 |
|------|-------------|------------|----------|
| **空间效率** | 1 bit/元素 | 8-12 bits/元素 | 32-64 bits/元素 |
| **设置位** | O(1) | O(k) k=哈希数 | O(log n) |
| **测试位** | O(1) | O(k) | O(log n) |
| **内存访问** | 缓存友好 | 缓存友好 | 可能缓存不友好 |

#### 🎯 布隆过滤器参数调优

```cpp
// 误判率计算公式
double false_positive_rate(int hash_count, int bit_array_size, int element_count) {
    double ratio = (double)hash_count * element_count / bit_array_size;
    return std::pow(1 - std::exp(-ratio), hash_count);
}

// 最优哈希函数数量
int optimal_hash_count(int bit_array_size, int expected_elements) {
    return std::round((double)bit_array_size / expected_elements * std::log(2));
}
```

### 🔧 容器适配器最佳实践

#### 📚 底层容器选择指南

| 适配器 | 推荐底层容器 | 原因 | 性能特点 |
|--------|-------------|------|----------|
| **stack** | `vector` | 尾部操作高效 | push/pop: O(1) |
| **queue** | `deque` | 双端操作均衡 | push/pop: O(1) |
| **priority_queue** | `vector` | 堆操作需随机访问 | push/pop: O(log n) |

#### ⚠️ 迭代器失效规则

```cpp
// 容器适配器迭代器失效规则
stack<int> st;
auto it = st.underlying_container().begin();  // 获取底层迭代器
st.push(42);  // 可能导致迭代器失效
// it 现在可能无效，需重新获取
```

### 🧠 智能指针与异常安全

#### 🔒 线程安全考虑

| 智能指针 | 线程安全性 | 性能开销 | 使用建议 |
|----------|------------|----------|----------|
| **shared_ptr** | 引用计数线程安全 | 原子操作开销 | 多线程共享 |
| **unique_ptr** | 非线程安全 | 几乎无开销 | 单线程独占 |
| **weak_ptr** | 观察线程安全 | 轻微开销 | 打破循环引用 |

#### 🛡️ 异常安全保证

```cpp
// RAII 异常安全模式
class ExceptionSafeContainer {
public:
    void safe_operation() noexcept {  // 析构函数必须 noexcept
        try {
            // 可能抛异常的操作
            risky_operation();
        } catch (...) {
            // 清理资源，不重新抛出
            cleanup();
        }
    }
    
    ~ExceptionSafeContainer() noexcept {  // 析构函数不能抛异常
        // 安全清理资源
    }
};
```

### 🔐 哈希函数自定义

#### 🎨 高度可定制的哈希系统

```cpp
// 自定义类型哈希示例
struct CustomType {
    int id;
    std::string name;
};

// 方法1: 特化 hash_function
template<>
struct hash_function<CustomType> {
    size_t operator()(const CustomType& obj) const {
        return hash_combine(std::hash<int>{}(obj.id), 
                           std::hash<std::string>{}(obj.name));
    }
};

// 方法2: 自定义哈希函数对象
struct CustomHasher {
    size_t operator()(const CustomType& obj) const {
        return obj.id ^ (std::hash<std::string>{}(obj.name) << 1);
    }
};

// 使用自定义哈希
hash_map<CustomType, int, CustomHasher> custom_map;
```

---

