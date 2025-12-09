
#include <iostream>
#include <format>
#include <coroutine>
#include <string>
#include "../include/wan.hpp"

namespace http = wan::network::http;

// 参数 socket 通过移动语义获取，确保唯一所有权
boost::asio::awaitable<void> echo(boost::asio::ip::tcp::socket socket)
{
  try
  {
    std::string data;
    data.resize(1024);
    for (;;)
    {
      // 异步读取数据，co_await 会挂起协程直到有数据可读或出错
      // use_awaitable 指定此异步操作使用协程方式等待
      std::size_t n = co_await socket.async_read_some(boost::asio::buffer(data), boost::asio::use_awaitable);

      std::cout << "Received: " << std::string(data, n);

      // 异步将收到的数据回写给客户端
      co_await boost::asio::async_write(socket, boost::asio::buffer(data, n), boost::asio::use_awaitable);

      std::cout << "Echoed back." << std::endl;
    }
  }
  catch (const std::exception &e)
  {
    // 异常通常由异步操作失败（如客户端断开连接）引起
    std::cout << "Echo Exception: " << e.what() << " for client "<< socket.remote_endpoint() << std::endl;
  }
  // 协程结束，socket 析构函数会自动关闭连接
}

boost::asio::awaitable<void> listener(boost::asio::io_context& ioc, unsigned short port)
{
  // 获取当前协程所在的执行器 (Executor)
  auto executor = co_await boost::asio::this_coro::executor;
  // 创建监听器 Acceptor，绑定到指定端口
  boost::asio::ip::tcp::acceptor acceptor(executor, {boost::asio::ip::tcp::v4(), port});
  std::cout << "Server listening on port " << port << std::endl;

  for (;;)
  {
    // 异步接受一个新连接。协程在此挂起，直到有新客户端连接
    auto socket = co_await acceptor.async_accept(boost::asio::use_awaitable);
    std::cout << "New client connected: " << socket.remote_endpoint() << std::endl;

    // 为这个新连接启动一个独立的 echo 协程进行处理
    // co_spawn 用于启动新的协程
    // detached 表示不关心新协程的返回结果，让它独立运行
    boost::asio::co_spawn(executor, echo(std::move(socket)),boost::asio::detached);
  }
}

int main()
{
  // {
  //   wan::mco::concurrent_string str("hello world");
  //   std::cout << std::format("线程安全封装的字符串打印:{}", str.str()) << std::endl;
  // }
  // {
  //   http::request<> request;
  //   const http::response<> response;
  //   request.set(http::field::host, "www.google.com");
  //   request.set(http::field::user_agent, "Mozart");
  //   request.set(http::field::content_type, "application/json");
  //   request.set(http::field::accept, "application/json");
  //   std::cout << std::format("请求：{}", request.to_string()) << std::endl;
  //   std::cout << std::format("响应：{}", response.to_string()) << std::endl;
  // }
  {
    try
    {
      // io_context 是 Asio 库的事件循环核心，处理所有异步任务
      boost::asio::io_context ioc;

      // 设置信号处理，用于捕获 Ctrl+C 等中断信号，实现优雅退出
      boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);

      auto stop_function = [&](auto,auto)
      {
        std::cout << "Stopping..." << std::endl;
        ioc.stop();
      };
      signals.async_wait(stop_function);

      // 启动监听协程
      boost::asio::co_spawn(ioc, listener(ioc, 10086),boost::asio::detached);

      std::cout << "Server started. Press Ctrl+C to exit." << std::endl;

      // 运行事件循环。主线程将在此阻塞，处理所有异步操作，直到 ioc.stop() 被调用
      ioc.run();

    }
    catch (const std::exception& e)
    {
      std::cerr << "Main Exception: " << e.what() << std::endl;
      return 1;
    }
  }
  return 0;
}