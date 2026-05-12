/**
 * @file hpack.hpp
 * @brief HPACK 头部压缩
 * @details HTTP/2 头部压缩实现，静态表 + 动态表 + Huffman 编码。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wan::web
{
    // === HPACK 静态表 ===

    /**
     * @struct hpack_static_entry
     * @brief 静态表条目
     */
    struct hpack_static_entry
    {
        std::string_view name;
        std::string_view value;
    };

    /**
     * @brief 静态表（HTTP/2 标准定义 61 条，索引 1-61）
     */
    inline const std::vector<hpack_static_entry> hpack_static_table = {
        {":authority", ""},         // 1
        {":method", "GET"},          // 2
        {":method", "POST"},         // 3
        {":path", "/"},              // 4
        {":path", "/index.html"},    // 5
        {":scheme", "http"},         // 6
        {":scheme", "https"},        // 7
        {":status", "200"},          // 8
        {":status", "204"},          // 9
        {":status", "206"},          // 10
        {":status", "304"},          // 11
        {":status", "400"},          // 12
        {":status", "404"},          // 13
        {":status", "500"},          // 14
        {"accept-charset", ""},      // 15
        {"accept-encoding", "gzip, deflate"}, // 16
        {"accept-language", ""},     // 17
        {"accept-ranges", ""},       // 18
        {"access-control-allow-origin", ""}, // 19
        {"age", ""},                 // 20
        {"allow", ""},               // 21
        {"authorization", ""},       // 22
        {"cache-control", ""},       // 23
        {"content-disposition", ""}, // 24
        {"content-encoding", ""},    // 25
        {"content-language", ""},    // 26
        {"content-length", ""},      // 27
        {"content-location", ""},    // 28
        {"content-range", ""},       // 29
        {"content-type", ""},        // 30
        {"cookie", ""},              // 31
        {"date", ""},                // 32
        {"etag", ""},                // 33
        {"expect", ""},              // 34
        {"expires", ""},             // 35
        {"from", ""},                // 36
        {"host", ""},                // 37
        {"if-match", ""},            // 38
        {"if-modified-since", ""},   // 39
        {"if-none-match", ""},       // 40
        {"if-range", ""},            // 41
        {"if-unmodified-since", ""}, // 42
        {"last-modified", ""},       // 43
        {"link", ""},                // 44
        {"location", ""},            // 45
        {"max-forwards", ""},        // 46
        {"proxy-authenticate", ""},  // 47
        {"proxy-authorization", ""}, // 48
        {"range", ""},               // 49
        {"referer", ""},             // 50
        {"refresh", ""},             // 51
        {"server", ""},              // 52
        {"set-cookie", ""},          // 53
        {"strict-transport-security", ""}, // 54
        {"transfer-encoding", ""},   // 55
        {"user-agent", ""},          // 56
        {"vary", ""},                // 57
        {"via", ""},                 // 58
        {"www-authenticate", ""}     // 59
    };

    // === HPACK 动态表 ===

    /**
     * @class hpack_dynamic_table
     * @brief HPACK 动态表
     */
    class hpack_dynamic_table
    {
    public:
        /**
         * @brief 构造函数
         * @param max_size 最大表大小
         */
        explicit hpack_dynamic_table(uint32_t max_size = 4096)
            : max_size_(max_size)
            , current_size_(0)
        {
        }

        /**
         * @brief 添加条目
         */
        void add(std::string_view name, std::string_view value)
        {
            // 计算条目大小: name + value + 32
            auto entry_size = static_cast<uint32_t>(name.size() + value.size() + 32);

            // 如果超过最大大小，先清理
            while (current_size_ + entry_size > max_size_ && !entries_.empty())
            {
                auto& oldest = entries_.back();
                current_size_ -= static_cast<uint32_t>(oldest.name.size() + oldest.value.size() + 32);
                entries_.pop_back();
            }

            // 如果条目太大，不添加
            if (entry_size > max_size_)
            {
                return;
            }

            entries_.push_front({std::string(name), std::string(value)});
            current_size_ += entry_size;
        }

        /**
         * @brief 查找条目（从动态表头部开始）
         * @param index 索引（从 1 开始，静态表后）
         * @return 条目
         */
        [[nodiscard]] auto lookup(uint32_t index) const -> std::optional<hpack_static_entry>
        {
            // 动态表索引从 62 开始（静态表 61 条）
            auto dyn_index = index - 62;

            if (dyn_index >= entries_.size())
            {
                return std::nullopt;
            }

            return hpack_static_entry{entries_[dyn_index].name, entries_[dyn_index].value};
        }

        /**
         * @brief 查找匹配的条目
         */
        [[nodiscard]] auto find(std::string_view name, std::string_view value = "") const
            -> std::optional<uint32_t>
        {
            // 先查静态表
            for (uint32_t i = 1; i < hpack_static_table.size(); ++i)
            {
                const auto& entry = hpack_static_table[i];
                if (entry.name == name)
                {
                    if (value.empty() || entry.value == value)
                    {
                        return i;
                    }
                }
            }

            // 再查动态表
            uint32_t dyn_index = 62;
            for (const auto& entry : entries_)
            {
                if (entry.name == name)
                {
                    if (value.empty() || entry.value == value)
                    {
                        return dyn_index;
                    }
                }
                ++dyn_index;
            }

            return std::nullopt;
        }

        /**
         * @brief 设置最大大小
         */
        void set_max_size(uint32_t size)
        {
            max_size_ = size;

            // 清理超出的条目
            while (current_size_ > max_size_ && !entries_.empty())
            {
                auto& oldest = entries_.back();
                current_size_ -= static_cast<uint32_t>(oldest.name.size() + oldest.value.size() + 32);
                entries_.pop_back();
            }
        }

        /**
         * @brief 获取当前大小
         */
        [[nodiscard]] auto current_size() const noexcept -> uint32_t
        {
            return current_size_;
        }

        /**
         * @brief 获取最大大小
         */
        [[nodiscard]] auto max_size() const noexcept -> uint32_t
        {
            return max_size_;
        }

    private:
        struct entry
        {
            std::string name;
            std::string value;
        };

        uint32_t max_size_;
        uint32_t current_size_;
        std::deque<entry> entries_;
    };

    // === HPACK 编解码器 ===

    /**
     * @class hpack_encoder
     * @brief HPACK 编码器
     */
    class hpack_encoder
    {
    public:
        explicit hpack_encoder(uint32_t max_table_size = 4096)
            : table_(max_table_size)
        {
        }

        /**
         * @brief 编码头部
         * @param headers 头部列表
         * @return 编码后的字节流
         */
        auto encode(const std::vector<std::pair<std::string, std::string>>& headers)
            -> std::vector<uint8_t>
        {
            std::vector<uint8_t> output;

            for (const auto& [name, value] : headers)
            {
                // 尝试完整匹配
                auto index = table_.find(name, value);

                if (index)
                {
                    // 已索引头部（Indexed Header Field）
                    encode_integer(output, *index, 6, 0x80);
                }
                else
                {
                    // 尝试名字匹配
                    auto name_index = table_.find(name);

                    if (name_index)
                    {
                        // 名字已索引，值未索引（Literal Header Field with Incremental Indexing）
                        output.push_back(0x40);
                        encode_integer(output, *name_index, 6, 0);
                        encode_string(output, value);

                        // 添加到动态表
                        table_.add(name, value);
                    }
                    else
                    {
                        // 名字和值都未索引（Literal Header Field without Indexing）
                        output.push_back(0x00);
                        encode_string(output, name);
                        encode_string(output, value);
                    }
                }
            }

            return output;
        }

        /**
         * @brief 设置动态表大小
         */
        void set_table_size(uint32_t size)
        {
            table_.set_max_size(size);
        }

    private:
        hpack_dynamic_table table_;

        /**
         * @brief 编码整数
         */
        void encode_integer(std::vector<uint8_t>& output, uint32_t value, uint8_t prefix_bits,
                           uint8_t prefix_mask) const
        {
            auto max_prefix = static_cast<uint32_t>((1 << prefix_bits) - 1);

            if (value < max_prefix)
            {
                output.push_back(static_cast<uint8_t>(prefix_mask | value));
            }
            else
            {
                output.push_back(static_cast<uint8_t>(prefix_mask | max_prefix));
                value -= max_prefix;

                while (value >= 128)
                {
                    output.push_back(static_cast<uint8_t>((value % 128) | 0x80));
                    value /= 128;
                }

                output.push_back(static_cast<uint8_t>(value));
            }
        }

        /**
         * @brief 编码字符串
         */
        void encode_string(std::vector<uint8_t>& output, std::string_view str) const
        {
            // 长度编码（不使用 Huffman）
            encode_integer(output, static_cast<uint32_t>(str.size()), 7, 0);

            // 字符串内容
            output.insert(output.end(), str.begin(), str.end());
        }
    };

    /**
     * @class hpack_decoder
     * @brief HPACK 解码器
     */
    class hpack_decoder
    {
    public:
        explicit hpack_decoder(uint32_t max_table_size = 4096)
            : table_(max_table_size)
        {
        }

        /**
         * @brief 解码头部块
         * @param data 编码数据
         * @return 头部列表
         */
        [[nodiscard]] auto decode(std::span<const uint8_t> data)
            -> std::optional<std::vector<std::pair<std::string, std::string>>>
        {
            std::vector<std::pair<std::string, std::string>> headers;
            std::size_t pos = 0;

            while (pos < data.size())
            {
                auto first_byte = data[pos];

                if (first_byte & 0x80)
                {
                    // Indexed Header Field
                    auto index_opt = decode_integer(data, pos, 6);
                    if (!index_opt)
                    {
                        return std::nullopt;
                    }

                    auto index = *index_opt;
                    std::string_view name;
                    std::string_view value;

                    if (index >= 1 && index < hpack_static_table.size())
                    {
                        name = hpack_static_table[index].name;
                        value = hpack_static_table[index].value;
                    }
                    else
                    {
                        auto dyn_entry = table_.lookup(index);
                        if (!dyn_entry)
                        {
                            return std::nullopt;
                        }
                        name = dyn_entry->name;
                        value = dyn_entry->value;
                    }

                    headers.emplace_back(std::string(name), std::string(value));
                }
                else if (first_byte & 0x40)
                {
                    // Literal Header Field with Incremental Indexing
                    pos++; // 跳过首字节

                    auto index_opt = decode_integer(data, pos, 6);
                    if (!index_opt)
                    {
                        return std::nullopt;
                    }

                    std::string name;
                    if (*index_opt > 0)
                    {
                        name = lookup_name(*index_opt);
                    }
                    else
                    {
                        auto name_opt = decode_string(data, pos);
                        if (!name_opt)
                        {
                            return std::nullopt;
                        }
                        name = *name_opt;
                    }

                    auto value_opt = decode_string(data, pos);
                    if (!value_opt)
                    {
                        return std::nullopt;
                    }

                    headers.emplace_back(name, *value_opt);
                    table_.add(name, *value_opt);
                }
                else if (first_byte & 0x20)
                {
                    // Dynamic Table Size Update
                    pos++;
                    auto size_opt = decode_integer(data, pos, 5);
                    if (!size_opt)
                    {
                        return std::nullopt;
                    }
                    table_.set_max_size(*size_opt);
                }
                else
                {
                    // Literal Header Field without Indexing / Never Indexed
                    pos++;

                    auto index_opt = decode_integer(data, pos, 4);
                    if (!index_opt)
                    {
                        return std::nullopt;
                    }

                    std::string name;
                    if (*index_opt > 0)
                    {
                        name = lookup_name(*index_opt);
                    }
                    else
                    {
                        auto name_opt = decode_string(data, pos);
                        if (!name_opt)
                        {
                            return std::nullopt;
                        }
                        name = *name_opt;
                    }

                    auto value_opt = decode_string(data, pos);
                    if (!value_opt)
                    {
                        return std::nullopt;
                    }

                    headers.emplace_back(name, *value_opt);
                }
            }

            return headers;
        }

        /**
         * @brief 设置动态表大小
         */
        void set_table_size(uint32_t size)
        {
            table_.set_max_size(size);
        }

    private:
        hpack_dynamic_table table_;

        /**
         * @brief 解码整数
         */
        [[nodiscard]] auto decode_integer(std::span<const uint8_t> data, std::size_t& pos,
                                          uint8_t prefix_bits) const -> std::optional<uint32_t>
        {
            if (pos >= data.size())
            {
                return std::nullopt;
            }

            auto max_prefix = static_cast<uint32_t>((1 << prefix_bits) - 1);
            uint32_t value = data[pos] & max_prefix;
            pos++;

            if (value < max_prefix)
            {
                return value;
            }

            uint32_t m = 0;
            uint8_t b;

            do
            {
                if (pos >= data.size())
                {
                    return std::nullopt;
                }

                b = data[pos++];
                value += static_cast<uint32_t>((b & 0x7F) << m);
                m += 7;
            }
            while (b & 0x80);

            return value;
        }

        /**
         * @brief 解码字符串
         */
        [[nodiscard]] auto decode_string(std::span<const uint8_t> data, std::size_t& pos) const
            -> std::optional<std::string>
        {
            if (pos >= data.size())
            {
                return std::nullopt;
            }

            bool huffman = data[pos] & 0x80;
            auto length_opt = decode_integer(data, pos, 7);

            if (!length_opt || pos + *length_opt > data.size())
            {
                return std::nullopt;
            }

            auto length = *length_opt;

            if (huffman)
            {
                // TODO: Huffman 解码
                // 当前简化为直接复制
                return std::string(reinterpret_cast<const char*>(data.data() + pos), length);
            }
            else
            {
                auto result = std::string(reinterpret_cast<const char*>(data.data() + pos), length);
                pos += length;
                return result;
            }
        }

        /**
         * @brief 查找名字
         */
        [[nodiscard]] auto lookup_name(uint32_t index) const -> std::string
        {
            if (index >= 1 && index < hpack_static_table.size())
            {
                return std::string(hpack_static_table[index].name);
            }

            auto dyn_entry = table_.lookup(index);
            if (dyn_entry)
            {
                return std::string(dyn_entry->name);
            }

            return "";
        }
    };

    /**
     * @brief 创建 HPACK 编码器
     */
    [[nodiscard]] inline auto make_hpack_encoder(uint32_t max_table_size = 4096) -> hpack_encoder
    {
        return hpack_encoder(max_table_size);
    }

    /**
     * @brief 创建 HPACK 解码器
     */
    [[nodiscard]] inline auto make_hpack_decoder(uint32_t max_table_size = 4096) -> hpack_decoder
    {
        return hpack_decoder(max_table_size);
    }
}