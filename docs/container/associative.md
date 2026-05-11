## 关联式容器
### `map`
### `tree_map` 
### 类概述
* `tree_map` 是一个基于红黑树`（rb_Tree）`实现的有序映射容器，存储键值对并按键的顺序维护元素。
该容器支持高效的插入、删除和查找操作，所有操作的时间复杂度均为 `O (log n)`。元素始终按键的升序排列，遍历时将按此顺序返回元素。
#### **类及其函数定义**

```cpp
namespace map_container 
{
  template <typename map_type_k,typename map_type_v,
            typename comparators = template_container::imitation_functions::less<map_type_k>>
  class tree_map 
  {
  private:
    using key_val_type = template_container::practicality::pair<map_type_k,map_type_v>;
    struct key_val 
    {
       const map_type_k& operator()(const key_val_type& kv);
    };
    using instance_rb = base_class_container::rb_tree<map_type_k,key_val_type,key_val,comparators>;
    instance_rb instance_tree_map;

  public:
    using iterator = typename instance_rb::iterator;
    using const_iterator = typename instance_rb::const_iterator;
    using reverse_iterator = typename instance_rb::reverse_iterator;
    using const_reverse_iterator = typename instance_rb::const_reverse_iterator;
    using map_iterator = template_container::practicality::pair<iterator,bool>;

    tree_map();
    explicit tree_map(const key_val_type& kv);
    tree_map(std::initializer_list<key_val_type> il);
    tree_map(const tree_map& other);
    tree_map(tree_map&& other) noexcept;
    ~tree_map() = default;

    tree_map& operator=(const tree_map& other);
    tree_map& operator=(tree_map&& other) noexcept;

    map_iterator push(const key_val_type& kv);
    map_iterator push(key_val_type&& kv) noexcept;
    map_iterator pop(const key_val_type& kv);
    iterator find(const key_val_type& kv);

    void middle_order_traversal();
    void pre_order_traversal();

    [[nodiscard]] size_t size() const;
    bool empty() const;

    iterator begin();
    iterator end();
    const_iterator cbegin() const;
    const_iterator cend() const;
    reverse_iterator rbegin();
    reverse_iterator rend();
    const_reverse_iterator crbegin() const;
    const_reverse_iterator crend() const;

    iterator operator[](const key_val_type& kv);
  };
}
```

#### **作用描述**

* **映射类型**：基于红黑树（`base_class_container::rb_tree`），按键有序存储 `pair<key,value>`。
* **模板参数**：

  * `map_type_k`：键类型，决定排序。
  * `map_type_v`：值类型。
  * `comparators`：键比较仿函数。
* **核心接口**：

  * `push(kv)`：插入/更新键值对，返回迭代器及插入标志。
  * `pop(kv)`：删除指定键对应节点，返回删除状态。
  * `find(kv)`：按键查找节点迭代器。
  * `operator[](kv)`：访问节点或插入默认值。
* **遍历**：

  * 中序 `begin()/end()`：按键升序。
  * 反序 `rbegin()/rend()`：按键降序。
  * `middle_order_traversal()`、`pre_order_traversal()` 调试或打印。
* **容量检查**：

  * `size()` 返回元素数；`empty()` 检测是否为空。

#### **返回值说明**

* `push` → `map_iterator`：

  * `.first`：指向对应节点。
  * `.second`：`true` 插入了新节点，`false` 已存在更新。
* `pop` → `map_iterator.second`：

  * `true`：节点删除成功；`false`：键不存在。
* `find` → `iterator`：

  * 命中返回节点；否则 `end()`。
* `size()` → `size_t`。
* `empty()` → `bool`。

#### **内部原理**

* **红黑树存储**：

  * 键值对自定义为 `key_val_type`。
  * `key_val` 提取器将 `pair.first` 作为树节点关键字。
  * 树的所有平衡与遍历由底层 `rb_tree` 完成。

#### **复杂度分析**

* **插入/删除/查找**：平均与最坏均 `O(log n)`。
* **遍历**：中/前序 `O(n)`。
* **空间**：`O(n)` 节点。

#### **注意事项**

* 非线程安全。
* `operator[](kv)` 若键不存在可能插入默认值。
* 底层 `rb_tree` 异常将向上传递。

