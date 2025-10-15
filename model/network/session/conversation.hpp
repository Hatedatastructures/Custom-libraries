/**
 * @file conversation.hpp
 * @brief 会话定义
 * @details 提供会话的定义与操作，包括会话的创建、销毁、消息的发送、接收等功能
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "../sched/thread_pool.hpp"
#include "./fundamental.hpp"

namespace conversation
{
  /**
   * @brief 复用`fundamental::stringable_constraints`约束
   */
  template <class protocol_t> 
  concept serializable_constraints = fundamental::stringable_constraints<protocol_t>;

  using request = fundamental::request;
  using response = fundamental::response;

  class session_management_config
  {
  public:
    std::uint64_t thread_size{10}; // 线程池大小
    std::uint64_t thread_max_size{64}; // 线程池最大线程数
  }; // end class session_management_config
  /**
   * @brief 会话管理类
   * @details 提供会话的创建、销毁、消息的发送、接收等功能
   */
  template<serializable_constraints request_t = request, serializable_constraints response_t = response>
  class session_management
  {
  public:
    using session_ptr = std::shared_ptr<session<request_t, response_t>>;
    using thread_pool = wan::thread_pool;
  private:
    std::atomic<bool> _running{false}; // 会话管理是否正在运行
    mutable std::shared_mutex _sessions_mutex; // 会话映射的互斥锁

    boost::asio::io_context& _io_context; // io上下文
    boost::asio::steady_timer _cleanup_timer; // 会话清理定时器
    std::chrono::seconds _cleanup_interval{60}; // 会话清理时间间隔
    std::unordered_map<std::string, session_ptr> _sessions_map; // 会话映射

    session_management_config _config; // 默认配置

    std::unique_ptr<thread_pool> _thread_pool; // 线程池
    std::atomic<bool> _thread_pool_running{false}; // 线程池是否正在运行
  private:
    /**
     * @brief 初始化线程池
     * @return true 线程池初始化成功
     * @return false 线程池初始化失败
     */
    void _initialize_thread_pool()
    {
      if(!_thread_pool)
      {
        pool_config thread_pool_config;
        thread_pool_config._pool_name = "session_management";
        thread_pool_config._initial_threads = _config.thread_size * 2;
        thread_pool_config._min_threads = 2;
        thread_pool_config._max_threads = _config.thread_max_size * 4;
        thread_pool_config._core_threads = _config.thread_size * 2;
        thread_pool_config._queue_policy = rank_strategy::priority;
        thread_pool_config._scheduling_tactics = scheduling_tactics::adaptive;
        thread_pool_config._enable_monitoring = true;
        thread_pool_config._enable_performance_profiling = false;
        _thread_pool = wan::make_thread_pool(thread_pool_config);
        if (_thread_pool && _thread_pool->start())
        {
          _thread_pool_running.store(true);
          return true;
        }
      }
      return false;
    }
    /**
     * @brief 停止线程池
     * @return true 线程池停止成功
     * @return false 线程池停止失败
     */
    bool _stop_thread_pool()
    {
      if(_thread_pool && _thread_pool_running.load())
      {
        if(_thread_pool->shutdown(std::chrono::seconds(5)))
        {
          _thread_pool_running.store(false);
          return true;
        }
      }
      return false;
    }
    /**
     * @brief 启动线程池
     * @return `true` 线程池启动成功
     * @return `false` 线程池启动失败
     */
    bool _start_thread_pool()
    {
      if(_thread_pool && !_thread_pool_running.load())
      {
        if(_thread_pool->start())
        {
          _thread_pool_running.store(true);
          return true;
        }
      }
      return false;
    }
    /**
     * @brief 启动清理定时器
     */
    void _start_cleanup_timer()
    {
      if(!_running.load())
        return;
      _cleanup_timer.expires_after(_cleanup_interval);
      auto cleanup_function = [this](const boost::system::error_code& ec)
      {
        if(!ec && _running.load())
        {
          auto _cleanup_task = [this]()
          {
            _cleanup_inactive_sessions();
          };
          if(_thread_pool_running.load() && _thread_pool)
            _thread_pool->submit_priority(weight::low, _cleanup_task);
          else
            std::async(std::launch::async, _cleanup_task);
          _start_cleanup_timer();
        }
      };
      _cleanup_timer.async_wait(cleanup_function);
    }
    /**
     * @brief 清理过期会话
     */
    void _cleanup_inactive_sessions()
    {
      std::vector<std::string> inactive_sessions;
      {
        std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
        for (auto pair : _sessions_map)
        {
          const auto &session = pair.second;
          if (!session->is_connected() || session->get_statistics().get_idle_time() > std::chrono::minutes(10))
          {
            inactive_sessions.push_back(pair.first);
          }
        }
      }
      for(const auto session_string_id : inactive_sessions)
      {
        remove_session(session_string_id);
      }
    }
  public:
    session_management(boost::asio::io_context& io_context,
      const session_management_config& config = session_management_config())
      : _io_context(io_context),_config(config),_cleanup_timer(_io_context)
    {
      _initialize_thread_pool();
    }
    ~session_management()
    {
      stop();
      _stop_thread_pool();
    }
    session_management(const session_management &) = delete;
    session_management &operator=(const session_management &) = delete;
    /**
     * @brief 启动会话管理
     * @return `true` 会话管理启动成功
     * @return `false` 会话管理启动失败
     */
    bool start()
    {
      if(!_running.load())
      {
        _running.store(true);
        if(_start_thread_pool())
        {
          _start_cleanup_timer();
          return true;
        }
      }
      return false;
    }
    /**
     * @brief 停止会话管理
     * @return `true` 会话管理停止成功
     */
    bool stop()
    {
      _running.store(false);
      _cleanup_timer.cancel();
      auto stop_session = [this](const boost::system::error_code& ec)
      {
        if(!ec)
        {
          std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
          for(auto& pair : _sessions_map)
            pair.second->close();
          _sessions_map.clear();
        }
      };
      if(_thread_pool_running.load() && _thread_pool)
        _thread_pool->submit_priority(weight::high, stop_session);
      else
        std::async(std::launch::async, stop_session);
    }
    auto create_session(boost::asio::ip::tcp::socket&& socket)
    -> std::shared_ptr<session<request,response>>
    {
      if(socket.is_open())
      {
        session_ptr sess = std::make_shared<fundamental::session<request_t,response_t>>(std::move(socket));
        {
          std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
          session_string_id = sess->get_session_id();
          _sessions_map[session_string_id] = sess;
        }
        return sess;
      }
      return nullptr;
    }
    /**
     * @brief 创建客户端会话
     * @param endpoint 会话端点
     * @return `std::pair<string,std::shared_ptr<session<request,response>>>` 会话指针
     */
    auto create_client_session(const boost::asio::ip::tcp::endpoint& endpoint)
    -> std::pair<std::string, std::shared_ptr<session<request_t,response_t>>>
    {
      std::string session_string_id;
      boost::asio::ip::tcp::socket socket(_io_context);
      socket.connect(endpoint);
      auto sess = create_session(std::move(socket));
      if(sess)
        return std::make_pair(sess->get_session_id(), sess);
      return std::make_pair(session_string_id, nullptr);
    }
    /**
     * @brief 创建服务器会话
     * @param socket 会话套接字
     * @return `std::pair<string,std::shared_ptr<session<request,response>>>` 会话指针
     */
    auto create_server_session(boost::asio::ip::tcp::socket&& socket)
    -> std::pair<std::string, std::shared_ptr<session<request_t,response_t>>>
    {
      auto sess = create_session(std::move(socket));
      if(sess)
        return std::make_pair(sess->get_session_id(), sess);
      return std::make_pair(std::string{}, nullptr);
    }
    /**
     * @brief 获取会话
     * @param session_string_id 会话ID
     * @return `std::shared_ptr<session<request,response>>` 会话指针
     */
    session_ptr get_session(const std::string& session_string_id)
    {
      std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
      auto it = _sessions_map.find(session_string_id);
      if(it != _sessions_map.end())
        return it->second;
      return nullptr;
    }
    /**
     * @brief 移除会话
     * @param session_string_id 会话`ID`
     * @return `true` 会话移除成功
     */
    bool remove_session(const std::string& session_string_id)
    {
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      auto it = _sessions_map.find(session_string_id);
      if(it != _sessions_map.end())
      {
        it->second->close();
        _sessions_map.erase(it);
        return true;
      }
      return false;
    }
    /**
     * @brief 获取会话数量
     * @return  会话数量
     */
    std::uint64_t get_session_count() const
    {
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      return _sessions_map.size();
    }
    /**
     * @brief 获取所有会话`ID`列表
     * @return `std::vector<std::string>` 会话`ID`列表
     */
    std::vector<std::string> get_session_ids() const
    {
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      std::vector<std::string> session_ids;
      session_ids.reserve(_sessions_map.size());
      for(const auto& pair : _sessions_map)
        session_ids.push_back(pair.first);
      return session_ids;
    }
    /**
     * @brief 获取内部线程池统计信息
     * @return 线程池统计信息
     */
    std::optional<pool_statistics> get_thread_pool_statistics() const
    {
      if(_thread_pool_running.load() && _thread_pool)
        return _thread_pool->get_statistics();
      return std::nullopt;
    }
  }; // end class session_management
} // end namespace conversation