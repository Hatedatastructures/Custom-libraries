// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE StressTests

#include <boost/test/included/unit_test.hpp>

#include "../Include/Wan.hpp"

#include <atomic>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

BOOST_AUTO_TEST_CASE(VectorHandlesHighVolumeAppendAndRead)
{
    constexpr std::size_t Count = 100'000;
    StandardCon::Vector<std::size_t> Values;
    for (std::size_t I = 0; I < Count; ++I)
    {
        Values.PushBack(I);
    }

    BOOST_REQUIRE_EQUAL(Values.Size(), Count);
    BOOST_TEST(Values[0] == 0);
    BOOST_TEST(Values[Count / 2] == Count / 2);
    BOOST_TEST(Values[Count - 1] == Count - 1);
}

BOOST_AUTO_TEST_CASE(HashSetHandlesRehashAndDeletion)
{
    constexpr int Count = 5'000;
    StandardCon::HashSet<int> Values;

    for (int I = 0; I < Count; ++I)
    {
        BOOST_REQUIRE(Values.Push(I));
    }
    BOOST_TEST(Values.Size() == static_cast<std::uint64_t>(Count));

    for (int I = 0; I < Count; I += 2)
    {
        BOOST_REQUIRE(Values.Pop(I));
    }
    BOOST_TEST(Values.Size() == static_cast<std::uint64_t>(Count / 2));

    for (int I = 1; I < Count; I += 2)
    {
        BOOST_REQUIRE(Values.Find(I).GetNode() != nullptr);
    }
}

BOOST_AUTO_TEST_CASE(RedBlackTreeHandlesBulkInsertAndDelete)
{
    constexpr int Count = 2'000;
    StandardCon::TreeSet<int> Values;
    for (int I = 0; I < Count; ++I)
    {
        BOOST_REQUIRE(Values.Push(I).Second);
    }
    BOOST_TEST(Values.Size() == static_cast<std::uint64_t>(Count));

    for (int I = Count - 1; I >= 0; --I)
    {
        BOOST_REQUIRE(Values.Pop(I).Second);
    }
    BOOST_TEST(Values.Empty());
}

BOOST_AUTO_TEST_CASE(AvlTreeHandlesBulkInsertAndDelete)
{
    constexpr int Count = 1'000;
    StandardCon::BalanceTree<int, int> Values;
    for (int I = 0; I < Count; ++I)
    {
        BOOST_REQUIRE(Values.Push(I, I * 2));
    }
    BOOST_TEST(Values.Size() == static_cast<std::uint64_t>(Count));

    for (int I = 0; I < Count; I += 2)
    {
        Values.Pop(I);
    }
    BOOST_TEST(Values.Size() == static_cast<std::uint64_t>(Count / 2));
}

BOOST_AUTO_TEST_CASE(SessionStoreIsSafeUnderConcurrentCreation)
{
    Wan::Web::SessionStore Store;
    std::mutex Mutex;
    std::unordered_set<std::string> Ids;
    std::atomic<int> Completed{0};
    std::atomic<int> Failures{0};

    std::vector<std::thread> Workers;
    for (int Worker = 0; Worker < 8; ++Worker)
    {
        Workers.emplace_back([&]() {
            for (int I = 0; I < 100; ++I)
            {
                auto Session = Store.Create(std::chrono::seconds{60});
                Store.Set(Session.Id, "worker", "active");
                auto Current = Store.Get(Session.Id);
                if (!Current.has_value()
                    || Current->Values.at("worker") != "active")
                {
                    Failures.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                {
                    std::lock_guard Lock{Mutex};
                    Ids.insert(Session.Id);
                }
                Completed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& Worker : Workers)
    {
        Worker.join();
    }

    BOOST_TEST(Completed.load() == 800);
    BOOST_TEST(Failures.load() == 0);
    BOOST_TEST(Ids.size() == 800);
}

BOOST_AUTO_TEST_CASE(SessionStoreEnforcesResourceBounds)
{
    Wan::Web::SessionConfig Config;
    Config.MaxSessions = 2;
    Config.MaxValuesPerSession = 1;
    Config.MaxKeySize = 4;
    Config.MaxValueSize = 8;
    Wan::Web::SessionStore Store{Config};

    auto First = Store.Create(std::chrono::seconds{1});
    Store.Set(First.Id, "key", "value");
    BOOST_CHECK_THROW(Store.Set(First.Id, "more", "value"),
        std::length_error);
    BOOST_CHECK_THROW(Store.Set(First.Id, "key", std::string(9, 'x')),
        std::length_error);

    auto Second = Store.Create(std::chrono::seconds{60});
    BOOST_CHECK_THROW((void)Store.Create(std::chrono::seconds{60}),
        std::length_error);

    std::this_thread::sleep_for(std::chrono::milliseconds{1100});
    auto Reused = Store.Create(std::chrono::seconds{60});
    BOOST_TEST(!Reused.Id.empty());
    BOOST_TEST(!Store.Get(First.Id).has_value());
    BOOST_TEST(Store.Get(Second.Id).has_value());

    Store.Destroy(Second.Id);
    BOOST_CHECK_THROW((void)Store.Create(std::chrono::seconds{0}),
        std::invalid_argument);
    BOOST_CHECK_THROW((void)Store.Create(std::chrono::seconds::max()),
        std::invalid_argument);

    Config.MaxSessions = 0;
    BOOST_CHECK_THROW((void)Wan::Web::SessionStore{Config},
        std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(RouterHandlesBatchMatching)
{
    Wan::Web::Router Router;
    for (int I = 0; I < 1'000; ++I)
    {
        Router.Get("/items/" + std::to_string(I),
            [](Wan::Web::Context&) -> boost::asio::awaitable<void> {
                co_return;
            });
    }

    for (int I = 0; I < 1'000; ++I)
    {
        BOOST_REQUIRE(Router.Match(
            boost::beast::http::verb::get,
            "/items/" + std::to_string(I)).has_value());
    }
}

BOOST_AUTO_TEST_CASE(MalformedInputsAreRejectedWithoutCrashing)
{
    std::mt19937 Generator{0xC0FFEEu}; // NOLINT(bugprone-random-generator-seed)
    std::uniform_int_distribution<int> Length{0, 256};
    std::uniform_int_distribution<int> Byte{0, 255};
    int Processed = 0;

    for (int I = 0; I < 2'000; ++I)
    {
        std::string Data(static_cast<std::size_t>(Length(Generator)), '\0');
        for (auto& Character : Data)
        {
            Character = static_cast<char>(Byte(Generator));
        }

        Protocol::RequestHeader RequestHeader;
        Protocol::ResponseHeader ResponseHeader;
        Wan::Network::Http::Request<> HttpRequest;
        (void)RequestHeader.FromString(Data);
        (void)ResponseHeader.FromString(Data);
        (void)HttpRequest.FromString(Data);

        const auto Encoded = Encryption::ToBase64(Data);
        try
        {
            (void)Encryption::ArcaneSymmetric::AesGcmDecrypt(
                Encoded, "fuzz-pass");
        }
        catch (...)
        {
            (void)0;
        }
        try
        {
            (void)Encryption::ArcaneSymmetric::AesCtrDecrypt(
                Encoded, "fuzz-pass");
        }
        catch (...)
        {
            (void)0;
        }
        try
        {
            (void)Encryption::ArcaneSymmetric::TwofishCtrDecrypt(
                Encoded, "fuzz-pass");
        }
        catch (...)
        {
            (void)0;
        }
        ++Processed;
    }
    BOOST_TEST(Processed == 2'000);
}
