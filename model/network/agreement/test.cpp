#include "protocol.hpp"
#include "boost/json/serializer.hpp"
#include <boost/json/serialize.hpp>
#include <iostream>
#include <string>
#include <fstream>
std::string serialize_with_indent(const boost::json::value &jv)
{
  // 设置格式化选项
  boost::json::serialize_options opts;
  opts.allow_infinity_and_nan = false;

  return boost::json::serialize(jv, opts);
}
int main()
{
  protocol::request req;
  req.header().set_method("GET");
  protocol::response resp;
  resp.header().set_status_code(200);
  resp.header().set_status_message("OK");
  auto request_str = req.to_string();
  std::cout << "request:" << request_str << std::endl;
  auto json_value = resp.to_json().to_string();
  std::fstream file("response.json", std::ios::out);
  file << json_value;
  file.close();
  //
  protocol::json object;
  object.set("name", "wang");
  object.set("age", 18);
  std::cout << object.to_string() << std::endl;
  
  //
  auto str = serialize_with_indent(resp.to_json().value());
  std::cout << "response:" << str << std::endl;
  return 0;
}
// int main()
// {
//   protocol::json j;
//   j.set("name", "test");
//   j.set("version", 1.0);
//   j.set("features", std::vector<std::string>{"format", "indent"});
//   boost::json::value nested;
//   boost::json::object nested_obj;
//   // 写入带缩进的JSON文件（缩进2个空格）
//   bool ok = j.to_formatted_file("formatted.json", 2);
//   if (ok)
//   {
//     std::cout << "文件已按格式化写入" << std::endl;
//   }
//   return 0;
// }
// int main()
// {
//   boost::json::serializer serializer;
//   serializer.reset();
//   boost::json::serialize_options opts;
//   opts.allow_infinity_and_nan = false;

//   return 0;
// }