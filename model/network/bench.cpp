// #include "agreement.hpp"
// #include <chrono>
// #include <iostream>
// #include <string>
// #include <vector>
// #include <unordered_map>
// #include <sstream>
// #include <algorithm>
// #include <charconv>
// #include <format>
// #include <random>
// #include <cstdint>

// using namespace std;
// using clk = std::chrono::steady_clock;
// using msd = std::chrono::duration<double, std::milli>;

// // ----------------- 辅助函数 -----------------
// static std::string make_random_string(std::size_t len)
// {
//   static std::mt19937_64 rng(123456789);
//   static std::uniform_int_distribution<int> dist('a', 'z');
//   std::string s;
//   s.reserve(len);
//   for (std::size_t i = 0; i < len; ++i)
//     s.push_back(static_cast<char>(dist(rng)));
//   return s;
// }

// // 生成一个 headers block（不含 body）
// // 格式示例： "METHOD 12345\r\nKey0: value0\r\nKey1: value1\r\n...\r\n\r\n"
// static std::string build_headers_block(const std::string &method,
//                                        uint32_t verification_code,
//                                        int num_headers,
//                                        int value_len)
// {
//   std::string out;
//   out.reserve(1024 + (std::size_t)num_headers * (value_len + 32));
//   out += std::format("{} {}\r\n", method, verification_code);
//   for (int i = 0; i < num_headers; ++i)
//   {
//     out += std::format("Key{}: {}\r\n", i, make_random_string(value_len));
//   }
//   out += "\r\n";
//   return out;
// }

// // ----------------- Baseline 实现（原始风格：stringstream + std::format） -----------------
// struct RequestHeaderBaseline
// {
//   std::string method;
//   std::uint32_t verification_code = 0;
//   std::unordered_map<std::string, std::string> headers;

//   RequestHeaderBaseline() { headers.reserve(12); }

//   // 基线序列化：使用 std::format（可读但可能有额外开销）
//   std::string to_string() const
//   {
//     std::string out;
//     out += std::format("{} {}\r\n", method, verification_code);
//     for (const auto &kv : headers)
//     {
//       out += std::format("{}: {}\r\n", kv.first, kv.second);
//     }
//     out += "\r\n";
//     return out;
//   }

//   // 基线解析：使用 std::stringstream 与 getline（原始方案）
//   bool from_string(const std::string &header_block)
//   {
//     std::stringstream ss(header_block);
//     std::string line;
//     if (!std::getline(ss, line))
//       return false;
//     if (!line.empty() && line.back() == '\r')
//       line.pop_back();
//     std::stringstream firstline(line);
//     if (!(firstline >> method >> verification_code))
//       return false;
//     headers.clear();
//     while (std::getline(ss, line))
//     {
//       if (!line.empty() && line.back() == '\r')
//         line.pop_back();
//       if (line.empty())
//         break;
//       size_t colon = line.find(':');
//       if (colon == std::string::npos)
//         return false;
//       std::string key = line.substr(0, colon);
//       size_t vstart = colon + 1;
//       while (vstart < line.size() && line[vstart] == ' ')
//         ++vstart;
//       std::string value = line.substr(vstart);
//       headers.emplace(std::move(key), std::move(value));
//     }
//     return true;
//   }
// };

// // ----------------- Optimized 实现（string_view + append/to_chars） -----------------
// struct RequestHeaderOptimized
// {
//   std::string method;
//   std::uint32_t verification_code = 0;
//   std::unordered_map<std::string, std::string> headers;

//   RequestHeaderOptimized() { headers.reserve(12); }

//   // 将 unsigned 添加到字符串（使用 to_chars，避免格式化开销）
//   static void append_uint(std::string &out, std::uint64_t v)
//   {
//     char buf[32];
//     auto r = std::to_chars(buf, buf + sizeof(buf), v);
//     out.append(buf, static_cast<size_t>(r.ptr - buf));
//   }

