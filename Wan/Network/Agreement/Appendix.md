# 协议接口

`Wan/Network/Network.hpp` 导出常用协议类型：

```cpp
#include "Wan/Network/Network.hpp"

using Wan::Network::Agreement::Request;
using Wan::Network::Agreement::Response;
using Wan::Network::Agreement::RequestHeader;
using Wan::Network::Agreement::ResponseHeader;
using Wan::Network::Agreement::ProtocolConverter;
```

协议头提供 `ToString`、`FromString`、`ToJson`、`FromJson`、`CalculateAndSetChecksum` 和 `VerifyIntegrity` 等接口。

`Request<HeaderT>` 和 `Response<HeaderT>` 保存协议头和字符串正文。JSON 表示将协议头保存为对象、正文保存在 `body` 键下；从 JSON 还原时会检查内容长度和校验和。

`Wan::Network::Http` 封装 Beast 的请求与响应类型，保留 Beast 的字段名和方法名。`FromString` 拒绝不完整消息、尾部多余字节和超出解析器限制的正文。

wire 字符串和 JSON 键是协议值，不随源码 PascalCase 命名调整。
