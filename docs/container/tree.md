## 树容器 `tree_container`

### 二叉搜索树

### `binary_search_tree`
* **概述**：`binary_search_tree` 类是一个模板化的二叉搜索树实现，支持任意数据类型和自定义比较策略。
二叉搜索树是一种有序数据结构，具有以下性质：左子树所有节点值小于根节点，右子树所有节点值大于根节点。
该类实现了树的插入、删除、查找和遍历等核心操作，并提供了拷贝构造、移动语义等高级功能。

#### **类及其函数定义**

```cpp
template <typename binary_search_tree_type,
    typename container_imitate_function = template_container::imitation_functions::less<binary_search_tree_type>>
class binary_search_tree 
{
private:
    class binary_search_tree_type_node 
    {
    public:
        binary_search_tree_type_node* _left;
        binary_search_tree_type_node* _right;
        binary_search_tree_type _data;
        explicit binary_search_tree_type_node(const binary_search_tree_type& val = binary_search_tree_type());
        ~binary_search_tree_type_node();
    };

    using container_node = binary_search_tree_type_node;
    container_node* _root;
    container_imitate_function function_policy;

    // 内部遍历与辅助
    void interior_middle_order_traversal(container_node* node);
    size_t interior_middle_order_traversal(container_node* node, size_t& counter);
    void interior_pre_order_traversal(container_node* node);
    void clear() noexcept;

public:
    // 构造与析构
    binary_search_tree() = default;
    explicit binary_search_tree(const binary_search_tree_type& val);
    binary_search_tree(std::initializer_list<binary_search_tree_type> init);
    binary_search_tree(const binary_search_tree& other);
    binary_search_tree(binary_search_tree&& other) noexcept;
    ~binary_search_tree() noexcept;

    // 核心操作
    bool push(const binary_search_tree_type& val);
    binary_search_tree& pop(const binary_search_tree_type& val);
    container_node* find(const binary_search_tree_type& val);
    void insert(const binary_search_tree_type& existing, const binary_search_tree_type& new_val);

    // 信息查询
    size_t size();
    [[nodiscard]] size_t size() const;

    // 遍历接口
    void middle_order_traversal();
    void pre_order_traversal();

    // 赋值运算符
    binary_search_tree& operator=(const binary_search_tree& other);
    binary_search_tree& operator=(binary_search_tree&& other) noexcept;
};
```

---

#### **作用描述**

* **模板参数**：

  * `binary_search_tree_type`：节点存储的数据类型。
  * `container_imitate_function`：用于比较节点值的仿函数（默认 `less<T>`）。
* **`push(val)`**：向树中插入新节点；若根为空则新建根，否则根据比较函数向左或右子树递归定位。
* **`pop(val)`**：删除具有指定值的节点，分三种情况：

  1. **无左子**：替换为右子树。
  2. **无右子**：替换为左子树。
  3. **双子**：找到右子树最小节点，与待删节点数据交换，再删除该最小节点。
* **`find(val)`**：在树中查找值等于 `val` 的节点，返回指向该节点的指针或 `nullptr`。
* **`insert(existing, new_val)`**：在已有节点后作为其右子树插入新节点。
* **遍历**：

  * `middle_order_traversal()`：非递归中序。
  * `pre_order_traversal()`：非递归前序。
* **`size()`**：通过中序遍历计数节点总数；常量/非常量版本。
* **`clear()`**：非递归释放所有节点。
* **赋值运算符**：支持深拷贝和移动赋值，清理旧树后接管或复制节点。


#### **返回值说明**

* `push` → `bool`：插入成功返回 `true`，遇到重复元素返回 `false`。
* `pop` → `binary_search_tree&`：操作完成后返回自身引用。
* `find` → `container_node*`：指向找到的节点或 `nullptr`。
* `size()` → `size_t`：返回当前节点总数。
* 遍历/clear → `void`。
* 构造/赋值 → 实例化或赋值，无返回。


#### **内部原理剖析**

* **节点结构**：`_left`、`_right` 双向指针与数据 `_data`。
* **插入**：从根开始，使用 `function_policy` 决定向左或向右，直到空位置。
* **删除**：三种子树情况处理，并使用 `swap` 数据简化双子删除。
* **非递归遍历**：利用自定义 `stack_adapter::stack<container_node*>` 实现中序和前序遍历。
* **清理**：同样利用栈做后序释放，防止递归栈溢。


