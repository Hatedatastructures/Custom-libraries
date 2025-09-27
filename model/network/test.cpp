// // #include "encryption.hpp"
// // #include <iostream>
// // int main()
// // {
// //   using namespace encryption;
// //   std::string plain = "hello server adakjdhagdajhgsdgfsahdfsadasd sdsjadhjagdja";
// //   std::string pass = "my strong passphrase 123!";

// //   auto cipher = arcane_symmetric::AESGCM_Encrypt(plain, pass);
// //   std::cout << "长度: " << cipher.size() << "加密字符：" << cipher << std::endl;
// //   auto recovered = arcane_symmetric::AESGCM_Decrypt(cipher, pass);
// //   std::cout << "Recovered: " << recovered << "\n";

// //   auto key_value = penumbra_asymmetric::GenerateElGamalKeypair();
// //   auto private_key = key_value.first;
// //   auto public_key = key_value.second;

// //   auto cipher_text = penumbra_asymmetric::ElGamal_Encrypt(plain, public_key);
// //   std::cout << "长度：" << cipher_text.size() << "非对称加密：" << cipher_text << std::endl;
// //   auto decrypted = penumbra_asymmetric::ElGamal_Decrypt(cipher_text, private_key);
// //   auto decrypted_val = penumbra_asymmetric::ElGamal_Decrypt(cipher_text, public_key);
// //   std::cout << "Recovered: " << decrypted << "\n";
// //   std::cout << "Recovered: " << decrypted_val << "\n";
// //   return 0;
// // }
// #define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
// #include "encryption.hpp" // <-- 把你上面的头文件保存为此名，或改为实际文件名
// #include <iostream>
// #include <chrono>
// #include <fstream>
// #include <thread>
// #include <windows.h>
// using namespace encryption;

// static void expect_true(bool cond, const std::string &msg)
// {
//   std::cout << (cond ? "[PASS] " : "[FAIL] ") << msg << "\n";
//   if (!cond)
//   {
//     // don't exit immediately so we run all tests; could throw if you prefer.
//   }
// }

// static void test_hash()
// {
//   std::cout << "\n=== Hash tests ===\n";
//   auto md5_hello = umbrage_hash::MD5("hello");
//   expect_true(md5_hello == "5d41402abc4b2a76b9719d911017c592", "MD5(\"hello\") matches expected");

//   auto sha256_hello = umbrage_hash::SHA256("hello");
//   expect_true(sha256_hello == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
//               "SHA256(\"hello\") matches expected");
// }

// static void test_symmetric_roundtrips()
// {
//   std::cout << "\n=== Symmetric tests ===\n";
//   std::string plain = "The quick brown fox jumps over the lazy dog. 0123456789";
//   std::string pass = "my strong passphrase 123!";

//   // AES-GCM
//   {
//     std::string c = arcane_symmetric::AESGCM_Encrypt(plain, pass);
//     std::string p = arcane_symmetric::AESGCM_Decrypt(c, pass);
//     expect_true(p == plain, "AES-GCM roundtrip");
//     // tamper detection: decode base64, flip a byte, re-encode -> should throw
//     try
//     {
//       std::string bin = FromBase64(c);
//       if (bin.size() > 20)
//         bin[20] ^= 0xFF;
//       std::string tampered = ToBase64(bin);
//       bool thrown = false;
//       try
//       {
//         auto r = arcane_symmetric::AESGCM_Decrypt(tampered, pass);
//       }
//       catch (...)
//       {
//         thrown = true;
//       }
//       expect_true(thrown, "AES-GCM tamper detection (decrypt should fail)");
//     }
//     catch (...)
//     {
//       std::cout << "[WARN] Could not perform tamper test for AES-GCM\n";
//     }
//     // wrong password
//     bool wrong_throw = false;
//     try
//     {
//       arcane_symmetric::AESGCM_Decrypt(c, std::string("wrongpass"));
//     }
//     catch (...)
//     {
//       wrong_throw = true;
//     }
//     expect_true(wrong_throw, "AES-GCM wrong-pass causes failure");
//   }

//   // AES-CBC + HMAC
//   {
//     std::string c = arcane_symmetric::AESCBC_Encrypt(plain, pass);
//     std::string p = arcane_symmetric::AESCBC_Decrypt(c, pass);
//     expect_true(p == plain, "AES-CBC(+HMAC) roundtrip");
//     // wrong pass
//     bool wrong_throw = false;
//     try
//     {
//       arcane_symmetric::AESCBC_Decrypt(c, "bad");
//     }
//     catch (...)
//     {
//       wrong_throw = true;
//     }
//     expect_true(wrong_throw, "AES-CBC wrong-pass causes failure");
//   }

//   // AES-CTR + HMAC
//   {
//     std::string c = arcane_symmetric::AESCTR_Encrypt(plain, pass);
//     std::string p = arcane_symmetric::AESCTR_Decrypt(c, pass);
//     expect_true(p == plain, "AES-CTR(+HMAC) roundtrip");
//   }

