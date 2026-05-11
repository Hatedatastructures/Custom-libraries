## 🚶 队列适配器 `queue_adapter`

    // 7. 异常处理示例
    std::cout << "\n=== 异常处理示例 ===\n";

### 🎯 队列概览

#### 🔄 `queue` - 先进先出队列

`queue` 类是一个基于双向链表的队列适配器，采用先进先出（FIFO）的操作方式。通过模板参数 `queue_type` 支持任意数据类型，并可指定底层容器类型（默认为 `template_container::list_container::list`）。该类将链表的接口转换为队列的标准接口，提供了队列的所有核心操作。

#### 🏗️ 类定义与结构

```cpp
namespace queue_adapter 
{
    template <typename queue_type, typename list_based_queue = template_container::list_container::list<queue_type>>
    class queue 
    {
    private:
        list_based_queue _list_object;  // 底层链表对象
        
    public:
        queue() = default;
        ~queue();

        // 构造与赋值
        explicit queue(const queue& queue_data);
        explicit queue(queue&& queue_data) noexcept;
        queue(std::initializer_list<queue_type> list_data);
        explicit queue(const queue_type& single_data);
        queue& operator=(const queue& queue_data);
        queue& operator=(queue&& queue_data) noexcept;

        // 核心操作
        void push(const queue_type& data);
        void push(queue_type&& data);
        void pop();
        [[nodiscard]] queue_type& front() noexcept;
        [[nodiscard]] queue_type& back() noexcept;
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
    };
}
```

#### 🧱 适配器设计

| 特点 | 说明 | 优势 |
|------|------|------|
| **FIFO 语义** | 先进先出的访问模式 | 符合队列的标准行为 |
| **链表底层** | 基于双向链表实现 | O(1) 插入删除性能 |
| **适配器模式** | 封装底层链表容器 | 复用现有容器功能 |
| **模板化** | 支持任意数据类型 | 类型安全和泛用性 |

#### 🔄 操作映射

| 队列操作 | 底层链表操作 | 说明 |
|----------|-------------|------|
| `push()` | `push_back()` | 在链表尾部添加元素 |
| `pop()` | `pop_front()` | 移除链表头部元素 |
| `front()` | `front()` | 访问链表头部元素 |
| `back()` | `back()` | 访问链表尾部元素 |
| `empty()` | `empty()` | 检查链表是否为空 |
| `size()` | `size()` | 获取链表元素数量 |

#### 💡 使用示例

```cpp
using namespace template_container::queue_adapter;

int main()
{
    // 1. 构造函数示例
    queue<int> q1;                          // 默认构造
    queue<double> q2(3.14);                 // 元素构造
    queue<std::string> q3 = {"hello", "world"};  // 初始化列表
    
    // 2. 队列操作
    q1.push(10);
    q1.push(20);
    q1.push(30);
    
    std::cout << "队首: " << q1.front() << ", 队尾: " << q1.back() << std::endl;
    
    q1.pop();
    std::cout << "出队后队首: " << q1.front() << std::endl;
    
    return 0;
}
```

---

#### 🎯 `priority_queue` - 优先级队列

`priority_queue` 类是一个基于动态数组的优先级队列实现，采用堆数据结构。支持自定义比较器，默认为大顶堆（最大元素优先）。

#### 🏗️ 类定义与结构

```cpp
namespace queue_adapter 
{
    template <typename priority_queue_type, 
              typename compare = template_container::imitation_functions::less<priority_queue_type>,
              typename vector_based_priority_queue = template_container::vector_container::vector<priority_queue_type>>
    class priority_queue 
    {
    private:
        vector_based_priority_queue _data;  // 底层数组容器
        compare _comp;                       // 比较器
        
        void adjust_up(int idx) noexcept;    // 向上调整堆
        void adjust_down(int idx = 0) noexcept;  // 向下调整堆
        
    public:
        priority_queue() = default;
        ~priority_queue() noexcept;
        priority_queue(std::initializer_list<priority_queue_type> init);
        priority_queue(const priority_queue& other);
        priority_queue(priority_queue&& other) noexcept;
        explicit priority_queue(const priority_queue_type& single);
        priority_queue& operator=(const priority_queue& other);
        priority_queue& operator=(priority_queue&& other) noexcept;

        // 核心操作
        void push(const priority_queue_type& value);
        [[nodiscard]] priority_queue_type& top() noexcept;
        void pop();
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
    };
}
```

#### 🏔️ 堆数据结构

| 特性 | 说明 | 优势 |
|------|------|------|
| **堆性质** | 父节点优先级 ≥ 子节点优先级 | 保证堆顶为最高优先级 |
| **完全二叉树** | 基于数组的紧凑存储 | 空间效率高，缓存友好 |
| **动态调整** | 插入删除后自动维护堆性质 | 保持优先级队列语义 |
| **自定义比较** | 支持用户定义的比较器 | 灵活的优先级定义 |

#### 🔄 堆操作

| 操作 | 算法 | 时间复杂度 |
|------|------|------------|
| `push()` | 末尾插入 + 向上调整 | O(log n) |
| `pop()` | 交换首尾 + 删除末尾 + 向下调整 | O(log n) |
| `top()` | 直接访问首元素 | O(1) |
| `empty()` / `size()` | 查询底层容器 | O(1) |

#### 💡 使用示例

