## 🧠 智能指针 `smart_pointer`

> ⚠️ **开发状态**：当前未处理个别错误，及涉及到指针管理权转移等，还未完全测试

### 📊 模块概览

`smart_pointer` 命名空间提供了 4 种智能指针的实现，用于自动内存管理，避免手动 `delete` 操作：

| 智能指针类型 | 所有权模型 | 线程安全 | 主要用途 |
|-------------|-----------|----------|----------|
| `smart_ptr` | 转移所有权 | ✅ | 简单的所有权转移 |
| `unique_ptr` | 独占所有权 | ✅ | 独占资源管理 |
| `shared_ptr` | 共享所有权 | ✅ | 多对象共享资源 |
| `weak_ptr` | 弱引用观察 | ✅ | 解决循环引用 |

### 🔧 `smart_ptr<T>` - 转移所有权指针

#### 类特性
```cpp
template<typename smart_ptr_type>
class smart_ptr 
{
private:
    smart_ptr_type* _ptr;  // 裸指针，指向托管对象
    
public:
    explicit smart_ptr(smart_ptr_type* p);
    smart_ptr(const smart_ptr& other);      // 转移所有权
    smart_ptr& operator=(const smart_ptr& other);  // 转移所有权
    ~smart_ptr();
    
    smart_ptr_type& operator*() const;
    smart_ptr_type* operator->() const;
};
```

#### 🔄 所有权转移机制
- **拷贝构造**：指针管理权转移，原智能指针变为空
- **拷贝赋值**：指针管理权转移，原智能指针变为空
- **析构**：作用域结束时自动释放资源

#### ⚠️ 使用注意
```cpp
template_container::smart_pointer::smart_ptr<MyClass> sp1(new MyClass());
{
    auto sp2 = sp1;  // ⚠️ 管理权转移，sp1 变为空
    // sp1 现在是空指针，访问会出错！
} // sp2 离开作用域，对象被删除
```

#### 📈 性能特点
- **时间复杂度**：拷贝构造/赋值 O(1)，访问 O(1)
- **空间开销**：仅一个指针的存储开销
- **适用场景**：简单的所有权转移，类似 `std::auto_ptr`

---

### 🔒 `unique_ptr<T>` - 独占所有权指针

#### 类特性
```cpp
template<typename unique_ptr_type>
class unique_ptr 
{
private:
    unique_ptr_type* _ptr;  // 裸指针，指向托管对象
    
public:
    explicit unique_ptr(unique_ptr_type* p);
    unique_ptr(const unique_ptr&) = delete;           // 禁用拷贝
    unique_ptr& operator=(const unique_ptr&) = delete; // 禁用赋值
    unique_ptr(unique_ptr&& other) noexcept;          // 移动构造
    unique_ptr& operator=(unique_ptr&& other) noexcept; // 移动赋值
    ~unique_ptr();
    
    unique_ptr_type& operator*() const;
    unique_ptr_type* operator->() const;
    unique_ptr_type* get_ptr() const noexcept;
};
```

#### 🛡️ 独占特性
- **拷贝禁用**：不允许拷贝构造和拷贝赋值
- **移动语义**：支持移动构造和移动赋值
- **独占管理**：确保资源只有一个所有者

#### 💡 使用示例
```cpp
template_container::smart_pointer::unique_ptr<MyClass> up1(new MyClass());
// auto up2 = up1;  // ❌ 编译错误：拷贝被禁用
auto up2 = std::move(up1);  // ✅ 移动语义，up1 变为空
```

#### 📈 性能特点
- **时间复杂度**：移动操作 O(1)，访问 O(1)
- **空间开销**：仅一个指针的存储开销
- **适用场景**：独占资源管理，类似 `std::unique_ptr`

---

### 🤝 `shared_ptr<T>` - 共享所有权指针

#### 类特性
```cpp
template<typename shared_ptr_type>
class shared_ptr 
{
private:
    shared_ptr_type* _ptr;        // 裸指针，指向托管对象
    size_t* _shared_pcount;       // 引用计数指针
    std::mutex* _pmutex;          // 线程安全互斥锁
    
public:
    shared_ptr();
    explicit shared_ptr(shared_ptr_type* p);
    shared_ptr(const shared_ptr& other);
    shared_ptr(shared_ptr&& other) noexcept;
    ~shared_ptr();
    
    shared_ptr_type* get_ptr() const noexcept;
    shared_ptr_type& operator*() const;
    shared_ptr_type* operator->() const;
    int get_sharedp_count() const noexcept;
    void release() noexcept;
};
```

