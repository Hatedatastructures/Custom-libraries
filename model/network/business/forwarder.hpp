#pragma once
#include "../agreement/http.hpp"
#include "../session/fundamental.hpp"
#include "../session/conversation.hpp"
#include "../agreement/json.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <boost/system/error_code.hpp>
#include <type_traits>
#include <fstream>
#include <unordered_map>
#include <optional>
#include <functional>
#include <algorithm>
#include <string>
#include <string_view>

namespace represents
{
  /**
   * @brief 基于 `http` 协议的服务端 `http / https` 请求转发器(代理)
   * @details 用于将客户端的http请求转发到指定的http服务器，并将服务器的响应返回给客户端
   * @note 支持 `http` 和 `https` 协议 , 并且可支持消息拦截规则
   * @note 支持 `server`   --`https`->     `agent`    --`https`->    `client` 来保证安全传输
   * @warning 需要 `json` 配置
   */
  template <class body = boost::beast::http::string_body, class fields = boost::beast::http::fields>
  class transponder
  {
  public:
    using request_t = protocol::http::request<body, fields>;
    using response_t = protocol::http::response<body, fields>;
    /**
     * @brief 上游服务器配置
     */
    struct upstream
    {
      std::string host; // 上游服务器主机名
      std::uint16_t port{80}; // 上游服务器端口号
      bool use_https{false}; // 是否使用 HTTPS
    }; // end struct upstream
    /**
     * @brief 请求匹配规则
     */
    struct matcher
    {
      std::optional<std::size_t> size_gt; // 内容大小大于匹配
      std::optional<std::string> path_prefix; // 路径前缀匹配
      std::optional<std::string> content_type; // 内容类型匹配
      std::optional<std::vector<std::string>> methods; // 方法匹配
    };  // end struct matcher
    /**
     * @brief 路由项配置
     */
    struct route_item
    {
      matcher match; // 路由匹配器
      std::string upstream_name; // 上游服务器名称
      std::unordered_map<std::string, std::string> header_set; // 路由级头部改写
    }; // end struct route_item

    /**
     * @brief 处理阶段项配置
     */
    struct stage_item
    {
      matcher match; // 阶段匹配器
      boost::json::object op; // { type: "rewrite_headers" | "short_circuit" | "json_patch" , ... }
    }; // end struct stage_item


    /**
     * @brief 请求过滤项配置
     */
    struct request_filter_entry
    {
      matcher match; // 请求匹配器
      std::uint8_t priority{0}; // 优先级
      std::function<std::optional<response_t>(request_t &)> fn; // 过滤函数
    }; // end struct request_filter_entry

    /**
     * @brief 响应过滤项配置
     */
    struct response_filter_entry
    {
      matcher match; // 响应匹配器
      std::uint8_t priority; // 优先级
      std::function<void(response_t &)> fn; // 过滤函数
    }; // end struct response_filter_entry

    /**
     * @brief 异步请求过滤项配置
     */
    struct request_filter_async_entry
    {
      matcher match; // 请求匹配器
      std::uint8_t priority; // 优先级
      std::function<void(request_t &, std::function<void(std::optional<response_t>)>)> fn; // 异步过滤函数
    }; // end struct request_filter_async_entry


    // 上游选择器（可覆盖路由选择）
    struct upstream_selector_entry
    {
      std::uint8_t priority; // 优先级
      std::function<std::optional<std::string>(request_t &)> fn;
    }; // end struct upstream_selector_entry


    /**
     * @brief 管道策略配置
     */
    struct pipeline_policy
    {
      bool external_request_filters_first{true}; // 外部请求拦截器优先
      bool external_response_filters_after_json{true}; // 外部响应拦截器优先于 JSON 处理
      bool run_sync_filters_in_async_forward{true}; // 异步转发时运行同步拦截器
    }; // end struct pipeline_policy
  private:
    using response_interception_t = std::function<void(response_t&)>; // 响应拦截器函数
    using synchronous_interception_t = std::function<std::optional<response_t>(request_t&)>; // 同步请求拦截器函数
    using async_interception_t = std::function<void(request_t&,std::function<void(std::optional<response_t>)>)>; // 异步请求拦截器函数
    