//   // 优化的序列化：method + code 用 format（保持可读），headers 使用 append 并按 key 排序保证顺序确定
//   std::string to_string() const
//   {
//     std::string out;
//     out.reserve(method.size() + headers.size() * 32 + 64);
//     out += std::format("{} ", method);
//     append_uint(out, verification_code);
//     out += "\r\n";

//     // 稳定序列化：按 key 排序
//     std::vector<const std::string *> keys;
//     keys.reserve(headers.size());
//     for (const auto &kv : headers)
//       keys.push_back(&kv.first);
//     std::sort(keys.begin(), keys.end(), [](const std::string *a, const std::string *b)
//               { return *a < *b; });

//     for (const std::string *kp : keys)
//     {
//       const std::string &k = *kp;
//       const std::string &v = headers.at(k);
//       out.append(k);
//       out.append(": ");
//       out.append(v);
//       out.append("\r\n");
//     }
//     out.append("\r\n");
//     return out;
//   }

//   // 优化的解析：接受 header_block 的 string_view（零拷贝解析）
//   bool from_string_view(std::string_view header_view)
//   {
//     if (header_view.empty())
//       return false;
//     headers.clear();
//     headers.reserve(12);
//     const char *data = header_view.data();
//     std::uint64_t total_len = static_cast<std::uint64_t>(header_view.size());
//     std::uint64_t pos = 0;

//     // 第一行结束
//     std::size_t first_nl = header_view.find('\n', pos);
//     if (first_nl == std::string_view::npos)
//       return false;
//     std::uint64_t first_end = first_nl;
//     if (first_end > 0 && data[first_end - 1] == '\r')
//       --first_end;

//     // parse method
//     std::uint64_t idx = 0;
//     while (idx < first_end && data[idx] != ' ' && data[idx] != '\t')
//       ++idx;
//     if (idx == 0 || idx >= first_end)
//       return false;
//     method.assign(data, static_cast<size_t>(idx));

//     // parse code token
//     std::uint64_t code_start = idx;
//     while (code_start < first_end && (data[code_start] == ' ' || data[code_start] == '\t'))
//       ++code_start;
//     if (code_start >= first_end)
//       return false;
//     std::uint64_t code_end = code_start;
//     while (code_end < first_end && data[code_end] != ' ' && data[code_end] != '\t')
//       ++code_end;
//     auto r = std::from_chars(data + code_start, data + code_end, verification_code);
//     if (r.ec != std::errc())
//       return false;
//     pos = first_nl + 1;

//     // parse headers lines
//     while (pos < total_len)
//     {
//       std::size_t line_nl = header_view.find('\n', pos);
//       std::uint64_t line_end = (line_nl == std::string_view::npos) ? total_len : static_cast<std::uint64_t>(line_nl);
//       std::uint64_t line_start = pos;
//       std::uint64_t line_len = (line_end > line_start) ? (line_end - line_start) : 0;
//       if (line_len > 0 && data[line_start + line_len - 1] == '\r')
//         --line_len;
//       if (line_len == 0)
//         break; // empty line -> end
//       std::string_view line(data + line_start, static_cast<size_t>(line_len));
//       std::size_t colon = line.find(':');
//       if (colon == std::string_view::npos)
//         return false;
//       std::string_view key = line.substr(0, colon);
//       // rtrim key
//       while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
//         key.remove_suffix(1);
//       if (key.empty())
//         return false;
//       std::string_view val = line.substr(colon + 1);
//       // trim val
//       while (!val.empty() && (val.front() == ' ' || val.front() == '\t'))
//         val.remove_prefix(1);
//       while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
//         val.remove_suffix(1);
//       headers.emplace(std::string(key), std::string(val));
//       if (line_nl == std::string_view::npos)
//       {
//         pos = total_len;
//         break;
//       }
//       pos = static_cast<std::uint64_t>(line_nl) + 1;
//     }
//     return true;
//   }

