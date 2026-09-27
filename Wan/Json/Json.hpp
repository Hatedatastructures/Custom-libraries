/**
 * @file Json.hpp
 * @brief JSON 工具模块
 * @details 提供 JSON 解析、序列化和类型化访问；转换错误按各接口约定返回或传播。
 * @author Hatedatastructures
 * @date 2026-05-22
 */
#pragma once

#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <optional>
#include <stdexcept>
#include <system_error>

namespace Wan::Json
{
    namespace Bj = boost::json;

    /**
     * @brief 解析 JSON 文本并通过错误码报告失败。
     * @param Data 待解析文本。
     * @param Ec 接收错误码。
     * @return 解析得到的 JSON 值；失败时返回空值。
     */
    [[nodiscard]] inline auto Parse(std::string_view Data, std::error_code& Ec) -> Bj::value
    {
        auto val = Bj::parse(Data, Ec);
        if (Ec)
        {
            Ec = std::make_error_code(std::errc::invalid_argument);
            return {};
        }
        Ec.clear();
        return val;
    }

    /**
     * @brief 解析 JSON 文本。
     * @param Data 待解析文本。
     * @return 解析得到的 JSON 值。
     * @note 解析错误由 Boost.JSON 异常机制报告。
     */
    [[nodiscard]] inline auto Parse(std::string_view Data) -> Bj::value
    {
        return Bj::parse(Data);
    }

    /**
     * @brief 将 JSON 值序列化为文本。
     * @param Val 待序列化的 JSON 值。
     * @return JSON 文本。
     */
    [[nodiscard]] inline auto Serialize(const Bj::value& Val) -> std::string
    {
        return Bj::serialize(Val);
    }

    /**
     * @class Document
     * @brief JSON 文档封装
     * @details 以 JSON 值保存数据，并提供类型化读写接口。
     */
    class Document
    {
    public:
        /** @brief 创建空 JSON 文档。 */
        Document() = default;

        /**
         * @brief 从已有 JSON 值创建文档。
         * @param Val 初始 JSON 值。
         */
        explicit Document(Bj::value Val)
            : MemberValue(std::move(Val))
        {
        }

        /**
         * @brief 从文本解析 JSON 文档。
         * @param Data 待解析文本。
         * @param Ec 接收错误码。
         * @return 解析成功时返回文档；失败时返回空文档。
         */
        [[nodiscard]] static auto FromString(std::string_view Data, std::error_code& Ec) -> Document
        {
            auto val = Bj::parse(Data, Ec);
            if (Ec)
            {
                Ec = std::make_error_code(std::errc::invalid_argument);
                return {};
            }
            Ec.clear();
            return Document(std::move(val));
        }

        /**
         * @brief 获取指定键的类型化值。
         * @tparam T 目标类型。
         * @param Key 键名。
         * @param Fallback 键缺失或转换失败时返回的值。
         * @return 转换后的值，或 `Fallback`。
         */
        template <typename T>
        [[nodiscard]] auto Get(std::string_view Key, T Fallback = {}) const -> T
        {
            try
            {
                if (auto* Obj = MemberValue.if_object())
                {
                    if (auto It = Obj->find(Key); It != Obj->end())
                    {
                        return Bj::value_to<T>(It->value());
                    }
                }
            }
            catch (...)
            {
                // Conversion failures use the documented fallback value.
            }
            return Fallback;
        }

        /**
         * @brief 设置指定键的 JSON 值。
         * @tparam T 可转换为 JSON 值的类型。
         * @param Key 键名。
         * @param Val 要保存的值。
         * @note 值转换失败时，异常会向调用方传播。
         */
        template <typename T>
        void Set(std::string_view Key, T&& Val)
        {
            auto Converted = Bj::value_from(std::forward<T>(Val));
            if (!MemberValue.is_object())
            {
                MemberValue = Bj::object();
            }
            MemberValue.as_object()[Key] = std::move(Converted);
        }

        /**
         * @brief 将文档序列化为 JSON 文本。
         * @return JSON 文本。
         */
        [[nodiscard]] auto ToString() const -> std::string
        {
            return Bj::serialize(MemberValue);
        }

        /**
         * @brief 获取底层 JSON 值。
         * @return 底层值的可写引用。
         */
        [[nodiscard]] auto Raw() noexcept -> Bj::value&
        {
            return MemberValue;
        }

        /**
         * @brief 以只读方式获取底层 JSON 值。
         * @return 底层值的只读引用。
         */
        [[nodiscard]] auto Raw() const noexcept -> const Bj::value&
        {
            return MemberValue;
        }

    private:
        /** @brief 当前文档包装的 Boost.JSON 值。 */
        Bj::value MemberValue;
    };
}
