## 📝 字符数组 `string_container`
* **内容**：基本算法工具 `swap`
* **用途**：数据类型深拷贝交换（需要提前重载数据类型的拷贝构造）
* **示例**

## 📝 字符数组 `string_container`

### 🎯 `string` 类概览

`string` 类是一个自定义实现的字符串容器，模拟了标准库 `std::string` 的核心功能，同时提供了额外的字符串操作方法。该类使用动态内存分配管理字符数据，支持迭代器遍历、字符串修改、子串操作等功能。

### 🏗️ 类定义与结构

```cpp
namespace string_container 
{
    class string 
    {
    private:
        char* _data;      // 指向已分配内存区域的首地址
        size_t _size;     // 当前字符串长度
        size_t _capacity; // 当前分配的内存容量
        
    public:
        // 迭代器类型定义
        using iterator = char*;
        using const_iterator = const char*;
        using reverse_iterator = iterator;
        using const_reverse_iterator = const_iterator;
        constexpr static const size_t nops = -1;
        
        // 构造函数、析构函数及赋值运算符
        string(const char* str_data = " ");
        string(char*&& str_data) noexcept;
        string(const string& str_data);
        string(string&& str_data) noexcept;
        string(const std::initializer_list<char> str_data);
        ~string() noexcept;
        
        // 迭代器相关方法
        [[nodiscard]] iterator begin() const noexcept;
        [[nodiscard]] iterator end() const noexcept;
        [[nodiscard]] const_iterator cbegin() const noexcept;
        [[nodiscard]] const_iterator cend() const noexcept;
        [[nodiscard]] reverse_iterator rbegin() const noexcept;
        [[nodiscard]] reverse_iterator rend() const noexcept;
        
        // 容量相关方法
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] size_t capacity() const noexcept;
        
        // 元素访问方法
        [[nodiscard]] char* c_str() const noexcept;
        [[nodiscard]] char back() const noexcept;
        [[nodiscard]] char front() const noexcept;
        char& operator[](const size_t& access_location);
        const char& operator[](const size_t& access_location) const;
        
        // 字符串修改方法
        string& uppercase() noexcept;
        string& lowercase() noexcept;
        string& prepend(const char*& sub_string);
        string& insert_sub_string(const char*& sub_string, const size_t& start_position);
        string sub_string(const size_t& start_position) const;
        string sub_string_from(const size_t& start_position) const;
        string sub_string(const size_t& start_position, const size_t& terminate_position) const;
        void allocate_resources(const size_t& new_inaugurate_capacity);
        string& push_back(const char& temporary_str_data);
        string& push_back(const string& temporary_string_data);
        string& push_back(const char* temporary_str_ptr_data);
        string& resize(const size_t& inaugurate_size, const char& default_data = '\0');
        iterator reserve(const size_t& new_container_capacity);
        string& swap(string& str_data) noexcept;
        [[nodiscard]] string reverse() const;
        [[nodiscard]] string reverse_sub_string(const size_t& start_position, const size_t& terminate_position) const;
        
        // 输出方法
        void string_print() const noexcept;
        void string_reverse_print() const noexcept;
        
        // 运算符重载
        friend std::ostream& operator<<(std::ostream& string_ostream, const string& str_data);
        friend std::istream& operator>>(std::istream& string_istream, string& str_data);
        string& operator=(const string& str_data);
        string& operator=(const char* str_data);
        string& operator=(string&& str_data) noexcept;
        string& operator+=(const string& str_data);
        bool operator==(const string& str_data) const noexcept;
        bool operator<(const string& str_data) const noexcept;
        bool operator>(const string& str_data) const noexcept;
        [[nodiscard]] string operator+(const string& string_array) const;
    };
}
```

### 🧱 内部数据结构

#### 📊 成员变量

| 成员变量 | 类型 | 作用 |
|----------|------|------|
| `_data` | `char*` | 指向已分配内存区域的首地址 |
| `_size` | `size_t` | 当前字符串长度 |
| `_capacity` | `size_t` | 当前分配的内存容量 |

