#include <boost/asio.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include "Wan.hpp"
#include "Wan/Transport/Transmission.hpp"
#include "Wan/Transport/Reliable.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <list>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Net = boost::asio;
using Clock = std::chrono::steady_clock;

namespace
{
    constexpr std::size_t Repetitions = 5;
    std::uint64_t Sink = 0;

    struct ConstantHash
    {
        std::size_t operator()(int) const noexcept
        {
            return 1;
        }
    };

    void WriteRow(std::string_view Implementation,
        std::string_view Container, std::string_view Workload,
        std::string_view Operation, std::size_t Size,
        std::int64_t MedianNanoseconds)
    {
        std::printf("%.*s,%.*s,%.*s,%.*s,%zu,%lld\n",
            static_cast<int>(Implementation.size()), Implementation.data(),
            static_cast<int>(Container.size()), Container.data(),
            static_cast<int>(Workload.size()), Workload.data(),
            static_cast<int>(Operation.size()), Operation.data(), Size,
            static_cast<long long>(MedianNanoseconds));
    }

    template <typename Factory, typename Prepare, typename Operation>
    void Measure(std::string_view Implementation,
        std::string_view Container, std::string_view Workload,
        std::string_view OperationName, std::size_t Size,
        Factory MakeContainer, Prepare PrepareContainer,
        Operation RunOperation)
    {
        auto Run = [&](bool Timed)
        {
            auto Values = MakeContainer();
            PrepareContainer(Values);
            const auto Begin = Timed ? Clock::now() : Clock::time_point{};
            RunOperation(Values);
            if (!Timed)
            {
                return std::int64_t{0};
            }
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now() - Begin).count();
        };

