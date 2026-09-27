# 密码学接口

公开加密类型由 `Wan::Network::Ciphertext` 导出。

| 类型 | 用途 |
|---|---|
| `AeonRandom` | Crypto++ 随机池封装 |
| `ArcaneSymmetric` | AES-GCM、AES-CBC/HMAC、AES-CTR/HMAC、Twofish-CTR/HMAC |
| `PenumbraAsymmetric` | RSA-OAEP、RSA-PSS 和 ElGamal 操作 |
| `UmbrageHash` | MD5 兼容哈希和 SHA256 |
| `Binary` | 内部算法标识符 |

口令派生密钥使用 PBKDF2-HMAC-SHA256。对称解密在调用 Crypto++ 前检查数据包魔数、算法、IV 长度、tag/MAC 长度及各段偏移；CBC/CTR/Twofish 的 MAC 使用常量时间比较。RSA 和 ElGamal 不接受低于 2048 位的密钥长度。

```cpp
auto Cipher = Wan::Network::Ciphertext::ArcaneSymmetric::AesGcmEncrypt(
    "payload", "passphrase");
auto Plain = Wan::Network::Ciphertext::ArcaneSymmetric::AesGcmDecrypt(
    Cipher, "passphrase");
```

`CRC32` 只能检查非密码学完整性，不能认证数据；`Mix64` 不能生成密码学安全令牌，不可用于 Session ID。
