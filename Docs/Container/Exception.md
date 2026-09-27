# 容器异常

`CustomException::Fault` 是继承自 `std::exception` 的项目异常，保存三项信息：错误文本、记录的函数名和源码行号。`what()` 返回错误文本；`FunctionNameGet()` 和 `LineNumberGet()` 分别读取另外两项信息。`StandardCon` 通过 `using namespace CustomException` 重新导出该类型。

```cpp
#include <Wan.hpp>
#include <iostream>

int main()
{
    try
    {
        throw CustomException::Fault("invalid input", "main", 1);
    }
    catch (const CustomException::Fault& Error)
    {
        std::cerr << Error.what() << " at "
                  << Error.FunctionNameGet() << ':'
                  << Error.LineNumberGet() << '\n';
    }
    return 0;
}
```

`Fault` 只保存和提供诊断信息，不负责记录日志或终止程序；是否输出、转换或继续抛出由调用方决定。部分容器接口会在抛出前自行将越界诊断写入 `std::cerr`，这一副作用由具体接口说明。