```cpp
using namespace template_container::queue_adapter;

int main() 
{
    // 1. 默认构造（大顶堆）
    priority_queue<int> pq1;
    pq1.push(3);
    pq1.push(1);
    pq1.push(4);
    pq1.push(1);
    pq1.push(5);
    
    std::cout << "大顶堆: ";
    while (!pq1.empty()) 
    {
        std::cout << pq1.top() << " ";  // 输出: 5 4 3 1 1
        pq1.pop();
    }
    std::cout << std::endl;
    
    // 2. 自定义比较器（小顶堆）
    using MinHeap = priority_queue<int, template_container::imitation_functions::greater<int>>;
    
    MinHeap pq2;
    pq2.push(3);
    pq2.push(1);
    pq2.push(4);
    
    std::cout << "小顶堆: ";
    while (!pq2.empty()) 
    {
        std::cout << pq2.top() << " ";  // 输出: 1 3 4
        pq2.pop();
    }
    std::cout << std::endl;
    
    return 0;
}
```

### ⚡ 性能对比

#### 时间复杂度对比

| 操作 | `queue` | `priority_queue` | 说明 |
|------|---------|------------------|------|
| **插入** | O(1) | O(log n) | 队列直接插入，优先队列需调整堆 |
| **删除** | O(1) | O(log n) | 队列直接删除，优先队列需调整堆 |
| **访问** | O(1) | O(1) | 都是直接访问 |
| **查询** | O(1) | O(1) | 状态查询都是常量时间 |

#### 适用场景

| 容器 | 适用场景 | 典型应用 |
|------|----------|----------|
| **queue** | 需要 FIFO 顺序处理 | 任务队列、BFS 算法、缓冲区 |
| **priority_queue** | 需要优先级处理 | 任务调度、Dijkstra 算法、事件模拟 |

### 🚨 注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **空容器检查** | 操作前检查 `empty()` | 避免未定义行为 |
| **无迭代器** | 适配器不提供迭代接口 | 只能通过标准操作访问 |
| **线程安全** | 非线程安全 | 多线程环境需外部同步 |
| **比较器一致性** | 优先队列的比较器需满足严格弱序 | 确保堆性质正确维护 |

---

### 🎯 树形容器概览

树形容器提供了多种基于树结构的数据容器，包括二叉搜索树、AVL 树等自平衡树结构。这些容器支持高效的查找、插入和删除操作，适用于需要有序存储和快速检索的场景。

### 🌲 `binary_search_tree` - 二叉搜索树

#### 🏗️ 类定义与结构

```cpp
namespace tree_container 
{
    template <typename tree_type, typename compare = template_container::imitation_functions::less<tree_type>>
    class binary_search_tree 
    {
    public:
        // 嵌套节点结构
        template<typename tree_type_function_node>
        struct tree_container_node;
        
        // 迭代器类型
        template<typename treeNodeTypeIterator, typename Ref, typename Ptr>
        class tree_iterator;
        
        using container_node = tree_container_node<tree_type>;
        using iterator = tree_iterator<tree_type, tree_type&, tree_type*>;
        using const_iterator = tree_iterator<tree_type, const tree_type&, const tree_type*>;
        
    private:
        container_node* _root;  // 根节点指针
        compare _comp;          // 比较器
        size_t _size;          // 节点数量
        
    public:
        // 构造与析构
        binary_search_tree();
        ~binary_search_tree() noexcept;
        binary_search_tree(const binary_search_tree& other);
        binary_search_tree(binary_search_tree&& other) noexcept;
        binary_search_tree(std::initializer_list<tree_type> init);
        
        // 核心操作
        iterator insert(const tree_type& value);
        iterator insert(tree_type&& value);
        iterator find(const tree_type& value);
        const_iterator find(const tree_type& value) const;
        bool erase(const tree_type& value);
        iterator erase(iterator pos);
        
        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        void clear() noexcept;
        
        // 迭代器接口
        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
        
        // 树特有操作
        iterator lower_bound(const tree_type& value);
        iterator upper_bound(const tree_type& value);
        size_t count(const tree_type& value) const;
    };
}
```

#### 🧱 节点结构

```cpp
template<typename tree_type_function_node>
struct tree_container_node 
{
    tree_type_function_node _data;     // 存储的数据
    tree_container_node* _left;        // 左子节点
    tree_container_node* _right;       // 右子节点
    tree_container_node* _parent;      // 父节点
    
    explicit tree_container_node(const tree_type_function_node& val);
    ~tree_container_node();
};
```

#### 📊 BST 特性

| 特性 | 说明 | 优势 |
|------|------|------|
| **有序性** | 左子树 < 根节点 < 右子树 | 支持有序遍历 |
| **查找效率** | 平均 O(log n)，最坏 O(n) | 快速定位元素 |
| **动态结构** | 支持动态插入删除 | 灵活的数据管理 |
| **中序遍历** | 自动获得有序序列 | 排序功能内置 |

#### 🔄 核心操作

| 操作 | 平均时间复杂度 | 最坏时间复杂度 | 说明 |
|------|---------------|---------------|------|
| `insert()` | O(log n) | O(n) | 插入新节点 |
| `find()` | O(log n) | O(n) | 查找指定值 |
| `erase()` | O(log n) | O(n) | 删除节点 |
| `lower_bound()` | O(log n) | O(n) | 查找第一个不小于给定值的元素 |
| `upper_bound()` | O(log n) | O(n) | 查找第一个大于给定值的元素 |

#### 💡 使用示例

```cpp
using namespace template_container::tree_container;

int main()
{
    // 1. 构造和插入
    binary_search_tree<int> bst;
    bst.insert(50);
    bst.insert(30);
    bst.insert(70);
    bst.insert(20);
    bst.insert(40);
    bst.insert(60);
    bst.insert(80);
    
    // 2. 查找操作
    auto it = bst.find(40);
    if (it != bst.end()) 
    {
        std::cout << "找到元素: " << *it << std::endl;
    }
    
    // 3. 有序遍历
    std::cout << "中序遍历: ";
    for (auto val : bst) 
    {
        std::cout << val << " ";  // 输出: 20 30 40 50 60 70 80
    }
    std::cout << std::endl;
    
    // 4. 范围查询
    auto lower = bst.lower_bound(35);
    auto upper = bst.upper_bound(65);
    std::cout << "范围 [35, 65]: ";
    for (auto it = lower; it != upper; ++it) 
    {
        std::cout << *it << " ";  // 输出: 40 50 60
    }
    std::cout << std::endl;
    
    return 0;
}
```