#### **复杂度分析**

* 平均情况下，操作（`push`/`pop`/`find`）时间为 `O(log n)`；最坏为退化为链表 `O(n)`。
* `size()`：`O(n)` 遍历所有节点。
* 遍历/`clear`：`O(n)`。
* 空间：`O(n)`（存储节点）+ `O(h)`额外栈空间，`h` 为树高。


#### **边界条件和错误处理**

* **空树**：在根为空时 `push` 会创建根节点；`pop`/`find` 返回空或保持无变化。
* **重复元素**：`push` 返回 `false` 而不插入。
* **内存分配失败**：抛 `std::bad_alloc`。
* **仿函数要求**：`container_imitate_function` 应提供严格弱序。


#### **注意事项**

* 非线程安全，需外部同步。
* 对深度大或不平衡树，递归遍历易栈溢，故使用显式栈。
* `pop` 删除后，外部持有的节点指针可能失效。

#### **使用示例**

```cpp
using namespace template_container::tree_container;
int main() 
{
    // 1. 构造二叉搜索树
    binary_search_tree<int> bst = {5, 3, 7, 2, 4, 6, 8};
    
    std::cout << "中序遍历: ";
    bst.middle_order_traversal();  // 输出: 2 3 4 5 6 7 8
    std::cout << "\n前序遍历: ";
    bst.pre_order_traversal();   // 输出: 5 3 2 4 7 6 8
    std::cout << "\n树大小: " << bst.size() << std::endl;  // 输出: 7
    
    // 2. 插入节点
    bst.push(9);
    std::cout << "插入9后中序遍历: ";
    bst.middle_order_traversal();  // 输出: 2 3 4 5 6 7 8 9
    
    // 3. 删除节点
    bst.pop(3);
    std::cout << "删除3后中序遍历: ";
    bst.middle_order_traversal();  // 输出: 2 4 5 6 7 8 9
    
    // 4. 查找节点
    auto node = bst.find(6);
    if (node) 
    {
        std::cout << "\n找到节点: " << node->_data << std::endl;
    }
    
    // 5. 拷贝构造
    binary_search_tree<int> bst_copy = bst;
    std::cout << "拷贝树中序遍历: ";
    bst_copy.middle_order_traversal();
    
    return 0;
}
```
> **引用**： 头文件 `tree_container` 命名空间

---
### AVL树
### `avl_tree`
### 类概述
* `avl_tree` 类是一个模板化的平衡二叉搜索树实现（`AVL` 树），支持键值对存储和自动平衡功能。`AVL` 树通过旋转操作保持平衡性质：任意节点的左右子树高度差不超过 1，确保了插入、删除和查找操作的平均时间复杂度为 `O (log n)`。该类实现了完整的迭代器系统、平衡旋转算法和异常处理机制。

#### **类及其函数定义**

