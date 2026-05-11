## 哈希表
## `hash_table`
### 类概述
* `hash_table` 是一个模板化的哈希表实现，采用链地址法（拉链法）解决哈希冲突，支持键值对的高效存储与查询。
该类通过维护全局插入顺序链表和桶内链表，实现了快速哈希查找和有序遍历的双重功能，并具备动态扩容机制以保持高效性能。 

#### **类及其函数定义**

```cpp
template <typename hash_table_type_key,typename hash_table_type_value,typename container_imitate_function,
          typename hash_function = std::hash<hash_table_type_value>>
class hash_table 
{
private:
    class hash_table_node 
    {
    public:
        hash_table_type_value _data;                // 存储的值
        hash_table_node*      _next;                // 桶内链指针
        hash_table_node*      overall_list_prev;    // 全局链表前向指针
        hash_table_node*      overall_list_next;    // 全局链表后向指针

        explicit hash_table_node(const hash_table_type_value& v);
        explicit hash_table_node(hash_table_type_value&& v) noexcept;
    };

    using container_node = hash_table_node;
    container_imitate_function         value_imitation_functions; // 键比较仿函数
    hash_function                      hash_function_object;    // 哈希函数

    size_t _size;          // 当前元素数量
    size_t load_factor;    // 负载因子阈值*10
    size_t hash_capacity;  // 桶数量

    template_container::vector_container::vector<container_node*> vector_hash_table; // 桶数组
    container_node* overall_list_head_node;   // 全局插入链表头
    container_node* overall_list_before_node; // 全局插入链表尾

    // 链调整辅助
    void hash_chain_adjustment(container_node*& parent, container_node*& node, size_t index);

    // 全局链表及桶重建辅助（扩容时使用）

public:
    template <typename K, typename V>
    class hash_iterator 
    {
    private:
        container_node* ptr;
    public:
        explicit hash_iterator(container_node* p);
        V& operator*() const;
        V* operator->() const;
        hash_iterator& operator++();   // 顺序遍历整体链表
        hash_iterator operator++(int);
        bool operator==(const hash_iterator& o) const;
        bool operator!=(const hash_iterator& o) const;
    };

    using iterator = hash_iterator<hash_table_type_key, hash_table_type_value>;
    using const_iterator = hash_iterator<const hash_table_type_key, const hash_table_type_value>;

    // 构造与析构
    hash_table();
    explicit hash_table(size_t initial_capacity);
    hash_table(const hash_table& other);
    hash_table(hash_table&& other) noexcept;
    ~hash_table() noexcept;

    // 核心操作
    bool change_load_factor(size_t new_load_factor);
    bool push(const hash_table_type_value& v);
    bool push(hash_table_type_value&& v) noexcept;
    bool pop(const hash_table_type_value& v);
    iterator find(const hash_table_type_value& v);

    // 查询接口
    size_t size() const;
    bool empty() const;
    size_t capacity() const;

    iterator begin();
    const_iterator cbegin() const;
    static iterator end();
    static const_iterator cend();

    // 全局顺序遍历（按插入顺序）

    // 运算符重载
    iterator operator[](const hash_table_type_key& key);
    hash_table& operator=(const hash_table& other);
    hash_table& operator=(hash_table&& other) noexcept;
};
```

#### **作用描述**

1. **模板参数**

   * `hash_table_type_key`：用于查找的键类型。
   * `hash_table_type_value`：存储的值类型。
   * `container_imitate_function`：键比较或提取仿函数。
   * `hash_function`：对值或键生成桶索引的哈希函数。

2. **构造与析构**

   * `hash_table()`：使用默认容量 `10`，负载因子 `7` (即 0.7) 初始化桶数组。
   * `hash_table(cap)`：使用指定 `cap` 大小初始化。
   * `hash_table(const other)`：深拷贝所有节点、桶结构和全局链表顺序。
   * `hash_table(hash_table&&)`：移动构造，接管内部容器与链表指针。
   * `~hash_table()`：遍历所有桶，删除每个节点。

3. **插入 (`push`)**

   1. **重复检测**：调用 `find` 查找值是否已存在，若存在返回 `false`。
   2. **扩容判断**：若 `_size * 10 >= hash_capacity * load_factor`，则：

      * 计算新容量 `new_cap = max(10, 2*hash_capacity)`。
      * 重新分配新桶数组，并遍历全局链表按插入顺序重建桶链和全局链。
   3. **插入操作**：

      * 计算 `h = hash_function_object(v) % hash_capacity`。
      * 在桶链头部执行头插：`new_node->_next = vector_hash_table[h]`，`vector_hash_table[h] = new_node`。
      * 在全局链表尾追加：链接 `overall_list_before_node` → `new_node`。
      * 更新 `_size++`。
   4. **返回**：插入成功返回 `true`。

4. **删除 (`pop`)**

   1. **查找节点**：计算 `h` 并遍历桶链，定位目标节点及其桶内父节点。
   2. **全局链表移除**：根据节点在全局链表的头/中/尾位置，更新 `overall_list_*` 指针。
   3. **桶链移除**：调用 `hash_chain_adjustment` 将父或桶头指向被删节点的下一个。
   4. **释放节点**，`_size--`，返回 `true`；未找到返回 `false`。

5. **查找 (`find`)**

   * 若 `_size==0` 返回 `end()`；否则计算 `h`，遍历桶链比较 `value_imitation_functions`。

6. **查询**

   * `size()`、`empty()`、`capacity()` 直接返回内部字段。
   * `begin()` / `end()`、`cbegin()` / `cend()` 在全局链表上按插入顺序遍历。

#### **返回值说明**

* `push(...)` → `bool`：

  * `true`：新值已插入；
  * `false`：值已存在。
* `pop(...)` → `bool`：

  * `true`：删除成功；
  * `false`：未找到。
* `find(...)` → `iterator`：

  * 指向目标；未找到为 `end()`。
* `change_load_factor` → `bool`：

  * `true`：新负载因子生效；
  * `false`：输入无效。
* `size()`, `capacity()` → `size_t`。
* `empty()` → `bool`。

#### **内部原理剖析**

* **桶数组**：`vector_hash_table` 存储每个桶的头节点指针，桶内使用单链 `_next` 处理冲突。
* **全局链表**：`overall_list_head_node` 与 `overall_list_before_node` 串联所有节点，支持按插入顺序遍历。
* **扩容**：依据负载因子动态触发；重建时保持全局插入顺序。
* **链表移除**：

  * `hash_chain_adjustment` 检查父节点是否为空，若为空更新桶头，否则更新 `parent->_next`。
  * 全局链表删除需要处理头/中/尾三类情况。

#### **复杂度分析**

* `push` / `pop` / `find`：平均 `O(1)`，最坏 `O(n)`（全部冲突或重建时为 `O(n)`。
* 扩容：`O(n)` 需遍历全局链表重建。
* 空间：`O(n + cap)` 桶数组 + 全局链表。

#### **边界条件和错误处理**

* **空表**：`find`、`pop` 安全返回；`push` 正常插入首节点。
* **重复值**：`push` 返回 `false`。
* **负载因子**：`change_load_factor` 保证新值 ≥ 1。
* **内存不足**：节点分配或扩容时抛 `std::bad_alloc`。

#### **注意事项**

* 非线程安全，需外部同步。
* 扩容过程中，原节点不失序；中途异常会导致部分重建，可考虑事务回滚。
* 全局链表内存管理需与桶链同步。

#### **使用示例**
* 当前类不推荐直接使用，建议使用上层容器
> **引用** 头文件 `base_class_container` 命名空间
---