---

### 🏔️ `avl_tree` - AVL 自平衡树

#### 🏗️ 类定义与结构

```cpp
namespace tree_container 
{
    template <typename tree_type, typename compare = template_container::imitation_functions::less<tree_type>>
    class avl_tree 
    {
    public:
        // 继承 BST 的基本结构
        using base_tree = binary_search_tree<tree_type, compare>;
        
        // AVL 特有的节点结构
        template<typename avl_tree_type_function_node>
        struct avl_tree_container_node;
        
        using container_node = avl_tree_container_node<tree_type>;
        using iterator = typename base_tree::iterator;
        using const_iterator = typename base_tree::const_iterator;
        
    private:
        container_node* _root;  // 根节点指针
        compare _comp;          // 比较器
        size_t _size;          // 节点数量
        
        // AVL 特有的平衡操作
        int get_height(container_node* node) const noexcept;
        int get_balance_factor(container_node* node) const noexcept;
        container_node* rotate_left(container_node* node) noexcept;
        container_node* rotate_right(container_node* node) noexcept;
        container_node* balance_node(container_node* node) noexcept;
        
    public:
        // 构造与析构
        avl_tree();
        ~avl_tree() noexcept;
        avl_tree(const avl_tree& other);
        avl_tree(avl_tree&& other) noexcept;
        avl_tree(std::initializer_list<tree_type> init);
        
        // 重写的平衡操作
        iterator insert(const tree_type& value);
        iterator insert(tree_type&& value);
        bool erase(const tree_type& value);
        iterator erase(iterator pos);
        
        // 继承的操作
        using base_tree::find;
        using base_tree::empty;
        using base_tree::size;
        using base_tree::clear;
        using base_tree::begin;
        using base_tree::end;
        using base_tree::lower_bound;
        using base_tree::upper_bound;
        
        // AVL 特有操作
        bool is_balanced() const noexcept;
        int height() const noexcept;
    };
}
```

#### 🧱 AVL 节点结构

```cpp
template<typename avl_tree_type_function_node>
struct avl_tree_container_node 
{
    avl_tree_type_function_node _data;     // 存储的数据
    avl_tree_container_node* _left;        // 左子节点
    avl_tree_container_node* _right;       // 右子节点
    avl_tree_container_node* _parent;      // 父节点
    int _height;                           // 节点高度
    
    explicit avl_tree_container_node(const avl_tree_type_function_node& val);
    ~avl_tree_container_node();
};
```

#### ⚖️ AVL 平衡特性

| 特性 | 说明 | 优势 |
|------|------|------|
| **平衡因子** | 左右子树高度差 ≤ 1 | 保证 O(log n) 性能 |
| **自动平衡** | 插入删除后自动调整 | 避免退化为链表 |
| **旋转操作** | 左旋、右旋维护平衡 | 高效的平衡调整 |
| **高度平衡** | 树高度始终为 O(log n) | 稳定的查找性能 |

#### 🔄 平衡操作

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| **左旋转** | O(1) | 处理右重情况 |
| **右旋转** | O(1) | 处理左重情况 |
| **双旋转** | O(1) | 处理复杂不平衡 |
| **平衡检查** | O(1) | 计算平衡因子 |

#### 📈 性能对比

| 操作 | BST (平均) | BST (最坏) | AVL | 说明 |
|------|-----------|-----------|-----|------|
| **查找** | O(log n) | O(n) | O(log n) | AVL 保证对数时间 |
| **插入** | O(log n) | O(n) | O(log n) | AVL 需要额外平衡 |
| **删除** | O(log n) | O(n) | O(log n) | AVL 保证稳定性能 |
| **空间** | O(n) | O(n) | O(n) | AVL 需要存储高度信息 |

#### 💡 使用示例

```cpp
using namespace template_container::tree_container;

int main()
{
    // 1. 构造 AVL 树
    avl_tree<int> avl;
    
    // 2. 插入数据（自动平衡）
    std::vector<int> data = {10, 20, 30, 40, 50, 25};
    for (int val : data) 
    {
        avl.insert(val);
        std::cout << "插入 " << val << " 后，树高度: " << avl.height() 
                  << ", 是否平衡: " << (avl.is_balanced() ? "是" : "否") << std::endl;
    }
    
    // 3. 有序遍历
    std::cout << "AVL 树中序遍历: ";
    for (auto val : avl) 
    {
        std::cout << val << " ";  // 输出: 10 20 25 30 40 50
    }
    std::cout << std::endl;
    
    // 4. 删除操作
    avl.erase(30);
    std::cout << "删除 30 后，树高度: " << avl.height() 
              << ", 是否平衡: " << (avl.is_balanced() ? "是" : "否") << std::endl;
    
    return 0;
}
```

### 🎯 树容器选择指南

| 使用场景 | 推荐容器 | 原因 |
|----------|----------|------|
| **一般有序存储** | `binary_search_tree` | 实现简单，平均性能好 |
| **性能敏感场景** | `avl_tree` | 保证 O(log n) 性能 |
| **频繁插入删除** | `avl_tree` | 自动平衡，避免性能退化 |
| **静态数据集** | `binary_search_tree` | 无需平衡开销 |

### 🚨 注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **比较器一致性** | 必须满足严格弱序关系 | 确保树结构正确 |
| **迭代器失效** | 插入删除可能导致失效 | 操作后重新获取迭代器 |
| **线程安全** | 非线程安全 | 多线程环境需外部同步 |
| **内存管理** | 节点动态分配 | 注意内存泄漏 |