//   // 兼容：接受 std::string
//   bool from_string(const std::string &s)
//   {
//     return from_string_view(std::string_view(s));
//   }
// };

// // ----------------- Benchmark 测试逻辑 -----------------
// struct BenchConfig
// {
//   int loops = 200000;         // 每项循环次数（较大值减少噪声）
//   int num_headers = 12;       // header 数量
//   int value_len = 24;         // 每个 value 的长度
//   std::string method = "REQ"; // 请求方法文本
// };

// static void bench_serialize_baseline(const BenchConfig &cfg, RequestHeaderBaseline &obj)
// {
//   // warmup
//   for (int i = 0; i < 1000; ++i)
//     obj.to_string();
//   auto t0 = clk::now();
//   for (int i = 0; i < cfg.loops; ++i)
//     obj.to_string();
//   auto t1 = clk::now();
//   double ms = msd(t1 - t0).count();
//   cout << "[Baseline] serialize: total " << ms << " ms, ns/op " << (ms * 1e6 / cfg.loops) << ", ops/s " << (cfg.loops / (ms / 1000.0)) << "\n";
// }

// static void bench_deserialize_baseline(const BenchConfig &cfg, const std::string &sample_block)
// {
//   RequestHeaderBaseline tmp;
//   // warmup
//   for (int i = 0; i < 1000; ++i)
//     tmp.from_string(sample_block);
//   auto t0 = clk::now();
//   for (int i = 0; i < cfg.loops; ++i)
//   {
//     tmp.from_string(sample_block);
//   }
//   auto t1 = clk::now();
//   double ms = msd(t1 - t0).count();
//   cout << "[Baseline] deserialize: total " << ms << " ms, ns/op " << (ms * 1e6 / cfg.loops) << ", ops/s " << (cfg.loops / (ms / 1000.0)) << "\n";
// }

// static void bench_serialize_optimized(const BenchConfig &cfg, RequestHeaderOptimized &obj)
// {
//   // warmup
//   for (int i = 0; i < 1000; ++i)
//     obj.to_string();
//   auto t0 = clk::now();
//   for (int i = 0; i < cfg.loops; ++i)
//     obj.to_string();
//   auto t1 = clk::now();
//   double ms = msd(t1 - t0).count();
//   cout << "[Opt] serialize: total " << ms << " ms, ns/op " << (ms * 1e6 / cfg.loops) << ", ops/s " << (cfg.loops / (ms / 1000.0)) << "\n";
// }

// static void bench_deserialize_optimized(const BenchConfig &cfg, const std::string &sample_block)
// {
//   RequestHeaderOptimized tmp;
//   // warmup
//   for (int i = 0; i < 1000; ++i)
//     tmp.from_string(sample_block);
//   auto t0 = clk::now();
//   for (int i = 0; i < cfg.loops; ++i)
//     tmp.from_string(sample_block);
//   auto t1 = clk::now();
//   double ms = msd(t1 - t0).count();
//   cout << "[Opt] deserialize: total " << ms << " ms, ns/op " << (ms * 1e6 / cfg.loops) << ", ops/s " << (cfg.loops / (ms / 1000.0)) << "\n";
// }

// // round-trip test
// static void bench_roundtrip(const BenchConfig &cfg, const RequestHeaderBaseline &base_obj, const std::string &sample_block)
// {
//   // baseline roundtrip
//   RequestHeaderBaseline tmpb;
//   auto t0 = clk::now();
//   for (int i = 0; i < cfg.loops; ++i)
//   {
//     std::string s = base_obj.to_string();
//     tmpb.from_string(s);
//   }
//   auto t1 = clk::now();
//   double msb = msd(t1 - t0).count();
//   cout << "[Baseline] roundtrip: total " << msb << " ms, ns/op " << (msb * 1e6 / cfg.loops) << ", ops/s " << (cfg.loops / (msb / 1000.0)) << "\n";