#### 🔢 引用计数机制
- **构造时**：引用计数初始化为 1
- **拷贝时**：引用计数递增
- **析构时**：引用计数递减，为 0 时删除对象
- **线程安全**：通过互斥锁保护引用计数操作

#### 💡 使用示例
```cpp
template_container::smart_pointer::shared_ptr<MyClass> sp1(new MyClass());
{
    auto sp2 = sp1;  // 引用计数从 1 增到 2
    std::cout << sp1.get_sharedp_count();  // 输出: 2
} // sp2 离开作用域，引用计数减为 1
sp1.release();  // 引用计数减为 0，对象被删除
```

#### 📈 性能特点
- **时间复杂度**：拷贝/赋值/析构 O(1)（含锁开销）
- **空间开销**：指针 + 引用计数 + 互斥锁
- **适用场景**：多对象共享资源，类似 `std::shared_ptr`

---

### 👁️ `weak_ptr<T>` - 弱引用观察指针

#### 类特性
```cpp
template<typename weak_ptr_type>
class weak_ptr 
{
private:
    size_t* _weak_pcount;  // 弱引用计数指针
    std::mutex* _pmutex;   // 共享互斥锁
    
public:
    weak_ptr();
    weak_ptr(const shared_ptr<weak_ptr_type>& sp);
    weak_ptr(const weak_ptr& other);
    ~weak_ptr();
    
    int get_sharedp_count() const noexcept;
    void release() noexcept;
    shared_ptr<weak_ptr_type> lock() const;  // 尝试获取 shared_ptr
};
```

#### 🔗 弱引用特性
- **不影响生命周期**：不增加强引用计数
- **循环引用解决**：打破 `shared_ptr` 循环引用
- **安全访问**：通过 `lock()` 安全获取 `shared_ptr`

#### 💡 使用示例
```cpp
auto sp = template_container::smart_pointer::shared_ptr<MyClass>(new MyClass);
template_container::smart_pointer::weak_ptr<MyClass> wp = sp;

if (auto locked = wp.lock()) 
{
    // 对象仍然存在，可以安全使用 locked
    locked->do_something();
}
sp.release();  // 对象被删除
// wp.lock() 现在返回空的 shared_ptr
```

#### 📈 性能特点
- **时间复杂度**：`lock()` 操作 O(1)（含锁开销）
- **空间开销**：弱引用计数 + 共享互斥锁
- **适用场景**：观察者模式、缓存、父子关系等

### 🎯 智能指针选择指南

| 使用场景 | 推荐类型 | 原因 |
|----------|----------|------|
| 独占资源管理 | `unique_ptr` | 性能最优，语义清晰 |
| 多对象共享资源 | `shared_ptr` | 自动引用计数管理 |
| 观察共享资源 | `weak_ptr` | 避免循环引用 |
| 简单所有权转移 | `smart_ptr` | 轻量级转移语义 |

### ⚠️ 注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **循环引用** | `shared_ptr` 可能造成内存泄漏 | 使用 `weak_ptr` 打破循环 |
| **线程安全** | 引用计数操作是线程安全的 | 对象本身访问需要额外同步 |
| **性能开销** | `shared_ptr` 有锁和计数开销 | 性能敏感场景考虑 `unique_ptr` |
| **所有权转移** | `smart_ptr` 转移后原指针失效 | 转移后不要访问原指针 |

---

## 📦 模板容器 `template_container`

### 🛠️ 基础工具 `practicality`

#### 🔗 `pair<K, V>` - 键值对容器

**定义**：类似于 `std::pair`，用于键值对存储，广泛应用于 map/set 等关联容器中。