#### **使用示例**

```cpp
using namespace template_container;
int main()
{
    map_container::tree_map<int,std::string> m;
    m.push({1,"one"});
    m.push({2,"two"});
    for(auto it=m.begin(); it!=m.end(); ++it)
    {
        std::cout<<it->first<<":"<<it->second<<" ";
    }
    return 0;
}
```
> **引用** 头文件 `template_container::map_container` 命名空间
---

### `hash_map` 
### 类概述
* `hash_map` 是一个基于哈希表实现的无序映射容器，存储键值对并通过哈希函数快速定位元素。
该容器支持高效的插入、删除和查找操作，平均时间复杂度为 `O (1)`。遍历时元素顺序不确定（按插入顺序）
#### **类及其函数定义**

```cpp
namespace map_container 
{
  template <typename hash_map_type_key,typename hash_map_type_value,
            typename first_hash = template_container::imitation_functions::hash_imitation_functions,
            typename second_hash = template_container::imitation_functions::hash_imitation_functions>
  class hash_map 
  {
  private:
    using key_val_type = template_container::practicality::pair<hash_map_type_key,hash_map_type_value>;
    struct key_val { const hash_map_type_key& operator()(const key_val_type& kv); };
    class inbuilt_map_hash_functor { /* 组合键值哈希 */ };
    using hash_table = base_class_container::hash_table<hash_map_type_key,key_val_type,key_val,inbuilt_map_hash_functor>;
    hash_table instance_hash_map;
  public:
    using iterator = typename hash_table::iterator;
    using const_iterator = typename hash_table::const_iterator;

    hash_map();
    explicit hash_map(const key_val_type& kv);
    hash_map(const hash_map& other);
    hash_map(hash_map&& other) noexcept;
    ~hash_map() = default;

    bool push(const key_val_type& kv);
    bool push(key_val_type&& kv) noexcept;
    bool pop(const key_val_type& kv);
    iterator find(const key_val_type& kv);

    size_t size() const;
    size_t capacity() const;
    bool empty() const;

    iterator begin();
    iterator end();
    const_iterator cbegin() const;
    const_iterator cend() const;

    iterator operator[](const key_val_type& kv);
  };
}
```

#### **作用描述**

* **映射类型**：基于底层 `hash_table`，通过复合哈希函数按插入顺序索引键值对。
* **模板参数**：
  * `ash_map_type_key`：键的类型（必须支持哈希函数）
  * `hash_map_type_value`：值的类型
  * `first_external_hash_functions`：键的哈希函数（默认使用 `hash_imitation_functions`）
  * `second_external_hash_functions`：值的哈希函数（默认使用 `hash_imitation_functions`）
* **核心接口**：

  * `push(kv)`、`pop(kv)`、`find(kv)`。
  * `operator[](kv)`：访问或插入。
* **遍历**：

  * `begin()/end()` 按全局链表插入顺序。

#### **返回值说明**

* `push`/`pop` → `bool`。
* `find`/`operator[]` → `iterator`。
* `size()`/`capacity()` → `size_t`。
* `empty()` → `bool`。

#### **内部原理**

* **复合哈希**：将键与值哈希值混合，增强分布。
* **底层哈希表**：使用 `instance_hash_map` 管理存储与顺序。

#### **复杂度分析**

* 平均 O(1)，最坏 O(n)。

#### **注意事项**

* 非线程安全。
* 重复键 `push` 返回 `false`。
* 底层扩容影响迭代器有效性。

#### **使用示例**

```cpp
using namespace template_container;
int main()
{
    map_container::hash_map<int,std::string,template_container::imitation_functions::hash_imitation_functions,std::hash<std::string>> hm;
    hm.push({1,"one"});
    hm.push({2,"two"});
    for(auto it=hm.begin(); it!=hm.end(); ++it)
    {
        std::cout<<*it<<" ";
    }
    return 0;
}
```
> **引用** 头文件 `template_container::map_container` 命名空间
---
### `set`
### `tree_set`
### 类概述
* `tree_set` 是一个基于红黑树`（rb_Tree`）`实现的有序集合容器，存储唯一元素并按键的顺序维护。
该容器支持高效的插入、删除和查找操作，所有操作的时间复杂度均为 `O (log n)`。元素始终按升序排列，遍历时将按此顺序返回元素，适用于需要有序且唯一元素存储的场景。
#### **类及其函数定义**

```cpp
namespace set_container 
{
  template <typename set_type,typename comparators = template_container::imitation_functions::less<set_type>>
  class tree_set 
  {
  private:
    using key_val_type = set_type;
    struct key_val 
    {
      const set_type& operator()(const key_val_type& kv) { return kv; }
    };
    using instance_rb = base_class_container::rb_tree<set_type,key_val_type,key_val,comparators>;
    instance_rb instance_tree_set;

