#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <concepts>
#include <sstream>

namespace protocol
{
  namespace http
  {
    using field = boost::beast::http::field;

    // 按照 boost.beast 限制：body 必须满足 is_body，且可实例化为 request/response 的正文
    template <class underlying_structure>
    concept body_structure_constraint = boost::beast::http::is_body<underlying_structure>::value && requires 
    {
      typename boost::beast::http::request<underlying_structure>;
      typename boost::beast::http::response<underlying_structure>;
    };
    /**
     * @brief HTTP 请求类，封装了 boost.beast 的 request 类型
     * 
     * @tparam message_body 请求正文类型，必须满足 boost.beast::http::is_body 概念
     * @tparam fields_container 头部字段容器类型，默认使用 boost.beast::http::fields
     */
    template <body_structure_constraint message_body = boost::beast::http::string_body, class fields_container = boost::beast::http::fields>
    class request
    {
    private:
      boost::beast::http::request<message_body, fields_container> _req;

    public:
      using message_body_type = message_body;
      using fields_container_type = fields_container;
      using underlying_container = boost::beast::http::request<message_body, fields_container>;

      request() = default;
      request(boost::beast::http::verb m, unsigned v, boost::beast::string_view t)
          : _req{m, v, t} {}

      underlying_container &base() { return _req; }
      underlying_container const &base() const { return _req; }

      /**
       * @brief 获取/设置 HTTP 请求方法
       * @return boost::beast::http::verb 当前请求方法
       */
      boost::beast::http::verb method() const { return _req.method(); }
      /**
       * @brief 设置 HTTP 请求方法
       * @param v 要设置的请求方法
       */
      void method(boost::beast::http::verb method_value) { _req.method(method_value); }
      /**
       * @brief 获取/设置 HTTP 请求方法的字符串表示
       * @return std::string 引用当前请求方法的字符串表示
       */
      boost::beast::string_view method_string() { return _req.method_string(); }
      /**
       * @brief 获取 HTTP 请求方法的字符串表示（只读）
       * @return boost::beast::string_view const& 当前请求方法的字符串表示
       */
      boost::beast::string_view const &method_string() const { return _req.method_string(); }
      /**
       * @brief 获取/设置 HTTP 请求目标路径
       * @return boost::beast::string_view 引用当前请求目标路径
       */
      boost::beast::string_view target() { return _req.target(); }
      /**
       * @brief 设置 HTTP 请求目标路径
       * @param t 要设置的请求目标路径
       */
      void target(boost::beast::string_view target_value) { _req.target(target_value); }
      /**
       * @brief 获取 HTTP 请求目标路径（只读）
       * @return boost::beast::string_view 当前请求目标路径
       */
      boost::beast::string_view const &target() const { return _req.target(); }
      /**
       * @brief 获取/设置 HTTP 请求版本
       * @return unsigned 当前请求版本
       */
      unsigned version() const { return _req.version(); }
      /**
       * @brief 设置 HTTP 请求版本
       * @param v 要设置的请求版本
       */
      void version(unsigned version_value) { _req.version(version_value); }

      /**
       * @brief 获取/设置 HTTP 请求是否保持连接
       * @return bool 当前是否保持连接
       */
      bool keep_alive() const { return _req.keep_alive(); }
      /**
       * @brief 设置 HTTP 请求是否保持连接
       * @param v 是否保持连接
       */
      void keep_alive(bool keep_alive_value) { _req.keep_alive(keep_alive_value); }

      /**
       * @brief 设置 HTTP 请求头部字段
       * @param boost::beast::http::field key 要设置的头部字段 
       * @param std::string value 要设置的字段值
       */
      void set(boost::beast::http::field key, std::string value) { _req.set(key, value); }
      /**
       * @brief 获取 HTTP 请求头部字段值（只读）
       * @param boost::beast::http::field key 要获取的头部字段
       * @return std::string 当前字段值
       */
      std::string at(boost::beast::http::field key) const { return _req.at(key); }
      /**
       * @brief 移除 HTTP 请求头部字段
       * @param boost::beast::http::field key 要移除的头部字段
       */
      void erase(boost::beast::http::field key) { _req.erase(key); }