    using json_request = std::function<std::optional<response_t>(request_t&,const boost::json::object&)>; // JSON 请求拦截器函数类型
    using json_response = std::function<void(boost::beast::http::response<body,fields>&, const boost::json::object&)>; // JSON 响应拦截器函数类型

    boost::asio::io_context& _io_context; // io上下文

    std::vector<route_item> _routes; // 路由表
    std::vector<stage_item> _request_stage; // 请求阶段拦截器
    std::vector<stage_item> _response_stage; // 响应阶段拦截器
    std::optional<std::string> _default_upstream; // 默认上游服务器名称
    std::unordered_map<std::string, upstream> _upstreams; // 上游服务器配置

    std::vector<request_filter_entry> _request_filters;  // 同步请求拦截器
    std::vector<response_filter_entry> _response_filters; // 响应拦截器
    std::vector<request_filter_async_entry> _request_filters_async; // 异步请求拦截器

    std::vector<upstream_selector_entry> _upstream_selectors; // 上游选择器

    pipeline_policy _policy; // 管道策略配置

    std::unordered_map<std::string, json_request> _req_json_op_plugins; // 请求 JSON 操作插件
    std::unordered_map<std::string, json_response> _resp_json_op_plugins; // 响应 JSON 操作插件

  public:
    explicit transponder(boost::asio::io_context& io_context) : _io_context(io_context) {}

    /**
     * @brief 注册请求过滤项
     * @param match 请求匹配器
     * @param function 过滤函数
     * @param priority 优先级
     */
    void register_request_filter(const matcher &match,synchronous_interception_t function, std::uint8_t priority = 0)
    {
      auto sort_priority = [](const auto& first,const auto& second)
      {
        return first.priority < second.priority;
      };
      _request_filters.push_back({match, priority, std::move(function)});
      std::stable_sort(_request_filters.begin(), _request_filters.end(),sort_priority);
    }

    /**
     * @brief 注册响应过滤项
     * @param match 响应匹配器
     * @param function 过滤函数
     * @param priority 优先级
     */
    void register_response_filter(const matcher &match,response_interception_t function,std::uint8_t priority = 0)
    {
      auto sort_priority = [](const auto& first,const auto& second)
      {
        return first.priority < second.priority;
      };
      _response_filters.push_back({match, priority, std::move(function)});
      std::stable_sort(_response_filters.begin(), _response_filters.end(),sort_priority);
    }

    /**
     * @brief 注册异步请求过滤项
     * @param match 请求匹配器
     * @param function 异步过滤函数
     * @param priority 优先级
     */
    void register_request_filter_async(const matcher &match,async_interception_t function,std::uint8_t priority = 0)
    {
      auto sort_priority = [](const auto& first,const auto& second)
      {
        return first.priority < second.priority;
      };
      _request_filters_async.push_back({match, priority, std::move(function)});
      std::stable_sort(_request_filters_async.begin(), _request_filters_async.end(),sort_priority);
    }

    /**
     * @brief 注册上游选择器
     * @param function 上游选择函数
     * @param priority 优先级
     */
    void register_upstream_selector(std::function<std::optional<std::string>(request_t &)> function, std::uint8_t priority = 0)
    {
      auto sort_priority = [](const auto& first,const auto& second)
      {
        return first.priority < second.priority;
      };
      _upstream_selectors.push_back({priority, std::move(function)});
      std::stable_sort(_upstream_selectors.begin(), _upstream_selectors.end(),sort_priority);
    }

    /**
     * @brief 设置管道策略
     * @param p 管道策略
     */
    void set_pipeline_policy(const pipeline_policy &p) { _policy = p; }

    /**
     * @brief 注册 JSON 请求操作插件
     * @param type 操作类型
     * @param function 操作函数
     */
    void register_request_json_op(const std::string &type,json_request function)
    { 
      _req_json_op_plugins[type] = std::move(function); 
    }

    /**
     * @brief 注册 JSON 响应操作插件
     * @param type 操作类型
     * @param function 操作函数
     */
    void register_response_json_op(const std::string &type,json_response function)
    { 
      _resp_json_op_plugins[type] = std::move(function); 
    }

    /**
     * @brief 清除所有请求过滤项
     */
    void clear_request_filters() 
    { 
      _request_filters.clear(); 
    }