```cpp
template <typename avl_tree_type_k,typename avl_tree_type_v,
          typename container_imitate_function = template_container::imitation_functions::less<avl_tree_type_k>,
          typename avl_tree_node_pair = template_container::practicality::pair<avl_tree_type_k,avl_tree_type_v>>
class avl_tree 
{
private:
    class avl_tree_type_node 
    {
    public:
        avl_tree_node_pair _data;               // 键值对数据
        avl_tree_type_node* _left;              // 左子树指针
        avl_tree_type_node* _right;             // 右子树指针
        avl_tree_type_node* _parent;            // 父节点指针
        int _balance_factor;                    // 平衡因子：右子高 - 左子高

        explicit avl_tree_type_node(const avl_tree_type_k& key = avl_tree_type_k(),const avl_tree_type_v& val = avl_tree_type_v());
        explicit avl_tree_type_node(const avl_tree_node_pair& pair_data);
    };

    using container_node = avl_tree_type_node;
    container_node* _root;                      // 树根
    container_imitate_function function_policy;  // 键比较策略

    // 旋转操作
    void left_revolve(container_node*& subtree);
    void right_revolve(container_node*& subtree);
    void left_right_revolve(container_node*& subtree);
    void right_left_revolve(container_node*& subtree);

    // 插入与删除辅助
    bool insert_node(container_node*& subtree, container_node* parent, const avl_tree_node_pair& pr);
    container_node* remove_node(container_node*& subtree, const avl_tree_type_k& key, bool& erased);

    // 非递归遍历
    void interior_pre_order_traversal(container_node* start);
    void interior_middle_order_traversal(container_node* start);

    // 资源清理
    void clear_tree(container_node* node) noexcept;

public:
    using iterator = avl_tree_iterator<avl_tree_node_pair, avl_tree_node_pair&, avl_tree_node_pair*>;
    using const_iterator = avl_tree_iterator<avl_tree_node_pair, const avl_tree_node_pair&, const avl_tree_node_pair*>;
    using reverse_iterator = avl_tree_reverse_iterator<iterator>;
    using const_reverse_iterator = avl_tree_reverse_iterator<const_iterator>;

    // 构造与析构
    avl_tree();  // 空树
    explicit avl_tree(const avl_tree_node_pair& pr, container_imitate_function comp = container_imitate_function());
    avl_tree(const avl_tree_type_k& key, const avl_tree_type_v& val = avl_tree_type_v(), container_imitate_function comp = container_imitate_function());
    avl_tree(std::initializer_list<avl_tree_node_pair> init, container_imitate_function comp = container_imitate_function());
    avl_tree(const avl_tree& other);  // 深拷贝
    avl_tree(avl_tree&& other) noexcept;  // 移动
    ~avl_tree() noexcept;

    // 核心操作
    bool push(const avl_tree_node_pair& pr);  // 插入键值对
    bool push(const avl_tree_type_k& key, const avl_tree_type_v& val = avl_tree_type_v());
    bool pop(const avl_tree_type_k& key);   // 删除指定键
    avl_tree_node_pair* find(const avl_tree_type_k& key);  // 查找

    // 信息查询
    [[nodiscard]] size_t size() const;  // 总节点数
    [[nodiscard]] bool empty() const;   // 是否为空

    // 遍历
    iterator begin();  // 中序首
    iterator end();    // 中序尾
    const_iterator cbegin() const;
    const_iterator cend() const;
    reverse_iterator rbegin();
    reverse_iterator rend();
    const_reverse_iterator crbegin() const;
    const_reverse_iterator crend() const;

    // 赋值运算符
    avl_tree& operator=(const avl_tree& other);
    avl_tree& operator=(avl_tree&& other) noexcept;
};
```

#### **作用描述**
**模板参数**
* `avl_tree_type_k`：键的类型（用于排序和查找）
* `avl_tree_type_v`：值的类型（与键关联的数据）
* `container_imitate_function`：键的比较器（默认：`less<avl_tree_type_k>`）
* `avl_tree_node_pair`：键值对类型（默认：`pair<avl_tree_type_k, avl_tree_type_v>`）

 **构造与析构**

   * **空构造**：`avl_tree()` 初始化 `_root=nullptr`，无内存分配。
   * **键值对构造**：`avl_tree(pr)` 在空树创建根节点，`_balance_factor=0`。
   * **键/值构造**：`avl_tree(key,val)` 同上，可指定初始键值。
   * **初始化列表**：依次调用 `push` 对每个元素执行插入并自动平衡。
   * **拷贝构造**：递归或显式栈深拷贝所有节点与平衡因子。
   * **移动构造**：直接接管 `other._root` 与 `other.function_policy`，清空源。
   * **析构**：调用 `clear_tree(_root)` 释放所有节点。

