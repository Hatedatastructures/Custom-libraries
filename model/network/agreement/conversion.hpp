// #pragma once
// #include "./auxiliary.hpp"
// #include "./json.hpp"
// #include <string>
// #include <string_view>
// #include <optional>
// #include <type_traits>

// namespace protocol 
// {
//   namespace conversion 
//   {
//     /**
//      * @brief 协议转换器基类
//      * @tparam From 源协议类型
//      * @tparam To 目标协议类型
//      * @details 提供协议间转换的基础接口
//      */
//     template<typename From, typename To>
//     class converter
//     {
//     public:
//       /**
//        * @brief 转换协议
//        * @param from 源协议对象
//        * @return 转换后的目标协议对象，失败时返回nullopt
//        * @details 纯虚函数，由具体转换器实现
//        */
//       virtual std::optional<To> convert(const From& from) const noexcept = 0;
      
//       /**
//        * @brief 虚析构函数
//        */
//       virtual ~converter() = default;
//     };

//     /**
//      * @brief JSON到协议头的转换器
//      * @tparam HeaderType 协议头类型
//      */
//     template<aux::header_constraint HeaderType>
//     class json_to_header_converter : public converter<json, HeaderType>
//     {
//     public:
//       /**
//        * @brief 将JSON转换为协议头
//        * @param from JSON对象
//        * @return 转换后的协议头，失败时返回nullopt
//        */
//       std::optional<HeaderType> convert(const json& from) const noexcept override
//       {
//         try
//         {
//           HeaderType header;
//           if (header.from_json(from))
//             return header;
//           return std::nullopt;
//         }
//         catch (...)
//         {
//           return std::nullopt;
//         }
//       }
//     };

//     /**
//      * @brief 协议头到JSON的转换器
//      * @tparam HeaderType 协议头类型
//      */
//     template<aux::header_constraint HeaderType>
//     class header_to_json_converter : public converter<HeaderType, json>
//     {
//     public:
//       /**
//        * @brief 将协议头转换为JSON
//        * @param from 协议头对象
//        * @return 转换后的JSON，失败时返回nullopt
//        */
//       std::optional<json> convert(const HeaderType& from) const noexcept override
//       {
//         try
//         {
//           return from.to_json();
//         }
//         catch (...)
//         {
//           return std::nullopt;
//         }
//       }
//     };

//     /**
//      * @brief 字符串到协议头的转换器
//      * @tparam HeaderType 协议头类型
//      */
//     template<aux::header_constraint HeaderType>
//     class string_to_header_converter : public converter<std::string_view, HeaderType>
//     {
//     public:
//       /**
//        * @brief 将字符串转换为协议头
//        * @param from 字符串视图
//        * @return 转换后的协议头，失败时返回nullopt
//        */
//       std::optional<HeaderType> convert(const std::string_view& from) const noexcept override
//       {
//         try
//         {
//           HeaderType header;
//           if (header.from_string(from))
//             return header;
//           return std::nullopt;
//         }
//         catch (...)
//         {
//           return std::nullopt;
//         }
//       }
//     };

//     /**
//      * @brief 协议头到字符串的转换器
//      * @tparam HeaderType 协议头类型
//      */
//     template<aux::header_constraint HeaderType>
//     class header_to_string_converter : public converter<HeaderType, std::string>
//     {
//     public:
//       /**
//        * @brief 将协议头转换为字符串
//        * @param from 协议头对象
//        * @return 转换后的字符串，失败时返回nullopt
//        */
//       std::optional<std::string> convert(const HeaderType& from) const noexcept override
//       {
//         try
//         {
//           return from.to_string();
//         }
//         catch (...)
//         {
//           return std::nullopt;
//         }
//       }
//     };
//   } // end namespace conversion
// } // end namespace protocol

// namespace protocol::conversion
// {
//   /**
//    * @brief 便捷转换函数：JSON到协议头
//    * @tparam HeaderType 协议头类型
//    * @param json_obj JSON对象
//    * @return 转换后的协议头，失败时返回nullopt
//    */
//   template<aux::header_constraint HeaderType>
//   std::optional<HeaderType> from_json(const json& json_obj) noexcept
//   {
//     json_to_header_converter<HeaderType> converter;
//     return converter.convert(json_obj);
//   }

//   /**
//    * @brief 便捷转换函数：协议头到JSON
//    * @tparam HeaderType 协议头类型
//    * @param header 协议头对象
//    * @return 转换后的JSON，失败时返回nullopt
//    */
//   template<aux::header_constraint HeaderType>
//   std::optional<json> to_json(const HeaderType& header) noexcept
//   {
//     header_to_json_converter<HeaderType> converter;
//     return converter.convert(header);
//   }

//   /**
//    * @brief 便捷转换函数：字符串到协议头
//    * @tparam HeaderType 协议头类型
//    * @param str 字符串视图
//    * @return 转换后的协议头，失败时返回nullopt
//    */
//   template<aux::header_constraint HeaderType>
//   std::optional<HeaderType> from_string(std::string_view str) noexcept
//   {
//     string_to_header_converter<HeaderType> converter;
//     return converter.convert(str);
//   }

//   /**
//    * @brief 便捷转换函数：协议头到字符串
//    * @tparam HeaderType 协议头类型
//    * @param header 协议头对象
//    * @return 转换后的字符串，失败时返回nullopt
//    */
//   template<aux::header_constraint HeaderType>
//   std::optional<std::string> to_string(const HeaderType& header) noexcept
//   {
//     header_to_string_converter<HeaderType> converter;
//     return converter.convert(header);
//   }

//   /**
//    * @brief 通用转换函数
//    * @tparam From 源类型
//    * @tparam To 目标类型
//    * @param from 源对象
//    * @return 转换后的目标对象，失败时返回nullopt
//    * @details 根据类型自动选择合适的转换器
//    */
//   template<typename From, typename To>
//   std::optional<To> convert(const From& from) noexcept
//   {
//     if constexpr (std::is_same_v<From, json> && aux::header_constraint<To>)
//     {
//       return from_json<To>(from);
//     }
//     else if constexpr (aux::header_constraint<From> && std::is_same_v<To, json>)
//     {
//       return to_json<From>(from);
//     }
//     else if constexpr (std::is_convertible_v<From, std::string_view> && aux::header_constraint<To>)
//     {
//       return from_string<To>(std::string_view(from));
//     }
//     else if constexpr (aux::header_constraint<From> && std::is_same_v<To, std::string>)
//     {
//       return to_string<From>(from);
//     }
//     else
//     {
//       static_assert(std::is_same_v<From, void>, "Unsupported conversion types");
//       return std::nullopt;
//     }
//   }

// } // end namespace protocol::conversion