//   // optimized roundtrip
//   RequestHeaderOptimized tmpo;
//   RequestHeaderOptimized sample_opt;
//   sample_opt.method = base_obj.method;
//   sample_opt.verification_code = base_obj.verification_code;
//   sample_opt.headers = base_obj.headers;

//   t0 = clk::now();
//   for (int i = 0; i < cfg.loops; ++i)
//   {
//     std::string s = sample_opt.to_string();
//     tmpo.from_string(s);
//   }
//   t1 = clk::now();
//   double mso = msd(t1 - t0).count();
//   cout << "[Opt] roundtrip: total " << mso << " ms, ns/op " << (mso * 1e6 / cfg.loops) << ", ops/s " << (cfg.loops / (mso / 1000.0)) << "\n";
// }

// // ----------------- main -----------------
// int main(int argc, char **argv)
// {
//   BenchConfig cfg;
//   // 可通过命令行简单覆盖参数（可扩展）
//   // 例如： ./bench 100000 20 64
//   if (argc >= 2)
//     cfg.loops = std::stoi(argv[1]);
//   if (argc >= 3)
//     cfg.num_headers = std::stoi(argv[2]);
//   if (argc >= 4)
//     cfg.value_len = std::stoi(argv[3]);

//   cout << "Bench config: loops=" << cfg.loops << " num_headers=" << cfg.num_headers << " value_len=" << cfg.value_len << "\n";

//   // 构造示例对象
//   RequestHeaderBaseline base;
//   base.method = cfg.method;
//   base.verification_code = 123456u;
//   for (int i = 0; i < cfg.num_headers; ++i)
//   {
//     base.headers.emplace(std::format("Key{}", i), make_random_string((std::size_t)cfg.value_len));
//   }
//   std::string baseline_block = base.to_string();

//   RequestHeaderOptimized opt;
//   opt.method = base.method;
//   opt.verification_code = base.verification_code;
//   opt.headers = base.headers;
//   std::string optimized_block = opt.to_string();

//   cout << "baseline header block size: " << baseline_block.size() << " bytes\n";
//   cout << "optimized header block size: " << optimized_block.size() << " bytes\n";

//   // 运行测试
//   bench_serialize_baseline(cfg, base);
//   bench_deserialize_baseline(cfg, baseline_block);

//   bench_serialize_optimized(cfg, opt);
//   bench_deserialize_optimized(cfg, optimized_block);

//   bench_roundtrip(cfg, base, baseline_block);

//   return 0;
// }

// bench_request_headers_cn.cpp
// C++20 单文件压测（中文输出，可调参数）
// 编译：g++ -O3 -march=native -std=c++20 bench_request_headers_cn.cpp -o bench_cn
#include "agreement.hpp"
#include <windows.h>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <charconv>
#include <format>
#include <random>
#include <cstdint>
#include <numeric>
#include <cstring>

using namespace std;
using clk = std::chrono::high_resolution_clock;
using msd = std::chrono::duration<double, std::milli>;

// ----------------- 辅助：随机字符串 -----------------
static std::string make_random_string(std::size_t len)
{
  static std::mt19937_64 rng(123456789);
  static std::uniform_int_distribution<int> dist('a', 'z');
  std::string s;
  s.reserve(len);
  for (std::size_t i = 0; i < len; ++i)
    s.push_back(static_cast<char>(dist(rng)));
  return s;
}

// ----------------- Baseline：原始 (stringstream + format) -----------------
struct RequestHeaderBaseline
{
  std::string method;
  std::uint32_t verification_code = 0;
  std::unordered_map<std::string, std::string> headers;

  RequestHeaderBaseline() { headers.reserve(12); }

  std::string to_string() const
  {
    std::string out;
    out += std::format("{} {}\r\n", method, verification_code);
    for (const auto &kv : headers)
    {
      out += std::format("{}: {}\r\n", kv.first, kv.second);
    }
    out += "\r\n";
    return out;
  }

