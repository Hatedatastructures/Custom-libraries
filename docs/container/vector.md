## 📊 动态数组 `vector_container`
## 📊 动态数组 `vector_container`

### 🎯 `vector` 类概览

`vector` 类是一个模板化的动态数组容器，模拟了标准库 `std::vector` 的核心功能。该容器使用连续内存存储元素，支持动态扩容、随机访问和各种容器操作。通过模板参数 `vector_type` 支持任意数据类型，并提供了完整的迭代器系统和异常处理机制。

### 🏗️ 类定义与结构

```cpp
namespace vector_container 
{
    template <typename vector_type>
    class vector 
    {
    public:
        using iterator = vector_type*;
        using const_iterator = const vector_type*;
        using reverse_iterator = iterator;
        using const_reverse_iterator = const_iterator;
    
    private:
        iterator _data_pointer;     // 指向数据起始位置
        iterator _size_pointer;     // 指向数据结束位置（最后一个元素的下一个位置）
        iterator _capacity_pointer; // 指向容量结束位置
    
    public:
        // 构造函数
        vector() noexcept;
        explicit vector(const size_t& container_capacity, const vector_type& vector_data = vector_type());
        vector(std::initializer_list<vector_type> lightweight_container);
        vector(const vector<vector_type>& vector_data);
        vector(vector<vector_type>&& vector_data) noexcept;
        ~vector() noexcept;
        
        // 迭代器方法
        [[nodiscard]] iterator begin() noexcept;
        [[nodiscard]] iterator end() noexcept;
        
        // 容量方法
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] size_t capacity() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
        
        // 元素访问
        [[nodiscard]] vector_type& front() const noexcept;
        [[nodiscard]] vector_type& back() const noexcept;
        [[nodiscard]] vector_type& head() const noexcept;
        [[nodiscard]] vector_type& tail() const noexcept;
        vector_type& find(const size_t& find_size);
        vector_type& operator[](const size_t& access_location);
        const vector_type& operator[](const size_t& access_location) const;
        
        // 修改方法
        void swap(vector<vector_type>& vector_data) noexcept;
        iterator erase(iterator delete_position) noexcept;
        vector<vector_type>& resize(const size_t& new_container_capacity, const vector_type& vector_data = vector_type());
        vector<vector_type>& push_back(const vector_type& vector_type_data);
        vector<vector_type>& push_back(vector_type&& vector_type_data);
        vector<vector_type>& pop_back();
        vector<vector_type>& push_front(const vector_type& vector_type_data);
        vector<vector_type>& pop_front();
        vector<vector_type>& size_adjust(const size_t& data_size, const vector_type& padding_temp_data = vector_type());
        
        // 赋值运算符
        vector<vector_type>& operator=(const vector<vector_type>& vector_data);
        vector<vector_type>& operator=(vector<vector_type>&& vector_mobile_data) noexcept;
        vector<vector_type>& operator+=(const vector<vector_type>& vector_data);
        
        // 友元运算符
        template <typename const_vector_output_templates>
        friend std::ostream& operator<<(std::ostream& vector_ostream, const vector<const_vector_output_templates>& dynamic_arrays_data);
    };
}
```

### 🧱 内部数据结构

#### 📊 成员变量

| 成员变量 | 类型 | 作用 |
|----------|------|------|
| `_data_pointer` | `iterator` | 指向已分配内存区域的首地址 |
| `_size_pointer` | `iterator` | 指向当前数据末尾的下一个位置 |
| `_capacity_pointer` | `iterator` | 指向已分配内存容量的末尾 |

#### 🏗️ 内存布局
- **底层实现**：基于动态内存的连续数组，使用原生指针管理
- **内存布局**：位置连续，存储任意自定义类型的 `vector_type` 对象
- **扩容策略**：初始容量 10，不足时翻倍扩容

### 🔧 构造与析构

#### 构造函数类型

| 构造函数 | 功能 | 特点 |
|----------|------|------|
| **默认构造** | `vector() noexcept` | 初始化空向量，所有指针为 `nullptr` |
| **容量构造** | `vector(size_t, const T&)` | 分配指定容量并初始化所有元素 |
| **初始化列表** | `vector(std::initializer_list<T>)` | 使用初始化列表元素初始化向量 |
| **拷贝构造** | `vector(const vector&)` | 深拷贝另一个向量的所有元素 |
| **移动构造** | `vector(vector&&) noexcept` | 接管右值资源，避免深拷贝 |

#### 💡 构造示例

```cpp
using namespace template_container::vector_container;

// 默认构造
vector<int> v1;                         // 空向量

// 容量构造
vector<double> v2(5, 3.14);             // 5个元素，都是3.14

// 初始化列表构造
vector<std::string> v3 = {"hello", "world", "!"};

// 拷贝构造
vector<double> v4(v2);                  // 深拷贝

// 移动构造
vector<double> v5(std::move(v1));       // v1 变为空
```

### 🔄 迭代器支持

#### 迭代器类型

| 迭代器类型 | 定义 | 用途 |
|------------|------|------|
| `iterator` | `vector_type*` | 可修改的正向迭代器 |
| `const_iterator` | `const vector_type*` | 只读的正向迭代器 |
| `reverse_iterator` | `iterator` | 反向迭代器 |
| `const_reverse_iterator` | `const_iterator` | 只读反向迭代器 |

#### 💡 迭代器使用

```cpp
vector<int> v = {1, 2, 3, 4, 5};

// 正向遍历
for (auto it = v.begin(); it != v.end(); ++it) 
{
    std::cout << *it << " ";  // 输出: 1 2 3 4 5
}

// 范围for循环
for (int val : v) 
{
    std::cout << val << " ";  // 输出: 1 2 3 4 5
}
```