  public:
    using iterator = typename instance_rb::iterator;
    using const_iterator = typename instance_rb::const_iterator;
    using reverse_iterator = typename instance_rb::reverse_iterator;
    using const_reverse_iterator = typename instance_rb::const_reverse_iterator;
    using set_iterator = template_container::practicality::pair<iterator,bool>;

    tree_set();
    explicit tree_set(const key_val_type& v);
    tree_set(std::initializer_list<key_val_type> il);
    tree_set(const tree_set& other);
    tree_set(tree_set&& other) noexcept;
    ~tree_set() = default;

    tree_set& operator=(const tree_set& other);
    tree_set& operator=(tree_set&& other) noexcept;
    tree_set& operator=(std::initializer_list<key_val_type> il);

    set_iterator push(const key_val_type& v);
    set_iterator push(key_val_type&& v) noexcept;
    set_iterator pop(const key_val_type& v);
    iterator find(const key_val_type& v);

    void middle_order_traversal();
    void pre_order_traversal();

    [[nodiscard]] size_t size() const;
    bool empty() const;

    iterator begin();
    iterator end();
    const_iterator cbegin() const;
    const_iterator cend() const;
    reverse_iterator rbegin();
    reverse_iterator rend();
    const_reverse_iterator crbegin() const;
    const_reverse_iterator crend() const;

    iterator operator[](const key_val_type& v);
  };
}
```

#### **作用描述**

* 基于红黑树 `rb_tree`，存储唯一 `set_type` 元素，有序且不重复。
* **模板参数**：

  * `set_type`：元素类型。
  * `comparators`：元素比较策略。
* **核心方法**：

  * `push` 插入元素，返回迭代器和是否插入标志。
  * `pop` 删除指定元素。
  * `find` 查找元素。
  * `operator[]` 访问元素（存在即返回迭代器）。
* **遍历接口**：

  * `begin/end`：中序正序访问。
  * `rbegin/rend`：中序逆序访问。
  * `middle_order_traversal`/`pre_order_traversal`：打印或调试。
* **查询接口**：

  * `size()`：元素数量。
  * `empty()`：是否为空。

#### **返回值说明**

* `push(...)` → `set_iterator`：

  * `.first`：指向插入或已存在元素。
  * `.second`：`true` 表示新插入；`false` 已存在。
* `pop(...)` → `set_iterator.second`：

  * `true` 删除成功；`false` 未找到。
* `find(...)` → `iterator`：命中或 `end()`。
* `operator[](...)` → `iterator`。
* `size()` → `size_t`。
* `empty()` → `bool`。

#### **内部原理剖析**

* 采用底层 `rb_tree` 实现红黑树插入、删除与平衡。
* 提取器 `key_val` 将元素自身作为键。
* 所有平衡、旋转由 `rb_tree` 完成。

#### **复杂度分析**

* 插入/删除/查找：平均与最坏 `O(log n)`。
* 遍历：`O(n)`。

#### **注意事项**

* 非线程安全。
* `operator[]` 仅用于存在元素访问，不插入。
* 异常由底层 `rb_tree` 抛出。

#### **使用示例**

```cpp
using namespace template_container::set_container;
int main()
{
    tree_set<int> ts = {3, 1, 2, 4};

    // 2. 插入元素
    ts.push(5);
    // 3. 查找元素
    auto it = ts.find(2);
    if (it != ts.end()) 
    {
        std::cout << "找到元素: " << *it << std::endl;  // 输出: 2
    }
    // 4. 中序遍历（按升序）
    std::cout << "中序遍历结果: ";
    ts.middle_order_traversal();  // 输出: 1 2 3 4 5
    // 5. 删除元素
    ts.pop(3);
    std::cout << "\n删除后大小: " << ts.size() << std::endl;  // 输出: 4
    return 0;
}
```
> **引用** 头文件 `template_container::set_container` 命名空间
---

### `hash_set` 

#### **类及其函数定义**
#### 类概述
* `hash_set` 是一个基于哈希表实现的无序集合容器，存储唯一元素并通过哈希函数快速定位。该容器支持高效的插入、删除和查找操作，平均时间复杂度为` O (1)`。
遍历时元素顺序不确定（按插入顺序），适用于需要快速查找唯一元素的场景。
```cpp
namespace set_container 
{
  template <typename set_type_val,typename external_hash_functions = template_container::imitation_functions::hash_imitation_functions>
  class hash_set 
  {
  private:
    using key_val_type = set_type_val;
    class inbuilt_set_hash_functor 
    {
    private:
      external_hash_functions hash_functions_object;
    public:
      size_t operator()(const key_val_type& key) const { return hash_functions_object(key) * 131; }
    };
    struct key_val 
    {
      const key_val_type& operator()(const key_val_type& kv) { return kv; }
    };
    using hash_table = base_class_container::hash_table<set_type_val,key_val_type,key_val,nbuilt_set_hash_functor>;
    hash_table instance_hash_set;