---

## 🏛️ 基类容器 `base_class_container`

    // 3. 删除操作示例
    std::cout << "\n=== 删除操作示例 ===\n";

### 🎯 基类容器概览

基类容器提供了多种高级数据结构的实现，包括红黑树、哈希表和位集等。这些容器作为更复杂数据结构的基础，提供了高效的存储和检索机制。

### 🔴 `rb_tree` - 红黑树

#### 🏗️ 类定义与结构

```cpp
namespace base_class_container 
{
    template <typename rb_tree_type, typename compare = template_container::imitation_functions::less<rb_tree_type>>
    class rb_tree 
    {
    public:
        // 节点颜色枚举
        enum class Color { RED, BLACK };
        
        // 节点结构
        template<typename rb_tree_type_function_node>
        struct rb_tree_container_node;
        
        // 迭代器类型
        template<typename rbTreeNodeTypeIterator, typename Ref, typename Ptr>
        class rb_tree_iterator;
        
        using container_node = rb_tree_container_node<rb_tree_type>;
        using iterator = rb_tree_iterator<rb_tree_type, rb_tree_type&, rb_tree_type*>;
        using const_iterator = rb_tree_iterator<rb_tree_type, const rb_tree_type&, const rb_tree_type*>;
        
    private:
        container_node* _root;      // 根节点指针
        container_node* _nil;       // NIL 哨兵节点
        compare _comp;              // 比较器
        size_t _size;              // 节点数量
        
        // 红黑树特有的平衡操作
        void left_rotate(container_node* x) noexcept;
        void right_rotate(container_node* x) noexcept;
        void insert_fixup(container_node* z) noexcept;
        void delete_fixup(container_node* x) noexcept;
        
    public:
        // 构造与析构
        rb_tree();
        ~rb_tree() noexcept;
        rb_tree(const rb_tree& other);
        rb_tree(rb_tree&& other) noexcept;
        rb_tree(std::initializer_list<rb_tree_type> init);
        
        // 核心操作
        iterator insert(const rb_tree_type& value);
        iterator insert(rb_tree_type&& value);
        iterator find(const rb_tree_type& value);
        const_iterator find(const rb_tree_type& value) const;
        bool erase(const rb_tree_type& value);
        iterator erase(iterator pos);
        
        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        void clear() noexcept;
        
        // 迭代器接口
        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
        
        // 红黑树特有操作
        bool is_valid_rb_tree() const noexcept;
        int black_height() const noexcept;
    };
}
```

#### 🧱 红黑树节点结构

```cpp
template<typename rb_tree_type_function_node>
struct rb_tree_container_node 
{
    rb_tree_type_function_node _data;      // 存储的数据
    rb_tree_container_node* _left;         // 左子节点
    rb_tree_container_node* _right;        // 右子节点
    rb_tree_container_node* _parent;       // 父节点
    Color _color;                          // 节点颜色
    
    explicit rb_tree_container_node(const rb_tree_type_function_node& val, Color c = Color::RED);
    ~rb_tree_container_node();
};
```

#### 🔴⚫ 红黑树性质

| 性质 | 说明 | 作用 |
|------|------|------|
| **根节点黑色** | 根节点必须是黑色 | 统一树的起始状态 |
| **红节点子节点黑色** | 红色节点的子节点必须是黑色 | 避免连续红节点 |
| **NIL 节点黑色** | 所有 NIL 节点都是黑色 | 简化边界处理 |
| **黑高度相等** | 从任一节点到其叶子节点的黑色节点数相同 | 保证平衡性 |

#### 🔄 平衡操作

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| **左旋转** | O(1) | 调整局部结构 |
| **右旋转** | O(1) | 调整局部结构 |
| **插入修复** | O(log n) | 维护红黑树性质 |
| **删除修复** | O(log n) | 维护红黑树性质 |

#### 💡 使用示例

```cpp
using namespace template_container::base_class_container;

int main()
{
    // 1. 构造红黑树
    rb_tree<int> rbt;
    
    // 2. 插入数据
    std::vector<int> data = {10, 5, 15, 3, 7, 12, 18};
    for (int val : data) 
    {
        rbt.insert(val);
        std::cout << "插入 " << val << " 后，黑高度: " << rbt.black_height() 
                  << ", 是否有效: " << (rbt.is_valid_rb_tree() ? "是" : "否") << std::endl;
    }
    
    // 3. 有序遍历
    std::cout << "红黑树中序遍历: ";
    for (auto val : rbt) 
    {
        std::cout << val << " ";  // 输出: 3 5 7 10 12 15 18
    }
    std::cout << std::endl;
    
    return 0;
}
```

---

### 🗂️ `hash_table` - 哈希表

#### 🏗️ 类定义与结构