  bool from_string(const std::string &header_block)
  {
    std::stringstream ss(header_block);
    std::string line;
    if (!std::getline(ss, line))
      return false;
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    std::stringstream firstline(line);
    if (!(firstline >> method >> verification_code))
      return false;
    headers.clear();
    while (std::getline(ss, line))
    {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty())
        break;
      size_t colon = line.find(':');
      if (colon == std::string::npos)
        return false;
      std::string key = line.substr(0, colon);
      size_t vstart = colon + 1;
      while (vstart < line.size() && line[vstart] == ' ')
        ++vstart;
      std::string value = line.substr(vstart);
      headers.emplace(std::move(key), std::move(value));
    }
    return true;
  }
};

// ----------------- Optimized：string_view + append/to_chars -----------------
struct RequestHeaderOptimized
{
  std::string method;
  std::uint32_t verification_code = 0;
  std::unordered_map<std::string, std::string> headers;

  RequestHeaderOptimized() { headers.reserve(12); }

  // to_chars 追加整数
  static void append_uint(std::string &out, std::uint64_t v)
  {
    char buf[32];
    auto r = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, static_cast<size_t>(r.ptr - buf));
  }

  // 高性能序列化：append + to_chars，按 key 排序保证确定性
  std::string to_string() const
  {
    std::string out;
    out.reserve(method.size() + headers.size() * 32 + 32);
    out.append(method);
    out.push_back(' ');
    append_uint(out, verification_code);
    out.append("\r\n");

    // 收集键值指针，排序后直接输出，避免 at() 二次哈希
    std::vector<std::pair<const std::string *, const std::string *>> kvs;
    kvs.reserve(headers.size());
    for (const auto &kv : headers)
      kvs.emplace_back(&kv.first, &kv.second);
    std::sort(kvs.begin(), kvs.end(), [](auto &a, auto &b)
              { return *a.first < *b.first; });
    for (const auto &p : kvs)
    {
      out.append(*p.first);
      out.append(": ");
      out.append(*p.second);
      out.append("\r\n");
    }
    out.append("\r\n");
    return out;
  }

  // 解析（string_view 版本），并复用 headers（建议外部 reserve）
  bool from_string_view(std::string_view header_view)
  {
    if (header_view.empty())
      return false;
    headers.clear();
    const char *data = header_view.data();
    std::uint64_t total_len = static_cast<std::uint64_t>(header_view.size());
    std::uint64_t pos = 0;

    // 第一行结束
    std::size_t first_nl = header_view.find('\n', pos);
    if (first_nl == std::string_view::npos)
      return false;
    std::uint64_t first_end = first_nl;
    if (first_end > 0 && data[first_end - 1] == '\r')
      --first_end;

    // method
    std::uint64_t idx = 0;
    while (idx < first_end && data[idx] != ' ' && data[idx] != '\t')
      ++idx;
    if (idx == 0 || idx >= first_end)
      return false;
    method.assign(data, static_cast<size_t>(idx));

    // code
    std::uint64_t code_start = idx;
    while (code_start < first_end && (data[code_start] == ' ' || data[code_start] == '\t'))
      ++code_start;
    if (code_start >= first_end)
      return false;
    std::uint64_t code_end = code_start;
    while (code_end < first_end && data[code_end] != ' ' && data[code_end] != '\t')
      ++code_end;
    auto r = std::from_chars(data + code_start, data + code_end, verification_code);
    if (r.ec != std::errc())
      return false;
    pos = first_nl + 1;

    // 每行解析
    while (pos < total_len)
    {
      std::size_t line_nl = header_view.find('\n', pos);
      std::uint64_t line_end = (line_nl == std::string_view::npos) ? total_len : static_cast<std::uint64_t>(line_nl);
      std::uint64_t line_start = pos;
      std::uint64_t line_len = (line_end > line_start) ? (line_end - line_start) : 0;
      if (line_len > 0 && data[line_start + line_len - 1] == '\r')
        --line_len;
      if (line_len == 0)
        break; // 空行结束
      std::string_view line(data + line_start, static_cast<size_t>(line_len));
      std::size_t colon = line.find(':');
      if (colon == std::string_view::npos)
        return false;

      // key rtrim
      std::string_view key = line.substr(0, colon);
      while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
        key.remove_suffix(1);
      if (key.empty())
        return false;
      // value trim
      std::string_view val = line.substr(colon + 1);
      while (!val.empty() && (val.front() == ' ' || val.front() == '\t'))
        val.remove_prefix(1);
      while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
        val.remove_suffix(1);
      headers.emplace(std::string(key), std::string(val));

      if (line_nl == std::string_view::npos)
      {
        pos = total_len;
        break;
      }
      pos = static_cast<std::uint64_t>(line_nl) + 1;
    }
    return true;
  }

  bool from_string(const std::string &s)
  {
    return from_string_view(std::string_view(s));
  }
};

