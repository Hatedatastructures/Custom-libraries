
#include "../include/wan.hpp"


namespace http = wan::network::http;
int main()
{
  {
    wan::mco::concurrent_string str("hello world");
    std::cout << std::format("线程安全封装的字符串打印:{}", str.str()) << std::endl;
  }
  {
    http::request<> request;
    const http::response<> response;
    request.set(http::field::host, "www.google.com");
    request.set(http::field::user_agent, "Mozart");
    request.set(http::field::content_type, "application/json");
    request.set(http::field::accept, "application/json");
    std::cout << std::format("请求：{}",request.to_string()) << std::endl;
    std::cout << std::format("响应：{}",response.to_string()) << std::endl;
  }
  return 0;
}