## 🔗 链表容器 `list_container`
    
    // += 运算符
### 🎯 `list` 类概览

`list` 类是一个模板化的双向链表容器，模拟标准库 `std::list` 的核心功能，通过哨兵节点实现统一边界处理，支持常量时间的任意位置插入与删除。

### 🏗️ 类定义与结构

```cpp
namespace list_container 
{
    template <typename list_type>
    class list 
    {
    public:
        // 嵌套节点与迭代器
        template<typename list_type_function_node>
        struct list_container_node;
        template<typename listNodeTypeIterator, typename Ref, typename Ptr>
        class list_iterator;
        template<typename iterator>
        class reverse_list_iterator;

        using container_node = list_container_node<list_type>;
        using iterator = list_iterator<list_type, list_type&, list_type*>;
        using const_iterator = list_iterator<list_type, const list_type&, const list_type*>;
        using reverse_iterator = reverse_list_iterator<iterator>;
        using reverse_const_iterator = reverse_list_iterator<const_iterator>;

        // 构造与析构
        list();
        ~list() noexcept;
        list(iterator first, iterator last);
        list(const_iterator first, const_iterator last);
        list(std::initializer_list<list_type> lightweight_container);
        list(const list<list_type>& list_data);
        list(list<list_type>&& list_data) noexcept;

        // 迭代器接口
        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
        reverse_iterator rbegin() noexcept;
        reverse_iterator rend() noexcept;
        reverse_const_iterator rcbegin() const noexcept;
        reverse_const_iterator rcend() const noexcept;

        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        list_type& front() noexcept;
        const list_type& front() const noexcept;
        list_type& back() noexcept;
        const list_type& back() const noexcept;

        // 插入/删除操作
        void push_back(const list_type& data);
        void push_back(list_type&& data);
        void push_front(const list_type& data);
        void push_front(list_type&& data);
        void pop_back();
        iterator pop_front();
        iterator insert(iterator pos, const list_type& data);
        iterator insert(iterator pos, list_type&& data);
        iterator erase(iterator pos);

        // 调整与管理
        void resize(const size_t new_container_size, const list_type& data = list_type());
        void clear() noexcept;
        void swap(list<list_type>& swap_target) noexcept;

        // 运算符重载
        list<list_type>& operator=(list<list_type> list_data) noexcept;
        list<list_type>& operator=(std::initializer_list<list_type> lightweight_container);
        list<list_type>& operator=(list<list_type>&& list_data) noexcept;
        list<list_type> operator+(const list<list_type>& list_data);
        list<list_type>& operator+=(const list<list_type>& list_data);

        template <typename const_list_output_templates>
        friend std::ostream& operator<< (std::ostream& os, const list<const_list_output_templates>& lst);
    };
}
```

### 🧱 内部数据结构

#### 📊 节点结构

```cpp
template<typename list_type_function_node>
struct list_container_node 
{
    list_type_function_node _data;                    // 存储用户数据
    list_container_node* _prev;                       // 指向前驱节点
    list_container_node* _next;                       // 指向后继节点
    
    explicit list_container_node(const list_type_function_node& val = list_type_function_node());
    ~list_container_node();
};
```

#### 🏗️ 哨兵节点设计

| 组件 | 作用 | 特点 |
|------|------|------|
| **哨兵节点 (`_head`)** | 统一边界处理 | 不存储有效数据，简化插入删除逻辑 |
| **循环结构** | `_head->_prev` 和 `_head->_next` 指向自身 | 初始状态形成循环 |
| **动态分配** | 通过 `new/delete` 管理节点 | 支持任意大小的链表 |

### 🔧 构造与析构

#### 构造函数类型

| 构造函数 | 功能 | 特点 |
|----------|------|------|
| **默认构造** | `list()` | 创建哨兵节点，初始化空表状态 |
| **范围构造** | `list(iterator, iterator)` | 从区间复制或移动元素 |
| **初始化列表** | `list(std::initializer_list<T>)` | 按顺序插入列表元素 |
| **拷贝构造** | `list(const list&)` | 深拷贝所有节点数据 |
| **移动构造** | `list(list&&) noexcept` | 接管原链表头指针，置空源对象 |

#### 💡 构造示例

```cpp
using namespace template_container::list_container;

// 默认构造
list<int> l1;                           // 空链表

// 初始化列表构造
list<std::string> l2 = {"hello", "world", "!"};

// 拷贝构造
list<std::string> l3(l2);               // 深拷贝

// 移动构造
list<std::string> l4(std::move(l2));    // l2 变为空
```

### 🔄 迭代器系统

#### 迭代器类型