    /**
     * @brief 清除所有响应过滤项
     */
    void clear_response_filters() 
    { 
      _response_filters.clear(); 
    }

    /**
     * @brief 清除所有异步请求过滤项
     */
    void clear_request_filters_async() 
    { 
      _request_filters_async.clear(); 
    }

    /**
     * @brief 添加或更新上游服务器配置
     * @param name 上游服务器名称
     * @param host 上游服务器主机名
     * @param port 上游服务器端口号
     * @param use_https 是否使用 HTTPS
     */
    void add_or_update_upstream(const std::string &name, const std::string &host, std::uint16_t port, bool use_https)
    {
      _upstreams[name] = upstream{host, port, use_https};
      if (!_default_upstream) 
        _default_upstream = name;
    }

    /**
     * @brief 移除上游服务器配置
     * @param name 上游服务器名称
     */
    void remove_upstream(const std::string &name) 
    { 
      _upstreams.erase(name); 
    }

    /**
     * @brief 设置默认上游服务器
     * @param name 上游服务器名称
     */
    void set_default_upstream(const std::string &name) 
    { 
      _default_upstream = name; 
    }

    /**
     * @brief 添加路由规则
     * @param m 路由匹配器
     * @param upstream_name 上游服务器名称
     * @param header_set 额外的请求头设置
     */
    void add_route(const matcher &m, const std::string &upstream_name,
      const std::unordered_map<std::string, std::string> &header_set = {})
    { 
      _routes.push_back({m, upstream_name, header_set}); 
    }

    /**
     * @brief 清除所有路由规则
     */
    void clear_routes() 
    { 
      _routes.clear(); 
    }
    /**
     * @brief 加载json配置文件
     * @param path json文件路径
     * @return 是否加载成功
     */
    bool load_config_file(const std::string &path)
    {
      std::ifstream in(path, std::ios::binary);
      if (!in) return false;
      std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      return load_config_json(data);
    }
    /**
     * @brief 同步转发请求
     * @param req 请求
     * @return 响应
     */
    response_t forward_sync(request_t req)
    {
      // 1) 选择上游（先外部选择器，再路由匹配，最后默认）
      std::string upstream_name = select_upstream_and_apply_route_headers(req);
      if (upstream_name.empty())
      {
        return make_error_response(502, "Bad Gateway", "no upstream");
      }

      // 2) 请求阶段拦截器（可能短路）
      if (auto alt = apply_request_ops(req))
      {
        auto bres = alt->base();
        apply_response_ops(bres);
        response_t out; out.base() = std::move(bres);
        return out;
      }

      // 3) Host 兜底
      auto &up = _upstreams.at(upstream_name);
      apply_host_header_if_missing(req, up);

      // 4) 执行上游访问
      auto bres = perform_upstream(req.base(), up);

      // 5) 响应阶段拦截
      apply_response_ops(bres);

      response_t out;
      out.base() = std::move(bres);
      return out;
    }
    // 异步转发（支持外部注册的异步请求过滤链）
    void forward_async(request_t req, std::function<void(response_t)> cb)
    {
      boost::asio::post(_io_context, [this, req = std::move(req), cb = std::move(cb)]() mutable
                        {
        struct runner 
        {
          transponder* self;
          request_t req;
          std::function<void(response_t)> cb;
          std::size_t idx{0};
          void step() 
          {
            if (idx >= self->_request_filters_async.size()) 
            {
              auto res = self->forward_sync(std::move(req));
              cb(std::move(res));
              return;
            }
            const auto& e = self->_request_filters_async[idx++];
            if (!self->matches_request(req, e.match)) { step(); return; }
            e.fn(req, [this](std::optional<response_t> alt)
            {
              if (alt) 
              {
                auto bres = alt->base();
                self->apply_response_ops(bres);
                response_t out; out.base() = std::move(bres);
                cb(std::move(out));
              } 
              else 
              {
                step();
              }
            });
          }
        };
        runner r{ this, std::move(req), std::move(cb) };
        r.step(); });
    }

