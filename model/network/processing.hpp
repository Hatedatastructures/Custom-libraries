#pragma once
#include <iostream>
#include <string_view>
#include <string>

namespace agreement
{
  namespace detail
  {

    inline std::string_view trim_both_ends(std::string_view sv)
    {
      std::uint64_t begin_index = 0;
      std::uint64_t end_index = static_cast<std::uint64_t>(sv.size());
      while (begin_index < end_index && (sv[begin_index] == ' ' || sv[begin_index] == '\t'))
        ++begin_index;
      while (end_index > begin_index && (sv[end_index - 1] == ' ' || sv[end_index - 1] == '\t'))
        --end_index;
      return sv.substr(begin_index, end_index - begin_index);
    }

    inline std::string_view rtrim_right(std::string_view sv)
    {
      std::uint64_t end_index = static_cast<std::uint64_t>(sv.size());
      std::uint64_t begin_index = 0;
      while (end_index > begin_index && (sv[end_index - 1] == ' ' || sv[end_index - 1] == '\t'))
        --end_index;
      return sv.substr(begin_index, end_index - begin_index);
    }

    // 将无符号整数追加到字符串，使用 std::to_chars（避免 sprintf 等调用）
    inline void append_uint_to_string(std::string &out, std::uint64_t value)
    {
      char buffer[32];
      auto res = std::to_chars(buffer, buffer + sizeof(buffer), value);
      out.append(buffer, static_cast<std::size_t>(res.ptr - buffer));
    }

  }
} // namespace agreement::detail