//   // Twofish-CTR + HMAC
//   {
//     std::string c = arcane_symmetric::TwofishCTR_Encrypt(plain, pass);
//     std::string p = arcane_symmetric::TwofishCTR_Decrypt(c, pass);
//     expect_true(p == plain, "Twofish-CTR(+HMAC) roundtrip");
//   }
// }

// static void test_asymmetric_basic_and_signing()
// {
//   std::cout << "\n=== Asymmetric tests ===\n";
//   // RSA
//   auto rsa_pair = penumbra_asymmetric::GenerateRSAKeypair(2048);
//   std::string priv_b64 = rsa_pair.first;
//   std::string pub_b64 = rsa_pair.second;

//   std::string secret = "top secret";
//   std::string cipher = penumbra_asymmetric::RSA_OAEP_Encrypt(secret, pub_b64);
//   std::string recovered = penumbra_asymmetric::RSA_OAEP_Decrypt(cipher, priv_b64);
//   expect_true(recovered == secret, "RSA-OAEP encrypt/decrypt");

//   // RSA sign/verify
//   std::string msg = "message to sign";
//   std::string sig = penumbra_asymmetric::RSA_PSS_Sign(msg, priv_b64);
//   bool ok = penumbra_asymmetric::RSA_PSS_Verify(msg, sig, pub_b64);
//   expect_true(ok, "RSA-PSS sign/verify success");

//   // wrong pub verification should fail
//   auto rsa_pair2 = penumbra_asymmetric::GenerateRSAKeypair(2048);
//   bool ok2 = penumbra_asymmetric::RSA_PSS_Verify(msg, sig, rsa_pair2.second);
//   expect_true(!ok2, "RSA-PSS verify fails with wrong public key");

//   // persist keys to files and reload test
//   {
//     std::ofstream("rsa_priv.b64") << priv_b64;
//     std::ofstream("rsa_pub.b64") << pub_b64;
//     std::string loaded_priv;
//     std::ifstream in1("rsa_priv.b64");
//     std::getline(in1, loaded_priv);
//     std::string loaded_pub;
//     std::ifstream in2("rsa_pub.b64");
//     std::getline(in2, loaded_pub);
//     std::string rec = penumbra_asymmetric::RSA_OAEP_Decrypt(penumbra_asymmetric::RSA_OAEP_Encrypt("x", loaded_pub), loaded_priv);
//     expect_true(rec == "x", "RSA key save/load roundtrip");
//   }

//   // ElGamal (may be slower)
//   try
//   {
//     auto eg_pair = penumbra_asymmetric::GenerateElGamalKeypair(1024); // use 1024 for speed in test; for security use 2048+
//     std::string eg_cipher = penumbra_asymmetric::ElGamal_Encrypt("elgamal test", eg_pair.second);
//     std::string eg_plain = penumbra_asymmetric::ElGamal_Decrypt(eg_cipher, eg_pair.first);
//     expect_true(eg_plain == "elgamal test", "ElGamal encrypt/decrypt");
//   }
//   catch (...)
//   {
//     std::cout << "[WARN] ElGamal test skipped or failed (can be slow / platform dependent)\n";
//   }
// }

// static void simple_bench()
// {
//   std::cout << "\n=== Simple Benchmark ===\n";
//   // AES-GCM throughput (using pass-derived key path)
//   const int ITER = 200;
//   std::string payload(16 * 1024, 'A'); // 16 KB
//   std::string pass = "benchmark password 1!";

//   // warmup
//   for (int i = 0; i < 10; i++)
//     arcane_symmetric::AESGCM_Encrypt(payload, pass);

//   auto t0 = std::chrono::high_resolution_clock::now();
//   size_t total = 0;
//   for (int i = 0; i < ITER; i++)
//   {
//     auto c = arcane_symmetric::AESGCM_Encrypt(payload, pass);
//     total += payload.size();
//   }
//   auto t1 = std::chrono::high_resolution_clock::now();
//   double secs = std::chrono::duration<double>(t1 - t0).count();
//   double mbps = (double)total / (1024.0 * 1024.0) / secs;
//   std::cout << "AES-GCM (derived-key path) throughput: " << mbps << " MB/s (" << secs << "s)\n";

//   // RSA roundtrip ops/s (short message)
//   auto rsa_pair = penumbra_asymmetric::GenerateRSAKeypair(2048);
//   std::string small = "hi";
//   auto t2 = std::chrono::high_resolution_clock::now();
//   const int RITER = 300;
//   for (int i = 0; i < RITER; i++)
//   {
//     auto c = penumbra_asymmetric::RSA_OAEP_Encrypt(small, rsa_pair.second);
//     auto p = penumbra_asymmetric::RSA_OAEP_Decrypt(c, rsa_pair.first);
//     (void)p;
//   }
//   auto t3 = std::chrono::high_resolution_clock::now();
//   double secs2 = std::chrono::duration<double>(t3 - t2).count();
//   std::cout << "RSA-2048 roundtrip: " << (double)RITER / secs2 << " ops/s (" << secs2 << "s)\n";
// }