  private:
    /**
     * @brief 解析路由匹配器
     *
     * @param mo JSON 对象
     * @return matcher 路由匹配器
     */
    static matcher parse_match(const boost::json::object &mo)
    {
      matcher m;
      if (auto v = mo.if_contains("path_prefix"); v && v->is_string())
        m.path_prefix = boost::json::value_to<std::string>(*v);
      if (auto v = mo.if_contains("methods"); v && v->is_array())
      {
        std::vector<std::string> vv;
        vv.reserve(v->as_array().size());
        for (const auto &x : v->as_array())
          vv.push_back(boost::json::value_to<std::string>(x));
        m.methods = std::move(vv);
      }
      if (auto v = mo.if_contains("content_type"); v && v->is_string())
        m.content_type = boost::json::value_to<std::string>(*v);
      if (auto v = mo.if_contains("size_gt"); v && v->is_int64())
        m.size_gt = static_cast<std::size_t>(boost::json::value_to<std::int64_t>(*v));
      return m;
    }
    /**
     * @brief 检查请求是否匹配路由
     *
     * @param req 请求
     * @param m 路由匹配器
     * @return true 匹配
     * @return false 不匹配
     */
    static bool matches_request(const request_t &req, const matcher &m)
    {
      if (m.path_prefix)
      {
        std::string path(req.target());
        if (path.rfind(*m.path_prefix, 0) != 0)
          return false;
      }
      if (m.methods)
      {
        std::string meth(req.method_string());
        auto &arr = *m.methods;
        if (std::find(arr.begin(), arr.end(), meth) == arr.end())
          return false;
      }
      return true;
    }
    /**
     * @brief 检查响应是否匹配路由
     *
     * @param res 响应
     * @param m 路由匹配器
     * @return true 匹配
     * @return false 不匹配
     */
    static bool matches_response(const response_t &res, const matcher &m)
    {
      if (m.content_type)
      {
        auto it = res.base().find(boost::beast::http::field::content_type);
        std::string ct = it != res.base().end() ? std::string(it->value()) : "";
        if (ct.find(*m.content_type) == std::string::npos)
          return false;
      }
      if (m.size_gt)
      {
        std::size_t n = res.body().size();
        if (n <= *m.size_gt)
          return false;
      }
      return true;
    }
    /**
     * @brief 应用请求阶段拦截器（外部注册/JSON 插件，顺序可配置）
     * 
     * @param req 请求
     * @return std::optional<response_t> 拦截器返回的响应（如果有）
     */
    std::optional<response_t> apply_request_ops(request_t& req)
    {
      auto run_external = [&]() -> std::optional<response_t> {
        for(const auto& e : _request_filters)
        {
          if(!matches_request(req, e.match)) continue;
          if(auto alt = e.fn(req)) return alt;
        }
        return std::nullopt;
      };
      auto run_json = [&]() -> std::optional<response_t> {
        return run_request_json_ops(req);
      };

      if(_policy.external_request_filters_first)
      {
        if(auto alt = run_external()) return alt;
        if(auto alt = run_json()) return alt;
      }
      else
      {
        if(auto alt = run_json()) return alt;
        if(auto alt = run_external()) return alt;
      }
      return std::nullopt;
    }

    // 仅做 JSON 拦截的副作用改写（无短路）
    void apply_request_ops_json_only(request_t& req)
    {
      (void)run_request_json_ops(req);
    }