| 迭代器类型 | 定义 | 用途 |
|------------|------|------|
| `iterator` | `list_iterator<T, T&, T*>` | 可修改的双向迭代器 |
| `const_iterator` | `list_iterator<T, const T&, const T*>` | 只读的双向迭代器 |
| `reverse_iterator` | `reverse_list_iterator<iterator>` | 反向遍历迭代器 |
| `reverse_const_iterator` | `reverse_list_iterator<const_iterator>` | 只读反向迭代器 |

#### 🎯 迭代器特性

- **双向访问**：支持 `++` 和 `--` 操作
- **解引用**：支持 `*` 和 `->` 操作
- **比较操作**：支持 `==` 和 `!=` 比较
- **边界处理**：通过哨兵节点统一处理边界情况

#### 💡 迭代器使用

```cpp
list<int> l = {1, 2, 3, 4, 5};

// 正向遍历
for (auto it = l.begin(); it != l.end(); ++it) 
{
    std::cout << *it << " ";  // 输出: 1 2 3 4 5
}

// 反向遍历
for (auto it = l.rbegin(); it != l.rend(); ++it) 
{
    std::cout << *it << " ";  // 输出: 5 4 3 2 1
}

// 范围for循环
for (const auto& val : l) 
{
    std::cout << val << " ";  // 输出: 1 2 3 4 5
}
```

### 🎯 元素访问

#### 访问方法

| 方法 | 功能 | 返回值 | 注意事项 |
|------|------|--------|----------|
| `front()` | 访问首元素 | `list_type&` | 须保证链表非空 |
| `back()` | 访问尾元素 | `list_type&` | 须保证链表非空 |
| `empty()` | 检查是否为空 | `bool` | O(1) 时间复杂度 |
| `size()` | 获取元素数量 | `size_t` | O(n) 时间复杂度 |

#### 💡 访问示例

```cpp
list<std::string> l = {"hello", "world"};

if (!l.empty()) 
{
    std::cout << l.front();  // 输出: hello
    std::cout << l.back();   // 输出: world
}

std::cout << "Size: " << l.size() << std::endl;  // 输出: Size: 2
```

### ✏️ 修改操作

#### 🔤 插入操作

| 方法 | 功能 | 时间复杂度 | 特点 |
|------|------|------------|------|
| `push_back(const T&)` | 尾部插入 | O(1) | 在哨兵前插入节点 |
| `push_back(T&&)` | 尾部插入（移动） | O(1) | 避免不必要拷贝 |
| `push_front(const T&)` | 头部插入 | O(1) | 在哨兵后插入节点 |
| `push_front(T&&)` | 头部插入（移动） | O(1) | 移动语义版本 |
| `insert(iterator, const T&)` | 指定位置插入 | O(1) | 在指定位置前插入 |
| `insert(iterator, T&&)` | 指定位置插入（移动） | O(1) | 移动语义版本 |

#### 🗑️ 删除操作

| 方法 | 功能 | 时间复杂度 | 返回值 |
|------|------|------------|--------|
| `pop_back()` | 删除尾节点 | O(1) | `void` |
| `pop_front()` | 删除头节点 | O(1) | 下一个位置迭代器 |
| `erase(iterator)` | 删除指定位置 | O(1) | 下一个节点迭代器 |
| `clear()` | 清空所有节点 | O(n) | 保留哨兵节点 |

#### 📏 调整与管理

| 方法 | 功能 | 行为 |
|------|------|------|
| `resize(size_t, const T&)` | 调整大小 | 根据目标大小增删节点 |
| `swap(list&)` | 交换内容 | O(1) 交换哨兵指针 |

### ⚡ 性能特点

#### 时间复杂度

| 操作类型 | 时间复杂度 | 说明 |
|----------|------------|------|
| **插入/删除（任意位置）** | O(1) | 无需移动其他节点 |
| **访问首尾元素** | O(1) | 直接通过哨兵访问 |
| **查找元素** | O(n) | 需要遍历链表 |
| **获取大小** | O(n) | 遍历统计（可优化为 O(1)） |

#### 空间复杂度

- **存储空间**：O(n)，每个节点存储数据 + 两个指针
- **额外开销**：一个哨兵节点的固定开销

### 🚨 异常安全与注意事项

#### 异常处理

| 异常类型 | 触发条件 | 处理建议 |
|----------|----------|----------|
| `std::bad_alloc` | 节点分配失败 | 使用 try-catch 处理 |
| `custom_exception` | 空链表访问 `front()/back()` | 先检查 `empty()` |
| `custom_exception` | 无效迭代器操作 | 确保迭代器有效性 |

#### 迭代器失效规则

- **插入操作**：不影响现有迭代器
- **删除操作**：仅失效被删除节点的迭代器
- **其他迭代器**：保持有效

#### 最佳实践