**插入 (`push`)**

   * **查重**：若在定位过程中遇 `comp(a,b)==false && comp(b,a)==false` 则为重复，返回 `false`。
   * **节点创建**：新节点 `_balance_factor=0`，父指针指向插入位置。
   * **回溯更新**：从插入节点沿父链向上更新 `_balance_factor`：右插增+1，左插减-1。
   * **失衡检测**：当因子绝对值>1，识别为 LL/RR/LR/RL 失衡并调用相应旋转：

     * **LL**：`right_revolve`
     * **RR**：`left_revolve`
     * **LR**：`left_revolve(left child)` 后 `right_revolve`
     * **RL**：`right_revolve(right child)` 后 `left_revolve`
   * **因子修正**：旋转后子树高度恢复，更新相关因子。

 **删除 (`pop`)**

   * **定位**：类似 `find` 查找节点。
   * **三种删除情况**：

     1. **叶节点**：直接删除并将父链接置 `nullptr`。
     2. **单子**：用唯一子节点替代被删除节点。
     3. **双子**：查找右子树最左（中序后继），交换数据并删除该后继。
   * **回溯平衡**：从删除点父节点起向上更新因子并执行旋转。

 **查找 (`find`)**

   * 自 `_root` 比较 `key` 与 `node->_data.first`，左滑或右滑直至命中或 `nullptr`。

 **查询 (`size`/`empty`)**

   * `empty()`：`O(1)` 检查 `_root==nullptr`。
   * `size()`：`O(n)` 通过 `_size()` 递归或迭代统计。

 **遍历接口**

   * **中序**：`begin()`、`end()` 生成正序迭代器，内部调 `interior_middle_order_traversal`。
   * **逆序**：`rbegin()`、`rend()` 生成反向迭代器。
   * **前序**：`interior_pre_order_traversal` 用于序列化或打印调试。

#### **返回值说明**

* `push(...)` → `bool`：成功插入并平衡返回 `true`；重复键返回 `false`。
* `pop(key)` → `bool`：存在则删除并返回 `true`；不存在返回 `false`。
* `find(key)` → `Pair*`：命中返回指针；未命中返回 `nullptr`。
* `size()` → `size_t`：当前节点数。
* `empty()` → `bool`：是否无节点。
* 构造/析构/旋转/遍历 → `void` 或迭代器。

#### **内部原理剖析**

* **节点结构**：包含键值对 `_data`、四指针与 `_balance_factor`。
* **平衡因子维护**：插入删除后更新并触发局部旋转。
* **四种失衡与旋转**：LL/RR 用单旋，LR/RL 用双旋。
* **旋转细节**：

  1. **单旋**：重挂子指针，更新父子关系与因子。
  2. **双旋**：先旋转子树，再旋转当前树。
* **无递归遍历**：借助适配器 `stack_adapter::stack`，模拟系统栈。
* **资源释放**：后序栈或递归，确保先删除子节点。

#### **复杂度分析**

* **时间**：插入/删除/查找 `O(log n)` 平均,`O(n)` 最坏。
* **空间**：节点存储 `O(n)`，递归或栈辅助 `O(h)`，h=树高。
* **遍历/清理**：`O(n)` 时间。

#### **边界条件和错误处理**

* 空树：`pop`/`find` 返回 `false`/`nullptr`；`size=0`。
* 重复键：`push` 不插入，返回 `false`。
* 内存不足：新节点分配时抛 `std::bad_alloc`。
* 无效仿函数：若不满足严格弱序，行为未定义。

#### **注意事项**

* **线程安全**：非线程安全，需外部同步。
* **迭代器失效**：树结构变更后所有迭代器失效。
* **连续旋转性能**：频繁批量插入可能触发大量旋转。
* **平衡因子同步**：任何指针操作后都需更新因子。

#### **使用示例**

