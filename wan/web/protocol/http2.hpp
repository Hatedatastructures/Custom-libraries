/**
 * @file http2.hpp
 * @brief HTTP/2 协议封装
 * @details 基于 Boost.Asio 的 HTTP/2 实现，支持多路复用、流管理、帧解析。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;

    // === HTTP/2 帧类型 ===

    /**
     * @enum frame_type
     * @brief HTTP/2 帧类型
     */
    enum class frame_type : uint8_t
    {
        data = 0x0,         // 数据帧
        headers = 0x1,      // 头部帧
        priority = 0x2,     // 优先级帧
        rst_stream = 0x3,   // 流重置帧
        settings = 0x4,     // 设置帧
        push_promise = 0x5, // 推送承诺帧
        ping = 0x6,         // Ping 帧
        goaway = 0x7,       // 关闭连接帧
        window_update = 0x8,// 窗口更新帧
        continuation = 0x9  // 头部续帧
    };

    /**
     * @enum frame_flag
     * @brief HTTP/2 帧标志
     */
    enum class frame_flag : uint8_t
    {
        none = 0x0,
        end_stream = 0x1,   // 流结束
        end_headers = 0x4,  // 头部结束
        padded = 0x8,       // 有填充
        priority = 0x20     // 有优先级
    };

    /**
     * @enum settings_id
     * @brief HTTP/2 设置参数
     */
    enum class settings_id : uint16_t
    {
        header_table_size = 0x1,        // HPACK 头部表大小
        enable_push = 0x2,              // 启用推送
        max_concurrent_streams = 0x3,   // 最大并发流
        initial_window_size = 0x4,      // 初始窗口大小
        max_frame_size = 0x5,           // 最大帧大小
        max_header_list_size = 0x6      // 最大头部列表大小
    };

    /**
     * @struct http2_frame
     * @brief HTTP/2 帧结构
     */
    struct http2_frame
    {
        uint32_t length;       // 帧长度（24位）
        frame_type type;       // 帧类型
        uint8_t flags;         // 标志
        uint32_t stream_id;    // 流ID（31位）
        std::vector<uint8_t> payload; // 帧数据

        static constexpr std::size_t header_size = 9;

        /**
         * @brief 解析帧头部
         */
        [[nodiscard]] static auto parse_header(std::span<const uint8_t> data) -> std::optional<http2_frame>
        {
            if (data.size() < header_size)
            {
                return std::nullopt;
            }

            http2_frame frame;
            frame.length = (static_cast<uint32_t>(data[0]) << 16)
                        | (static_cast<uint32_t>(data[1]) << 8)
                        | static_cast<uint32_t>(data[2]);
            frame.type = static_cast<frame_type>(data[3]);
            frame.flags = data[4];
            frame.stream_id = ((static_cast<uint32_t>(data[5]) << 24)
                             | (static_cast<uint32_t>(data[6]) << 16)
                             | (static_cast<uint32_t>(data[7]) << 8)
                             | static_cast<uint32_t>(data[8])) & 0x7FFFFFFF;

            return frame;
        }

        /**
         * @brief 编码帧头部
         */
        [[nodiscard]] auto encode_header() const -> std::array<uint8_t, header_size>
        {
            std::array<uint8_t, header_size> header{};
            header[0] = static_cast<uint8_t>(length >> 16);
            header[1] = static_cast<uint8_t>(length >> 8);
            header[2] = static_cast<uint8_t>(length);
            header[3] = static_cast<uint8_t>(type);
            header[4] = flags;
            header[5] = static_cast<uint8_t>((stream_id >> 24) & 0x7F); // 最高位保留
            header[6] = static_cast<uint8_t>(stream_id >> 16);
            header[7] = static_cast<uint8_t>(stream_id >> 8);
            header[8] = static_cast<uint8_t>(stream_id);
            return header;
        }
    };

    // === HTTP/2 流 ===

    /**
     * @enum stream_state
     * @brief HTTP/2 流状态
     */
    enum class stream_state : uint8_t
    {
        idle,
        reserved_local,
        reserved_remote,
        open,
        half_closed_local,
        half_closed_remote,
        closed
    };

    /**
     * @class http2_stream
     * @brief HTTP/2 流
     */
    class http2_stream : public std::enable_shared_from_this<http2_stream>
    {
    public:
        explicit http2_stream(uint32_t id)
            : id_(id)
            , state_(stream_state::idle)
            , window_size_(65535)  // 默认窗口
            , data_()
        {
        }

        [[nodiscard]] auto id() const noexcept -> uint32_t
        {
            return id_;
        }

        [[nodiscard]] auto state() const noexcept -> stream_state
        {
            return state_;
        }

        void set_state(stream_state s) noexcept
        {
            state_ = s;
        }

        [[nodiscard]] auto window_size() const noexcept -> int32_t
        {
            return window_size_;
        }

        void update_window(int32_t delta) noexcept
        {
            window_size_ += delta;
        }

        [[nodiscard]] auto data() const noexcept -> const std::vector<uint8_t>&
        {
            return data_;
        }

        void append_data(std::span<const uint8_t> data)
        {
            data_.insert(data_.end(), data.begin(), data.end());
        }

        void clear_data()
        {
            data_.clear();
        }

    private:
        uint32_t id_;
        stream_state state_;
        int32_t window_size_;
        std::vector<uint8_t> data_;
    };

    using http2_stream_ptr = std::shared_ptr<http2_stream>;

    // === HTTP/2 设置 ===

    /**
     * @struct http2_settings
     * @brief HTTP/2 连接设置
     */
    struct http2_settings
    {
        uint32_t header_table_size = 4096;
        bool enable_push = false;       // 默认禁用推送
        uint32_t max_concurrent_streams = 100;
        uint32_t initial_window_size = 65535;
        uint32_t max_frame_size = 16384;    // 最小 16KB
        uint32_t max_header_list_size = 65536;

        /**
         * @brief 编码为 SETTINGS 帧 payload
         */
        [[nodiscard]] auto encode() const -> std::vector<uint8_t>
        {
            std::vector<uint8_t> payload;

            // 每个设置项 6 字节
            auto add_setting = [&](settings_id id, uint32_t value)
            {
                uint16_t id_val = static_cast<uint16_t>(id);
                payload.push_back(static_cast<uint8_t>(id_val >> 8));
                payload.push_back(static_cast<uint8_t>(id_val));
                payload.push_back(static_cast<uint8_t>(value >> 24));
                payload.push_back(static_cast<uint8_t>(value >> 16));
                payload.push_back(static_cast<uint8_t>(value >> 8));
                payload.push_back(static_cast<uint8_t>(value));
            };

            add_setting(settings_id::header_table_size, header_table_size);
            add_setting(settings_id::enable_push, enable_push ? 1 : 0);
            add_setting(settings_id::max_concurrent_streams, max_concurrent_streams);
            add_setting(settings_id::initial_window_size, initial_window_size);
            add_setting(settings_id::max_frame_size, max_frame_size);
            add_setting(settings_id::max_header_list_size, max_header_list_size);

            return payload;
        }

        /**
         * @brief 从 payload 解析
         */
        [[nodiscard]] static auto decode(std::span<const uint8_t> payload) -> http2_settings
        {
            http2_settings settings;

            for (std::size_t i = 0; i + 5 < payload.size(); i += 6)
            {
                uint16_t id = (static_cast<uint16_t>(payload[i]) << 8)
                            | static_cast<uint16_t>(payload[i + 1]);
                uint32_t value = (static_cast<uint32_t>(payload[i + 2]) << 24)
                               | (static_cast<uint32_t>(payload[i + 3]) << 16)
                               | (static_cast<uint32_t>(payload[i + 4]) << 8)
                               | static_cast<uint32_t>(payload[i + 5]);

                switch (static_cast<settings_id>(id))
                {
                case settings_id::header_table_size:
                    settings.header_table_size = value;
                    break;
                case settings_id::enable_push:
                    settings.enable_push = value == 1;
                    break;
                case settings_id::max_concurrent_streams:
                    settings.max_concurrent_streams = value;
                    break;
                case settings_id::initial_window_size:
                    settings.initial_window_size = value;
                    break;
                case settings_id::max_frame_size:
                    settings.max_frame_size = std::max(16384u, std::min(value, 16777215u));
                    break;
                case settings_id::max_header_list_size:
                    settings.max_header_list_size = value;
                    break;
                default:
                    break;
                }
            }

            return settings;
        }
    };

    // === HTTP/2 连接 ===

    /**
     * @class http2_connection
     * @brief HTTP/2 连接
     * @details 基于 Boost.Asio TCP 的 HTTP/2 实现。
     */
    class http2_connection : public std::enable_shared_from_this<http2_connection>
    {
    public:
        using socket_type = net::ip::tcp::socket;
        using handler_type = std::function<net::awaitable<void>(http2_stream_ptr)>;

        /**
         * @brief 构造函数
         * @param socket TCP socket
         */
        explicit http2_connection(socket_type socket)
            : socket_(std::move(socket))
            , next_stream_id_(1)    // 客户端从 1 开始
            , settings_()
            , peer_settings_()
            , connection_window_(65535)
            , frame_buffer_()
        {
        }

        /**
         * @brief 发送连接前言
         */
        auto send_connection_preface() -> net::awaitable<void>
        {
            auto self = shared_from_this();

            // HTTP/2 连接前言
            constexpr std::string_view preface = "PRI * HTTP/2.0\r\r\nSM\r\r\n";

            boost::system::error_code ec;
            co_await net::async_write(socket_, net::buffer(preface),
                                      net::redirect_error(net::use_awaitable, ec));

            if (ec)
            {
                co_return;
            }

            // 发送初始 SETTINGS 帧
            co_await send_settings();
        }

        /**
         * @brief 发送 SETTINGS 帧
         */
        auto send_settings() -> net::awaitable<void>
        {
            auto self = shared_from_this();

            http2_frame frame;
            frame.type = frame_type::settings;
            frame.flags = 0;
            frame.stream_id = 0;
            frame.payload = settings_.encode();
            frame.length = static_cast<uint32_t>(frame.payload.size());

            co_await write_frame(frame);
        }

        /**
         * @brief 发送 SETTINGS ACK
         */
        auto send_settings_ack() -> net::awaitable<void>
        {
            http2_frame frame;
            frame.type = frame_type::settings;
            frame.flags = static_cast<uint8_t>(frame_flag::end_headers) | 0x1; // ACK flag
            frame.stream_id = 0;
            frame.length = 0;

            co_await write_frame(frame);
        }

        /**
         * @brief 创建新流
         */
        [[nodiscard]] auto create_stream() -> http2_stream_ptr
        {
            auto stream = std::make_shared<http2_stream>(next_stream_id_);
            next_stream_id_ += 2;    // 客户端流 ID 递增 2
            streams_[stream->id()] = stream;
            return stream;
        }

        /**
         * @brief 发送 HEADERS 帧
         */
        auto send_headers(uint32_t stream_id, std::span<const uint8_t> header_block,
                          bool end_stream = false) -> net::awaitable<void>
        {
            auto self = shared_from_this();

            http2_frame frame;
            frame.type = frame_type::headers;
            frame.flags = static_cast<uint8_t>(frame_flag::end_headers);
            if (end_stream)
            {
                frame.flags |= static_cast<uint8_t>(frame_flag::end_stream);
            }
            frame.stream_id = stream_id;
            frame.payload.assign(header_block.begin(), header_block.end());
            frame.length = static_cast<uint32_t>(frame.payload.size());

            co_await write_frame(frame);
        }

        /**
         * @brief 发送 DATA 帧
         */
        auto send_data(uint32_t stream_id, std::span<const uint8_t> data,
                       bool end_stream = false) -> net::awaitable<void>
        {
            auto self = shared_from_this();

            http2_frame frame;
            frame.type = frame_type::data;
            frame.flags = end_stream ? static_cast<uint8_t>(frame_flag::end_stream) : 0;
            frame.stream_id = stream_id;
            frame.payload.assign(data.begin(), data.end());
            frame.length = static_cast<uint32_t>(frame.payload.size());

            co_await write_frame(frame);
        }

        /**
         * @brief 发送 PING 帧
         */
        auto send_ping(std::array<uint8_t, 8> payload = {}) -> net::awaitable<void>
        {
            http2_frame frame;
            frame.type = frame_type::ping;
            frame.flags = 0;
            frame.stream_id = 0;
            frame.payload.assign(payload.begin(), payload.end());
            frame.length = 8;

            co_await write_frame(frame);
        }

        /**
         * @brief 发送 PING ACK
         */
        auto send_ping_ack(std::array<uint8_t, 8> payload) -> net::awaitable<void>
        {
            http2_frame frame;
            frame.type = frame_type::ping;
            frame.flags = static_cast<uint8_t>(frame_flag::end_headers); // ACK flag
            frame.stream_id = 0;
            frame.payload.assign(payload.begin(), payload.end());
            frame.length = 8;

            co_await write_frame(frame);
        }

        /**
         * @brief 发送 GOAWAY 帧
         */
        auto send_goaway(uint32_t last_stream_id, uint32_t error_code = 0,
                         std::string_view debug_data = "") -> net::awaitable<void>
        {
            http2_frame frame;
            frame.type = frame_type::goaway;
            frame.flags = 0;
            frame.stream_id = 0;

            // GOAWAY payload: last_stream_id(4) + error_code(4) + debug_data
            frame.payload.resize(8 + debug_data.size());
            frame.payload[0] = static_cast<uint8_t>(last_stream_id >> 24);
            frame.payload[1] = static_cast<uint8_t>(last_stream_id >> 16);
            frame.payload[2] = static_cast<uint8_t>(last_stream_id >> 8);
            frame.payload[3] = static_cast<uint8_t>(last_stream_id);
            frame.payload[4] = static_cast<uint8_t>(error_code >> 24);
            frame.payload[5] = static_cast<uint8_t>(error_code >> 16);
            frame.payload[6] = static_cast<uint8_t>(error_code >> 8);
            frame.payload[7] = static_cast<uint8_t>(error_code);

            if (!debug_data.empty())
            {
                std::copy(debug_data.begin(), debug_data.end(), frame.payload.begin() + 8);
            }

            frame.length = static_cast<uint32_t>(frame.payload.size());

            co_await write_frame(frame);
        }

        /**
         * @brief 发送 WINDOW_UPDATE 帧
         */
        auto send_window_update(uint32_t stream_id, uint32_t delta) -> net::awaitable<void>
        {
            http2_frame frame;
            frame.type = frame_type::window_update;
            frame.flags = 0;
            frame.stream_id = stream_id;
            frame.payload.resize(4);

            // Window Update payload: reserved(1) + window_size_increment(31)
            uint32_t increment = delta & 0x7FFFFFFF;
            frame.payload[0] = static_cast<uint8_t>(increment >> 24);
            frame.payload[1] = static_cast<uint8_t>(increment >> 16);
            frame.payload[2] = static_cast<uint8_t>(increment >> 8);
            frame.payload[3] = static_cast<uint8_t>(increment);

            frame.length = 4;

            co_await write_frame(frame);
        }

        /**
         * @brief 发送 RST_STREAM 帧
         */
        auto send_rst_stream(uint32_t stream_id, uint32_t error_code) -> net::awaitable<void>
        {
            http2_frame frame;
            frame.type = frame_type::rst_stream;
            frame.flags = 0;
            frame.stream_id = stream_id;
            frame.payload.resize(4);

            frame.payload[0] = static_cast<uint8_t>(error_code >> 24);
            frame.payload[1] = static_cast<uint8_t>(error_code >> 16);
            frame.payload[2] = static_cast<uint8_t>(error_code >> 8);
            frame.payload[3] = static_cast<uint8_t>(error_code);

            frame.length = 4;

            co_await write_frame(frame);
        }

        /**
         * @brief 读取帧
         */
        auto read_frame(std::error_code& ec) -> net::awaitable<std::optional<http2_frame>>
        {
            auto self = shared_from_this();

            // 读取帧头部
            std::array<uint8_t, http2_frame::header_size> header{};
            boost::system::error_code sys_ec;

            co_await socket_.async_read_some(net::buffer(header),
                                             net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::io_error), std::generic_category());
                co_return std::nullopt;
            }

            auto frame_opt = http2_frame::parse_header(header);
            if (!frame_opt)
            {
                ec = std::error_code(static_cast<int>(fault::parse_error), std::generic_category());
                co_return std::nullopt;
            }

            auto& frame = *frame_opt;

            // 读取 payload
            if (frame.length > 0)
            {
                frame.payload.resize(frame.length);
                co_await net::async_read(socket_, net::buffer(frame.payload),
                                        net::redirect_error(net::use_awaitable, sys_ec));

                if (sys_ec)
                {
                    ec = std::error_code(static_cast<int>(fault::io_error), std::generic_category());
                    co_return std::nullopt;
                }
            }

            ec.clear();
            co_return frame;
        }

        /**
         * @brief 处理帧
         */
        auto handle_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            auto self = shared_from_this();

            switch (frame.type)
            {
            case frame_type::data:
                co_await handle_data_frame(frame);
                break;

            case frame_type::headers:
                co_await handle_headers_frame(frame);
                break;

            case frame_type::settings:
                co_await handle_settings_frame(frame);
                break;

            case frame_type::ping:
                co_await handle_ping_frame(frame);
                break;

            case frame_type::goaway:
                co_await handle_goaway_frame(frame);
                break;

            case frame_type::window_update:
                co_await handle_window_update_frame(frame);
                break;

            case frame_type::rst_stream:
                co_await handle_rst_stream_frame(frame);
                break;

            default:
                // 忽略未知帧类型
                break;
            }
        }

        /**
         * @brief 设置流处理器
         */
        void set_stream_handler(handler_type handler)
        {
            stream_handler_ = std::move(handler);
        }

        /**
         * @brief 获取设置
         */
        [[nodiscard]] auto settings() noexcept -> http2_settings&
        {
            return settings_;
        }

        /**
         * @brief 获取对端设置
         */
        [[nodiscard]] auto peer_settings() noexcept -> http2_settings&
        {
            return peer_settings_;
        }

        /**
         * @brief 获取流
         */
        [[nodiscard]] auto get_stream(uint32_t id) -> http2_stream_ptr
        {
            auto it = streams_.find(id);
            if (it != streams_.end())
            {
                return it->second;
            }
            return nullptr;
        }

        /**
         * @brief 关闭连接
         */
        void close()
        {
            boost::system::error_code ec;
            socket_.close(ec);
        }

        /**
         * @brief 检查是否打开
         */
        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return socket_.is_open();
        }

        /**
         * @brief 获取执行器
         */
        [[nodiscard]] auto executor() noexcept -> net::any_io_executor
        {
            return socket_.get_executor();
        }

    private:
        socket_type socket_;
        uint32_t next_stream_id_;
        http2_settings settings_;
        http2_settings peer_settings_;
        int32_t connection_window_;
        std::vector<uint8_t> frame_buffer_;
        std::map<uint32_t, http2_stream_ptr> streams_;
        handler_type stream_handler_;

        /**
         * @brief 写入帧
         */
        auto write_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            auto self = shared_from_this();

            auto header = frame.encode_header();

            std::vector<uint8_t> buffer;
            buffer.reserve(http2_frame::header_size + frame.payload.size());
            buffer.insert(buffer.end(), header.begin(), header.end());
            buffer.insert(buffer.end(), frame.payload.begin(), frame.payload.end());

            boost::system::error_code ec;
            co_await net::async_write(socket_, net::buffer(buffer),
                                     net::redirect_error(net::use_awaitable, ec));
        }

        /**
         * @brief 处理 DATA 帧
         */
        auto handle_data_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            auto stream = get_stream(frame.stream_id);
            if (!stream)
            {
                // 未知流，发送 RST_STREAM
                co_await send_rst_stream(frame.stream_id, 1); // PROTOCOL_ERROR
                co_return;
            }

            stream->append_data(frame.payload);

            if (frame.flags & static_cast<uint8_t>(frame_flag::end_stream))
            {
                stream->set_state(stream_state::half_closed_remote);

                if (stream_handler_)
                {
                    co_await stream_handler_(stream);
                }
            }
        }

        /**
         * @brief 处理 HEADERS 帧
         */
        auto handle_headers_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            auto stream = get_stream(frame.stream_id);

            if (!stream)
            {
                // 服务器创建的流（ID 为偶数）
                if (frame.stream_id % 2 == 0)
                {
                    stream = std::make_shared<http2_stream>(frame.stream_id);
                    streams_[frame.stream_id] = stream;
                }
                else
                {
                    co_await send_rst_stream(frame.stream_id, 1);
                    co_return;
                }
            }

            stream->set_state(stream_state::open);

            // TODO: 解析头部块（需要 HPACK）
            if (frame.flags & static_cast<uint8_t>(frame_flag::end_stream))
            {
                stream->set_state(stream_state::half_closed_remote);
            }
        }

        /**
         * @brief 处理 SETTINGS 帧
         */
        auto handle_settings_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            // ACK 帧
            if (frame.flags & 0x1)
            {
                co_return;
            }

            peer_settings_ = http2_settings::decode(frame.payload);
            co_await send_settings_ack();
        }

        /**
         * @brief 处理 PING 帧
         */
        auto handle_ping_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            if (frame.flags & 0x1)
            {
                // PING ACK，忽略
                co_return;
            }

            // 回复 PING ACK
            std::array<uint8_t, 8> payload{};
            std::copy(frame.payload.begin(), frame.payload.begin() + 8, payload.begin());
            co_await send_ping_ack(payload);
        }

        /**
         * @brief 处理 GOAWAY 帧
         */
        auto handle_goaway_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            // 对端关闭连接
            close();
            co_return;
        }

        /**
         * @brief 处理 WINDOW_UPDATE 帧
         */
        auto handle_window_update_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            if (frame.payload.size() < 4)
            {
                co_return;
            }

            uint32_t delta = ((static_cast<uint32_t>(frame.payload[0]) & 0x7F) << 24)
                          | (static_cast<uint32_t>(frame.payload[1]) << 16)
                          | (static_cast<uint32_t>(frame.payload[2]) << 8)
                          | static_cast<uint32_t>(frame.payload[3]);

            if (frame.stream_id == 0)
            {
                connection_window_ += static_cast<int32_t>(delta);
            }
            else
            {
                auto stream = get_stream(frame.stream_id);
                if (stream)
                {
                    stream->update_window(static_cast<int32_t>(delta));
                }
            }
        }

        /**
         * @brief 处理 RST_STREAM 帧
         */
        auto handle_rst_stream_frame(const http2_frame& frame) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            auto stream = get_stream(frame.stream_id);
            if (stream)
            {
                stream->set_state(stream_state::closed);
                streams_.erase(frame.stream_id);
            }
            co_return;
        }
    };

    using http2_connection_ptr = std::shared_ptr<http2_connection>;

    /**
     * @brief 创建 HTTP/2 连接
     */
    [[nodiscard]] inline auto make_http2_connection(net::ip::tcp::socket socket) -> http2_connection_ptr
    {
        return std::make_shared<http2_connection>(std::move(socket));
    }

    // === HTTP/2 客户端 ===

    /**
     * @class http2_client
     * @brief HTTP/2 客户端
     */
    class http2_client : public std::enable_shared_from_this<http2_client>
    {
    public:
        /**
         * @brief 连接 HTTP/2 服务器
         * @param host 主机名
         * @param port 端口
         * @param ec 错误码
         * @return HTTP/2 连接
         */
        auto connect(std::string_view host, std::string_view port, std::error_code& ec)
            -> net::awaitable<http2_connection_ptr>
        {
            auto self = shared_from_this();

            auto executor = co_await net::this_coro::executor;

            // 解析
            net::ip::tcp::resolver resolver(executor);
            boost::system::error_code sys_ec;
            auto results = co_await resolver.async_resolve(host, port,
                                                           net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::dns_failed), std::generic_category());
                co_return nullptr;
            }

            // 连接
            net::ip::tcp::socket socket(executor);
            co_await socket.async_connect(*results.begin(),
                                          net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::connection_refused), std::generic_category());
                co_return nullptr;
            }

            // 创建 HTTP/2 连接
            auto conn = make_http2_connection(std::move(socket));

            // 发送连接前言
            co_await conn->send_connection_preface();

            ec.clear();
            co_return conn;
        }
    };

    /**
     * @brief 创建 HTTP/2 客户端
     */
    [[nodiscard]] inline auto make_http2_client() -> std::shared_ptr<http2_client>
    {
        return std::make_shared<http2_client>();
    }
}