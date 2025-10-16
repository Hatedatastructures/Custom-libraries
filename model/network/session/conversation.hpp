/**
 * @file conversation.hpp
 * @brief 会话定义
 * @details 提供会话的定义与操作，包括会话的创建、销毁、消息的发送、接收等功能
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <future>
#include <optional>
#include <memory>
#include <functional>

#include "../../sched/thread_pool.hpp"
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
   * @details 提供指定协议类型的会话管理功能
   * @warning 会话管理类在使用前必须先调用 `start()` 方法启动会话管理线程
   */
  template<serializable_constraints request_t = request, serializable_constraints response_t = response>
  class session_management
  {
  public:
    using session_ptr = std::shared_ptr<fundamental::session<request_t, response_t>>;
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
    bool _initialize_thread_pool()
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
            _cleanup_task();
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
      // 检查管理器是否仍在运行，避免在停止后触发清理
      if(!_running.load())
        return;
        
      std::vector<std::string> inactive_sessions;
      {
        std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
        for (const auto& pair : _sessions_map)
        {
          const auto &session = pair.second;
          if (!session->is_connected() || session->get_statistics().get_idle_time() > std::chrono::minutes(10))
          {
            inactive_sessions.push_back(pair.first);
          }
        }
      }
      
      if(!_running.load())
        return;
        
      for(const auto& session_string_id : inactive_sessions)
      {
        if(!_running.load())
          break;
        remove_session_if_disconnected(session_string_id);
      }
    }
  private:
    std::vector<session_ptr> _screening_session(const std::vector<std::string>& ids, bool only_connected) const
    {
      std::vector<session_ptr> targets;
      {
        std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
        targets.reserve(ids.size());
        for(const auto& id : ids)
        {
          auto it = _sessions_map.find(id);
          if(it != _sessions_map.end())
          {
            const auto& sp = it->second;
            if(!only_connected || sp->is_connected())
              targets.push_back(sp);
          }
        }
      }
      return targets;
    }
    std::vector<session_ptr> _all_session(bool only_connected) const
    {
      std::vector<session_ptr> snapshot;
      {
        std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
        snapshot.reserve(_sessions_map.size());
        for(const auto& kv : _sessions_map)
        {
          if(!only_connected || kv.second->is_connected())
            snapshot.push_back(kv.second);
        }
      }
      return snapshot;
    }
    // 基于谓词收集会话快照（谓词返回true则包含）
    template<class prediction>
    std::vector<session_ptr> _conditional_filtering(prediction&& pred, bool only_connected) const
    {
      std::vector<session_ptr> snapshot;
      {
        std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
        snapshot.reserve(_sessions_map.size());
        for(const auto& kv : _sessions_map)
        {
          const auto& id = kv.first;
          const auto& sp = kv.second;
          if(pred(id, sp) && (!only_connected || sp->is_connected()))
            snapshot.push_back(sp);
        }
      }
      return snapshot;
    }
  public:
    session_management(boost::asio::io_context& io_context,
      const session_management_config& config = session_management_config())
      : _io_context(io_context),_cleanup_timer(_io_context),_config(config)
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
      
      // 同步清理所有会话，避免异步清理的竞态条件
      {
        std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
        for(auto& pair : _sessions_map)
          pair.second->close();
        _sessions_map.clear();
      }
      return true;
    }
    
    /**
     * @brief 强制同步清理所有会话
     * @details 立即清理所有会话，不依赖定时器
     */
    void force_cleanup_all_sessions()
    {
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      for(auto& pair : _sessions_map)
        pair.second->close();
      _sessions_map.clear();
    }
    auto create_session(boost::asio::ip::tcp::socket&& socket)
    -> session_ptr
    {
      if(socket.is_open())
      {
        session_ptr sess = std::make_shared<fundamental::session<request_t,response_t>>(std::move(socket));
        {
          std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
          std::string session_string_id = sess->get_session_id();
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
    -> std::pair<std::string, session_ptr>
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
    -> std::pair<std::string, session_ptr>
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
     * @brief 安全移除断开的会话
     * @param session_string_id 会话`ID`
     * @return `true` 会话已断开并移除成功，`false` 会话不存在或仍连接
     * @details 只移除确实断开连接的会话，避免误删已重新连接的会话
     */
    bool remove_session_if_disconnected(const std::string& session_string_id)
    {
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      auto it = _sessions_map.find(session_string_id);
      if(it != _sessions_map.end())
      {
        if(!it->second->is_connected())
        {
          it->second->close();
          _sessions_map.erase(it);
          return true;
        }
        return false;
      }
      return false;
    }
    /**
     * @brief 获取会话数量
     * @return  会话数量
     */
    std::uint64_t get_session_count() const
    {
      std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
      return _sessions_map.size();
    }
    /**
     * @brief 获取所有会话`ID`列表
     * @return `std::vector<std::string>` 会话`ID`列表
     */
    std::vector<std::string> get_session_ids() const
    {
      std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
      std::vector<std::string> session_ids;
      session_ids.reserve(_sessions_map.size());
      for(const auto& pair : _sessions_map)
        session_ids.push_back(pair.first);
      return session_ids;
    }
    /**
     * @brief 添加已存在的会话到管理器
     * @param session 会话指针
     * @return `true` 添加成功，`false` 会话为空或ID已存在
     */
    bool add_session(session_ptr session)
    {
      if(!session)
        return false;
        
      std::string session_id = session->get_session_id();
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      
      // 检查ID是否已存在
      if(_sessions_map.find(session_id) != _sessions_map.end())
        return false;
        
      _sessions_map[session_id] = session;
      return true;
    }
    
    /**
     * @brief 添加已存在的会话到管理器（指定ID）
     * @param session_id 指定的会话ID
     * @param session 会话指针
     * @return `true` 添加成功，`false` 会话为空或ID已存在
     */
    bool add_session_with_id(const std::string& session_id, session_ptr session)
    {
      if(!session || session_id.empty())
        return false;
        
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      
      // 检查ID是否已存在
      if(_sessions_map.find(session_id) != _sessions_map.end())
        return false;
        
      _sessions_map[session_id] = session;
      return true;
    }
    
    /**
     * @brief 批量添加会话到管理器
     * @param sessions 会话指针列表
     * @return 成功添加的会话数量
     */
    std::uint64_t add_sessions(const std::vector<session_ptr>& sessions)
    {
      std::uint64_t added_count = 0;
      std::lock_guard<std::shared_mutex> lock(_sessions_mutex);
      
      for(const auto& session : sessions)
      {
        if(session)
        {
          std::string session_id = session->get_session_id();
          if(_sessions_map.find(session_id) == _sessions_map.end())
          {
            _sessions_map[session_id] = session;
            ++added_count;
          }
        }
      }
      return added_count;
    }
    
    /**
     * @brief 检查会话ID是否存在
     * @param session_id 会话ID
     * @return `true` 存在，`false` 不存在
     */
    bool has_session(const std::string& session_id) const
    {
      std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
      return _sessions_map.find(session_id) != _sessions_map.end();
    }
    
    /**
     * @brief 获取已连接会话数量
     * @return 已连接会话数量
     */
    std::uint64_t get_connected_session_count() const
    {
      std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
      std::uint64_t count = 0;
      for(const auto& pair : _sessions_map)
      {
        if(pair.second->is_connected())
          ++count;
      }
      return count;
    }
    
    /**
     * @brief 获取已断开会话数量
     * @return 已断开会话数量
     */
    std::uint64_t get_disconnected_session_count() const
    {
      std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
      std::uint64_t count = 0;
      for(const auto& pair : _sessions_map)
      {
        if(!pair.second->is_connected())
          ++count;
      }
      return count;
    }
    
    /**
     * @brief 批量移除断开的会话
     * @return 移除的会话数量
     */
    std::uint64_t remove_disconnected_sessions()
    {
      std::vector<std::string> disconnected_ids;
      {
        std::shared_lock<std::shared_mutex> lock(_sessions_mutex);
        for(const auto& pair : _sessions_map)
        {
          if(!pair.second->is_connected())
            disconnected_ids.push_back(pair.first);
        }
      }
      
      std::uint64_t removed_count = 0;
      for(const auto& id : disconnected_ids)
      {
        // 使用安全移除方法，在持锁状态下最终检查连接状态
        if(remove_session_if_disconnected(id))
          ++removed_count;
      }
      return removed_count;
    }
    /**
     * @brief 根据谓词筛选会话快照（公开接口）
     * @param pred 谓词函数，返回 `true` 则包含该会话，签名形如 `bool(const std::string&, const session_ptr&)`
     * @param only_connected 是否仅选择已连接会话，默认 `true`
     * @return 满足条件的会话指针快照
     */
    template<class prediction>
    std::vector<session_ptr> select_sessions_if(prediction&& pred, bool only_connected = true) const
    {
      return _conditional_filtering(std::forward<prediction>(pred), only_connected);
    }
    /**
     * @brief 使用会话指针执行一次性联动操作（单回调），在会话的io上下文中运行
     * @param sess 会话指针
     * @param linkage_operation 回调函数，签名形如 `void(session_ptr)`；内部可联动多步操作
     * @param priority 线程池提交优先级（仅影响调度入口）；默认 `weight::normal`
     * @return `true` 提交成功，`false` 会话为空
     */
    template<class operation>
    bool with_session(session_ptr sess, operation&& linkage_operation, weight priority = weight::normal)
    {
      if(!sess)
        return false;

      auto shell = [this, sp = std::move(sess), execute_function = std::forward<operation>(linkage_operation)]() mutable
      {
        auto linkage_function = [sp, execute_function]() mutable
        {
          try { execute_function(sp); } catch(...) { }
        };
        boost::asio::dispatch(_io_context, linkage_function);
      };

      if(_thread_pool_running.load() && _thread_pool)
        _thread_pool->submit_priority(priority, std::move(shell));
      else
        std::async(std::launch::async, std::move(shell));
      return true;
    }
    /**
     * @brief 使用会话ID执行一次性联动操作（单回调），在会话的io上下文中运行
     * @param session_string_id 会话ID
     * @param linkage_operation 回调函数，签名形如 `void(session_ptr)`；内部可联动多步操作
     * @param priority 线程池提交优先级（仅影响调度入口）；默认 `weight::normal`
     * @return `true` 提交成功，`false` 会话不存在
     */
    template<class operation>
    bool with_session_id(const std::string& session_string_id, operation&& linkage_operation, weight priority = weight::normal)
    {
      auto sess = get_session(session_string_id);
      if(!sess)
        return false;
      return with_session({sess}, std::forward<operation>(linkage_operation), priority);
    }
    /**
     * @brief 针对指定会话ID集合执行联动操作
     * @param ids 会话ID列表
     * @param linkage_operation 回调函数 `void(session_ptr)`
     * @param priority 入口调度优先级
     * @param only_connected 仅对已连接会话执行
     * @return `true` 有提交；`false` 无匹配会话
     */
    template<class operation>
    bool with_sessions(const std::vector<std::string>& ids, operation&& linkage_operation, weight priority = weight::normal, bool only_connected = true)
    {
      auto targets = _screening_session(ids, only_connected);
      if(targets.empty()) return false;

      auto shell = [this, vec = std::move(targets), execute_function = std::forward<operation>(linkage_operation)]() mutable
      {
        for(auto& sp : vec)
        {
          auto linkage_function = [sp, execute_function]() mutable
          {
            try { execute_function(sp); } catch(...) { }
          };
          boost::asio::dispatch(_io_context, linkage_function);
        }
      };
      if(_thread_pool_running.load() && _thread_pool)
        _thread_pool->submit_priority(priority, std::move(shell));
      else
        std::async(std::launch::async, std::move(shell));
      return true;
    }
    /**
     * @brief 遍历所有会话执行联动操作
     * @param linkage_operation 回调 `void(session_ptr)`
     * @param priority 入口调度优先级
     * @param only_connected 仅对已连接会话执行
     */
    template<class operation>
    void for_each_session(operation&& linkage_operation, weight priority = weight::normal, bool only_connected = true)
    {
      auto snapshot = _all_session(only_connected);
      auto dispatch_function = [this, vec = std::move(snapshot), capture = std::forward<operation>(linkage_operation)]() mutable
      {
        for(auto& sp : vec)
        {
          auto linkage_function = [sp, execute_function = capture]() mutable
          {
            try { execute_function(sp); } catch(...) { }
          };
          boost::asio::dispatch(_io_context, linkage_function);
        }
      };
      if(_thread_pool_running.load() && _thread_pool)
        _thread_pool->submit_priority(priority, std::move(dispatch_function));
      else
        std::async(std::launch::async, std::move(dispatch_function));
    }
    /**
     * @brief 向全部/指定连接状态的会话广播原始字节
     * @param data 原始数据
     * @param priority 入口调度优先级
     * @param only_connected 仅对已连接会话执行
     * @return `true` 有提交；`false` 无匹配会话
     */
    bool broadcast_bytes(std::string_view data, weight priority = weight::normal, bool only_connected = true)
    {
      auto snapshot = _all_session(only_connected);
      if(snapshot.empty()) return false;
      auto payload = std::make_shared<std::string>(data);
      auto dispatch_function = [this, vec = std::move(snapshot), payload]() mutable
      {
        for(auto& sp : vec)
        {
          auto send_fn = [sp, payload]() mutable
          {
            try { sp->send_bytes(*payload); } catch(...) { }
          };
          boost::asio::dispatch(_io_context, std::move(send_fn));
        }
      };
      if(_thread_pool_running.load() && _thread_pool)
        _thread_pool->submit_priority(priority, std::move(dispatch_function));
      else
        std::async(std::launch::async, std::move(dispatch_function));
      return true;
    }
    /**
     * @brief 向全部已管理的会话广播请求
     * @note 仅在会话管理器处于运行状态时有效
     * @param request 请求数据
     * @param priority 入口调度优先级
     * @param only_connected 是否仅对已连接会话执行
     */
    bool broadcast_request(const request_t& request, weight priority = weight::normal, bool only_connected = true)
    {
      return broadcast_bytes(request.to_string(), priority, only_connected);
    }
    /**
     * @brief 向全部已管理的会话广播响应
     * @note 仅在会话管理器处于运行状态时有效
     * @param response 响应数据
     * @param priority 入口调度优先级
     * @param only_connected 是否仅对已连接会话执行
     */
    bool broadcast_response(const response_t& response, weight priority = weight::normal, bool only_connected = true)
    {
      return broadcast_bytes(response.to_string(), priority, only_connected);
    }
    /**
     * @brief 获取内部线程池统计信息
     * @return 线程池统计信息
     */
    std::optional<pool_statistics> get_thread_pool_statistics() const
    {
      if(_thread_pool_running.load() && _thread_pool)
        return std::optional<pool_statistics>(_thread_pool->get_statistics());
      return std::nullopt;
    }
  }; // end class session_management
} // end namespace conversation