```cpp
using namespace template_container::tree_container;
int main() 
{
    // 1. 构造AVL树
    avl_tree<int, std::string> avl;
    avl.push(5, "A");
    avl.push(3, "B");
    avl.push(7, "C");
    avl.push(2, "D");
    avl.push(4, "E");
    
    std::cout << "中序遍历: ";
    avl.middle_order_traversal();  // 输出: 2 3 4 5 7
    
    // 2. 查找节点
    auto node = avl.find(3);
    if (node) 
    {
        std::cout << "\n找到键3的值: " << node->_data.second << std::endl;
    }
    
    // 3. 删除节点
    avl.pop(3);
    std::cout << "删除3后中序遍历: ";
    avl.middle_order_traversal();  // 输出: 2 4 5 7
    
    // 4. 迭代器遍历
    std::cout << "\n迭代器遍历: ";
    for (auto it = avl.begin(); it != avl.end(); ++it) 
    {
        std::cout << it->first<< " ";
    }
    
    return 0;
}
```
> **引用**：头文件 `tree_container` 命名空间。
---
## 基类容器 `base_class_container`
## 红黑树 
## `rb_tree`
### 类概述
* `rb_tree` 类是一个模板化的红黑树实现，支持键值对存储和自动平衡功能。
红黑树通过颜色标记和旋转操作保持平衡，确保插入、删除和查找操作的平均时间复杂度为 O (log n)。该类实现了完整的迭代器系统、红黑树调整算法和异常处理机制。
#### **类及其函数定义**
```cpp
template <typename rb_tree_type_key,typename rb_tree_type_value,typename container_imitate_function_visit,
    typename container_imitate_function = template_container::imitation_functions::less<rb_tree_type_key>>
class rb_tree 
{
private:
    enum rb_tree_color 
    { red, black };

    class rb_tree_node 
    {
    public:
        rb_tree_type_value _data;
        rb_tree_node*      _left;
        rb_tree_node*      _right;
        rb_tree_node*      _parent;
        rb_tree_color      _color;

        explicit rb_tree_node(const rb_tree_type_value& val_data = rb_tree_type_value());
        explicit rb_tree_node(rb_tree_type_value&& val_data) noexcept;
    };

    using container_node = rb_tree_node;
    container_node*                 _root;
    container_imitate_function_visit element;
    container_imitate_function      function_policy;

    // 核心旋转方法
    void left_revolve(container_node* x);
    void right_revolve(container_node* x);
    // 删除平衡
    void delete_adjust(container_node* x, container_node* parent);

    // 清理与遍历
    void clear(container_node* node) noexcept;
    void interior_middle_order_traversal(container_node* node);
    void interior_pre_order_traversal(container_node* node);
    size_t _size() const;

public:
    using iterator = rb_tree_iterator<rb_tree_type_value, rb_tree_type_value&, rb_tree_type_value*>;
    using const_iterator = rb_tree_iterator<rb_tree_type_value const, rb_tree_type_value const&, rb_tree_type_value const*>;
    using reverse_iterator = rb_tree_reverse_iterator<iterator>;
    using const_reverse_iterator = rb_tree_reverse_iterator<const_iterator>;
    using return_pair_value = template_container::practicality::pair<iterator, bool>;

    // 构造与析构
    rb_tree();
    explicit rb_tree(const rb_tree_type_value& data);
    explicit rb_tree(rb_tree_type_value&& data) noexcept;
    rb_tree(const rb_tree& other);
    rb_tree(rb_tree&& other) noexcept;
    ~rb_tree() noexcept;

    // 核心操作
    return_pair_value push(const rb_tree_type_value& v);
    return_pair_value push(rb_tree_type_value&& v) noexcept;
    return_pair_value pop(const rb_tree_type_value& v);
    iterator           find(const rb_tree_type_value& v);

    // 查询与遍历
    size_t            size() const;
    bool              empty() const;
    void              middle_order_traversal();
    void              pre_order_traversal();
    iterator          begin();
    iterator          end();
    const_iterator    cbegin() const;
    const_iterator    cend() const;
    reverse_iterator  rbegin();
    reverse_iterator  rend();
    const_reverse_iterator crbegin() const;
    const_reverse_iterator crend() const;

    // 运算符
    iterator operator[](const rb_tree_type_value& v);
    rb_tree& operator=(const rb_tree other);
    rb_tree& operator=(rb_tree&& other) noexcept;
};
```
#### **作用描述**
* **模板参数** 
* `rb_tree_type_key`：键的类型（用于排序和查找）
* `rb_tree_type_value`：值的类型（与键关联的数据）
* `container_imitate_function_visit`：值访问器（用于从值中提取键）
* `container_imitate_function`：键的比较器（默认：`less<rb_tree_type_key>`）

* **构造与析构**：

  * `rb_tree()`：创建空树，`_root=nullptr`。
  * `rb_tree(data)`：根节点为黑色新节点。
  * 拷贝/移动构造：复制或接管节点和颜色。
  * `~rb_tree()`：后序调用 `clear(_root)` 释放节点。

* **插入 (`push`)**：

  1. **定位**：按 `function_policy` 比较沿左右子树查找插入位置，若遇相等则返回 `.second = false`。
  2. **插入**：新节点 `_color = red`，挂接父指针。
  3. **修正红黑性质**：从新节点向上迭代：

     * 若父黑，终止。
     * 若父和叔叔均为红：父、叔改黑，祖父改红，继续向上。
     * 否则执行旋转

       * **LL**：`right_revolve(祖父)`；
       * **RR**：`left_revolve(祖父)`；
       * **LR**：`left_revolve(父)` 后 `right_revolve(祖父)`；
       * **RL**：`right_revolve(父)` 后 `left_revolve(祖父)`。
     * 每次旋转后调整节点颜色，确保子树平衡。
  4. **根染黑**。