  public:
    using iterator = typename hash_table::iterator;
    using const_iterator = typename hash_table::const_iterator;

    hash_set();
    explicit hash_set(const key_val_type& v);
    hash_set(const hash_set& other);
    hash_set(hash_set&& other) noexcept;
    ~hash_set() = default;

    bool push(const key_val_type& v);
    bool push(key_val_type&& v) noexcept;
    bool pop(const key_val_type& v);
    iterator find(const key_val_type& v);

    size_t size() const;
    bool empty() const;
    size_t capacity() const;

    iterator begin();
    iterator end();
    const_iterator cbegin() const;
    const_iterator cend() const;

    iterator operator[](const key_val_type& v);
  };
}
```

#### **作用描述**

* 基于哈希表 `hash_table`，存储唯一 `set_type_val` 元素，使用外部哈希仿函数。
* **模板参数**：

  * `set_type_val`：元素类型。
  * `external_hash_functions`：哈希仿函数。
* **核心方法**：

  * `push` 插入元素。
  * `pop` 删除元素。
  * `find` 查找元素。
* **遍历接口**：

  * `begin/end`：按插入顺序遍历。

#### **返回值说明**

* `push(...)` → `bool`：`true` 新插入；`false` 已存在。
* `pop(...)` → `bool`：`true` 删除成功；`false` 未找到。
* `find(...)` → `iterator`：命中或 `end()`。
* `size()` → `size_t`。
* `empty()` → `bool`。
* `capacity()` → `size_t`。

#### **内部原理剖析**

* 结合 `external_hash_functions` 生成哈希值后头插桶链。
* 底层 `hash_table` 管理存储与全局顺序。

#### **复杂度分析**

* 平均 `O(1)`，最坏 `O(n)`。

#### **注意事项**

* 非线程安全。
* 扩容后迭代器失效。
* 重复 `push` 返回 `false`。

#### **使用示例**

```cpp
using namespace template_container::set_container;
int main()
{
    // 1. 创建hash_set并初始化
    hash_set<int> hs = {3, 1, 2, 4};
    
    // 2. 插入元素
    hs.push(5);
    
    // 3. 查找元素
    auto it = hs.find(2);
    if (it != hs.end()) 
    {
        std::cout << "找到元素: " << *it << std::endl;  // 输出: 2
    }
    
    // 4. 遍历元素（按插入顺序）
    std::cout << "遍历元素: ";
    for (auto it = hs.begin(); it != hs.end(); ++it) \
    {
        std::cout << *it << " ";
    }
    // 输出: 3 1 2 4 5（顺序可能不同）
    
    // 5. 删除元素
    hs.pop(3);
    std::cout << "\n删除后大小: " << hs.size() << std::endl;  // 输出: 4
    return 0;
}
```
> **引用** 头文件 `template_container::set_container` 命名空间
---