// ----------------- 压测工具函数（中文输出） -----------------
struct BenchConfig
{
  int loops = 1000000;
  int num_headers = 12;
  int value_len = 35;
  int repeats = 5;            // 重复多少次取 median/mean
  std::string which = "both"; // both / baseline / opt
  std::string method = "REQ";
};

static double time_func_ms(function<void()> fn)
{
  auto t0 = clk::now();
  fn();
  auto t1 = clk::now();
  return msd(t1 - t0).count();
}

static void print_header()
{
  cout << "-------------------- 压测开始 --------------------\n";
}

// 求中位数、平均数（输入为 ms）
static double median(vector<double> v)
{
  if (v.empty())
    return 0.0;
  sort(v.begin(), v.end());
  size_t n = v.size();
  if (n % 2)
    return v[n / 2];
  return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
static double mean(const vector<double> &v)
{
  if (v.empty())
    return 0.0;
  double s = accumulate(v.begin(), v.end(), 0.0);
  return s / v.size();
}

// 单次完整测试（baseline 或 optimized），返回三项：serialize_ms, deserialize_ms, roundtrip_ms
static tuple<double, double, double> single_run(const BenchConfig &cfg, bool run_baseline)
{
  // 构造样本对象
  if (run_baseline)
  {
    RequestHeaderBaseline base;
    base.method = cfg.method;
    base.verification_code = 123456u;
    base.headers.reserve(static_cast<size_t>(cfg.num_headers));
    for (int i = 0; i < cfg.num_headers; ++i)
      base.headers.emplace(std::format("Key{}", i), make_random_string((size_t)cfg.value_len));
    string header_block = base.to_string();

    // 序列化：重复调用 base.to_string()（复用对象）
    RequestHeaderBaseline tmp_serial = base;
    auto serialize_ms = time_func_ms([&]()
                                     {
            for (int i = 0; i < cfg.loops; ++i) {
                (void)tmp_serial.to_string();
            } });

    // 反序列化：创建一个 tmp 对象并不断 from_string（复用 tmp.headers）
    RequestHeaderBaseline tmp_parse;
    tmp_parse.headers.reserve(static_cast<size_t>(cfg.num_headers));
    auto deserialize_ms = time_func_ms([&]()
                                       {
            for (int i = 0; i < cfg.loops; ++i) {
                tmp_parse.from_string(header_block);
            } });

    // roundtrip：每次序列化后解析
    RequestHeaderBaseline tmprt;
    tmprt.headers.reserve(static_cast<size_t>(cfg.num_headers));
    auto roundtrip_ms = time_func_ms([&]()
                                     {
            for (int i = 0; i < cfg.loops; ++i) {
                string s = base.to_string();
                tmprt.from_string(s);
            } });

    return {serialize_ms, deserialize_ms, roundtrip_ms};
  }
  else
  {
    // optimized path
    RequestHeaderOptimized base;
    base.method = cfg.method;
    base.verification_code = 123456u;
    base.headers.reserve(static_cast<size_t>(cfg.num_headers));
    for (int i = 0; i < cfg.num_headers; ++i)
      base.headers.emplace(std::format("Key{}", i), make_random_string((size_t)cfg.value_len));
    string header_block = base.to_string();

    // serialize
    RequestHeaderOptimized tmp_serial = base;
    auto serialize_ms = time_func_ms([&]()
                                     {
            for (int i = 0; i < cfg.loops; ++i) {
                (void)tmp_serial.to_string();
            } });

    // deserialize (复用 tmp.headers)
    RequestHeaderOptimized tmp_parse;
    tmp_parse.headers.reserve(static_cast<size_t>(cfg.num_headers));
    auto deserialize_ms = time_func_ms([&]()
                                       {
            for (int i = 0; i < cfg.loops; ++i) {
                tmp_parse.from_string(header_block);
            } });

    // roundtrip
    RequestHeaderOptimized tmprt;
    tmprt.headers.reserve(static_cast<size_t>(cfg.num_headers));
    auto roundtrip_ms = time_func_ms([&]()
                                     {
            for (int i = 0; i < cfg.loops; ++i) {
                string s = base.to_string();
                tmprt.from_string(s);
            } });

    return {serialize_ms, deserialize_ms, roundtrip_ms};
  }
}

// 执行 repeats 次 single_run 并打印中文结果
static void run_bench(const BenchConfig &cfg)
{
  print_header();
  cout << "配置：loops=" << cfg.loops << "  headers=" << cfg.num_headers << "  value_len=" << cfg.value_len
       << "  repeats=" << cfg.repeats << "  which=" << cfg.which << "\n\n";

  auto do_runs = [&](bool run_baseline)
  {
    vector<double> ser_ms, des_ms, rt_ms;
    for (int r = 0; r < cfg.repeats; ++r)
    {
      auto [s, d, rt] = single_run(cfg, run_baseline);
      ser_ms.push_back(s);
      des_ms.push_back(d);
      rt_ms.push_back(rt);
      cout << (run_baseline ? "[基线] " : "[优化] ")
           << "第 " << (r + 1) << " 次: 序列化 " << s << " ms, 反序列化 " << d << " ms, 往返 " << rt << " ms\n";
    }
    cout << "\n"
         << (run_baseline ? ">>> 基线 " : ">>> 优化 ") << "统计结果（重复 " << cfg.repeats << " 次）:\n";
    cout << "序列化: median(ms)=" << median(ser_ms) << "  mean(ms)=" << mean(ser_ms) << "  每次(ns)=" << (median(ser_ms) * 1e6 / cfg.loops) << "\n";
    cout << "反序列化: median(ms)=" << median(des_ms) << "  mean(ms)=" << mean(des_ms) << "  每次(ns)=" << (median(des_ms) * 1e6 / cfg.loops) << "\n";
    cout << "往返: median(ms)=" << median(rt_ms) << "  mean(ms)=" << mean(rt_ms) << "  每次(ns)=" << (median(rt_ms) * 1e6 / cfg.loops) << "\n";
    cout << "---------------------------------------------\n\n";
  };

  if (cfg.which == "both" || cfg.which == "baseline")
    do_runs(true);
  if (cfg.which == "both" || cfg.which == "opt")
    do_runs(false);
}

// ----------------- main -----------------
int main()
{
  SetConsoleOutputCP(CP_UTF8);
  BenchConfig cfg;
  // 解析命令行： ./bench loops num_headers value_len repeats which
  

  // 默认提示
  cout << "提示：建议使用 Release 编译（-O3 -march=native），并在静默环境下运行以减少噪声。\n";
  cout << "示例：./bench_cn 200000 12 24 5 both\n\n";

  run_bench(cfg);
  system("pause");
  return 0;
}