```cpp
namespace base_class_container 
{
    template <typename hash_table_key, 
              typename hash_table_value,
              typename hasher = template_container::imitation_functions::hash_imitation_functions,
              typename key_equal = template_container::imitation_functions::equal_to<hash_table_key>>
    class hash_table 
    {
    public:
        // 键值对类型
        using key_type = hash_table_key;
        using mapped_type = hash_table_value;
        using value_type = template_container::practicality::pair<const key_type, mapped_type>;
        
        // 节点结构
        template<typename hash_table_type_function_node>
        struct hash_table_container_node;
        
        using container_node = hash_table_container_node<value_type>;
        using bucket_type = template_container::list_container::list<container_node>;
        
    private:
        template_container::vector_container::vector<bucket_type> _buckets;  // 桶数组
        hasher _hash;                                                        // 哈希函数
        key_equal _equal;                                                    // 键比较器
        size_t _size;                                                       // 元素数量
        double _max_load_factor;                                            // 最大负载因子
        
        // 哈希表内部操作
        size_t bucket_index(const key_type& key) const noexcept;
        void rehash_if_needed();
        void rehash(size_t new_bucket_count);
        
    public:
        // 构造与析构
        hash_table(size_t bucket_count = 16, double max_load = 0.75);
        ~hash_table() noexcept;
        hash_table(const hash_table& other);
        hash_table(hash_table&& other) noexcept;
        hash_table(std::initializer_list<value_type> init);
        
        // 核心操作
        template_container::practicality::pair<iterator, bool> insert(const value_type& value);
        template_container::practicality::pair<iterator, bool> insert(value_type&& value);
        iterator find(const key_type& key);
        const_iterator find(const key_type& key) const;
        bool erase(const key_type& key);
        iterator erase(iterator pos);
        
        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        size_t bucket_count() const noexcept;
        double load_factor() const noexcept;
        void clear() noexcept;
        
        // 下标访问
        mapped_type& operator[](const key_type& key);
        mapped_type& at(const key_type& key);
        const mapped_type& at(const key_type& key) const;
    };
}
```

#### 🧱 哈希表特性

| 特性 | 说明 | 优势 |
|------|------|------|
| **链式哈希** | 使用链表解决冲突 | 简单有效的冲突处理 |
| **动态扩容** | 负载因子超限时自动扩容 | 维持良好性能 |
| **自定义哈希** | 支持用户定义哈希函数 | 适应不同数据类型 |
| **负载因子控制** | 可配置最大负载因子 | 平衡空间和时间效率 |

#### 🔄 核心操作性能

| 操作 | 平均时间复杂度 | 最坏时间复杂度 | 说明 |
|------|---------------|---------------|------|
| `insert()` | O(1) | O(n) | 扩容时需要重哈希 |
| `find()` | O(1) | O(n) | 取决于冲突程度 |
| `erase()` | O(1) | O(n) | 需要在链表中查找 |
| `operator[]` | O(1) | O(n) | 可能触发插入操作 |

#### 💡 使用示例

```cpp
using namespace template_container::base_class_container;

int main()
{
    // 1. 构造哈希表
    hash_table<std::string, int> ht;
    
    // 2. 插入键值对
    ht.insert({"apple", 5});
    ht.insert({"banana", 3});
    ht.insert({"orange", 8});
    
    // 3. 使用下标访问
    ht["grape"] = 12;
    
    // 4. 查找操作
    auto it = ht.find("apple");
    if (it != ht.end()) 
    {
        std::cout << "找到: " << it->first << " = " << it->second << std::endl;
    }
    
    // 5. 遍历哈希表
    std::cout << "哈希表内容: ";
    for (const auto& pair : ht) 
    {
        std::cout << "{" << pair.first << ": " << pair.second << "} ";
    }
    std::cout << std::endl;
    
    std::cout << "负载因子: " << ht.load_factor() << std::endl;
    
    return 0;
}
```

---

### 🎯 `bit_set` - 位集

#### 🏗️ 类定义与结构

```cpp
namespace base_class_container 
{
    template <size_t N>
    class bit_set 
    {
    private:
        static constexpr size_t BITS_PER_WORD = sizeof(size_t) * 8;
        static constexpr size_t WORD_COUNT = (N + BITS_PER_WORD - 1) / BITS_PER_WORD;
        
        size_t _data[WORD_COUNT];  // 存储位的数组
        
        // 内部辅助函数
        size_t word_index(size_t pos) const noexcept;
        size_t bit_index(size_t pos) const noexcept;
        size_t bit_mask(size_t pos) const noexcept;
        
    public:
        // 构造与析构
        bit_set() noexcept;
        bit_set(unsigned long long val) noexcept;
        bit_set(const bit_set& other) noexcept;
        bit_set& operator=(const bit_set& other) noexcept;
        
        // 位操作
        bit_set& set() noexcept;                    // 设置所有位为1
        bit_set& set(size_t pos, bool val = true); // 设置指定位
        bit_set& reset() noexcept;                  // 重置所有位为0
        bit_set& reset(size_t pos);                 // 重置指定位
        bit_set& flip() noexcept;                   // 翻转所有位
        bit_set& flip(size_t pos);                  // 翻转指定位
        
        // 查询操作
        bool test(size_t pos) const;                // 测试指定位
        bool all() const noexcept;                  // 是否所有位都为1
        bool any() const noexcept;                  // 是否有位为1
        bool none() const noexcept;                 // 是否所有位都为0
        size_t count() const noexcept;              // 统计1的个数
        size_t size() const noexcept;               // 返回位数
        
        // 位运算符
        bit_set operator~() const noexcept;
        bit_set& operator&=(const bit_set& other) noexcept;
        bit_set& operator|=(const bit_set& other) noexcept;
        bit_set& operator^=(const bit_set& other) noexcept;
        bit_set operator&(const bit_set& other) const noexcept;
        bit_set operator|(const bit_set& other) const noexcept;
        bit_set operator^(const bit_set& other) const noexcept;
        
        // 访问操作
        bool operator[](size_t pos) const noexcept;
        
        // 转换操作
        unsigned long long to_ullong() const;
        std::string to_string() const;
    };
}
```

#### 🧱 位集特性

| 特性 | 说明 | 优势 |
|------|------|------|
| **固定大小** | 编译时确定位数 | 高效的内存使用 |
| **位级操作** | 支持位运算符 | 高效的集合运算 |
| **紧凑存储** | 每位仅占用1bit | 极高的空间效率 |
| **快速统计** | 内置位计数功能 | O(1)或O(word_count)统计 |

#### 💡 使用示例

