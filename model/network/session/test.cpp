
#include "../session/fundamental.hpp"
#include "../session/conversation.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <random>
#include <random>
#include <thread>
#include <atomic>

// static std::string host = std::string{"124.71.136.228"};
static std::string host = std::string{"127.0.0.1"};
static std::uint16_t port = std::uint16_t(6779);

// [函数] 连接池并发借用/归还/失效 + 随机消息发送
void test_pool_concurrent_borrow_random()
{
  std::cout << "[START] test_pool_concurrent_borrow_random" << std::endl;
  using namespace conversation;
  boost::asio::io_context io;
  connection_pool pool(io);
  endpoint_config cfg;
  cfg.min_connections = 3;
  cfg.max_connections = 8;
  cfg.host = host;
  cfg.port = port;
  pool.add_endpoint(cfg);
  pool.start();

  auto make_msg = []()
  {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> len(16, 256);
    std::uniform_int_distribution<int> ch(33, 126);
    std::string s;
    s.resize(len(rng));
    for (auto &c : s)
      c = static_cast<char>(ch(rng));
    return s;
  };

  const int thread_count = 16; // Increased from 4
  const int iterations = 1000; // Increased from 300
  std::vector<std::thread> workers;
  for (int t = 0; t < thread_count; ++t)
  {
    workers.emplace_back([&, t]()
                         {
      for(int i = 0; i < iterations; ++i){
        auto sp_opt = pool.borrow(cfg.host, cfg.port, std::chrono::milliseconds(500));
        if(sp_opt){
          auto sp = sp_opt.value();
          auto msg = make_msg();
          auto ec = sp->send_bytes(msg);
          if(ec){
            std::cout << "[send error] t="<<t<<" i="<<i<<" ec="<<ec.value()<<std::endl;
          }
          if((i % 7) == 0) pool.invalidate(sp);
          else pool.give_back(sp);
        }else{
          std::cout << "[borrow failed] t="<<t<<" i="<<i<<std::endl;
        }
      }
      std::cout << "[END] test_pool_concurrent_borrow_random" << std::endl; });
  }
  for (auto &th : workers)
    th.join();
  auto stats = pool.get_pool_stats(cfg.host, cfg.port);
  std::cout << "[pool stats] remaining=" << stats.remaining_available
            << " in_use=" << stats.in_use
            << " total=" << stats.total << std::endl;
}

// [函数] try_borrow 快速尝试 + borrow 超时回退 + 随机消息
void test_try_borrow_and_timeout_random()
{
  std::cout << "[START] test_try_borrow_and_timeout_random" << std::endl;
  using namespace conversation;
  boost::asio::io_context io;
  connection_pool pool(io);
  endpoint_config cfg;
  cfg.min_connections = 2;
  cfg.max_connections = 4;
  cfg.host = host;
  cfg.port = port;
  pool.add_endpoint(cfg);
  pool.start();

  auto make_msg = []()
  {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> len(8, 128);
    std::uniform_int_distribution<int> ch(32, 126);
    std::string s;
    s.resize(len(rng));
    for (auto &c : s)
      c = static_cast<char>(ch(rng));
    return s;
  };

  for (int i = 0; i < 1000; ++i) // Increased from 200
  {
    auto sp_opt = pool.try_borrow(cfg.host, cfg.port);
    if (!sp_opt)
      sp_opt = pool.borrow(cfg.host, cfg.port, std::chrono::milliseconds(200));
    if (sp_opt)
    {
      auto sp = sp_opt.value();
      auto msg = make_msg();
      sp->send_bytes(msg);
      pool.give_back(sp);
    }
    else
    {
      std::cout << "[try/borrow failed] i=" << i << std::endl;
    }
  }
  auto stats = pool.get_pool_stats(cfg.host, cfg.port);
  std::cout << "[pool3 stats] remaining=" << stats.remaining_available
            << " in_use=" << stats.in_use
            << " total=" << stats.total << std::endl;
  std::cout << "[END] test_try_borrow_and_timeout_random" << std::endl;
}

// [函数] 单会话 async_connect + 多线程并发 async_send_bytes（随机）
void test_single_session_async_send_concurrent_random()
{
  std::cout << "[START] test_single_session_async_send_concurrent_random" << std::endl;
  using namespace conversation::fundamental;
  boost::asio::io_context io;
  auto sess = std::make_shared<session<>>(io);
  sess->set_reception_processing([](std::shared_ptr<session<>>, std::string_view data)
                                 { std::cout << "[receive] size=" << data.size() << std::endl; });

  auto make_msg = []()
  {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> len(64, 512);
    std::uniform_int_distribution<int> ch(33, 126);
    std::string s;
    s.resize(len(rng));
    for (auto &c : s)
      c = static_cast<char>(ch(rng));
    return s;
  };

  std::thread io_thread([&]()
                        { io.run(); });
  sess->async_connect(host, port, [sess, make_msg](const boost::system::error_code &ec)
                      {
    if(ec){
      std::cout << "[connect error] code="<<ec.value()<<std::endl;
      return;
    }
    const int threads = 8; // Increased from 3
    const int sends_per_thread = 500; // Increased from 100
    std::vector<std::thread> senders;
    for(int t=0; t<threads; ++t){
      senders.emplace_back([sess, make_msg, t](){
        for(int i=0; i<sends_per_thread; ++i){
          auto msg = make_msg();
          sess->async_send_bytes(msg, nullptr);
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
      });
    }
    for(auto& th : senders) th.join();
    sess->close();
    std::cout << "[END] test_single_session_async_send_concurrent_random" << std::endl; });
  std::this_thread::sleep_for(std::chrono::seconds(30));
  io.stop();
  io_thread.join();
}

// [函数] adopt_socket 接管外部 socket + start + 随机异步发送
void test_adopt_socket_random_async_send()
{
  std::cout << "[START] test_adopt_socket_random_async_send" << std::endl;
  using namespace conversation::fundamental;
  boost::asio::io_context io;
  boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(host), port);
  boost::asio::ip::tcp::socket sock(io);
  boost::system::error_code ec;
  sock.connect(ep, ec);
  if (ec)
  {
    std::cout << "[sock connect error] " << ec.message() << std::endl;
    std::cout << "[END] test_adopt_socket_random_async_send" << std::endl;
    return;
  }
  auto sess = std::make_shared<session<>>(io);
  if (sess->adopt_socket(std::move(sock), session_type::TCP_SERVER))
  {
    sess->start();
    auto make_msg = []()
    {
      thread_local std::mt19937 rng{std::random_device{}()};
      std::uniform_int_distribution<int> len(32, 256);
      std::uniform_int_distribution<int> ch(33, 126);
      std::string s;
      s.resize(len(rng));
      for (auto &c : s)
        c = static_cast<char>(ch(rng));
      return s;
    };
    std::thread io_thread([&]()
                          { io.run(); });
    for (int i = 0; i < 500; ++i) // Increased from 50
    {
      sess->async_send_bytes(make_msg(), nullptr);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    sess->close();
    io.stop();
    io_thread.join();
    std::cout << "[END] test_adopt_socket_random_async_send" << std::endl;
  }
  else
  {
    std::cout << "[adopt_socket failed]" << std::endl;
    std::cout << "[END] test_adopt_socket_random_async_send" << std::endl;
  }
}

int main()
{
  std::cout << "[START] All tests" << std::endl;
  test_pool_concurrent_borrow_random();
  test_try_borrow_and_timeout_random();
  test_single_session_async_send_concurrent_random();
  test_adopt_socket_random_async_send();
  std::cout << "[END] All tests" << std::endl;
  return 0;
}