```cpp
template<typename K, typename V>
class pair 
{
public:
    K first;   // 键类型或第一个元素
    V second;  // 值类型或第二个元素
    
    // 构造函数
    pair();
    pair(const K& key, const V& value);
    pair(const pair& other);
    pair(pair&& other) noexcept;
    
    // 运算符重载
    pair& operator=(const pair& other);
    pair& operator=(pair&& other) noexcept;
    bool operator==(const pair& other) const;
    bool operator!=(const pair& other) const;
    pair* operator->() noexcept;
    
    // 流输出支持
    friend std::ostream& operator<<(std::ostream& os, const pair& p);
};
```

#### 📋 功能特性

| 特性 | 说明 | 用途 |
|------|------|------|
| **键值存储** | 存储两个相关联的值 | 关联容器的基础元素 |
| **类型安全** | 编译时类型检查 | 避免类型错误 |
| **流输出** | 支持直接输出到流 | 调试和显示 |
| **比较操作** | 支持相等性比较 | 容器查找和排序 |

#### 💡 使用示例

```cpp
#include "Foundation.hpp"
using namespace template_container;

// 创建键值对
practicality::pair<int, std::string> p(1, "one");
std::cout << p.first << ": " << p.second << std::endl;  // 输出: 1: one

// 直接输出
std::cout << p << std::endl;  // 使用重载的 << 运算符

// 在关联容器中使用
// tree_map 内部使用 pair<key, value> 存储元素
```

#### 🏭 `make_pair` 工厂函数

**功能**：自动类型推导的工厂函数，返回 `pair` 类实例。

```cpp
template<typename T1, typename T2>
auto make_pair(T1&& first, T2&& second) -> pair<T1, T2>;
```

**优势**：
- ✅ **自动推导**：无需显式指定模板参数
- ✅ **简洁语法**：减少代码冗余
- ✅ **完美转发**：支持移动语义

```cpp
// 使用 make_pair 简化创建
auto p1 = make_pair(42, "answer");           // 自动推导类型
auto p2 = make_pair(std::string("key"), 100); // 支持复杂类型
```

---

### 🔧 仿函数 `imitation_functions`

#### 📊 仿函数概览

仿函数（Function Objects）提供了可调用对象，用于自定义比较、哈希等操作：

| 仿函数类型 | 功能 | 用途 |
|------------|------|------|
| `less<T>` | 小于比较 | 排序、有序容器 |
| `greater<T>` | 大于比较 | 逆序排序 |
| `hash_imitation_functions` | 哈希计算 | 哈希表、无序容器 |

#### 🔍 比较仿函数

```cpp
template<typename T>
struct less 
{
    bool operator()(const T& lhs, const T& rhs) const 
    {
        return lhs < rhs;
    }
};

template<typename T>
struct greater 
{
    bool operator()(const T& lhs, const T& rhs) const 
    {
        return lhs > rhs;
    }
};
```

#### 🎯 哈希仿函数

```cpp
struct hash_imitation_functions 
{
    template<typename T>
    size_t operator()(const T& value) const 
    {
        // 为内置类型提供哈希值计算
        return std::hash<T>{}(value);
    }
};
```

#### 💡 使用示例

```cpp
using namespace template_container::imitation_functions;

// 比较仿函数
less<int> cmp;
bool result = cmp(3, 5);  // true

// 哈希仿函数
hash_imitation_functions hasher;
size_t hash_value = hasher(42);

// 在容器中使用
// tree_map<int, string, less<int>> ordered_map;  // 使用 less 排序
// hash_map<int, string, hash_imitation_functions> unordered_map;  // 使用哈希
```

#### 🔧 自定义仿函数

```cpp
// 自定义类型的哈希仿函数
struct MyClass 
{
    int _id;
    std::string _name;
};

struct MyClassHasher 
{
    size_t operator()(const MyClass& obj) const 
    {
        return std::hash<int>{}(obj._id) ^ 
               (std::hash<std::string>{}(obj._name) << 1);
    }
};

// 使用自定义哈希仿函数
hash_map<MyClass, int, MyClassHasher> custom_map;
```

---

### ⚙️ 算法 `algorithm`

#### 📋 算法工具概览

| 算法 | 功能 | 时间复杂度 | 用途 |
|------|------|------------|------|
| `copy` | 线性拷贝 | O(n) | 数据复制 |
| `swap` | 交换数据 | O(1) | 数据交换 |
| `find` | 查找元素 | O(n) | 元素查找 |
| `hash_function` | 哈希计算 | O(k) | 哈希表支持 |

