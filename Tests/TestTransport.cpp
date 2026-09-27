/**
 * @file TestTransport.cpp
 * @brief 传输层测试
 * @details 测试 Reliable TCP 传输层的基础功能：
 *          构造、echo 回环、Close/Cancel、LowestLayer 导航、错误码映射。
 *          注意：在 MSYS2 UCRT64 + GCC 15.2 环境下，客户端协程须使用
 *          ioc.GetExecutor() 构造 Socket，而非 co_await this_coro::Executor，
 *          否则 async_connect 可能挂起（IOCP 兼容问题）。
 * @author Hatedatastructures
 * @date 2026-05-25
 */

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

#include "../Wan/Transport/Transmission.hpp"
#include "../Wan/Transport/Reliable.hpp"

namespace Net = boost::asio;
using namespace Wan::Transport;

class ScriptedTransmission final : public Transmission
{
public:
    using Transmission::AsyncReadSome;
    using Transmission::AsyncWriteSome;

    ScriptedTransmission(ExecutorType ExecutorValue,
        std::size_t MaximumChunk, bool Overreport = false)
        : MemberExecutor(std::move(ExecutorValue))
        , MemberMaximumChunk(MaximumChunk)
        , MemberOverreport(Overreport)
    {
    }

    [[nodiscard]] auto Executor() const -> ExecutorType override
    {
        return MemberExecutor;
    }

    auto AsyncReadSome(std::span<std::byte> Buffer, std::error_code& Ec)
        -> Net::awaitable<std::size_t> override
    {
        Ec.clear();
        co_return MemberOverreport
            ? Buffer.size() + 1
            : std::min(Buffer.size(), MemberMaximumChunk);
    }

    auto AsyncWriteSome(std::span<const std::byte> Buffer,
        std::error_code& Ec) -> Net::awaitable<std::size_t> override
    {
        Ec.clear();
        co_return MemberOverreport
            ? Buffer.size() + 1
            : std::min(Buffer.size(), MemberMaximumChunk);
    }

    void Close() override {}
    void Cancel() override {}

private:
    ExecutorType MemberExecutor;
    std::size_t MemberMaximumChunk;
    bool MemberOverreport;
};

static auto TestConstructFromExecutor() -> bool
{
    Net::io_context Ioc;
    auto T = MakeReliable(Ioc.get_executor());
    if (!T) { std::puts("FAIL: construct_from_executor - null"); return false; }
    if (T->TransportType() != Transmission::Type::Tcp)
    { std::puts("FAIL: construct_from_executor - wrong type"); return false; }
    std::puts("PASS: construct_from_executor");
    return true;
}

static auto TestConstructFromSocket() -> bool
{
    Net::io_context Ioc;
    Net::ip::tcp::socket Sock{Ioc};
    auto T = MakeReliable(std::move(Sock));
    if (!T) { std::puts("FAIL: construct_from_socket - null"); return false; }
    std::puts("PASS: construct_from_socket");
    return true;
}

static auto TestNextLayerIsNull() -> bool
{
    Net::io_context Ioc;
    Reliable R{Ioc.get_executor()};
    if (R.NextLayer() != nullptr)
    { std::puts("FAIL: next_layer_is_null"); return false; }
    std::puts("PASS: next_layer_is_null");
    return true;
}

static auto TestCloseAndCancel() -> bool
{
    Net::io_context Ioc;
    Reliable R{Ioc.get_executor()};
    R.Close();
    R.Cancel();
    std::puts("PASS: close_and_cancel");
    return true;
}

static auto TestReleaseSocket() -> bool
{
    Net::io_context Ioc;
    Net::ip::tcp::socket Sock{Ioc};
    Reliable R{std::move(Sock)};

    auto Released = R.ReleaseSocket();
    if (!Released.has_value())
    { std::puts("FAIL: release_socket - first release empty"); return false; }

    auto Released2 = R.ReleaseSocket();
    if (Released2.has_value())
    { std::puts("FAIL: release_socket - second release not empty"); return false; }

    R.Cancel();
    R.Close();

    std::puts("PASS: release_socket");
    return true;
}

static auto TestLowestLayer() -> bool
{
    Net::io_context Ioc;
    auto T = MakeReliable(Ioc.get_executor());

    auto* Bottom = T->LowestLayer<Reliable>();
    if (!Bottom) { std::puts("FAIL: lowest_layer - null"); return false; }
    if (Bottom != T.get())
    { std::puts("FAIL: lowest_layer - wrong pointer"); return false; }

    std::puts("PASS: lowest_layer");
    return true;
}

static auto TestExecutorMatches() -> bool
{
    Net::io_context Ioc;
    Reliable R{Ioc.get_executor()};
    if (!(R.Executor() == R.GetExecutor()))
    { std::puts("FAIL: executor_matches"); return false; }
    std::puts("PASS: executor_matches");
    return true;
}