    /**
     * @brief 应用响应阶段拦截器（顺序可配置 + JSON 插件）
     * 
     * @param res 响应
     */
    void apply_response_ops(boost::beast::http::response<body, fields>& res)
    {
      auto run_json = [&]() {
        for(const auto& s : _response_stage)
        {
          response_t wrap; wrap.base() = res; // 基于当前快照进行匹配
          if(!matches_response(wrap, s.match)) continue;
          auto type_it = s.op.find("type");
          if(type_it == s.op.end() || !type_it->value().is_string()) continue;
          const std::string type = boost::json::value_to<std::string>(type_it->value());
    
          if(type == "rewrite_headers")
          {
            if(auto setv = s.op.if_contains("set"); setv && setv->is_object())
            {
              for(const auto& kv : setv->as_object())
                res.set(kv.key_c_str(), boost::json::value_to<std::string>(kv.value()));
            }
          }
          else if(type == "json_patch")
          {
            if constexpr (std::is_same_v<body, boost::beast::http::string_body>) {
              auto ct_it = res.find(boost::beast::http::field::content_type);
              const std::string ct = ct_it != res.end() ? std::string(ct_it->value()) : "";
              if(ct.find("application/json") != std::string::npos)
              {
                boost::system::error_code ec;
                auto v = boost::json::parse(res.body(), ec);
                if(!ec)
                {
                  if(auto opsv = s.op.if_contains("ops"); opsv && opsv->is_array())
                  {
                    for(const auto& ov : opsv->as_array())
                    {
                      const auto& oo = ov.as_object();
                      const std::string op = boost::json::value_to<std::string>(oo.at("op"));
                      const std::string path = boost::json::value_to<std::string>(oo.at("path"));
                      const boost::json::value value = oo.if_contains("value") ? oo.at("value") : boost::json::value();
                      if(op == "replace") json_replace(v, path, value);
                    }
                  }
                  res.body() = boost::json::serialize(v);
                  res.set(boost::beast::http::field::content_type, "application/json");
                }
              }
            }
          }
          else
          {
            auto pit = _resp_json_op_plugins.find(type);
            if(pit != _resp_json_op_plugins.end())
            {
              pit->second(res, s.op);
            }
          }
        }
      };
    
      auto run_external = [&]() {
        for(const auto& e : _response_filters)
        {
          response_t w; w.base() = res;
          if(!matches_response(w, e.match)) continue;
          e.fn(w);
          res = std::move(w.base());
        }
      };
    
      if(_policy.external_response_filters_after_json) { run_json(); run_external(); }
      else { run_external(); run_json(); }
    
      if constexpr (std::is_same_v<body, boost::beast::http::string_body>) {
        if (res.find(boost::beast::http::field::transfer_encoding) == res.end()) {
          res.set(boost::beast::http::field::content_length, std::to_string(res.body().size()));
        }
      }
    }
    /**
     * @brief 递归替换 JSON 中的值
     * @param v JSON 值
     * @param path JSON Pointer 路径
     * @param nv 新值
     */
    static void json_replace(boost::json::value &v, const std::string &path, const boost::json::value &nv)
    {
      // 简化的 JSON Pointer（仅对象）形式：/a/b
      if (path.empty() || path[0] != '/')
        return;
      auto *cur = &v;
      std::size_t pos = 1;
      std::vector<std::string> keys;
      while (pos < path.size())
      {
        auto next = path.find('/', pos);
        if (next == std::string::npos)
          next = path.size();
        keys.emplace_back(path.substr(pos, next - pos));
        pos = next + 1;
      }
      for (std::size_t i = 0; i + 1 < keys.size(); ++i)
      {
        auto &key = keys[i];
        if (!cur->is_object())
        {
          cur->emplace_object();
        }
        auto &obj = cur->as_object();
        auto it = obj.find(key);
        if (it == obj.end())
        {
          obj.emplace(key, boost::json::object{});
          it = obj.find(key);
        }
        cur = &it->value();
      }
      if (cur->is_object())
        cur->as_object()[keys.back()] = nv;
    }

    // 选择上游并应用路由头部改写
    std::string select_upstream_and_apply_route_headers(request_t &req)
    {
      for (const auto &sel : _upstream_selectors)
      {
        if (auto name = sel.fn(req))
          return *name;
      }
      for (const auto &r : _routes)
      {
        if (matches_request(req, r.match))
        {
          for (const auto &kv : r.header_set)
            req.base().set(kv.first, kv.second);
          return r.upstream_name;
        }
      }
      return _default_upstream.value_or("");
    }

    // 如果缺失 Host，则兜底设置
    void apply_host_header_if_missing(request_t &req, const upstream &up)
    {
      if (req.base().find(boost::beast::http::field::host) == req.base().end())
      {
        bool is_https = up.use_https;
        std::uint16_t default_port = is_https ? 443 : 80;
        if (up.port != default_port)
          req.base().set(boost::beast::http::field::host, up.host + ":" + std::to_string(up.port));
        else
          req.base().set(boost::beast::http::field::host, up.host);
      }
    }