      /**
       * @brief 获取/设置 HTTP 请求正文
       * @return typename message_body::value_type& 引用当前请求正文
       */
      typename message_body::value_type &body() { return _req.body(); }
      /**
       * @brief 获取 HTTP 请求正文（只读）
       * @return typename message_body::value_type const& 当前请求正文
       */
      typename message_body::value_type const &body() const { return _req.body(); }
      /**
       * @brief 获取/设置 HTTP 请求正文最大长度
       * @return std::uint64_t 当前最大长度
       */
      std::uint64_t body_limit() const { return _req.body_limit(); }
      /**
       * @brief 设置 HTTP 请求正文最大长度
       * @param n 要设置的最大长度
       */
      void body_limit(std::uint64_t len) { _req.body_limit(len); }
      /**
       * @brief 准备 HTTP 请求正文，设置 Content-Length 头部字段
       */
      void prepare_payload() { _req.prepare_payload(); }
      /**
       * @bri
       */
      bool has_content_length() const { return _req.has_content_length(); }
      std::uint64_t content_length() const { return _req.content_length(); }

      // 序列化/反序列化
      std::string to_string() const
      {
        std::ostringstream os;
        os << _req;
        return os.str();
      }
      bool from_string(std::string_view sv)
      {
        boost::beast::error_code ec;
        boost::beast::http::request_parser<message_body> parser;
        parser.eager(true);
        parser.body_limit(64 * 1024 * 1024);
        parser.put(boost::asio::buffer(sv.data(), sv.size()), ec);
        if (ec)
          return false;
        _req = parser.get();
        return true;
      }
    }; // end class request

    template <body_structure_constraint message_body = boost::beast::http::string_body, class fields_container = boost::beast::http::fields>
    class response
    {
    private:
      boost::beast::http::response<message_body, fields_container> _res;

    public:
      using beast_response = boost::beast::http::response<message_body, fields_container>;
      using message_body_type = message_body;
      using fields_container_type = fields_container;

      response() = default;
      response(boost::beast::http::status s, unsigned v) : _res{s, v} {}

      // 直接暴露底层以保持与 Beast 用法一致
      beast_response &base() { return _res; }
      beast_response const &base() const { return _res; }

      // 状态码与原因短语
      boost::beast::http::status result() const { return _res.result(); }
      void result(boost::beast::http::status s) { _res.result(s); }
      unsigned result_int() const { return _res.result_int(); }
      std::string &reason() { return _res.reason(); }
      std::string const &reason() const { return _res.reason(); }

      // 版本/长连接
      unsigned version() const { return _res.version(); }
      void version(unsigned v) { _res.version(v); }
      bool keep_alive() const { return _res.keep_alive(); }
      void keep_alive(bool v) { _res.keep_alive(v); }

      // 头部字段快捷访问
      void set(boost::beast::http::field f, boost::beast::string_view v) { _res.set(f, v); }
      boost::beast::string_view at(boost::beast::http::field f) const { return _res.at(f); }
      void erase(boost::beast::http::field f) { _res.erase(f); }

      // 正文与负载
      typename message_body::value_type &body() { return _res.body(); }
      typename message_body::value_type const &body() const { return _res.body(); }
      std::uint64_t body_limit() const { return _res.body_limit(); }
      void body_limit(std::uint64_t n) { _res.body_limit(n); }
      void prepare_payload() { _res.prepare_payload(); }
      bool has_content_length() const { return _res.has_content_length(); }
      std::uint64_t content_length() const { return _res.content_length(); }

      // 序列化/反序列化
      std::string to_string() const
      {
        std::ostringstream os;
        os << _res;
        return os.str();
      }
      bool from_string(std::string_view sv)
      {
        boost::beast::error_code ec;
        boost::beast::http::response_parser<message_body> parser;
        parser.eager(true);
        parser.body_limit(64 * 1024 * 1024);
        parser.put(boost::asio::buffer(sv.data(), sv.size()), ec);
        if (ec)
          return false;
        _res = parser.get();
        return true;
      }
    }; // end class response
  }
} // end namespace protocol