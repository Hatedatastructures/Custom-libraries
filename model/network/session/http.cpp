#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <random>
#include <random>
#include <thread>
#include <atomic>

int main()
{
  
  boost::beast::http::request<boost::beast::http::string_body> req;
  req.method(boost::beast::http::verb::get);
  req.target("/");
  req.version(11);
  req.set(boost::beast::http::field::host, "example.com");
  req.set(boost::beast::http::field::user_agent, "Beast");
  req.body() = "Hello, World!";
  std::cout << req << std::endl << std::endl;

  std::cout << std::string(req.body()) << std::endl << std::endl;

  boost::beast::http::response<boost::beast::http::string_body> res;
  res.version(req.version());
  res.set(boost::beast::http::field::server, "Beast");
  res.set(boost::beast::http::field::content_type, "text/plain");
  res.body() = "Hello, World!";
  res.prepare_payload();
  std::cout << res << std::endl;
  return 0;
}