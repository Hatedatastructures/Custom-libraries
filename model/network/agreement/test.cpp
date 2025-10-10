#include "protocol.hpp"
#include "boost/json/serializer.hpp"
#include "conversion.hpp"
#include <boost/json/serialize.hpp>
#include <iostream>
#include <string>
#include <fstream>
// std::string serialize_with_indent(const boost::json::value &jv)
// {
//   // 设置格式化选项
//   boost::json::serialize_options opts;
//   opts.allow_infinity_and_nan = false;

//   return boost::json::serialize(jv, opts);
// }
// int main()
// {
//   auto start = std::chrono::system_clock::now();

//   protocol::request req;
//   req.header().set_method("GET");
//   protocol::response resp;
//   resp.header().set_status_code(200);
//   resp.header().set_status_message("OK");
//   resp.set_message("json test");
//   auto request_str = req.to_string();
//   std::cout << "request:" << request_str << std::endl;
//   auto json_value = resp.to_json().to_string();
//   protocol::response resp2;
//   resp2.from_string(resp.to_string());
//   std::cout << resp2.to_string() << std::endl;
//   std::fstream file("response.json", std::ios::out);
//   file << json_value << resp2.to_json().to_string();
//   std::cout << resp2.verify_integrity() << std::endl;
//   file.close();
//   //
//   protocol::json object;
//   object.set("name", "wang");
//   object.set("age", 18);
//   std::cout << object.to_string() << std::endl;
  
//   //
//   auto str = serialize_with_indent(resp.to_json().value());
//   std::cout << "response:" << str << std::endl;

//   auto end = std::chrono::system_clock::now();
//   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//   std::cout << "time:" << duration.count() << "ms" << std::endl;
//   return 0;
// }
int main()
{
  protocol::request req;
  req.header().set_header("Content-Type", "application/json");
  req.header().set_header("Content-Length", "123");
  req.header().set_method("GET");
  req.set_message("{这是一个请求协议内容}");
  protocol::response resp;
  resp.header().set_status_code(200);
  resp.header().set_status_message("OK");
  std::cout << json.to_string() << std::end;
  resp.header().set_header("Content-Type", "application/json");
  resp.header().set_header("Content-Length", "456");
  std::cout << resp.to_string() << std::endl;
  std::cout << req.to_string() << std::endl;
  protocol::conversion::protocol_converter conver;
  auto json = conver.request_to_json(req);
  auto req2 = conver.json_to_request(json);
  if (req2)
    std::cout << req2->to_string() << std::endl;
  auto json2 = conver.response_to_json(resp);
  std::cout << json2.to_string() << std::endl;
  std::cout << json.to_string() << std::endl;
  return 0;
}