## 🏗️ 新命名空间架构

### 📦 `con` 命名空间 - 容器集合

`con` 命名空间是所有容器类型的统一入口，提供简洁的容器访问方式：

#### 🗂️ 命名空间结构

```cpp
namespace con {
    // 基础容器
    using vector = dynamic_array_container::vector;
    using list = list_container::list;
    using string = char_array_container::string;
    
    // 关联容器
    using map = associative_container::tree_map;
    using set = associative_container::tree_set;
    using unordered_map = associative_container::hash_map;
    using unordered_set = associative_container::hash_set;
    
    // 适配器容器
    using stack = stack_adapter::stack;
    using queue = queue_adapter::queue;
    using priority_queue = queue_adapter::priority_queue;
    
    // 特殊容器
    using bitset = base_class_container::bit_set;
    using bloom_filter = bloom_filter_container::bloom_filter;
}
```

#### 🎯 使用优势

| 优势 | 说明 | 示例 |
|------|------|------|
| **简化命名** | 避免冗长的命名空间 | `con::vector` vs `dynamic_array_container::vector` |
| **统一接口** | 一致的容器访问方式 | 所有容器都在 `con` 下 |
| **易于迁移** | 类似标准库的使用体验 | 接近 `std::vector` 的使用方式 |

#### 💡 使用示例

```cpp
#include "Foundation.hpp"
using namespace template_container;

int main() {
    // 使用简化的命名空间
    con::vector<int> vec = {1, 2, 3, 4, 5};
    con::map<std::string, int> word_count;
    con::stack<int> st;
    con::bloom_filter<std::string> bf(1000);
    
    // 统一的容器操作风格
    vec.push_back(6);
    word_count["hello"] = 1;
    st.push(42);
    bf.set("test");
    
    std::cout << "Vector size: " << vec.size() << std::endl;
    std::cout << "Map contains 'hello': " << word_count.contains("hello") << std::endl;
    std::cout << "Stack top: " << st.top() << std::endl;
    std::cout << "Bloom filter test: " << bf.test("test") << std::endl;
    
    return 0;
}
```

> **引用**：头文件 `template_container::con` 命名空间