// int main()
// {
//   std::cout << "Crypto++ wrapper test suite\n";
//   test_hash();
//   test_symmetric_roundtrips();
//   test_asymmetric_basic_and_signing();
//   simple_bench();
//   std::cout << "\nAll tests finished (check PASS/FAIL messages above).\n";
//   system("pause");
//   return 0;
// }
// #include "encryption.hpp"

// int main()
// {
//   std::string value("hello,worsld!");
//   std::cout << encryption::CyclicRedundancyCheck32(value,19) << std::endl;
//   std::cout << value << std::endl;
//   return 0;
// }

// #include <boost/asio.hpp>
// #include <iostream>

// int main()
// {
//   boost::asio::io_context io_context;
//   // 创建解析器对象
//   boost::asio::ip::tcp::resolver resolver(io_context);
//   std::string hostname = "www.X.com";
//   std::string service = "http"; // 或直接使用端口号字符串 "80"

//   try
//   {
//     boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(hostname, service);
//     std::cout << "IP addresses for " << hostname << " are:" << std::endl;
//     // 遍历所有解析结果
//     for (const auto &endpoint : endpoints)
//     {
//       // endpoint.endpoint().address() 获取解析出的IP地址
//       std::cout << endpoint.endpoint().address() << std::endl;
//     }
//   }
//   catch (boost::system::system_error &e)
//   {
//     // 解析失败，处理异常
//     std::cerr << "Error: " << e.what() << std::endl;
//   }

//   return 0;
// }
// #include <iostream>
// #include "agreement.hpp"
// int main()
// {
//   agreement::request request;
//   request.headers["Content-Type"] = "application/json";
//   request.path = "{\"name\":\"test\"}";
//   request.streaming_message_body = "这个是请求体";
//   std::string request_str = request.to_string();

//   agreement::request re_v;
//   re_v.from_string(request_str);

//   std::cout << re_v.to_string() << std::endl;
//   std::cout << request_str << std::endl;
//   return 0;
// }

// #include "agreement.hpp"
// int main()
// {
//   agreement::request_header header;
//   header[std::string("Content-Type")] = "application/json";
//   std::cout << header[std::string("Content-Type")] << std::endl;
//   return 0;
// }
// #include "agreement.hpp"
// int main()
// {
//   std::unordered_map<int, std::string> m;
//   auto [it, inserted] = m.try_emplace(42, "answer");

//   if (inserted)
//     std::cout << "inserted: " << it->second << '\n';
//   else
//     std::cout << "already existed: " << it->second << '\n';
//   return 0;
// }
// #include "agreement.hpp"
// int main()
// {
//     agreement::request request;
//     request.headers["Content-Type"] = "application/json";
//     request.streaming_message_body = "这个是请求体";
//     std::string request_str = request.to_string();
//     std::cout << request_str << std::endl;
//     return 0;
// }
// #include <json/json.h>
// #include <iostream>
// int main()
// {
//     Json::Value root;
//     Json::Value array;
//     Json::Value array2;
//     array2.append("1");
//     array2.append("2");
//     array2.append(10ULL);
//     array.append(array2);
//     array.append("1");
//     array.append("2");
//     array.append(8ULL);
//     root["array"] = array;
//     root["name"] = "test";
//     root["age"] = 20;
//     std::cout << root.toStyledString() << std::endl;
//     return 0;
// }
// #include "boost/json.hpp"
// #include <iostream>
// int main()
// {
//     boost::json::object obj;
//     obj["wang"] = "123";
//     obj["age"] = 18;
//     obj["array"] = boost::json::array{1, 2, 3};
//     std::cout << boost::json::serialize(obj) << std::endl;
//     return 0;
// }

// #include "agreement.hpp"
// int main()
// {
//   agreement::request request;
//   request.information.headers["Content-Type"] = "application/json";
//   request.streaming_message_body = "这个是请求体";
//   std::string request_str = request.to_string();
//   auto v = agreement::to_json(request);
//   std::cout << boost::json::serialize(v) << std::endl;
//   auto string_val = boost::json::serialize(v);
//   std::cout << request_str << std::endl;
//   agreement::request re_v;
//   std::string erro;
//   agreement::from_json(v, re_v, &erro);
//   std::cout << boost::json::serialize(agreement::to_json(re_v)) << std::endl;
//   return 0;
// }
#include "conversation.hpp"
int main()
{
  boost::asio::io_context io;
  boost::asio::ip::tcp::socket socket(io);
  // boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("124.71.136.228"),static_cast<std::uint16_t>(6779));
  socket.connect(endpoint); // 连接到服务器地址
  conversation_management::conversation conversation(std::move(socket));
  std::string value ("hello world,来自客户端的消息");
  conversation.transmission(value);
  auto ip_value = conversation.remote_ip();
  std::cout << ip_value << std::endl;
  return 0;
}
