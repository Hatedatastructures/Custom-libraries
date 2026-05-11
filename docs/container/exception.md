## ⚠️ 异常处理 `custom_exception`
### 🔧 `customize_exception` 类

**定义位置**：`custom_exception` 命名空间

```cpp
namespace custom_exception
{
    class customize_exception final : public std::exception
    {
    private:
        char* _message;        // 错误消息
        char* _function_name;  // 函数名称
        size_t _line_number;   // 行号
        
    public:
        customize_exception(const char* message_target, 
                          const char* function_name_target, 
                          const size_t& line_number_target) noexcept;
        
        [[nodiscard]] const char* what() const noexcept override;
        [[nodiscard]] const char* function_name_get() const noexcept;
        [[nodiscard]] size_t line_number_get() const noexcept;
        
        ~customize_exception() noexcept override;
    };
}
```

> ⚠️ **注意**：该类不能被继承（`final` 关键字）

### 📋 功能特性

| 特性 | 说明 | 优势 |
|------|------|------|
| **详细错误信息** | 包含错误消息、函数名、行号 | 便于调试和错误定位 |
| **异常安全** | 构造和析构都是 `noexcept` | 避免异常传播问题 |
| **内存管理** | 自动管理字符串内存 | 防止内存泄漏 |
| **标准兼容** | 继承自 `std::exception` | 与标准异常处理兼容 |

### 🛠️ 构造函数

```cpp
customize_exception(const char* message_target, 
                   const char* function_name_target, 
                   const size_t& line_number_target) noexcept
```

#### 参数说明
- `message_target`：错误消息字符串
- `function_name_target`：抛出异常的函数名称
- `line_number_target`：抛出异常的代码行号

#### 实现细节
- 通过 `new char[]` 复制字符串到内部缓冲区
- 保存行号信息
- 构造过程保证 `noexcept`

> ⚠️ **边界检查**：头文件实现未对空指针进行检查，调用者需确保传入非空合法指针

### 🔍 成员方法

#### 1. `what()` - 获取错误消息
```cpp
[[nodiscard]] const char* what() const noexcept override
```
- **作用**：返回异常消息，覆写 `std::exception::what()`
- **返回值**：指向内部存储的消息字符串
- **生命周期**：与异常对象相同

#### 2. `function_name_get()` - 获取函数名
```cpp
[[nodiscard]] const char* function_name_get() const noexcept
```
- **作用**：返回抛出异常时的函数名字符串
- **返回值**：指向内部存储的函数名

#### 3. `line_number_get()` - 获取行号
```cpp
[[nodiscard]] size_t line_number_get() const noexcept
```
- **作用**：获取抛出异常的行号
- **返回值**：行号信息

#### 4. 析构函数
```cpp
~customize_exception() noexcept override
```
- **作用**：释放内部分配的内存，避免内存泄漏
- **异常规范**：`noexcept`，析构时不会抛出异常

### 💡 使用示例

```cpp
#include "Foundation.hpp"

void some_function()
{
    if (error_condition) 
    {
        throw custom_exception::customize_exception(
            "错误信息", 
            __func__, 
            __LINE__
        );
    }
}

int main()
{
    try 
    {
        some_function();
    }
    catch (const custom_exception::customize_exception& e) 
    {
        std::cerr << "Exception: " << e.what() 
                  << " in function " << e.function_name_get() 
                  << " at line " << e.line_number_get() << std::endl;
    }
    
    return 0;
}
```

### ⚡ 性能与安全特性

#### 复杂度分析
- **构造时间**：O(n)，n 为消息长度（字符串复制）
- **析构时间**：O(1)，固定释放开销
- **访问时间**：O(1)，直接返回指针

#### 安全特性
- ✅ **异常安全**：构造和析构都是 `noexcept`
- ✅ **内存安全**：自动管理内存，防止泄漏
- ❌ **拷贝限制**：不支持拷贝构造、移动构造、赋值操作
- ❌ **线程安全**：非线程安全，多线程环境需外部同步

### 🚨 注意事项

| 注意点 | 说明 | 建议 |
|--------|------|------|
| **空指针检查** | 未对传入参数进行空指针检查 | 调用者确保参数有效性 |
| **字符串生命周期** | 返回值生命周期与异常对象相同 | 不要在异常对象销毁后访问 |
| **多线程使用** | 非线程安全 | 根据环境重载标准异常 |
| **拷贝限制** | 不支持拷贝和赋值操作 | 使用引用传递异常对象 |

---