> ⚠️ **注意**：当前反向迭代器未实现封装，如需使用可参考链表迭代器类进行封装

### 🎯 元素访问

#### 访问方法

| 方法 | 功能 | 返回值 | 注意事项 |
|------|------|--------|----------|
| `operator[]` | 下标访问 | `vector_type&` | 边界检查，越界抛异常 |
| `front()` / `head()` | 首元素 | `vector_type&` | 容器非空时有效 |
| `back()` / `tail()` | 尾元素 | `vector_type&` | 容器非空时有效 |
| `find(size_t)` | 位置查找 | `vector_type&` | 越界时抛异常 |

#### 💡 访问示例

```cpp
vector<std::string> v = {"hello", "world", "!"};

std::cout << v[0];        // 输出: hello
std::cout << v.front();   // 输出: hello
std::cout << v.back();    // 输出: !
std::cout << v.find(1);   // 输出: world
```

### ✏️ 容器修改

#### 🔤 追加操作

| 方法 | 功能 | 扩容策略 | 时间复杂度 |
|------|------|----------|------------|
| `push_back(const T&)` | 末尾添加元素 | 容量不足时翻倍 | 均摊 O(1) |
| `push_back(T&&)` | 移动语义版本 | 避免不必要拷贝 | 均摊 O(1) |
| `push_front(const T&)` | 头部插入元素 | 需移动所有现有元素 | O(n) |

#### 🗑️ 删除操作

| 方法 | 功能 | 时间复杂度 |
|------|------|------------|
| `pop_back()` | 移除最后元素 | O(1) |
| `pop_front()` | 移除第一元素 | O(n) - 移动剩余元素 |
| `erase(iterator)` | 删除指定位置 | O(n) - 移动后续元素 |

#### 📏 内存管理

| 方法 | 功能 | 行为 |
|------|------|------|
| `resize(size, T)` | 调整容量 | 扩容时用指定值填充新增空间 |
| `size_adjust(size, T)` | 调整大小 | 支持扩容和缩容 |
| `swap(vector&)` | 交换内容 | O(1) 时间复杂度 |

### 📊 容量与工具方法

#### 容量查询

| 方法 | 功能 | 返回值 |
|------|------|--------|
| `empty()` | 判断是否为空 | `bool` |
| `size()` | 当前元素数量 | `size_t` |
| `capacity()` | 当前分配容量 | `size_t` |

### 🔧 运算符重载

#### 赋值运算符

| 运算符 | 功能 | 特点 |
|--------|------|------|
| `operator=(const vector&)` | 拷贝赋值 | 深拷贝赋值 |
| `operator=(vector&&)` | 移动赋值 | 接管右值资源 |
| `operator+=(const vector&)` | 拼接运算符 | 将另一个向量元素追加到末尾 |

#### 输出运算符

| 运算符 | 功能 | 实现方式 |
|--------|------|----------|
| `operator<<` | 流输出 | 遍历所有元素并输出 |

### ⚡ 性能特点

#### 时间复杂度

| 操作类型 | 时间复杂度 | 说明 |
|----------|------------|------|
| **随机访问** | O(1) | 直接索引访问 |
| **末尾插入/删除** | 均摊 O(1) | 扩容时 O(n) |
| **头部/中间插入/删除** | O(n) | 需移动元素 |
| **扩容** | O(n) | 复制所有元素 |

#### 空间复杂度

- **存储空间**：O(n)，n 为元素数量 + 额外容量
- **扩容策略**：翻倍增长，减少重分配次数

### 🚨 异常安全与注意事项

#### 异常处理

| 异常类型 | 触发条件 | 处理建议 |
|----------|----------|----------|
| `std::bad_alloc` | 内存分配失败 | 使用 try-catch 处理 |
| `custom_exception` | 越界操作 | 检查索引有效性 |

#### 迭代器失效

- **扩容操作**：所有迭代器和引用失效
- **修改操作**：可能导致迭代器失效
- **建议**：修改后重新获取迭代器

#### 最佳实践

| 建议 | 说明 | 原因 |
|------|------|------|
| **预分配空间** | 频繁插入前使用 `reserve()` | 减少重分配开销 |
| **移动语义** | 优先使用移动构造/赋值 | 避免不必要的深拷贝 |
| **异常安全** | 使用 try-catch 处理异常 | 确保程序健壮性 |
| **避免头部操作** | 尽量避免 `push_front` | O(n) 时间复杂度 |

### 💡 完整使用示例

```cpp
using namespace template_container::vector_container;

int main()
{
    // 1. 构造函数示例
    vector<double> v1;                      // 默认构造
    vector<double> v2(5, 3.14);             // 指定容量构造
    vector<std::string> v3 = {"hello", "world", "!"};  // 初始化列表
    
    // 2. 容器修改
    v3.push_back("new");                    // 末尾添加
    v3.push_front("start");                 // 头部插入
    v3.pop_back();                          // 删除末尾
    
    // 3. 元素访问
    std::cout << v3[0] << std::endl;        // 下标访问
    std::cout << v3.front() << std::endl;   // 首元素
    std::cout << v3.back() << std::endl;    // 尾元素
    
    // 4. 迭代器遍历
    for (auto it = v3.begin(); it != v3.end(); ++it) 
    {
        std::cout << *it << " ";
    }
    
    // 5. 容量管理
    std::cout << "Size: " << v3.size() << std::endl;
    std::cout << "Capacity: " << v3.capacity() << std::endl;
    
    // 6. 运算符使用
    v1 += v2;                               // 拼接
    std::cout << v1 << std::endl;           // 流输出
    
    return 0;
}
```

---