| 建议 | 说明 | 原因 |
|------|------|------|
| **优先使用链表** | 频繁插入删除场景 | O(1) 插入删除性能 |
| **避免随机访问** | 不支持下标操作 | 需要 O(n) 遍历 |
| **异常安全** | 访问前检查 `empty()` | 避免空链表访问 |
| **移动语义** | 优先使用移动版本 | 减少不必要拷贝 |

### 💡 完整使用示例

```cpp
using namespace template_container::list_container;

int main()
{
    // 1. 构造函数示例
    list<int> l1;                           // 默认构造
    list<std::string> l2 = {"hello", "world", "!"};  // 初始化列表
    list<std::string> l3(l2);               // 拷贝构造
    
    // 2. 插入操作
    l1.push_back(10);
    l1.push_back(20);
    l1.push_front(5);
    
    // 指定位置插入
    auto it = l1.begin();
    ++it;  // 指向第二个元素
    l1.insert(it, 15);
    
    // 3. 删除操作
    l1.pop_back();                          // 删除尾部
    l1.pop_front();                         // 删除头部
    
    it = l1.begin();
    l1.erase(it);                           // 删除指定位置
    
    // 4. 迭代器遍历
    std::cout << "正向遍历: ";
    for (auto list_it = l2.begin(); list_it != l2.end(); ++list_it) 
    {
        std::cout << *list_it << " ";
    }
    
    std::cout << "\n反向遍历: ";
    for (auto rev_it = l2.rbegin(); rev_it != l2.rend(); ++rev_it) 
    {
        std::cout << *rev_it << " ";
    }
    
    // 5. 其他操作
    std::cout << "\nSize: " << l2.size() << std::endl;
    std::cout << "Empty: " << (l2.empty() ? "true" : "false") << std::endl;
    
    l2.resize(5, "default");                // 调整大小
    
    // 6. 运算符重载
    list<std::string> l4 = {"a", "b"};
    l4 += l2;                               // 拼接
    list<std::string> l5 = l4 + l2;         // 创建新链表
    
    return 0;
}
```

### 🎯 `stack` 类概览

`stack` 类是一个基于向量容器的栈适配器，采用适配器设计模式实现。通过模板参数 `stack_type` 支持任意数据类型，并可指定底层容器类型（默认为 `template_container::vector_container::vector`）。该类将向量的接口转换为栈的后进先出（LIFO）接口，提供了标准栈的所有核心操作。

### 🏗️ 类定义与结构

```cpp
namespace stack_adapter 
{
    template <typename stack_type, typename vector_based_stack = template_container::vector_container::vector<stack_type>>
    class stack 
    {
    private:
        vector_based_stack _vector_object;  // 底层容器对象
        
    public:
        stack() = default;
        ~stack();

        // 构造与赋值
        explicit stack(const stack<stack_type>& stack_data);
        explicit stack(stack<stack_type>&& stack_data) noexcept;
        stack(std::initializer_list<stack_type> stack_type_data);
        explicit stack(const stack_type& stack_type_data);
        stack& operator=(const stack<stack_type>& stack_data);
        stack& operator=(stack<stack_type>&& stack_data) noexcept;

        // 栈操作
        void push(const stack_type& stack_type_data);
        void push(stack_type&& stack_type_data);
        void pop();
        [[nodiscard]] stack_type& top() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
        size_t size() noexcept;
        stack_type& footer() noexcept;
    };
}
```

### 🧱 适配器设计模式

#### 📊 设计特点

| 特点 | 说明 | 优势 |
|------|------|------|
| **适配器模式** | 封装底层 `vector` 容器 | 复用现有容器功能 |
| **LIFO 语义** | 后进先出的访问模式 | 符合栈的标准行为 |
| **模板化** | 支持任意数据类型 | 类型安全和泛用性 |
| **底层可配置** | 可指定底层容器类型 | 灵活的实现选择 |

#### 🔄 操作映射

| 栈操作 | 底层容器操作 | 说明 |
|--------|-------------|------|
| `push()` | `push_back()` | 在容器末尾添加元素 |
| `pop()` | `pop_back()` | 移除容器末尾元素 |
| `top()` | `back()` | 访问容器末尾元素 |
| `empty()` | `empty()` | 检查容器是否为空 |
| `size()` | `size()` | 获取容器元素数量 |

### 🔧 构造与析构

#### 构造函数类型

| 构造函数 | 功能 | 特点 |
|----------|------|------|
| **默认构造** | `stack() = default` | 创建空栈 |
| **拷贝构造** | `stack(const stack&)` | 深拷贝底层容器 |
| **移动构造** | `stack(stack&&) noexcept` | 接管底层容器资源 |
| **初始化列表** | `stack(std::initializer_list<T>)` | 按顺序将列表值压入栈中 |
| **单值构造** | `stack(const T&)` | 将单个元素压入栈 |