* **删除 (`pop`)**：

  1. **查找**：`find` 定位目标节点，若未找到返回 `.second = false`。
  2. **删除**：

     * **单/双子**：参照 BST 删除；双子先用中序后继替换。
     * 保留被删除节点原始颜色至 `orig_color`。
  3. **平衡调整**：若 `orig_color` 为 `black`，调用 `delete_adjust(node, parent)`：(因为是黑色节点删除的话会失衡：从任意节点到其所有后代叶节点的路径上，包含的黑色节点数量相同)

     * **兄弟为红**：对 `parent` 做旋转，交换 `parent` 与兄弟颜色，刷新兄弟。
     * **兄弟与子皆黑**：兄弟染红，继续向上修正。
     * **兄弟为黑，近侧子红**：先旋转兄弟使远侧子成为兄弟，再归入下一情况。
     * **兄弟为黑，远侧子红**：对 `parent` 做一次旋转，根据方向将兄弟和其子调整颜色，结束平衡。
  4. **根染黑**。

* **查找 (`find`)**：

  * 从 `_root` 开始，比较值后沿左右子树移动，返回节点迭代器或 `end()`。

* **遍历**：

  * **中序**：`middle_order_traversal` 使用显式栈保证顺序输出。
  * **前序**：`interior_pre_order_traversal` 同理实现。
  * 迭代器 `begin/end`、`rbegin/rend` 对应最左/最右节点。

* **查询**：

  * `empty()`：O(1) 检查 `_root == nullptr`。
  * `size()`：O(n) 调用 `_size()` 递归统计。

#### **返回值说明**

* `push` → `return_pair_value`：

  * `.first`：指向插入或已存在节点。
  * `.second`：`true` 插入并平衡；`false` 已存在。
* `pop` → `return_pair_value`：

  * `.first`：指向被删除节点后继或 `end()`。
  * `.second`：`true` 删除并平衡；`false` 未找到。
* `find` → `iterator`：命中返回节点；否则 `end()`。
* `size()` → `size_t`。
* `empty()` → `bool`。

#### **内部原理剖析**

* **节点颜色与路径**：

  * 红节点不能有红子节点。
  * 根到叶所有路径黑节点数相等。

* **`left_revolve(x)`**：

  * 设 `y=x->_right`。将 `y->_left` 赋给 `x->_right`，更新父指；将 `x` 设为 `y->_left`。
  * 调整 `x`, `y` 父指和 `_root` 链接；将 `y->_color=x_color`，并将 `x->_color=red`。

* **`right_revolve(x)`**：

  * 对称于 `left_revolve`，以 `y=x->_left`。

* **`delete_adjust(x, parent)`**：

  * 以 `x`（可能 `nullptr`）和其父为起点，修复双黑差：

    1. **兄弟红**：旋转 `parent`，交换颜色，刷新兄弟。
    2. **兄弟及兄弟子黑**：兄弟染红，继续向上。
    3. **兄弟黑且近侧子红**：旋转兄弟，交换颜色，转为第四种。
    4. **兄弟黑且远侧子红**：旋转 `parent`，兄弟与 `parent` 颜色互换，远侧子染黑。

* **遍历清理**：

  * 使用 `stack_adapter::stack` 显式管理栈，用后序方式释放。

#### **复杂度分析**

* 插入/删除/查找：平均与最坏均 O(log n)。
* 遍历/清理：O(n)，辅助 O(h) 栈空间。

#### **边界条件和错误处理**

* 空树：`push` 创建根；`pop`/`find` `.second=false`/返回 `end()`。
* 重复值：`push` 返回 `.second=false`。
* 内存不足：抛 `std::bad_alloc`。
* 比较函数须严格弱序。

#### **注意事项**

* 非线程安全。
* 结构修改后迭代器失效。
* 确保每次插入删除后根为黑。

#### **使用示例**
* 当前类不推荐直接使用，建议使用上层容器

> **引用** 头文件 `base_class_container` 命名空间


---
