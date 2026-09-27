# 容器命名空间

公开别名定义于 `Wan/Container/Container.hpp`：

    namespace Wan
    {
        namespace Scl = StandardCon;
    }

应用代码可使用 `StandardCon::Type` 或 `Wan::Scl::Type`。`VectorContainer`、`BaseContainer` 等实现命名空间不是扩展接口。