#### 💡 构造示例

```cpp
using namespace template_container::stack_adapter;

// 默认构造
stack<int> s1;                          // 空栈

// 元素构造
stack<double> s2(3.14);                 // 栈中有一个元素 3.14

// 初始化列表构造
stack<std::string> s3 = {"hello", "world"};  // 按顺序压入

// 拷贝构造
stack<double> s4(s2);                   // 深拷贝

// 移动构造
stack<double> s5(std::move(s2));        // s2 变为空
```

### 🎯 栈操作

#### 核心操作

| 方法 | 功能 | 时间复杂度 | 返回值 |
|------|------|------------|--------|
| `push(const T&)` | 压入元素（拷贝） | 均摊 O(1) | `void` |
| `push(T&&)` | 压入元素（移动） | 均摊 O(1) | `void` |
| `pop()` | 弹出栈顶元素 | O(1) | `void` |
| `top()` | 访问栈顶元素 | O(1) | `T&` |
| `footer()` | 访问栈顶元素（别名） | O(1) | `T&` |

#### 查询操作

| 方法 | 功能 | 时间复杂度 | 返回值 |
|------|------|------------|--------|
| `empty()` | 检查栈是否为空 | O(1) | `bool` |
| `size()` | 获取栈中元素数量 | O(1) | `size_t` |

### ⚡ 性能特点

#### 时间复杂度

| 操作类型 | 时间复杂度 | 说明 |
|----------|------------|------|
| **压入元素** | 均摊 O(1) | 底层容器扩容时为 O(n) |
| **弹出元素** | O(1) | 直接删除末尾元素 |
| **访问栈顶** | O(1) | 直接访问末尾元素 |
| **查询状态** | O(1) | 直接查询底层容器 |

#### 空间复杂度

- **存储空间**：O(n)，n 为栈中元素数量
- **额外开销**：底层容器的额外容量开销

### 🚨 异常安全与注意事项

#### 异常处理

| 异常类型 | 触发条件 | 处理建议 |
|----------|----------|----------|
| `std::bad_alloc` | 内存分配失败 | 使用 try-catch 处理 |
| **未定义行为** | 空栈调用 `top()` 或 `pop()` | 调用前检查 `empty()` |

#### 使用注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **空栈检查** | `top()` 和 `pop()` 不检查空栈 | 操作前调用 `empty()` |
| **无迭代器** | 适配器不提供迭代接口 | 只能通过 `pop`/`top` 访问 |
| **异常安全** | `push` 失败时无副作用 | 保持旧状态不变 |
| **线程安全** | 非线程安全 | 多线程访问需加锁 |

### 🔧 运算符重载

#### 赋值运算符

| 运算符 | 功能 | 特点 |
|--------|------|------|
| `operator=(const stack&)` | 拷贝赋值 | 深拷贝底层容器 |
| `operator=(stack&&)` | 移动赋值 | 接管底层容器资源 |

### 💡 完整使用示例

```cpp
using namespace template_container::stack_adapter;

int main()
{
    // 1. 构造函数示例
    std::cout << "=== 构造函数示例 ===\n";
    
    // 默认构造
    stack<int> s1;
    std::cout << "s1 (默认构造，空栈): size=" << s1.size() << std::endl;
    
    // 元素构造
    stack<double> s2(3.14);
    std::cout << "s2 (元素构造): top=" << s2.top() << std::endl;
    
    // 初始化列表构造
    stack<std::string> s3 = {"hello", "world"};
    std::cout << "s3 (初始化列表构造): ";
    while (!s3.empty()) 
    {
        std::cout << s3.top() << " ";
        s3.pop();
    }
    std::cout << std::endl;
    
    // 2. 栈操作示例
    std::cout << "\n=== 栈操作示例 ===\n";
    
    stack<int> s4;
    s4.push(10);
    s4.push(20);
    s4.push(30);
    
    std::cout << "s4 压栈后 size=" << s4.size() << ", top=" << s4.top() << std::endl;
    
    s4.pop();
    std::cout << "s4 弹栈后 size=" << s4.size() << ", top=" << s4.top() << std::endl;
    
    // 3. 拷贝与移动示例
    std::cout << "\n=== 拷贝与移动示例 ===\n";
    
    // 拷贝构造
    stack<int> s5(s4);
    std::cout << "s5 (拷贝构造自 s4): top=" << s5.top() << std::endl;
    
    // 移动构造
    stack<int> s6(std::move(s5));
    std::cout << "s6 (移动构造自 s5): top=" << s6.top() << ", s5 size=" << s5.size() << std::endl;
    
    return 0;
}
```

---