```cpp
using namespace template_container::base_class_container;

int main()
{
    // 1. 构造位集
    bit_set<32> bs1;                    // 全0位集
    bit_set<32> bs2(0b10101010);        // 从整数构造
    
    // 2. 位操作
    bs1.set(5);                         // 设置第5位
    bs1.set(10, true);                  // 设置第10位
    bs1.reset(3);                       // 重置第3位
    bs1.flip(7);                        // 翻转第7位
    
    // 3. 查询操作
    std::cout << "bs1[5] = " << bs1.test(5) << std::endl;
    std::cout << "bs1 count = " << bs1.count() << std::endl;
    std::cout << "bs1 any = " << bs1.any() << std::endl;
    std::cout << "bs1 all = " << bs1.all() << std::endl;
    
    // 4. 位运算
    bit_set<32> bs3 = bs1 & bs2;        // 按位与
    bit_set<32> bs4 = bs1 | bs2;        // 按位或
    bit_set<32> bs5 = bs1 ^ bs2;        // 按位异或
    bit_set<32> bs6 = ~bs1;             // 按位取反
    
    // 5. 转换操作
    std::cout << "bs1 as string: " << bs1.to_string() << std::endl;
    std::cout << "bs1 as ullong: " << bs1.to_ullong() << std::endl;
    
    return 0;
}
```

### 🎯 基类容器选择指南

| 使用场景 | 推荐容器 | 原因 |
|----------|----------|------|
| **有序存储 + 频繁插入删除** | `rb_tree` | 保证 O(log n) 性能 |
| **快速查找 + 键值存储** | `hash_table` | 平均 O(1) 查找性能 |
| **位操作 + 集合运算** | `bit_set` | 极高的空间效率 |
| **需要稳定性能** | `rb_tree` | 最坏情况性能保证 |

### 🚨 注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **哈希函数质量** | 影响哈希表性能 | 选择合适的哈希函数 |
| **负载因子控制** | 影响空间和时间平衡 | 根据场景调整负载因子 |
| **位集大小** | 编译时固定 | 根据实际需求选择大小 |
| **线程安全** | 所有容器都非线程安全 | 多线程环境需外部同步 |

---

### 🎯 关联式容器概览

关联式容器提供了基于键值对的存储和检索机制，包括有序的 map/set 和无序的 hash_map/hash_set。这些容器支持高效的查找、插入和删除操作，适用于需要快速检索和有序存储的场景。

### 🗺️ `tree_map` - 有序映射容器

#### 🏗️ 类定义与结构

```cpp
namespace map_container 
{
    template <typename map_key, 
              typename map_value,
              typename compare = template_container::imitation_functions::less<map_key>>
    class tree_map 
    {
    public:
        // 类型定义
        using key_type = map_key;
        using mapped_type = map_value;
        using value_type = template_container::practicality::pair<const key_type, mapped_type>;
        using key_compare = compare;
        
        // 底层红黑树
        using tree_type = template_container::base_class_container::rb_tree<value_type>;
        using iterator = typename tree_type::iterator;
        using const_iterator = typename tree_type::const_iterator;
        
    private:
        tree_type _tree;        // 底层红黑树
        key_compare _comp;      // 键比较器
        
    public:
        // 构造与析构
        tree_map();
        ~tree_map() noexcept;
        tree_map(const tree_map& other);
        tree_map(tree_map&& other) noexcept;
        tree_map(std::initializer_list<value_type> init);
        tree_map& operator=(const tree_map& other);
        tree_map& operator=(tree_map&& other) noexcept;
        
        // 核心操作
        template_container::practicality::pair<iterator, bool> insert(const value_type& value);
        template_container::practicality::pair<iterator, bool> insert(value_type&& value);
        iterator find(const key_type& key);
        const_iterator find(const key_type& key) const;
        bool erase(const key_type& key);
        iterator erase(iterator pos);
        
        // 下标访问
        mapped_type& operator[](const key_type& key);
        mapped_type& at(const key_type& key);
        const mapped_type& at(const key_type& key) const;
        
        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        void clear() noexcept;
        
        // 迭代器接口
        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
        
        // 有序容器特有操作
        iterator lower_bound(const key_type& key);
        iterator upper_bound(const key_type& key);
        template_container::practicality::pair<iterator, iterator> equal_range(const key_type& key);
        size_t count(const key_type& key) const;
    };
}
```

#### 🧱 tree_map 特性

| 特性 | 说明 | 优势 |
|------|------|------|
| **有序存储** | 按键的顺序自动排序 | 支持范围查询和有序遍历 |
| **红黑树底层** | 基于红黑树实现 | 保证 O(log n) 性能 |
| **唯一键** | 每个键只能出现一次 | 避免重复键的问题 |
| **下标访问** | 支持 `operator[]` | 方便的键值访问方式 |

#### 🔄 核心操作性能

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| `insert()` | O(log n) | 红黑树插入操作 |
| `find()` | O(log n) | 红黑树查找操作 |
| `erase()` | O(log n) | 红黑树删除操作 |
| `operator[]` | O(log n) | 可能触发插入操作 |
| `lower_bound()` / `upper_bound()` | O(log n) | 有序容器范围查询 |

#### 💡 使用示例

```cpp
using namespace template_container::map_container;

int main()
{
    // 1. 构造和插入
    tree_map<std::string, int> tm;
    tm.insert({"apple", 5});
    tm.insert({"banana", 3});
    tm.insert({"orange", 8});
    
    // 2. 下标访问
    tm["grape"] = 12;
    tm["apple"] = 6;  // 更新已存在的键
    
    // 3. 查找操作
    auto it = tm.find("banana");
    if (it != tm.end()) 
    {
        std::cout << "找到: " << it->first << " = " << it->second << std::endl;
    }
    
    // 4. 有序遍历
    std::cout << "有序遍历: ";
    for (const auto& pair : tm) 
    {
        std::cout << "{" << pair.first << ": " << pair.second << "} ";
    }
    std::cout << std::endl;
    
    // 5. 范围查询
    auto lower = tm.lower_bound("b");
    auto upper = tm.upper_bound("g");
    std::cout << "范围 [b, g): ";
    for (auto it = lower; it != upper; ++it) 
    {
        std::cout << "{" << it->first << ": " << it->second << "} ";
    }
    std::cout << std::endl;
    
    return 0;
}
```