        (void)Run(false);
        std::array<std::int64_t, Repetitions> Samples{};
        for (auto& Sample : Samples)
        {
            Sample = Run(true);
        }
        std::sort(Samples.begin(), Samples.end());
        WriteRow(Implementation, Container, Workload, OperationName,
            Size, Samples[Samples.size() / 2]);
    }

    auto MakeKeys(std::size_t Size, std::string_view Workload)
        -> std::vector<int>
    {
        std::vector<int> Keys(Size);
        std::iota(Keys.begin(), Keys.end(), 0);
        if (Workload == "reverse")
        {
            std::reverse(Keys.begin(), Keys.end());
        }
        else if (Workload == "random")
        {
            std::mt19937 Generator{0x57414E};
            std::shuffle(Keys.begin(), Keys.end(), Generator);
        }
        return Keys;
    }

    template <typename Factory, typename Insert, typename Find,
        typename Erase, typename Iterate>
    void BenchmarkAssociative(std::string_view Implementation,
        std::string_view Container, std::string_view Workload,
        const std::vector<int>& Keys, Factory MakeContainer,
        Insert InsertOne, Find FindOne, Erase EraseOne,
        Iterate IterateAll)
    {
        auto Empty = [](auto&) {};
        auto Fill = [&](auto& Values)
        {
            for (const auto Key : Keys)
            {
                InsertOne(Values, Key);
            }
        };
        Measure(Implementation, Container, Workload, "insert", Keys.size(),
            MakeContainer, Empty, [&](auto& Values)
            {
                for (const auto Key : Keys)
                {
                    InsertOne(Values, Key);
                }
            });
        Measure(Implementation, Container, Workload, "find", Keys.size(),
            MakeContainer, Fill, [&](auto& Values)
            {
                std::uint64_t Total = 0;
                for (const auto Key : Keys)
                {
                    Total += FindOne(Values, Key);
                }
                Sink ^= Total;
            });
        Measure(Implementation, Container, Workload, "iterate", Keys.size(),
            MakeContainer, Fill, [&](auto& Values)
            {
                Sink ^= IterateAll(Values);
            });
        Measure(Implementation, Container, Workload, "erase", Keys.size(),
            MakeContainer, Fill, [&](auto& Values)
            {
                if (!Keys.empty())
                {
                    EraseOne(Values, Keys[Keys.size() / 2]);
                }
            });
    }

    template <typename Factory, typename Insert, typename Find,
        typename Erase, typename Iterate>
    void RunMapSeries(std::string_view Implementation,
        std::string_view Container, std::string_view Workload,
        const std::vector<int>& Keys, Factory MakeContainer,
        Insert InsertOne, Find FindOne, Erase EraseOne,
        Iterate IterateAll)
    {
        BenchmarkAssociative(Implementation, Container, Workload, Keys,
            MakeContainer, InsertOne, FindOne, EraseOne, IterateAll);
    }

    void RunMapBenchmarks(std::size_t Size, std::string_view Workload,
        bool Collisions = false)
    {
        const auto Keys = MakeKeys(Size, Workload);
        const auto MapInsert = [](auto& Values, int Key)
        {
            (void)Values.Push(StandardCon::Pair<int, int>{Key, Key * 2});
        };
        const auto MapFind = [](auto& Values, int Key) -> std::uint64_t
        {
            auto It = Values.Find(Key);
            return It == Values.End() ? 0
                : static_cast<std::uint64_t>(It->Second + 1);
        };
        const auto MapErase = [](auto& Values, int Key)
        {
            (void)Values.Pop(Key);
        };
        const auto MapIterate = [](auto& Values) -> std::uint64_t
        {
            std::uint64_t Total = 0;
            for (auto It = Values.Begin(); It != Values.End(); ++It)
            {
                Total += static_cast<std::uint64_t>(It->First + It->Second);
            }
            return Total;
        };

        if (Collisions)
        {
            RunMapSeries("Wan", "HashMap", Workload, Keys,
                [] { return StandardCon::HashMap<int, int, ConstantHash>{}; },
                MapInsert, MapFind, MapErase, MapIterate);
            RunMapSeries("std", "unordered_map", Workload, Keys,
                [] { return std::unordered_map<int, int, ConstantHash>{}; },
                [](auto& Values, int Key) { Values.emplace(Key, Key * 2); },
                [](auto& Values, int Key) -> std::uint64_t
                {
                    const auto It = Values.find(Key);
                    return It == Values.end() ? 0
                        : static_cast<std::uint64_t>(It->second + 1);
                },
                [](auto& Values, int Key) { Values.erase(Key); },
                [](auto& Values) -> std::uint64_t
                {
                    std::uint64_t Total = 0;
                    for (const auto& [Key, Value] : Values)
                    {
                        Total += static_cast<std::uint64_t>(Key + Value);
                    }
                    return Total;
                });
            RunMapSeries("Boost", "unordered_flat_map", Workload, Keys,
                [] { return boost::unordered_flat_map<
                    int, int, ConstantHash>{}; },
                [](auto& Values, int Key) { Values.emplace(Key, Key * 2); },
                [](auto& Values, int Key) -> std::uint64_t
                {
                    const auto It = Values.find(Key);
                    return It == Values.end() ? 0
                        : static_cast<std::uint64_t>(It->second + 1);
                },
                [](auto& Values, int Key) { Values.erase(Key); },
                [](auto& Values) -> std::uint64_t
                {
                    std::uint64_t Total = 0;
                    for (const auto& [Key, Value] : Values)
                    {
                        Total += static_cast<std::uint64_t>(Key + Value);
                    }
                    return Total;
                });
            return;
        }

        RunMapSeries("Wan", "HashMap", Workload, Keys,
            [] { return StandardCon::HashMap<int, int, std::hash<int>>{}; },
            MapInsert, MapFind, MapErase, MapIterate);
        RunMapSeries("std", "unordered_map", Workload, Keys,
            [] { return std::unordered_map<int, int>{}; },
            [](auto& Values, int Key) { Values.emplace(Key, Key * 2); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                const auto It = Values.find(Key);
                return It == Values.end() ? 0
                    : static_cast<std::uint64_t>(It->second + 1);
            },
            [](auto& Values, int Key) { Values.erase(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (const auto& [Key, Value] : Values)
                {
                    Total += static_cast<std::uint64_t>(Key + Value);
                }
                return Total;
            });
        RunMapSeries("Boost", "unordered_flat_map", Workload, Keys,
            [] { return boost::unordered_flat_map<int, int>{}; },
            [](auto& Values, int Key) { Values.emplace(Key, Key * 2); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                const auto It = Values.find(Key);
                return It == Values.end() ? 0
                    : static_cast<std::uint64_t>(It->second + 1);
            },
            [](auto& Values, int Key) { Values.erase(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (const auto& [Key, Value] : Values)
                {
                    Total += static_cast<std::uint64_t>(Key + Value);
                }
                return Total;
            });

        const auto TreeInsert = [](auto& Values, int Key)
        {
            (void)Values.Push(StandardCon::Pair<int, int>{Key, Key * 2});
        };
        const auto TreeFind = [](auto& Values, int Key) -> std::uint64_t
        {
            auto It = Values.Find(Key);
            return It == Values.End() ? 0
                : static_cast<std::uint64_t>(It->Second + 1);
        };
        const auto TreeErase = [](auto& Values, int Key)
        {
            (void)Values.Pop(Key);
        };
        const auto TreeIterate = [](auto& Values) -> std::uint64_t
        {
            std::uint64_t Total = 0;
            for (auto It = Values.Begin(); It != Values.End(); ++It)
            {
                Total += static_cast<std::uint64_t>(It->First + It->Second);
            }
            return Total;
        };
        RunMapSeries("Wan", "TreeMap", Workload, Keys,
            [] { return StandardCon::TreeMap<int, int>{}; },
            TreeInsert, TreeFind, TreeErase, TreeIterate);
        RunMapSeries("Wan", "AVL", Workload, Keys,
            [] { return StandardCon::BalanceTree<int, int>{}; },
            [](auto& Values, int Key) { (void)Values.Push(Key, Key * 2); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                return Values.Find(Key) == nullptr ? 0 : 1;
            },
            [](auto& Values, int Key) { Values.Pop(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (auto It = Values.Begin(); It != Values.End(); ++It)
                {
                    Total += static_cast<std::uint64_t>(
                        It->First + It->Second);
                }
                return Total;
            });
        RunMapSeries("std", "map", Workload, Keys,
            [] { return std::map<int, int>{}; },
            [](auto& Values, int Key) { Values.emplace(Key, Key * 2); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                const auto It = Values.find(Key);
                return It == Values.end() ? 0
                    : static_cast<std::uint64_t>(It->second + 1);
            },
            [](auto& Values, int Key) { Values.erase(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (const auto& [Key, Value] : Values)
                {
                    Total += static_cast<std::uint64_t>(Key + Value);
                }
                return Total;
            });
        auto FlatMapKeys = Keys;
        auto FlatMapWorkload = Workload;
        if (Workload != "ascending" && Size > 1'000)
        {
            FlatMapKeys = MakeKeys(1'000, Workload);
            FlatMapWorkload = Workload == "reverse"
                ? "reverse_1k_capped" : "random_1k_capped";
        }
        RunMapSeries("Boost", "flat_map", FlatMapWorkload, FlatMapKeys,
            [] { return boost::container::flat_map<int, int>{}; },
            [](auto& Values, int Key) { Values.emplace(Key, Key * 2); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                const auto It = Values.find(Key);
                return It == Values.end() ? 0
                    : static_cast<std::uint64_t>(It->second + 1);
            },
            [](auto& Values, int Key) { Values.erase(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (const auto& [Key, Value] : Values)
                {
                    Total += static_cast<std::uint64_t>(Key + Value);
                }
                return Total;
            });
    }

    template <typename Factory, typename Push, typename Size,
        typename At, typename EraseMiddle>
    void RunVectorBenchmarks(std::string_view Implementation,
        std::string_view Container, std::string_view Workload,
        const std::vector<int>& Keys, Factory MakeContainer,
        Push PushBack, Size GetSize, At GetAt, EraseMiddle Erase)
    {
        auto NoPrepare = [](auto&) {};
        auto Fill = [&](auto& Values)
        {
            for (const auto Key : Keys)
            {
                PushBack(Values, Key);
            }
        };
        Measure(Implementation, Container, Workload, "insert", Keys.size(),
            MakeContainer, NoPrepare, Fill);
        Measure(Implementation, Container, Workload, "scan", Keys.size(),
            MakeContainer, Fill, [&](auto& Values)
            {
                std::uint64_t Total = 0;
                for (std::size_t I = 0; I < GetSize(Values); ++I)
                {
                    Total += static_cast<std::uint64_t>(GetAt(Values, I));
                }
                Sink ^= Total;
            });
        Measure(Implementation, Container, Workload, "erase_middle",
            Keys.size(), MakeContainer, Fill,
            [&](auto& Values) { Erase(Values, GetSize(Values) / 2); });
    }

    template <typename Factory, typename Push, typename Iterate,
        typename PopFront, typename EmptyCheck>
    void RunListBenchmarks(std::string_view Implementation,
        std::string_view Container, std::string_view Workload,
        const std::vector<int>& Keys, Factory MakeContainer,
        Push PushBack, Iterate IterateAll, PopFront Pop,
        EmptyCheck IsEmpty)
    {
        auto NoPrepare = [](auto&) {};
        auto Fill = [&](auto& Values)
        {
            for (const auto Key : Keys)
            {
                PushBack(Values, Key);
            }
        };
        Measure(Implementation, Container, Workload, "insert", Keys.size(),
            MakeContainer, NoPrepare, Fill);
        Measure(Implementation, Container, Workload, "iterate", Keys.size(),
            MakeContainer, Fill, [&](auto& Values)
            {
                Sink ^= IterateAll(Values);
            });
        Measure(Implementation, Container, Workload, "pop_front", Keys.size(),
            MakeContainer, Fill, [&](auto& Values)
            {
                while (!IsEmpty(Values))
                {
                    Sink += static_cast<std::uint64_t>(Pop(Values));
                }
            });
    }

    template <typename Factory, typename Push, typename Front,
        typename Pop, typename Empty>
    void RunQueueBenchmark(std::string_view Implementation,
        std::string_view Container, std::string_view Workload,
        const std::vector<int>& Keys, Factory MakeContainer,
        Push PushBack, Front ReadFront, Pop PopFront, Empty IsEmpty)
    {
        Measure(Implementation, Container, Workload, "push_pop", Keys.size(),
            MakeContainer, [](auto&) {}, [&](auto& Values)
            {
                for (const auto Key : Keys)
                {
                    PushBack(Values, Key);
                }
                std::uint64_t Total = 0;
                while (!IsEmpty(Values))
                {
                    Total += static_cast<std::uint64_t>(ReadFront(Values));
                    PopFront(Values);
                }
                Sink ^= Total;
            });
    }

    void RunSetBenchmarks(std::size_t Size, std::string_view Workload)
    {
        const auto Keys = MakeKeys(Size, Workload);
        BenchmarkAssociative("Wan", "TreeSet", Workload, Keys,
            [] { return StandardCon::TreeSet<int>{}; },
            [](auto& Values, int Key) { (void)Values.Push(Key); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                auto It = Values.Find(Key);
                return It == Values.End() ? 0
                    : static_cast<std::uint64_t>(*It + 1);
            },
            [](auto& Values, int Key) { (void)Values.Pop(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (auto It = Values.Begin(); It != Values.End(); ++It)
                {
                    Total += static_cast<std::uint64_t>(*It);
                }
                return Total;
            });
        BenchmarkAssociative("Wan", "HashSet", Workload, Keys,
            [] { return StandardCon::HashSet<int, std::hash<int>>{}; },
            [](auto& Values, int Key) { (void)Values.Push(Key); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                auto It = Values.Find(Key);
                return It == Values.End() ? 0
                    : static_cast<std::uint64_t>(*It + 1);
            },
            [](auto& Values, int Key) { (void)Values.Pop(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (auto It = Values.Begin(); It != Values.End(); ++It)
                {
                    Total += static_cast<std::uint64_t>(*It);
                }
                return Total;
            });
        BenchmarkAssociative("std", "set", Workload, Keys,
            [] { return std::set<int>{}; },
            [](auto& Values, int Key) { Values.insert(Key); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                const auto It = Values.find(Key);
                return It == Values.end() ? 0
                    : static_cast<std::uint64_t>(*It + 1);
            },
            [](auto& Values, int Key) { Values.erase(Key); },
            [](auto& Values) -> std::uint64_t
            {
                return static_cast<std::uint64_t>(
                    std::accumulate(Values.begin(), Values.end(), 0LL));
            });
        BenchmarkAssociative("std", "unordered_set", Workload, Keys,
            [] { return std::unordered_set<int>{}; },
            [](auto& Values, int Key) { Values.insert(Key); },
            [](auto& Values, int Key) -> std::uint64_t
            {
                const auto It = Values.find(Key);
                return It == Values.end() ? 0
                    : static_cast<std::uint64_t>(*It + 1);
            },
            [](auto& Values, int Key) { Values.erase(Key); },
            [](auto& Values) -> std::uint64_t
            {
                std::uint64_t Total = 0;
                for (const auto Value : Values)
                {
                    Total += static_cast<std::uint64_t>(Value);
                }
                return Total;
            });
    }

    template <bool Reliable>
    auto EchoServer(Net::ip::tcp::acceptor& Acceptor, std::size_t ByteCount)
        -> Net::awaitable<void>
    {
        auto Socket = co_await Acceptor.async_accept(Net::use_awaitable);
        std::vector<std::byte> Buffer(ByteCount);
        if constexpr (Reliable)
        {
            Wan::Transport::Reliable Stream{std::move(Socket)};
            std::error_code Ec;
            const auto Read = co_await Wan::Transport::AsyncRead(Stream,
                std::span<std::byte>{Buffer}, Ec);
            if (Ec || Read != ByteCount)
            {
                throw std::runtime_error("Reliable benchmark read failed");
            }
            const auto Written = co_await Wan::Transport::AsyncWrite(Stream,
                std::span<const std::byte>{Buffer}, Ec);
            if (Ec || Written != ByteCount)
            {
                throw std::runtime_error("Reliable benchmark write failed");
            }
        }
        else
        {
            (void)co_await Net::async_read(Socket, Net::buffer(Buffer),
                Net::use_awaitable);
            (void)co_await Net::async_write(Socket, Net::buffer(Buffer),
                Net::use_awaitable);
        }
    }

    template <bool Reliable>
    auto EchoClient(Net::io_context& Ioc, Net::ip::tcp::endpoint Endpoint,
        std::size_t ByteCount) -> Net::awaitable<void>
    {
        Net::ip::tcp::socket Socket{Ioc};
        co_await Socket.async_connect(Endpoint, Net::use_awaitable);
        std::vector<std::byte> SendBuffer(ByteCount, std::byte{0x5a});
        std::vector<std::byte> ReceiveBuffer(ByteCount);
        if constexpr (Reliable)
        {
            Wan::Transport::Reliable Stream{std::move(Socket)};
            std::error_code Ec;
            const auto Written = co_await Wan::Transport::AsyncWrite(Stream,
                std::span<const std::byte>{SendBuffer}, Ec);
            const auto Read = co_await Wan::Transport::AsyncRead(Stream,
                std::span<std::byte>{ReceiveBuffer}, Ec);
            if (Ec || Written != ByteCount || Read != ByteCount)
            {
                throw std::runtime_error("Reliable benchmark round trip failed");
            }
        }
        else
        {
            (void)co_await Net::async_write(Socket,
                Net::buffer(SendBuffer), Net::use_awaitable);
            (void)co_await Net::async_read(Socket,
                Net::buffer(ReceiveBuffer), Net::use_awaitable);
        }
        if (SendBuffer != ReceiveBuffer)
        {
            throw std::runtime_error("loopback benchmark payload mismatch");
        }
    }

    template <bool Reliable>
    auto IoSample(std::size_t ByteCount) -> std::int64_t
    {
        Net::io_context Ioc;
        Net::ip::tcp::acceptor Acceptor{Ioc,
            {Net::ip::make_address_v4("127.0.0.1"), 0}};
        const auto Endpoint = Acceptor.local_endpoint();
        auto Server = Net::co_spawn(Ioc,
            EchoServer<Reliable>(Acceptor, ByteCount), Net::use_future);
        auto Client = Net::co_spawn(Ioc,
            EchoClient<Reliable>(Ioc, Endpoint, ByteCount), Net::use_future);
        const auto Begin = Clock::now();
        Ioc.run();
        Server.get();
        Client.get();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - Begin).count();
    }

    template <bool Reliable>
    void RunIoBenchmark(std::string_view Implementation,
        std::size_t ByteCount)
    {
        (void)IoSample<Reliable>(ByteCount);
        std::array<std::int64_t, Repetitions> Samples{};
        for (auto& Sample : Samples)
        {
            Sample = IoSample<Reliable>(ByteCount);
        }
        std::sort(Samples.begin(), Samples.end());
        WriteRow(Implementation, "tcp_loopback", "fixed_payload",
            "echo_roundtrip", ByteCount, Samples[Samples.size() / 2]);
    }
}

auto main() -> int
{
    std::puts("implementation,container,workload,operation,size,median_ns");
    for (const auto Size : {std::size_t{1'000}, std::size_t{10'000},
             std::size_t{100'000}})
    {
        for (const auto Workload : {std::string_view{"ascending"},
                 std::string_view{"reverse"}, std::string_view{"random"}})
        {
            const auto Keys = MakeKeys(Size, Workload);
            RunVectorBenchmarks("Wan", "Vector", Workload, Keys,
                [] { return StandardCon::Vector<int>{}; },
                [](auto& Values, int Key) { Values.PushBack(Key); },
                [](auto& Values) { return Values.Size(); },
                [](auto& Values, std::size_t Index) { return Values[Index]; },
                [](auto& Values, std::size_t Index)
                {
                    (void)Values.Erase(Values.Begin() + Index);
                });
            RunVectorBenchmarks("std", "vector", Workload, Keys,
                [] { return std::vector<int>{}; },
                [](auto& Values, int Key) { Values.push_back(Key); },
                [](auto& Values) { return Values.size(); },
                [](auto& Values, std::size_t Index) { return Values[Index]; },
                [](auto& Values, std::size_t Index)
                {
                    Values.erase(Values.begin()
                        + static_cast<std::ptrdiff_t>(Index));
                });

            RunListBenchmarks("Wan", "List", Workload, Keys,
                [] { return StandardCon::List<int>{}; },
                [](auto& Values, int Key) { Values.PushBack(Key); },
                [](auto& Values)
                {
                    std::uint64_t Total = 0;
                    for (auto It = Values.Begin(); It != Values.End(); ++It)
                    {
                        Total += static_cast<std::uint64_t>(*It);
                    }
                    return Total;
                },
                [](auto& Values)
                {
                    const auto Front = Values.Front();
                    Values.PopFront();
                    return Front;
                },
                [](auto& Values) { return Values.Empty(); });
            RunListBenchmarks("std", "list", Workload, Keys,
                [] { return std::list<int>{}; },
                [](auto& Values, int Key) { Values.push_back(Key); },
                [](auto& Values)
                {
                    std::uint64_t Total = 0;
                    for (const auto Value : Values)
                    {
                        Total += static_cast<std::uint64_t>(Value);
                    }
                    return Total;
                },
                [](auto& Values)
                {
                    const auto Front = Values.front();
                    Values.pop_front();
                    return Front;
                },
                [](auto& Values) { return Values.empty(); });

            RunQueueBenchmark("Wan", "Queue", Workload, Keys,
                [] { return StandardCon::Queue<int>{}; },
                [](auto& Values, int Key) { Values.Push(Key); },
                [](auto& Values) { return Values.Front(); },
                [](auto& Values) { Values.Pop(); },
                [](auto& Values) { return Values.Empty(); });
            RunQueueBenchmark("std", "queue", Workload, Keys,
                [] { return std::queue<int>{}; },
                [](auto& Values, int Key) { Values.push(Key); },
                [](auto& Values) { return Values.front(); },
                [](auto& Values) { Values.pop(); },
                [](auto& Values) { return Values.empty(); });
            RunQueueBenchmark("std", "deque", Workload, Keys,
                [] { return std::deque<int>{}; },
                [](auto& Values, int Key) { Values.push_back(Key); },
                [](auto& Values) { return Values.front(); },
                [](auto& Values) { Values.pop_front(); },
                [](auto& Values) { return Values.empty(); });

            RunMapBenchmarks(Size, Workload);
            RunSetBenchmarks(Size, Workload);
        }
    }

    const auto CollisionKeys = MakeKeys(1'000, "ascending");
    RunMapBenchmarks(CollisionKeys.size(), "colliding", true);

    RunIoBenchmark<false>("Asio", 64 * 1024);
    RunIoBenchmark<true>("Wan", 64 * 1024);
    std::fprintf(stderr, "benchmark_sink=%llu\n",
        static_cast<unsigned long long>(Sink));
    return 0;
}