#### 🏗️ 内存布局
- **底层实现**：基于动态内存的连续字符数组
- **内存布局**：位置连续，以 `\0` 结尾，符合 C 风格字符串要求
- **扩容策略**：空间不足时 2 倍增容

### 🔧 构造与析构

#### 构造函数类型

| 构造函数 | 功能 | 特点 |
|----------|------|------|
| **默认构造** | `string(const char* str_data = " ")` | 初始化为输入字符串的副本 |
| **移动构造** | `string(char*&& str_data) noexcept` | 接管右值指针，避免复制 |
| **拷贝构造** | `string(const string& str_data)` | 深拷贝所有字符 |
| **移动构造** | `string(string&& str_data) noexcept` | 接管右值资源 |
| **初始化列表** | `string(const std::initializer_list<char>)` | 从字符列表构造 |

#### 💡 构造示例

```cpp
using namespace template_container::string_container;

// 默认构造
string s1;                              // 空字符串
string s2("Hello");                     // C风格字符串构造

// 拷贝构造
string s3(s2);                          // 深拷贝

// 移动构造
string s4(std::move(s2));               // s2 变为空

// 初始化列表构造
string s5({'H', 'e', 'l', 'l', 'o'});  // 从字符列表构造
```

### 🔄 迭代器支持

#### 迭代器类型

| 迭代器类型 | 定义 | 用途 |
|------------|------|------|
| `iterator` | `char*` | 可修改的正向迭代器 |
| `const_iterator` | `const char*` | 只读的正向迭代器 |
| `reverse_iterator` | `iterator` | 反向迭代器 |
| `const_reverse_iterator` | `const_iterator` | 只读反向迭代器 |

#### 💡 迭代器使用

```cpp
string s("Hello");

// 正向遍历
for (auto it = s.begin(); it != s.end(); ++it) 
{
    std::cout << *it;  // 输出: Hello
}

// 反向遍历
for (auto it = s.rbegin(); it != s.rend(); --it) 
{
    std::cout << *it;  // 输出: olleH
}

// 范围for循环
for (char c : s) 
{
    std::cout << c;    // 输出: Hello
}
```

### 🎯 元素访问

#### 访问方法

| 方法 | 功能 | 返回值 | 注意事项 |
|------|------|--------|----------|
| `operator[]` | 下标访问 | `char&` / `const char&` | 边界检查，越界抛异常 |
| `front()` | 首字符 | `char` | 须保证 `_size > 0` |
| `back()` | 尾字符 | `char` | 须保证 `_size > 0` |
| `c_str()` | C风格字符串 | `char*` | 以 `\0` 结尾 |

#### 💡 访问示例

```cpp
string s("Hello");

std::cout << s[0];        // 输出: H
std::cout << s.front();   // 输出: H
std::cout << s.back();    // 输出: o
std::cout << s.c_str();   // 输出: Hello
```

### ✏️ 字符串修改

#### 🔤 追加操作

| 方法 | 功能 | 扩容策略 |
|------|------|----------|
| `push_back(char)` | 追加单个字符 | 容量不足时 2 倍扩容 |
| `push_back(string)` | 追加字符串 | 自动扩容并深拷贝 |
| `push_back(const char*)` | 追加 C 字符串 | 自动扩容 |

#### 🔧 插入操作

| 方法 | 功能 | 时间复杂度 |
|------|------|------------|
| `prepend(const char*)` | 头部插入 | O(n) - 需移动原数据 |
| `insert_sub_string(const char*, pos)` | 指定位置插入 | O(n) - 移动后续数据 |

#### 📏 大小调整

| 方法 | 功能 | 行为 |
|------|------|------|
| `resize(size, char)` | 调整长度 | 扩容时用指定字符填充，缩容时截断 |
| `reserve(size)` | 预分配内存 | 返回首地址迭代器 |

#### 🔄 其他修改