---

### 🗂️ `hash_map` - 无序映射容器

#### 🏗️ 类定义与结构

```cpp
namespace map_container 
{
    template <typename map_key, 
              typename map_value,
              typename hasher = template_container::imitation_functions::hash_imitation_functions,
              typename key_equal = template_container::imitation_functions::equal_to<map_key>>
    class hash_map 
    {
    public:
        // 类型定义
        using key_type = map_key;
        using mapped_type = map_value;
        using value_type = template_container::practicality::pair<const key_type, mapped_type>;
        using hasher_type = hasher;
        using key_equal_type = key_equal;
        
        // 底层哈希表
        using table_type = template_container::base_class_container::hash_table<key_type, mapped_type, hasher, key_equal>;
        using iterator = typename table_type::iterator;
        using const_iterator = typename table_type::const_iterator;
        
    private:
        table_type _table;      // 底层哈希表
        
    public:
        // 构造与析构
        hash_map(size_t bucket_count = 16, double max_load = 0.75);
        ~hash_map() noexcept;
        hash_map(const hash_map& other);
        hash_map(hash_map&& other) noexcept;
        hash_map(std::initializer_list<value_type> init);
        hash_map& operator=(const hash_map& other);
        hash_map& operator=(hash_map&& other) noexcept;
        
        // 核心操作
        template_container::practicality::pair<iterator, bool> insert(const value_type& value);
        template_container::practicality::pair<iterator, bool> insert(value_type&& value);
        iterator find(const key_type& key);
        const_iterator find(const key_type& key) const;
        bool erase(const key_type& key);
        iterator erase(iterator pos);
        
        // 下标访问
        mapped_type& operator[](const key_type& key);
        mapped_type& at(const key_type& key);
        const mapped_type& at(const key_type& key) const;
        
        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        size_t bucket_count() const noexcept;
        double load_factor() const noexcept;
        void clear() noexcept;
        void rehash(size_t bucket_count);
        void reserve(size_t count);
        
        // 迭代器接口
        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
    };
}
```

#### 🧱 hash_map 特性

| 特性 | 说明 | 优势 |
|------|------|------|
| **无序存储** | 基于哈希值存储 | 平均 O(1) 访问性能 |
| **哈希表底层** | 链式哈希解决冲突 | 简单有效的冲突处理 |
| **动态扩容** | 负载因子控制扩容 | 维持良好性能 |
| **自定义哈希** | 支持用户定义哈希函数 | 适应不同数据类型 |

#### 💡 使用示例

```cpp
using namespace template_container::map_container;

int main()
{
    // 1. 构造哈希映射
    hash_map<std::string, int> hm;
    
    // 2. 插入和访问
    hm["apple"] = 5;
    hm["banana"] = 3;
    hm.insert({"orange", 8});
    
    // 3. 查找操作
    auto it = hm.find("apple");
    if (it != hm.end()) 
    {
        std::cout << "找到: " << it->first << " = " << it->second << std::endl;
    }
    
    // 4. 遍历（无序）
    std::cout << "哈希映射内容: ";
    for (const auto& pair : hm) 
    {
        std::cout << "{" << pair.first << ": " << pair.second << "} ";
    }
    std::cout << std::endl;
    
    // 5. 性能信息
    std::cout << "桶数量: " << hm.bucket_count() << std::endl;
    std::cout << "负载因子: " << hm.load_factor() << std::endl;
    
    return 0;
}
```

---

### 🔢 `tree_set` - 有序集合容器

#### 🏗️ 类定义与结构

```cpp
namespace set_container 
{
    template <typename set_type, typename compare = template_container::imitation_functions::less<set_type>>
    class tree_set 
    {
    public:
        // 类型定义
        using key_type = set_type;
        using value_type = set_type;
        using key_compare = compare;
        using value_compare = compare;
        
        // 底层红黑树
        using tree_type = template_container::base_class_container::rb_tree<value_type>;
        using iterator = typename tree_type::iterator;
        using const_iterator = typename tree_type::const_iterator;
        
    private:
        tree_type _tree;        // 底层红黑树
        key_compare _comp;      // 键比较器
        
    public:
        // 构造与析构
        tree_set();
        ~tree_set() noexcept;
        tree_set(const tree_set& other);
        tree_set(tree_set&& other) noexcept;
        tree_set(std::initializer_list<value_type> init);
        tree_set& operator=(const tree_set& other);
        tree_set& operator=(tree_set&& other) noexcept;
        
        // 核心操作
        template_container::practicality::pair<iterator, bool> insert(const value_type& value);
        template_container::practicality::pair<iterator, bool> insert(value_type&& value);
        iterator find(const key_type& key);
        const_iterator find(const key_type& key) const;
        bool erase(const key_type& key);
        iterator erase(iterator pos);
        
        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        void clear() noexcept;
        
        // 迭代器接口
        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
        
        // 有序容器特有操作
        iterator lower_bound(const key_type& key);
        iterator upper_bound(const key_type& key);
        template_container::practicality::pair<iterator, iterator> equal_range(const key_type& key);
        size_t count(const key_type& key) const;
        
        // 集合操作
        bool contains(const key_type& key) const;
    };
}
```

#### 💡 使用示例