#### 📋 `copy` - 数据拷贝算法

**功能**：将源序列的元素拷贝到目标序列。

```cpp
template<typename InputIt, typename OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt d_first);
```

**参数说明**：
- `first`：源序列起始迭代器
- `last`：源序列结束迭代器  
- `d_first`：目标序列起始迭代器

**返回值**：目标序列的结束迭代器

#### 💡 使用示例

```cpp
using namespace template_container::algorithm;

int source[] = {1, 2, 3, 4, 5};
int target[5];

// 拷贝数组
auto end_it = copy(source, source + 5, target);

// 验证拷贝结果
for (int i = 0; i < 5; ++i) 
{
    std::cout << target[i] << " ";  // 输出: 1 2 3 4 5
}
```

#### 🔄 `swap` - 数据交换算法

**功能**：交换两个对象的值，支持深拷贝交换。

```cpp
template<typename T>
void swap(T& a, T& b) noexcept;
```

**特点**：
- ✅ **异常安全**：`noexcept` 保证
- ✅ **深拷贝**：需要类型支持拷贝构造
- ✅ **高效**：O(1) 时间复杂度

#### 💡 使用示例

```cpp
using namespace template_container::algorithm;

int a = 10, b = 20;
swap(a, b);
std::cout << "a = " << a << ", b = " << b << std::endl;  // 输出: a = 20, b = 10

// 在容器中使用
list_container::list<int> list1, list2;
// ... 填充数据
list1.swap(list2);  // 内部调用 algorithm::swap
```

#### 🔍 `find` - 元素查找算法

**功能**：在指定范围内查找特定值的元素。

```cpp
template<typename InputIt, typename T>
InputIt find(InputIt first, InputIt last, const T& value);
```

**参数说明**：
- `first`：搜索范围起始迭代器
- `last`：搜索范围结束迭代器
- `value`：要查找的值

**返回值**：
- 找到：指向该元素的迭代器
- 未找到：返回 `last` 迭代器

#### 💡 使用示例

```cpp
using namespace template_container::algorithm;

vector_container::vector<int> vec = {1, 2, 3, 4, 5};

// 查找元素
auto it = find(vec.begin(), vec.end(), 3);
if (it != vec.end()) 
{
    std::cout << "找到元素: " << *it << std::endl;
}
else 
{
    std::cout << "未找到元素" << std::endl;
}
```

#### 🎯 `hash_function` - 哈希算法

**功能**：提供多种哈希函数实现，减少哈希冲突。

```cpp
namespace hash_algorithm 
{
    template<typename T, typename Hasher = std::hash<T>>
    class hash_function 
    {
    public:
        size_t hash_sdmmhash(const T& value);   // SDBM 哈希
        size_t hash_djbhash(const T& value);    // DJB 哈希  
        size_t hash_pjwhash(const T& value);    // PJW 哈希
    };
}
```

#### 🔧 哈希算法特点

| 算法 | 特点 | 适用场景 |
|------|------|----------|
| **SDBM** | 分布均匀，速度快 | 一般用途哈希表 |
| **DJB** | 简单高效 | 字符串哈希 |
| **PJW** | 冲突率低 | 高质量哈希需求 |

#### 💡 使用示例

```cpp
using namespace template_container::algorithm::hash_algorithm;

// 自定义哈希器
struct StringHasher 
{
    size_t operator()(const std::string& str) const 
    {
        size_t hash = 0;
        for (char c : str) 
        {
            hash = hash * 131 + static_cast<size_t>(c);
        }
        return hash;
    }
};

// 使用多种哈希函数
hash_function<std::string, StringHasher> hasher;
std::string test = "hello world";

size_t h1 = hasher.hash_sdmmhash(test);
size_t h2 = hasher.hash_djbhash(test);
size_t h3 = hasher.hash_pjwhash(test);

std::cout << "SDBM: " << h1 << std::endl;
std::cout << "DJB:  " << h2 << std::endl;
std::cout << "PJW:  " << h3 << std::endl;
```

#### ⚡ 性能特点

- **时间复杂度**：O(k)，k 为输入数据长度
- **空间复杂度**：O(1)，常量空间
- **适用性**：支持布隆过滤器、哈希表等数据结构

---

