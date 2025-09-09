#include <chrono>
/**
 * @brief #### 时间转换工具类
 */
class convert_time
  {
  public:
    /**
     * @brief #### 将任意时间单位转换为毫秒
     */
    template <typename rep, typename period>
    static std::chrono::milliseconds to_milliseconds(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    }
    /**
     * @brief #### 将任意时间单位转换为秒
     */
    template <typename rep, typename period>
    static std::chrono::seconds to_seconds(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::seconds>(duration);
    }
    /**
     * @brief #### 将任意时间单位转换为分钟
     */
    template <typename rep, typename period>
    static std::chrono::minutes to_minutes(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::minutes>(duration);
    }
    /**
     * @brief #### 将任意时间单位转换为小时
     */
    template <typename rep, typename period>
    static std::chrono::hours to_hours(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::hours>(duration);
    }
    /**
     * @brief #### 将任意时间单位转换为天
     */
    template <typename rep, typename period>
    static std::chrono::days to_days(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::days>(duration);
    }
    /**
     * @brief #### 将任意时间单位转换为周
     */
    template <typename rep, typename period>
    static std::chrono::weeks to_weeks(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::weeks>(duration);
    }
    /**
     * @brief #### 将任意时间单位转换为月
     */
    template <typename rep, typename period>
    static std::chrono::months to_months(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::months>(duration);
    }
    /**
     * @brief #### 将任意时间单位转换为年
     */
    template <typename rep, typename period>
    static std::chrono::years to_years(const std::chrono::duration<rep, period>& duration)
    {
      return std::chrono::duration_cast<std::chrono::years>(duration);
    }
    /**
     * @brief #### 时间点格式化为 UTC 字符串
     */
    static std::string to_utc_string(const std::chrono::system_clock::time_point& tp,
    std::string_view fmt = "%Y-%m-%dT%H:%M:%SZ") noexcept
    {
      const std::time_t t = std::chrono::system_clock::to_time_t(tp);
      std::tm tm{}; 
#if defined(_WIN32)
      gmtime_s(&tm, &t);
#else
      gmtime_r(&t, &tm);
#endif
      std::array<char, 128> buf{};
      std::strftime(buf.data(), buf.size(), fmt.data(), &tm);
      return std::string(buf.data());
    }
    /**
     * @brief #### 时间点格式化为本地时间字符串
     */
    static std::string to_local_string(const std::chrono::system_clock::time_point& tp,
    std::string_view fmt = "%Y-%m-%d %H:%M:%S") noexcept
    {
      const std::time_t t = std::chrono::system_clock::to_time_t(tp);
      std::tm tm{};
#if defined(_WIN32)
      localtime_s(&tm, &t);
#else
      localtime_r(&t, &tm);
#endif
      std::array<char, 128> buf{};
      std::strftime(buf.data(), buf.size(), fmt.data(), &tm);
      return std::string(buf.data());
    }
  };