static auto TestCompletionHandlerRejectsUnmanagedTransport() -> bool
{
    class BareTransmission final : public Transmission
    {
    public:
        using Transmission::AsyncReadSome;
        using Transmission::AsyncWriteSome;

        explicit BareTransmission(ExecutorType ExecutorValue)
            : MemberExecutor(ExecutorValue)
        {
        }

        [[nodiscard]] auto Executor() const -> ExecutorType override
        {
            return MemberExecutor;
        }

        auto AsyncReadSome(std::span<std::byte>, std::error_code& Ec)
            -> Net::awaitable<std::size_t> override
        {
            Ec.clear();
            co_return 0;
        }

        auto AsyncWriteSome(std::span<const std::byte>, std::error_code& Ec)
            -> Net::awaitable<std::size_t> override
        {
            Ec.clear();
            co_return 0;
        }

        void Close() override {}
        void Cancel() override {}

    private:
        ExecutorType MemberExecutor;
    };

    Net::io_context Ioc;
    BareTransmission R{Ioc.get_executor()};
    std::array<std::byte, 1> Buffer{};
    bool Called = false;
    boost::system::error_code ReceivedEc;
    R.AsyncReadSome(std::span<std::byte>(Buffer),
        [&](boost::system::error_code Ec, std::size_t) {
            Called = true;
            ReceivedEc = Ec;
        });

    if (!Called || ReceivedEc != Net::error::operation_aborted)
    {
        std::puts("FAIL: unmanaged_completion_handler");
        return false;
    }
    std::puts("PASS: unmanaged_completion_handler");
    return true;
}

static auto TestCompleteOperationsValidateByteCounts() -> bool
{
    Net::io_context Ioc;
    auto T = std::make_shared<ScriptedTransmission>(Ioc.get_executor(), 2);
    std::array<std::byte, 5> ReadBuffer{};
    std::array<std::byte, 5> WriteBuffer{};
    std::error_code ReadEc;
    std::error_code WriteEc;
    auto Read = Net::co_spawn(Ioc,
        AsyncRead(*T, std::span<std::byte>{ReadBuffer}, ReadEc),
        Net::use_future);
    auto Write = Net::co_spawn(Ioc,
        AsyncWrite(*T, std::span<const std::byte>{WriteBuffer}, WriteEc),
        Net::use_future);
    Ioc.run();
    if (Read.get() != ReadBuffer.size() || ReadEc
        || Write.get() != WriteBuffer.size() || WriteEc)
    {
        std::puts("FAIL: complete_operations_short_counts");
        return false;
    }

    Net::io_context ReadIoc;
    auto OversizedRead = std::make_shared<ScriptedTransmission>(
        ReadIoc.get_executor(), 0, true);
    std::array<std::byte, 4> SmallReadBuffer{};
    std::error_code OversizedReadEc;
    auto BadRead = Net::co_spawn(ReadIoc,
        AsyncRead(*OversizedRead,
            std::span<std::byte>{SmallReadBuffer}, OversizedReadEc),
        Net::use_future);
    ReadIoc.run();

    Net::io_context WriteIoc;
    auto OversizedWrite = std::make_shared<ScriptedTransmission>(
        WriteIoc.get_executor(), 0, true);
    std::array<std::byte, 4> SmallWriteBuffer{};
    std::error_code OversizedWriteEc;
    auto BadWrite = Net::co_spawn(WriteIoc,
        AsyncWrite(*OversizedWrite,
            std::span<const std::byte>{SmallWriteBuffer}, OversizedWriteEc),
        Net::use_future);
    WriteIoc.run();

    const auto MessageSize = std::make_error_code(std::errc::message_size);
    if (BadRead.get() != 0 || OversizedReadEc != MessageSize
        || BadWrite.get() != 0 || OversizedWriteEc != MessageSize)
    {
        std::puts("FAIL: complete_operations_oversized_counts");
        return false;
    }
    std::puts("PASS: complete_operations_validate_byte_counts");
    return true;
}

static auto TestCompletionHandlerExceptionIsNotRetried() -> bool
{
    Net::io_context Ioc;
    auto T = std::make_shared<ScriptedTransmission>(Ioc.get_executor(), 1);
    std::array<std::byte, 1> Buffer{};
    std::size_t Calls = 0;
    bool UserExceptionEscaped = false;
    bool UnexpectedExceptionEscaped = false;
    try
    {
        T->AsyncReadSome(std::span<std::byte>{Buffer},
            Net::any_completion_handler<void(
                boost::system::error_code, std::size_t)>{
                [&Calls](boost::system::error_code, std::size_t)
                {
                    ++Calls;
                    throw std::runtime_error("handler failure");
                }});
        Ioc.run();
    }
    catch (const std::runtime_error& Error)
    {
        UserExceptionEscaped = Error.what() == std::string_view{
            "handler failure"};
    }
    catch (...)
    {
        UnexpectedExceptionEscaped = true;
    }

    if (Calls != 1 || !UserExceptionEscaped || UnexpectedExceptionEscaped)
    {
        std::puts("FAIL: completion_handler_exception");
        return false;
    }
    std::puts("PASS: completion_handler_exception_is_not_retried");
    return true;
}