    // 抽出请求 JSON 拦截助手，供两个方法复用
    std::optional<response_t> run_request_json_ops(request_t &req)
    {
      for(const auto& s : _request_stage)
      {
        if(!matches_request(req, s.match)) continue;
        auto type_it = s.op.find("type");
        if(type_it == s.op.end() || !type_it->value().is_string()) continue;
        auto type = boost::json::value_to<std::string>(type_it->value());

        if(type == "rewrite_headers")
        {
          if(auto setv = s.op.if_contains("set"); setv && setv->is_object())
          {
            for(const auto& kv : setv->as_object())
              req.base().set(kv.key_c_str(), boost::json::value_to<std::string>(boost::json::value(kv.value())));
          }
        }
        else if(type == "short_circuit")
        {
          int status = 403; std::string body_string; std::string content_type = "text/plain";
          if(auto v = s.op.if_contains("status"); v && v->is_int64()) status = boost::json::value_to<int>(*v);
          if(auto v = s.op.if_contains("body"); v && v->is_string()) body_string = boost::json::value_to<std::string>(*v);
          if(auto v = s.op.if_contains("content_type"); v && v->is_string()) content_type = boost::json::value_to<std::string>(*v);
          response_t resp;
          resp.base().result(static_cast<boost::beast::http::status>(status));
          resp.base().set(boost::beast::http::field::content_type, content_type);
          resp.body() = body_string;
          resp.base().set(boost::beast::http::field::content_length, std::to_string(resp.body().size()));
          return resp;
        }
        else
        {
          auto pit = _req_json_op_plugins.find(type);
          if(pit != _req_json_op_plugins.end())
          {
            if(auto alt = pit->second(req, s.op)) return alt;
          }
        }
      }
      return std::nullopt;
    }

    // 执行纯 HTTP 上游访问
    boost::beast::http::response<body, fields> perform_upstream_plain(
        const boost::beast::http::request<body, fields> &req,
        const upstream &up)
    {
      namespace http = boost::beast::http;
      using tcp = boost::asio::ip::tcp;
      http::response<body, fields> res;
      boost::beast::flat_buffer buffer;
      tcp::resolver resolver(_io_context);
      auto results = resolver.resolve(up.host, std::to_string(up.port));
      tcp::socket socket(_io_context);
      boost::asio::connect(socket, results);
      http::write(socket, req);
      http::read(socket, buffer, res);
      boost::system::error_code ec;
      socket.shutdown(tcp::socket::shutdown_both, ec);
      return res;
    }

    /**
     * @brief 执行 HTTPS 上游访问
     */
    boost::beast::http::response<body, fields> perform_upstream_ssl(
        const boost::beast::http::request<body, fields> &req,
        const upstream &up)
    {
      namespace http = boost::beast::http;
      using tcp = boost::asio::ip::tcp;
      http::response<body, fields> res;
      boost::beast::flat_buffer buffer;
      tcp::resolver resolver(_io_context);
      auto results = resolver.resolve(up.host, std::to_string(up.port));
      boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::sslv23_client);
      ssl_ctx.set_default_verify_paths();
      ssl_ctx.set_verify_mode(boost::asio::ssl::verify_none);
      boost::asio::ssl::stream<tcp::socket> stream(_io_context, ssl_ctx);
      boost::asio::connect(stream.next_layer(), results);
      SSL_set_tlsext_host_name(stream.native_handle(), up.host.c_str());
      stream.handshake(boost::asio::ssl::stream_base::client);
      http::write(stream, req);
      http::read(stream, buffer, res);
      boost::system::error_code ec;
      stream.shutdown(ec);
      return res;
    }

    /**
     * @brief 执行上游请求（改为调用拆分后的实现）
     */
    boost::beast::http::response<body, fields> perform_upstream( const boost::beast::http::request<body, fields> &req,
        const upstream &up)
    {
      namespace http = boost::beast::http;
      try
      {
        return up.use_https ? perform_upstream_ssl(req, up) : perform_upstream_plain(req, up);
      }
      catch (const std::exception &e)
      {
        http::response<body, fields> res;
        res.result(http::status::bad_gateway);
        res.set(http::field::content_type, "text/plain");
        std::string msg = std::string("upstream error: ") + e.what();
        res.body() = msg;
        res.set(http::field::content_length, std::to_string(msg.size()));
        return res;
      }
    }

