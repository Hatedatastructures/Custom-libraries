/**
  * @file Json.hpp
  * @brief 协议 JSON 值的封装。
  */
#pragma once
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <string_view>

namespace Protocol
{
    /**
     * @brief 封装 Boost.JSON 值并提供类型化访问。
     */
    class Json
    {
    private:
        /** @brief 包装的 Boost.JSON 值。 */
        boost::json::value MemberValue;
        /** @brief 序列化后的 JSON 文本缓存。 */
        mutable std::string MemberCachedString;
        /** @brief 指示字符串缓存是否与当前值一致。 */
        mutable bool MemberStringCacheValid = false;

    private:
        /**
          * @brief 无效化字符串缓存
          */
        void InvalidateCache() const
        {
            MemberStringCacheValid = false;
        }
    public:
        /** @brief 创建空 JSON 值。 */
        Json() : MemberValue(boost::json::object{}) {}

        /**
         * @brief 从 JSON 值复制构造。
         * @param Value 初始 JSON 值。
         */
        explicit Json(const boost::json::value &Value) : MemberValue(Value) {}

        /**
         * @brief 从 JSON 值移动构造。
         * @param Value 要接管的 JSON 值。
         */
        explicit Json(boost::json::value &&Value) noexcept : MemberValue(std::move(Value)) {}

        /**
         * @brief 从 JSON 文本构造。
         * @param JsonString 待解析文本。
         * @note 解析失败时保留默认 JSON 值；需要检查结果时使用 `FromString()`。
         */
        explicit Json(const std::string &JsonString)
        {
            FromString(JsonString);
        }

        /**
         * @brief 从 JSON 文本构造。
         * @param JsonString 待解析文本。
         * @note 解析失败时保留默认 JSON 值；需要检查结果时使用 `FromString()`。
         */
        explicit Json(std::string_view JsonString)
        {
            FromString(JsonString);
        }

        // 拷贝和移动语义 - 优化缓存处理
        /**
         * @brief 复制构造 JSON 值及其有效缓存。
         * @param Other 源对象。
         */
        Json(const Json &Other)
        : MemberValue(Other.MemberValue), MemberCachedString(Other.MemberCachedString), MemberStringCacheValid(Other.MemberStringCacheValid)
        {
            // 拷贝时保留缓存状态，避免重复序列化
        }

        /**
         * @brief 移动构造 JSON 值及其缓存。
         * @param Other 源对象。
         */
        Json(Json &&Other) noexcept
        : MemberValue(std::move(Other.MemberValue)), MemberCachedString(std::move(Other.MemberCachedString)), MemberStringCacheValid(Other.MemberStringCacheValid)
        {
            Other.MemberStringCacheValid = false;
        }

        /**
         * @brief 复制赋值 JSON 值及其有效缓存。
         * @param Other 源对象。
         * @return 当前对象的引用。
         */
        Json &operator=(const Json &Other)
        {
            if (this != &Other)
            {
                MemberValue = Other.MemberValue;
                MemberCachedString = Other.MemberCachedString;
                MemberStringCacheValid = Other.MemberStringCacheValid;
                // 拷贝赋值时保留缓存状态，避免重复序列化
            }
            return *this;
        }