| 方法 | 功能 | 特点 |
|------|------|------|
| `uppercase()` | 转大写 | 原地修改，遍历调整 ASCII 码 |
| `lowercase()` | 转小写 | 原地修改 |
| `swap(string&)` | 交换内容 | O(1) 时间复杂度 |

### 📄 子串操作

#### 子串提取

| 方法 | 功能 | 参数 | 返回值 |
|------|------|------|--------|
| `sub_string(start)` | 从指定位置到末尾 | 起始位置 | 新字符串 |
| `sub_string_from(start)` | 同上 | 起始位置 | 新字符串 |
| `sub_string(start, end)` | 指定范围子串 | 起始和结束位置 | 新字符串 |

#### 反转操作

| 方法 | 功能 | 返回值 |
|------|------|--------|
| `reverse()` | 整个字符串反转 | 新的反转字符串 |
| `reverse_sub_string(start, end)` | 指定范围反转 | 新的反转子串 |

### 🔧 运算符重载

#### 赋值运算符

| 运算符 | 功能 | 特点 |
|--------|------|------|
| `operator=(const string&)` | 拷贝赋值 | 深拷贝，先释放原内存 |
| `operator=(const char*)` | C字符串赋值 | 重新分配内存 |
| `operator=(string&&)` | 移动赋值 | 接管右值资源 |

#### 拼接运算符

| 运算符 | 功能 | 返回值 |
|--------|------|--------|
| `operator+=(const string&)` | 原地拼接 | `string&` |
| `operator+(const string&)` | 创建新字符串 | 新的拼接字符串 |

#### 比较运算符

| 运算符 | 功能 | 比较方式 |
|--------|------|----------|
| `operator==(const string&)` | 相等比较 | 字典序比较 |
| `operator<(const string&)` | 小于比较 | 字典序比较 |
| `operator>(const string&)` | 大于比较 | 字典序比较 |

#### 输入输出运算符

| 运算符 | 功能 | 实现方式 |
|--------|------|----------|
| `operator<<` | 流输出 | 遍历字符输出 |
| `operator>>` | 流输入 | 逐字符读取直至换行或 EOF |

### ⚡ 性能特点

#### 时间复杂度

| 操作类型 | 时间复杂度 | 说明 |
|----------|------------|------|
| **随机访问** | O(1) | 直接索引访问 |
| **插入/删除中间** | O(n) | 需移动后续元素 |
| **扩容** | O(n) | 复制数据，摊销后均摊 O(1) |
| **子串操作** | O(k) | k 为子串长度 |

#### 空间复杂度

- **存储空间**：O(n)，n 为字符串长度 + `\0`
- **扩容策略**：2 倍增长，减少重分配次数

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
| **预分配空间** | 频繁拼接前使用 `reserve()` | 减少重分配开销 |
| **移动语义** | 优先使用移动构造/赋值 | 避免深拷贝性能损耗 |
| **异常安全** | 使用 try-catch 处理异常 | 确保程序健壮性 |
| **边界检查** | 访问前检查字符串长度 | 避免越界访问 |

### 💡 完整使用示例

```cpp
using namespace template_container::string_container;

int main() 
{
    // 1. 构造函数示例
    string s1;                              // 默认构造
    string s2("Hello");                     // C风格字符串构造
    string s3(s2);                          // 拷贝构造
    string s4({'W', 'o', 'r', 'l', 'd'});   // 初始化列表构造
    
    // 2. 字符串修改
    s2.push_back(' ');                      // 追加字符
    s2.push_back(s4);                       // 追加字符串
    s2.uppercase();                         // 转大写
    
    // 3. 子串操作
    string sub = s2.sub_string(0, 5);       // 提取子串
    string rev = s2.reverse();              // 反转字符串
    
    // 4. 运算符使用
    string result = s2 + " C++";            // 拼接
    if (s2 == "HELLO WORLD") 
    {
        std::cout << "字符串匹配" << std::endl;
    }
    
    // 5. 迭代器遍历
    for (char c : s2) 
    {
        std::cout << c;
    }
    
    return 0;
}
```

---

