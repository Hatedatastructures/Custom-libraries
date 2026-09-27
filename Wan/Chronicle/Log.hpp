/**
 * @file Log.hpp
 * @brief 提供同步与异步日志工具。
 */
#pragma once
#include <string>
#include <format>
#include <chrono>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <system_error>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/stream_file.hpp>

namespace PerformanceLog
{
    /**
     * @brief 检查类型是否可用于 Asio 缓冲区。
     * @tparam ContainerType 待检查的容器类型。
     */
    template <typename ContainerType>
    concept Compatible = requires(const ContainerType &X) { boost::asio::buffer(X); };

    /** @brief 日志级别，级别越高过滤阈值越严格。 */
    enum class Level
    {
        /** @brief 调试信息。 */
        Debug,
        /** @brief 普通运行信息。 */
        Info,
        /** @brief 可恢复的警告。 */
        Warn,
        /** @brief 操作错误。 */
        Error,
        /** @brief 致命错误。 */
        Fatal,
    };

    namespace Asio = boost::asio;
    namespace Fs = std::filesystem;

    /** @brief 支持协程和同步调用的日志写入器。 */
    class CoroutineLog
    {
        /** @brief 跟踪单个日志文件句柄及其已写入字节数。 */
        struct Context
        {
            /** @brief 打开的异步文件句柄。 */
            std::shared_ptr<Asio::stream_file> Handle;
            /** @brief 当前文件已写入的字节数。 */
            std::size_t CurrentSize = 0;
        };

    public:
        /**
         * @brief 使用指定执行器创建日志写入器。
         * @param Executor 日志异步操作使用的 Asio 执行器。
         */
        explicit CoroutineLog(const Asio::any_io_executor &Executor)
            : EventExecutor(Executor),
              SerialExec(Asio::make_strand(Executor))
        {
            FileMap.reserve(4);
            // 默认日志目录
            RootDirectory = Fs::current_path() / "logs";
            // 默认时间偏移为 +8 小时，保持与旧行为一致
            TimeOffset = std::chrono::hours(8ULL);
        }

        /**
         * @brief 设置日志输出目录
         * @param DirectoryName 日志输出目录的路径
         * @return 表示异步设置操作的 awaitable。
         */
        Asio::awaitable<void> SetOutputDirectory(const std::string &DirectoryName)
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            RootDirectory = DirectoryName;
            std::error_code DirectoryEc;
            if (!Fs::exists(RootDirectory, DirectoryEc)
                && !DirectoryEc)
            {
                // 创建目录
                Fs::create_directories(RootDirectory, DirectoryEc);
            }
        }