        /**
         * @brief 移动赋值 JSON 值及其缓存。
         * @param Other 源对象。
         * @return 当前对象的引用。
         */
        Json &operator=(Json &&Other) noexcept
        {
            if (this != &Other)
            {
                MemberValue = std::move(Other.MemberValue);
                MemberCachedString = std::move(Other.MemberCachedString);
                MemberStringCacheValid = Other.MemberStringCacheValid;
                Other.MemberStringCacheValid = false;
            }
            return *this;
        }
        /**
         * @brief 从字符串解析 JSON 值。
         * @param JsonString 待解析文本。
         * @return 解析成功时为 `true`；失败时保留原值并返回 `false`。
         */
        bool FromString(std::string_view JsonString) noexcept
        {
            try
            {
                MemberValue = boost::json::parse(JsonString);
                InvalidateCache();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        /**
         * @brief 将 JSON 值序列化为文本。
         * @return JSON 文本；序列化失败时返回空字符串并在后续调用时重试。
         */
        const std::string &ToString() const noexcept
        {
            if (!MemberStringCacheValid)
            {
                try
                {
                    MemberCachedString = boost::json::serialize(MemberValue);
                    MemberStringCacheValid = true;
                }
                catch (...)
                {
                    // 序列化失败时返回空字符串，保持缓存无效状态
                    MemberCachedString = "";
                    // 不设置MemberStringCacheValid = true，下次调用时会重试
                }
            }
            return MemberCachedString;
        }
        /**
         * @brief 以只读方式获取底层 JSON 值。
         * @return 底层值的只读引用。
         */
        const boost::json::value &Value() const noexcept { return MemberValue; }

        /**
         * @brief 获取可修改的底层 JSON 值。
         * @return 底层值的可写引用。
         */
        boost::json::value &Value() noexcept
        {
            InvalidateCache();
            return MemberValue;
        }

        /**
         * @brief 获取并转换指定键的值。
         * @tparam T 由 `boost::json::value_to` 支持的目标类型。
         * @param Key 键名。
         * @param DefaultValue 键缺失或转换失败时返回的值。
         * @return 转换后的值，或 `DefaultValue`。
         */
        template <typename T>
        T Get(const std::string &Key, const T &DefaultValue = T{}) const noexcept
        {
            try
            {
                // 确保Json是对象类型且包含指定键
                if (!MemberValue.is_object() || !MemberValue.as_object().contains(Key))
                    return DefaultValue;
                // 利用boost::json::value_to转换为目标类型（支持任意类型）
                return boost::json::value_to<T>(MemberValue.as_object().at(Key));
            }
            catch (...)
            {
                return DefaultValue;
            }
        }

        /**
         * @brief 将值转换为 JSON 并写入指定键。
         * @tparam T 由 `boost::json::value_from` 支持的值类型。
         * @param Key 键名。
         * @param Value 要保存的值。
         * @note 转换失败时，异常会向调用方传播。
         */
        template <typename T>
        void Set(const std::string &Key, const T &Value)
        {
            auto Converted = boost::json::value_from(Value);
            // 确保Json是对象类型
            if (!MemberValue.is_object())
                MemberValue = boost::json::object{};

            // 转换失败向调用方传播，避免静默丢字段。
            MemberValue.as_object()[Key] = std::move(Converted);
            InvalidateCache();
        }

        /**
         * @brief 检查对象是否包含指定键。
         * @param Key 键名。
         * @return 键存在时为 `true`。
         */
        bool Contains(const std::string &Key) const noexcept
        {
            if (!MemberValue.is_object())
                return false;

            const auto &Obj = MemberValue.as_object();
            return Obj.find(Key) != Obj.end();
        }

        /**
         * @brief 移除对象中的指定键。
         * @param Key 键名。
         * @return 键存在并被移除时为 `true`。
         */
        bool Remove(const std::string &Key) noexcept
        {
            if (!MemberValue.is_object())
                return false;

            auto &Obj = MemberValue.as_object();
            auto It = Obj.find(Key);
            if (It != Obj.end())
            {
                Obj.erase(It);
                InvalidateCache();
                return true;
            }
            return false;
        }

        /**
         * @brief 将底层值重置为空对象。
         */
        void Clear() noexcept
        {
            MemberValue = boost::json::object{};
            InvalidateCache();
        }

        /**
         * @brief 获取对象键值对数或数组元素数。
         * @return 对象或数组的元素数；其他 JSON 类型返回零。
         */
        std::size_t Size() const noexcept
        {
            if (MemberValue.is_object())
                return MemberValue.as_object().size();
            if (MemberValue.is_array())
                return MemberValue.as_array().size();
            return 0;
        }

        /**
         * @brief 检查 JSON 值是否没有元素。
         * @return `Size()` 为零时为 `true`。
         */
        bool Empty() const noexcept
        {
            return Size() == 0;
        }

        /**
         * @brief 比较两个 JSON 值是否相等。
         * @param Other 要比较的值。
         * @return 值相等时为 `true`。
         */
        bool operator==(const Json &Other) const noexcept
        {
            return MemberValue == Other.MemberValue;
        }

        /**
         * @brief 比较两个 JSON 值是否不等。
         * @param Other 要比较的值。
         * @return 值不等时为 `true`。
         */
        bool operator!=(const Json &Other) const noexcept
        {
            return !(*this == Other);
        }
    };
} // end namespace Protocol