    // 配置加载拆分：从 JSON 文本与 value 载入
    bool load_config_json(std::string_view json_text)
    {
      boost::system::error_code ec;
      auto jv = boost::json::parse(json_text, ec);
      if (ec) return false;
      return load_config_value(jv);
    }

    /**
     * @brief 从 JSON 值加载配置
     * @param jv JSON 值
     * @return 是否成功
     */
    bool load_config_value(const boost::json::value &jv)
    {
      try
      {
        _upstreams.clear();
        _routes.clear();
        _request_stage.clear();
        _response_stage.clear();
        _default_upstream.reset();
        const auto &obj = jv.as_object();
        if (auto it = obj.if_contains("upstreams"); it && it->is_array())
        {
          parse_upstreams(it->as_array());
        }
        if (auto it = obj.if_contains("routes"); it && it->is_array())
        {
          parse_routes(it->as_array());
        }
        if (auto it = obj.if_contains("request_stage"); it && it->is_array())
        {
          _request_stage.reserve(it->as_array().size());
          for (const auto &v : it->as_array())
          {
            _request_stage.emplace_back(parse_stage_item(v.as_object()));
          }
        }
        if (auto it = obj.if_contains("response_stage"); it && it->is_array())
        {
          _response_stage.reserve(it->as_array().size());
          for (const auto &v : it->as_array())
          {
            _response_stage.emplace_back(parse_stage_item(v.as_object()));
          }
        }
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

    /**
     * @brief 解析上游配置
     * @param arr JSON 数组
     */
    void parse_upstreams(const boost::json::array &arr)
    {
      for (const auto &u : arr)
      {
        auto &uo = u.as_object();
        std::string name = boost::json::value_to<std::string>(uo.at("name"));
        upstream up;
        up.host = boost::json::value_to<std::string>(uo.at("host"));
        up.port = static_cast<std::uint16_t>(boost::json::value_to<int>(uo.if_contains("port") ? uo.at("port") : boost::json::value(80)));
        up.use_https = boost::json::value_to<bool>(uo.if_contains("use_https") ? uo.at("use_https") : boost::json::value(false));
        _upstreams.emplace(name, std::move(up));
        if (!_default_upstream)
          _default_upstream = name;
      }
    }
    /**
     * @brief 解析路由配置
     * @param arr JSON 数组
     */
    void parse_routes(const boost::json::array &arr)
    {
      for (const auto &r : arr)
      {
        route_item item;
        const auto &ro = r.as_object();
        if (auto m = ro.if_contains("match"); m && m->is_object())
          item.match = parse_match(m->as_object());
        item.upstream_name = boost::json::value_to<std::string>(ro.at("upstream"));
        if (auto ops = ro.if_contains("ops"); ops && ops->is_array())
        {
          for (const auto &opv : ops->as_array())
          {
            const auto &op = opv.as_object();
            if (auto setv = op.if_contains("set"); setv && setv->is_object())
            {
              for (const auto &kv : setv->as_object())
              {
                item.header_set.emplace(kv.key_c_str(), boost::json::value_to<std::string>(boost::json::value(kv.value())));
              }
            }
          }
        }
        _routes.emplace_back(std::move(item));
      }
    }

    static stage_item parse_stage_item(const boost::json::object &o)
    {
      stage_item s;
      if (auto m = o.if_contains("match"); m && m->is_object())
        s.match = parse_match(m->as_object());
      if (auto op = o.if_contains("op"); op && op->is_object())
        s.op = op->as_object();
      return s;
    }
    /**
     * @brief 创建错误响应
     *
     * @param code HTTP 状态码
     * @param reason 状态原因
     * @param body_text 响应体文本
     * @return response_t 错误响应
     */
    response_t make_error_response(int code, const char *reason, const std::string &body_text)
    {
      response_t resp;
      resp.base().result(static_cast<boost::beast::http::status>(code));
      resp.base().reason(reason);
      resp.base().set(boost::beast::http::field::content_type, "text/plain");
      resp.body() = body_text;
      resp.base().set(boost::beast::http::field::content_length, std::to_string(resp.body().size()));
      return resp;
    }
  }; // end class transponder
} // end namespace represents