        /**
         * @brief 设置日志文件的最大大小，超过该大小则创建新的日志文件
         * @param Size 日志文件的最大大小，单位为字节
         * @return 表示异步设置操作的 awaitable。
         */
        Asio::awaitable<void> SetMaxFileSize(const std::size_t Size)
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            MaxFileSize = Size;
        }

        /**
         * @brief 设置时间偏移，用于日志中的时间戳
         * @param Offset 时间偏移量，默认 +8 小时
         * @return 表示异步设置操作的 awaitable。
         */
        Asio::awaitable<void> SetTimeOffset(const std::chrono::minutes Offset)
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            TimeOffset = Offset;
        }

        /**
         * @brief 设置输出到日志文件的级别阈值
         * @param Threshold 日志级别阈值
         * @return 表示异步设置操作的 awaitable。
         */
        Asio::awaitable<void> SetFileLevelThreshold(const Level Threshold)
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            FileLevelThreshold = Threshold;
        }

        /**
         * @brief 设置输出到控制台日志的级别阈值
         * @param Threshold 日志级别阈值
         * @return 表示异步设置操作的 awaitable。
         */
        Asio::awaitable<void> SetConsoleLevelThreshold(const Level Threshold)
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            ConsoleLevelThreshold = Threshold;
        }

        /**
         * @brief 设置日志文件最大归档数量，超过该数量则删除最旧的日志文件
         * @param Count 最大归档数量
         * @return 表示异步设置操作的 awaitable。
         */
        Asio::awaitable<void> SetMaxArchiveCount(const std::size_t Count)
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            MaxArchiveCount = Count;
        }

        /**
         * @brief 关闭指定路径对应的日志文件。
         * @param Path 日志文件路径。
         * @return 表示异步关闭操作的 awaitable。
         */
        Asio::awaitable<void> CloseFile(const std::string &Path) const
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            if (const auto It = FileMap.find(Path); It != FileMap.end())
            {
                if (It->second.Handle)
                    It->second.Handle->close();
                FileMap.erase(It);
            }
        }

        /**
         * @brief 关闭所有已打开的日志文件。
         * @return 表示异步关闭操作的 awaitable。
         */
        Asio::awaitable<void> Shutdown() const
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            for (auto It = FileMap.begin(); It != FileMap.end();)
            {
                if (It->second.Handle)
                    It->second.Handle->close();
                It = FileMap.erase(It);
            }
        }

        /**
         * @brief 将日志消息写入文件。
         * @tparam Container 满足 `Compatible` 的缓冲区类型。
         * @param Filename 日志文件名。
         * @param Data 日志消息。
         * @return 写入的字节数；如果文件不存在，会尝试创建目录并打开文件。
         */
        template <Compatible Container>
        auto FileWrite(const std::string &Filename, const Container &Data) const
            -> Asio::awaitable<std::size_t>
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);

            // 1. 获取文件路径
            Fs::path TargetPath = RootDirectory / Filename;
            if (Fs::path(Filename).is_absolute())
            { // 如果路径是绝对路径，则直接使用，避免路径拼接错误
                TargetPath = Filename;
            }
            std::string Key = TargetPath.string();

            // 2. 获取或创建文件上下文
            auto It = FileMap.find(Key);
            if (It == FileMap.end() || !It->second.Handle || !It->second.Handle->is_open())
            {
                std::error_code DirectoryEc;
                if (!Fs::exists(TargetPath.parent_path(), DirectoryEc)
                    && !DirectoryEc)
                { // 如果目录不存在，尝试创建
                    Fs::create_directories(TargetPath.parent_path(),
                        DirectoryEc);
                }
                if (DirectoryEc)
                {
                    co_return 0;
                }

                Asio::stream_file Fp(SerialExec, Key,
                                     Asio::file_base::write_only | Asio::file_base::create | Asio::file_base::append);

                if (!Fp.is_open())
                { // 尝试自愈：如果文件存在但无法打开，可能是损坏，尝试删除并重新创建
                    if (std::error_code Ec; Fs::exists(TargetPath, Ec))
                    {
                        Fs::remove(TargetPath, Ec);
                        Asio::stream_file FpRetry(SerialExec, Key,
                                                   Asio::file_base::write_only | Asio::file_base::create | Asio::file_base::append);
                        if (!FpRetry.is_open())
                            co_return 0;
                        Fp = std::move(FpRetry);
                    }
                    else
                    {
                        co_return 0;
                    }
                }

                Context Ctx; // 初始化上下文并获取当前文件大小
                Ctx.Handle = std::make_shared<Asio::stream_file>(std::move(Fp));
                std::error_code Ec;
                Ctx.CurrentSize = Fs::file_size(TargetPath, Ec);
                It = FileMap.insert_or_assign(Key, std::move(Ctx)).first;
            }

            // 3. 滚动检查
            std::size_t WriteSize = Asio::buffer_size(Asio::buffer(Data));
            if (WriteSize > MaxFileSize
                || It->second.CurrentSize > MaxFileSize - WriteSize)
            {
                // 关闭当前句柄
                It->second.Handle->close();

                // 生成归档文件名（保持扩展名在末尾）
                auto Now = std::chrono::system_clock::now();
                auto TimestampStringValue = std::format("{:%Y%m%d_%H%M%S}", Now);
                Fs::path Parent = TargetPath.parent_path();
                std::string Stem = TargetPath.stem().string();
                std::string Ext = TargetPath.extension().string();
                Fs::path ArchivePath;
                std::error_code Ec;
                bool Archived = false;
                for (std::size_t Suffix = 0; Suffix < 1000; ++Suffix)
                {
                    const auto SuffixText = Suffix == 0
                        ? std::string{}
                        : "_" + std::to_string(Suffix);
                    ArchivePath = Parent / (Stem + "-"
                        + TimestampStringValue + SuffixText + Ext);
                    if (Fs::exists(ArchivePath, Ec))
                    {
                        continue;
                    }
                    Ec.clear();
                    Fs::rename(TargetPath, ArchivePath, Ec);
                    if (!Ec)
                    {
                        Archived = true;
                        break;
                    }
                }
                if (!Archived)
                {
                    FileMap.erase(It);
                    co_return 0;
                }
                // 归档保留策略清理
                CleanupOldArchives(TargetPath);

                // 重新创建新文件
                Asio::stream_file NewFp(SerialExec, Key,
                                         Asio::file_base::write_only | Asio::file_base::create | Asio::file_base::truncate); // truncate 清空

                if (!NewFp.is_open())
                { // 滚动失败，移除映射
                    FileMap.erase(It);
                    co_return 0;
                }

                It->second.Handle = std::make_shared<Asio::stream_file>(std::move(NewFp));
                It->second.CurrentSize = 0;
            }

            // 4. 执行写入
            boost::system::error_code Ec;
            std::size_t N = co_await Asio::async_write(*It->second.Handle, Asio::buffer(Data),
                                                       Asio::redirect_error(Asio::use_awaitable, Ec));

            if (Ec)
            {
                FileMap.erase(It); // 写入失败，移除映射
                co_return 0;
            }

            It->second.CurrentSize += N;
            co_return N;
        }

        /**
         * @brief 依次合并并写入多条日志消息。
         * @param Path 日志文件名。
         * @param Data 待写入的消息列表。
         * @return 写入的总字节数。
         */
        auto FileWrite(const std::string &Path, const std::vector<std::string> &Data) const
            -> Asio::awaitable<std::size_t>
        {
            std::size_t Total = 0;
            for (auto const &S : Data)
            {
                Total += S.size();
            }
            std::string Joined;
            Joined.reserve(Total);
            for (auto const &S : Data)
            {
                Joined.append(S);
            }

            co_return co_await FileWrite(Path, Joined);
        }

        /**
         * @brief 获取日志级别的文本表示。
         * @param LogLevel 日志级别。
         * @return 对应的级别文本。
         */
        static std::string ToString(const Level &LogLevel)
        {
            switch (LogLevel)
            {
            case Level::Debug:
                return "DEBUG";
            case Level::Info:
                return "INFO";
            case Level::Warn:
                return "WARN";
            case Level::Error:
                return "ERROR";
            case Level::Fatal:
                return "FATAL";
            default:
                return "";
            }
        }

        /**
         * @brief 将日志消息输出到控制台
         * @param LogLevel 日志级别
         * @param Data 日志消息
         * @return 写入的字节数
         * @note 如果日志级别低于 ConsoleLevelThreshold，则不输出
         */
        auto ConsoleWrite(const Level &LogLevel, const std::string &Data) const
            -> Asio::awaitable<std::size_t>
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            if (static_cast<int>(LogLevel) < static_cast<int>(ConsoleLevelThreshold))
            {
                co_return 0;
            }
            const std::string LogString = TimestampString() + std::format("[{}] ", ToString(LogLevel)) + Data;
            std::cout.write(LogString.data(), static_cast<std::streamsize>(LogString.size()));
            std::cout.flush();
            std::size_t N = LogString.size();
            co_return N;
        }

        /**
         * @brief 将日志消息输出到控制台，并在末尾添加换行符
         * @param LogLevel 日志级别
         * @param Data 日志消息
         * @return 写入的字节数
         * @note 如果日志级别低于 ConsoleLevelThreshold，则不输出
         */
        auto ConsoleWriteLine(const Level &LogLevel, const std::string &Data) const
            -> Asio::awaitable<std::size_t>
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            if (static_cast<int>(LogLevel) < static_cast<int>(ConsoleLevelThreshold))
            {
                co_return 0;
            }
            std::string LogString = TimestampString() + std::format("[{}] ", ToString(LogLevel)) + Data + "\n";
            std::cout.write(LogString.data(), static_cast<std::streamsize>(LogString.size()));
            std::cout.flush();
            std::size_t N = LogString.size();
            co_return N;
        }

        /**
         * @brief 按格式化字符串写入文件日志。
         * @tparam FormatArgs 格式参数类型。
         * @param Filename 日志文件名。
         * @param LogLevel 日志级别。
         * @param Format 格式字符串。
         * @param FormatArguments 格式参数。
         * @return 写入的字节数；级别低于文件阈值时返回零。
         */
        template <typename... FormatArgs>
        auto FileWriteFmt(const std::string &Filename, Level LogLevel, const std::string &Format, FormatArgs &&...FormatArguments) const
            -> Asio::awaitable<std::size_t>
        {
            co_await Asio::dispatch(SerialExec, Asio::use_awaitable);
            if (static_cast<int>(LogLevel) < static_cast<int>(FileLevelThreshold))
            {
                co_return 0;
            }
            const std::string DataString = TimestampString() + std::format("[{}] ", ToString(LogLevel)) + std::vformat(Format, std::make_format_args(std::forward<FormatArgs>(FormatArguments)...));
            co_return co_await FileWrite(Filename, DataString);
        }

        /**
         * @brief 写入日志消息到文件，并在末尾添加换行符
         * @param Filename 日志文件名
         * @param Data 日志消息
         * @return 写入的字节数
         * @note 会在日志消息前添加时间戳
         */
        Asio::awaitable<std::size_t> FileWriteLine(const std::string &Filename, const std::string &Data) const
        {
            std::string DataString = TimestampString() + Data + "\n";
            co_return co_await FileWrite(Filename, DataString);
        }

        /**
         * @brief 按格式化字符串输出控制台日志。
         * @tparam FormatArgs 格式参数类型。
         * @param LogLevel 日志级别。
         * @param Format 格式字符串。
         * @param FormatArguments 格式参数。
         * @return 写入的字节数；级别低于控制台阈值时返回零。
         */
        template <typename... FormatArgs>
        auto ConsoleWriteFmt(const Level LogLevel, const std::string &Format, FormatArgs &&...FormatArguments) const
            -> Asio::awaitable<std::size_t>
        {
            std::string DataString = std::vformat(Format, std::make_format_args(std::forward<FormatArgs>(FormatArguments)...));
            co_return co_await ConsoleWrite(LogLevel, DataString);
        }

    private:
        /** @brief 创建日志对象时提供的 Asio 执行器。 */
        Asio::any_io_executor EventExecutor;
        /** @brief 串行化日志异步操作和状态访问的 strand。 */
        Asio::strand<Asio::any_io_executor> SerialExec;
        /** @brief 按文件路径缓存打开的文件句柄及大小。 */
        mutable std::unordered_map<std::string, Context> FileMap;

        // 配置项
        /** @brief 日志文件的根目录。 */
        Fs::path RootDirectory;
        /** @brief 格式化日志时间戳时使用的时区偏移。 */
        std::chrono::minutes TimeOffset{0ULL};
        /** @brief 最多保留的归档文件数；零表示不限制。 */
        std::size_t MaxArchiveCount = 0; // 0 表示不限制归档个数
        /** @brief 低于该级别的日志不写入文件。 */
        Level FileLevelThreshold = Level::Debug;
        /** @brief 低于该级别的日志不写入控制台。 */
        Level ConsoleLevelThreshold = Level::Debug;
        /** @brief 单个日志文件的最大字节数。 */
        std::size_t MaxFileSize = 10 * 1024 * 1024; // 10MB
    private:
        /**
         * @brief 构建时间戳字符串（使用实例级时间偏移）
         *
         * @return std::string 时间戳字符串，格式为 "[YYYY-MM-DD HH:MM]"
         */
        std::string TimestampString() const
        {
            auto Now = std::chrono::system_clock::now() + TimeOffset;
            auto Milliseconds = std::chrono::time_point_cast<std::chrono::milliseconds>(Now);
            // 提取秒数（整数部分）和毫秒数
            auto Seconds = std::chrono::time_point_cast<std::chrono::seconds>(Milliseconds);
            auto Millis = std::chrono::duration_cast<std::chrono::milliseconds>(Milliseconds - Seconds).count();

            return std::format("[{:%Y-%m-%d %H:%M:%S}.{:03d}]", Seconds, Millis);
        }

        /**
         * @brief 清理旧归档文件
         * @param TargetPath 当前日志文件路径，用于定位所属日志目录。
         */
        void CleanupOldArchives(const Fs::path &TargetPath) const
        {
            if (MaxArchiveCount == 0)
            {
                return;
            }
            std::error_code Ec;
            const Fs::path Parent = TargetPath.parent_path();
            if (!Fs::exists(Parent, Ec))
            {
                return;
            }
            const std::string Stem = TargetPath.stem().string();
            const std::string Ext = TargetPath.extension().string();
            const std::string Prefix = Stem + "-";
            std::vector<Fs::directory_entry> Archives;
            for (Fs::directory_iterator It(Parent, Ec), End; It != End && !Ec; ++It)
            {
                const auto &Entry = *It;
                if (!Entry.is_regular_file(Ec))
                {
                    continue;
                }
                if (std::string Name = Entry.path().filename().string(); Name.size() >= Prefix.size() + Ext.size() && Name.starts_with(Prefix) && Name.ends_with(Ext))
                {
                    Archives.push_back(Entry);
                }
            }
            if (Archives.size() <= MaxArchiveCount)
            {
                return;
            }
            auto ArchivesSorted = [](const Fs::directory_entry &A, const Fs::directory_entry &B)
            {
                return A.path().filename().string() < B.path().filename().string();
            };
            std::sort(Archives.begin(), Archives.end(), ArchivesSorted);
            const std::size_t RemoveCount = Archives.size() - MaxArchiveCount;
            for (std::size_t I = 0; I < RemoveCount; ++I)
            {
                Fs::remove(Archives[I], Ec);
            }
        }
    };
}