static auto TestEchoWithReliable() -> bool
{
    Net::io_context Ioc;
    Net::ip::tcp::acceptor Acc{Ioc, Net::ip::tcp::endpoint(
        Net::ip::tcp::v4(), 0)};
    auto Ep = Acc.local_endpoint();

    bool ServerOk = false;
    bool ClientOk = false;

    // 服务端：accept → read → write Back
    Net::co_spawn(Ioc,
        [&Acc, &ServerOk]() -> Net::awaitable<void>
        {
            auto Sock = co_await Acc.async_accept(Net::use_awaitable);
            Reliable Conn{std::move(Sock)};

            std::array<std::byte, 256> Buf{};
            std::error_code Ec;
            const auto N = co_await Conn.AsyncReadSome(
                std::span<std::byte>{Buf.data(), Buf.size()}, Ec);
            if (Ec) { co_return; }

            co_await AsyncWrite(Conn,
                std::span<const std::byte>{Buf.data(), N}, Ec);
            if (Ec) { co_return; }

            ServerOk = (N == 5);
            Conn.Close();
        },
        Net::detached);

    // 客户端：connect → write → read
    Net::co_spawn(Ioc,
        [&Ioc, Ep, &ClientOk]() -> Net::awaitable<void>
        {
            // 使用 ioc.GetExecutor() 构造 Socket，
            // 避免 this_coro::Executor 在 IOCP 下的挂起问题
            Net::ip::tcp::socket Sock{Ioc.get_executor()};
            co_await Sock.async_connect(
                Net::ip::tcp::endpoint{
                    Net::ip::make_address_v4("127.0.0.1"), Ep.port()},
                Net::use_awaitable);
            Reliable Conn{std::move(Sock)};

            const char Msg[] = "hello";
            std::error_code Ec;
            co_await AsyncWrite(Conn,
                std::span<const std::byte>{
                    reinterpret_cast<const std::byte*>(Msg), 5},
                Ec);
            if (Ec) { co_return; }

            std::array<std::byte, 64> Buf{};
            const auto N = co_await Conn.AsyncReadSome(
                std::span<std::byte>{Buf.data(), Buf.size()}, Ec);
            if (Ec) { co_return; }

            ClientOk = (N == 5)
                && (std::memcmp(Buf.data(), Msg, 5) == 0);
            Conn.Close();
        },
        Net::detached);

    Ioc.run();

    if (ServerOk && ClientOk)
    {
        std::puts("PASS: echo_with_reliable");
        return true;
    }
    std::printf("FAIL: echo_with_reliable server=%d client=%d\n",
        ServerOk, ClientOk);
    return false;
}

static auto TestEofMapping() -> bool
{
    Net::io_context Ioc;

    Net::ip::tcp::acceptor Acc{Ioc, Net::ip::tcp::endpoint(
        Net::ip::tcp::v4(), 0)};
    auto Ep = Acc.local_endpoint();

    bool GotEof = false;

    Net::co_spawn(Ioc,
        [&Acc]() -> Net::awaitable<void>
        {
            auto Sock = co_await Acc.async_accept(Net::use_awaitable);
            Sock.close();
        },
        Net::detached);

    Net::co_spawn(Ioc,
        [&Ioc, Ep, &GotEof]() -> Net::awaitable<void>
        {
            Net::ip::tcp::socket Sock{Ioc.get_executor()};
            co_await Sock.async_connect(
                Net::ip::tcp::endpoint{
                    Net::ip::make_address_v4("127.0.0.1"), Ep.port()},
                Net::use_awaitable);
            Reliable Conn{std::move(Sock)};

            std::array<std::byte, 64> Buf{};
            std::error_code Ec;
            co_await Conn.AsyncReadSome(
                std::span<std::byte>{Buf.data(), Buf.size()}, Ec);
            GotEof = (Ec == Wan::Fault::Code::Eof);
        },
        Net::detached);

    Ioc.run();

    std::puts(GotEof ? "PASS: eof_mapping" : "FAIL: eof_mapping");
    return GotEof;
}

auto main() -> int // NOLINT(bugprone-exception-escape)
{
    int Failed = 0;

    if (!TestConstructFromExecutor()) ++Failed;
    if (!TestConstructFromSocket()) ++Failed;
    if (!TestNextLayerIsNull()) ++Failed;
    if (!TestCloseAndCancel()) ++Failed;
    if (!TestReleaseSocket()) ++Failed;
    if (!TestLowestLayer()) ++Failed;
    if (!TestExecutorMatches()) ++Failed;
    if (!TestCompletionHandlerRejectsUnmanagedTransport()) ++Failed;
    if (!TestCompleteOperationsValidateByteCounts()) ++Failed;
    if (!TestCompletionHandlerExceptionIsNotRetried()) ++Failed;
    if (!TestEchoWithReliable()) ++Failed;
    if (!TestEofMapping()) ++Failed;

    std::printf("\n%d/%d tests passed\n", 12 - Failed, 12);
    return Failed;
}