```cpp
using namespace template_container::set_container;

int main()
{
    // 1. 构造和插入
    tree_set<int> ts = {5, 2, 8, 2, 1, 9};  // 重复元素会被忽略
    
    // 2. 插入操作
    auto result = ts.insert(3);
    if (result.second) 
    {
        std::cout << "成功插入: " << *result.first << std::endl;
    }
    
    // 3. 查找操作
    if (ts.contains(5)) 
    {
        std::cout << "集合包含元素 5" << std::endl;
    }
    
    // 4. 有序遍历
    std::cout << "有序集合: ";
    for (int val : ts) 
    {
        std::cout << val << " ";  // 输出: 1 2 3 5 8 9
    }
    std::cout << std::endl;
    
    // 5. 范围操作
    auto lower = ts.lower_bound(3);
    auto upper = ts.upper_bound(7);
    std::cout << "范围 [3, 7]: ";
    for (auto it = lower; it != upper; ++it) 
    {
        std::cout << *it << " ";  // 输出: 3 5
    }
    std::cout << std::endl;
    
    return 0;
}
```

---

### 🔢 `hash_set` - 无序集合容器

#### 🏗️ 类定义与结构

```cpp
namespace set_container 
{
    template <typename set_type, 
              typename hasher = template_container::imitation_functions::hash_imitation_functions,
              typename key_equal = template_container::imitation_functions::equal_to<set_type>>
    class hash_set 
    {
    public:
        // 类型定义
        using key_type = set_type;
        using value_type = set_type;
        using hasher_type = hasher;
        using key_equal_type = key_equal;
        
        // 底层哈希表（值作为键）
        using table_type = template_container::base_class_container::hash_table<key_type, key_type, hasher, key_equal>;
        using iterator = typename table_type::iterator;
        using const_iterator = typename table_type::const_iterator;
        
    private:
        table_type _table;      // 底层哈希表
        
    public:
        // 构造与析构
        hash_set(size_t bucket_count = 16, double max_load = 0.75);
        ~hash_set() noexcept;
        hash_set(const hash_set& other);
        hash_set(hash_set&& other) noexcept;
        hash_set(std::initializer_list<value_type> init);
        hash_set& operator=(const hash_set& other);
        hash_set& operator=(hash_set&& other) noexcept;
        
        // 核心操作
        template_container::practicality::pair<iterator, bool> insert(const value_type& value);
        template_container::practicality::pair<iterator, bool> insert(value_type&& value);
        iterator find(const key_type& key);
        const_iterator find(const key_type& key) const;
        bool erase(const key_type& key);
        iterator erase(iterator pos);
        
        // 容量与访问
        bool empty() const noexcept;
        size_t size() const noexcept;
        size_t bucket_count() const noexcept;
        double load_factor() const noexcept;
        void clear() noexcept;
        void rehash(size_t bucket_count);
        void reserve(size_t count);
        
        // 迭代器接口
        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
        
        // 集合操作
        bool contains(const key_type& key) const;
    };
}
```

#### 💡 使用示例

```cpp
using namespace template_container::set_container;

int main()
{
    // 1. 构造无序集合
    hash_set<std::string> hs = {"apple", "banana", "orange", "apple"};  // 重复会被忽略
    
    // 2. 插入操作
    auto result = hs.insert("grape");
    if (result.second) 
    {
        std::cout << "成功插入: " << *result.first << std::endl;
    }
    
    // 3. 查找操作
    if (hs.contains("banana")) 
    {
        std::cout << "集合包含 banana" << std::endl;
    }
    
    // 4. 遍历（无序）
    std::cout << "无序集合: ";
    for (const std::string& val : hs) 
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    // 5. 性能信息
    std::cout << "桶数量: " << hs.bucket_count() << std::endl;
    std::cout << "负载因子: " << hs.load_factor() << std::endl;
    
    return 0;
}
```

### 🎯 关联式容器选择指南

| 使用场景 | 推荐容器 | 原因 |
|----------|----------|------|
| **需要有序的键值映射** | `tree_map` | 支持范围查询和有序遍历 |
| **快速键值查找** | `hash_map` | 平均 O(1) 查找性能 |
| **需要有序的唯一值集合** | `tree_set` | 自动排序和去重 |
| **快速成员检查** | `hash_set` | 平均 O(1) 查找性能 |
| **范围查询需求** | `tree_map` / `tree_set` | 支持 lower_bound/upper_bound |
| **内存敏感场景** | `tree_map` / `tree_set` | 无哈希表的额外开销 |

### ⚡ 性能对比

#### 时间复杂度对比

| 操作 | tree_map/set | hash_map/set | 说明 |
|------|-------------|-------------|------|
| **插入** | O(log n) | 平均 O(1) | 哈希表在冲突少时更快 |
| **查找** | O(log n) | 平均 O(1) | 哈希表平均性能更优 |
| **删除** | O(log n) | 平均 O(1) | 哈希表平均性能更优 |
| **范围查询** | O(log n + k) | 不支持 | 树结构独有优势 |
| **有序遍历** | O(n) | 不支持 | 树结构自然有序 |

#### 空间复杂度对比

| 容器类型 | 空间复杂度 | 额外开销 |
|----------|------------|----------|
| **tree_map/set** | O(n) | 节点指针 + 颜色信息 |
| **hash_map/set** | O(n) | 桶数组 + 链表节点 |

### 🚨 注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **键的不可变性** | map 的键不能修改 | 使用 const key_type |
| **哈希函数质量** | 影响 hash 容器性能 | 选择合适的哈希函数 |
| **比较器一致性** | tree 容器需要严格弱序 | 确保比较器正确实现 |
| **迭代器失效** | 插入删除可能导致失效 | 操作后重新获取迭代器 |
| **线程安全** | 所有容器都非线程安全 | 多线程环境需外部同步 |

---

