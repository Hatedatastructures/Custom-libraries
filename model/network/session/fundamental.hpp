/**
 * @file fundamental.hpp
 * @brief 会话辅助类定义
 * @details 提供会话管理、状态监控、错误处理等功能
 */
#pragma once
#include <memory>
#include <string>

#include <boost/pool/object_pool.hpp>

namespace conversation
{
  namespace fundamental
  {
  } // end namespace fundamental
} // end namespace conversation

namespace conversation::fundamental
{
  /**
   * @brief 会话状态枚举
   */
  enum class session_state : std::uint8_t
  {
    DISCONNECTED,     // 未连接
    CONNECTING,       // 连接中
    CONNECTED,        // 已连接
    DISCONNECTING,    // 断开连接中
    ERROR_STATE       // 错误状态
  };
  /**
   * @brief 会话类型枚举
   */
  enum class session_type : std::uint8_t
  {
    TCP_CLIENT, // TCP客户端
    TCP_SERVER, // TCP服务端
    UDP_CLIENT, // UDP客户端
    UDP_SERVER  // UDP服务端
  };
  /**
   * @brief 会话事件类型
   */
  enum class session_event : std::uint8_t
  {
    CONNECTED = 0,  // 连接建立
    DISCONNECTED,   // 连接断开
    DATA_RECEIVED,  // 数据接收
    DATA_SENT,      // 数据发送
    ERROR_OCCURRED, // 错误发生
    TIMEOUT         // 超时
  };
  /**
   * @brief 会话统计信息
   */
  struct session_statistics
  {
    std::atomic<std::uint64_t> _bytes_sent{0};            // 发送字节数
    std::atomic<std::uint64_t> _bytes_received{0};        // 接收字节数
    std::atomic<std::uint64_t> _messages_sent{0};         // 发送消息数
    std::atomic<std::uint64_t> _messages_received{0};     // 接收消息数
    std::chrono::system_clock::time_point _created_time;  // 创建时间
    std::chrono::system_clock::time_point _last_activity; // 最后活动时间

    session_statistics() : _created_time(std::chrono::system_clock::now()), 
      _last_activity(std::chrono::system_clock::now()) {}

    /**
     * @brief 更新最后活动时间
     */
    void update_activity() noexcept
    {
      _last_activity = std::chrono::system_clock::now();
    }

    /**
     * @brief 获取会话持续时间（秒）
     * @return 持续时间
     */
    std::chrono::seconds get_duration() const noexcept
    {
      return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - _created_time);
    }

    /**
     * @brief 获取空闲时间（秒）
     * @return 空闲时间
     */
    std::chrono::seconds get_idle_time() const noexcept
    {
      return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - _last_activity);
    }
  };

  /**
   * @brief 会话配置
   */
  struct session_config
  {
    std::chrono::milliseconds _read_timeout{30000};       // 读取超时
    std::chrono::milliseconds _write_timeout{30000};      // 写入超时
    std::chrono::milliseconds _connect_timeout{30000};    // 连接超时
    std::chrono::milliseconds _keepalive_interval{60000}; // 心跳间隔

    std::size_t _max_buffer_size{65536};    // 最大缓冲区大小
    std::size_t _max_message_size{1048576}; // 最大消息大小
  };
  
  template <request_t,response_t>
  class fundamental_session
  {

  }; // end class fundamental_session
} // end namespace fundamental
