// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE PublicApiTests

#include <boost/test/included/unit_test.hpp>

#include "../Include/Wan.hpp"

#include <chrono>
#include <atomic>
#include <memory>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

BOOST_AUTO_TEST_CASE(PublicHeaderCompiles)
{
    Wan::Sched::Pool Pool{1};
    Pool.Stop();
    Pool.Join();
    Wan::Web::HttpClient Client{boost::asio::system_executor{}};
    auto Operation = Client.Get("127.0.0.1", "1", "/");
    (void)Operation;

    Wan::Web::HttpClientConfig ClientConfig;
    ClientConfig.MaxResponseHeaderSize = 1024;
    ClientConfig.MaxResponseBodySize = 4096;
    Wan::Web::HttpClient ConfiguredClient{
        boost::asio::system_executor{}, ClientConfig};
    (void)ConfiguredClient;

    ClientConfig.Timeout = std::chrono::seconds{0};
    BOOST_CHECK_THROW((void)(Wan::Web::HttpClient{
        boost::asio::system_executor{}, ClientConfig}),
        std::invalid_argument);
    ClientConfig.Timeout = std::chrono::seconds{30};
    ClientConfig.MaxResponseHeaderSize = 0;
    BOOST_CHECK_THROW((void)(Wan::Web::HttpClient{
        boost::asio::system_executor{}, ClientConfig}),
        std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(ExecutorLifecycleIsSingleStartAndReportsWorkerFailures)
{
    Wan::Web::Executor Executor{1};
    Executor.Run();
    BOOST_CHECK_THROW(Executor.Run(), std::logic_error);
    Executor.Stop();
    Executor.Join();
    BOOST_CHECK_THROW(Executor.Run(), std::logic_error);

    Wan::Web::Executor FailingExecutor{1};
    boost::asio::post(FailingExecutor.Ioc(), []
    {
        throw std::runtime_error("worker failure");
    });
    FailingExecutor.Run();
    BOOST_CHECK_THROW(FailingExecutor.Join(), std::runtime_error);

    Wan::Web::Executor CoroutineFailureExecutor{1};
    CoroutineFailureExecutor.Spawn(
        []() -> boost::asio::awaitable<void>
        {
            throw std::runtime_error("spawned coroutine failure");
            co_return;
        });
    CoroutineFailureExecutor.Run();
    BOOST_CHECK_THROW(CoroutineFailureExecutor.Join(), std::runtime_error);

    Wan::Web::Executor OwnedExecutor{1};
    boost::asio::io_context ExternalIoc;
    BOOST_CHECK_THROW(OwnedExecutor.Spawn(ExternalIoc.get_executor(),
        []() -> boost::asio::awaitable<void>
        {
            co_return;
        }), std::invalid_argument);

    Wan::Web::Executor DrainExecutor{1};
    int DrainedHandlerCalls = 0;
    boost::asio::post(DrainExecutor.Ioc(), [&DrainedHandlerCalls]
    {
        ++DrainedHandlerCalls;
    });
    DrainExecutor.Stop();
    DrainExecutor.Join();
    DrainExecutor.DrainStopped();
    BOOST_TEST(DrainedHandlerCalls == 1);
}

BOOST_AUTO_TEST_CASE(JsonDocumentPublicHeaderCompiles)
{
    std::error_code Ec;
    auto Document = Wan::Json::Document::FromString(
        R"({"count":7,"name":"wan"})", Ec);
    BOOST_TEST(!Ec);
    BOOST_TEST(Document.Get<int>("count", 0) == 7);
    BOOST_TEST(Document.Get<int>("name", 42) == 42);
    Document.Set("enabled", true);
    BOOST_TEST(Document.Get<bool>("enabled", false));

    auto Invalid = Wan::Json::Document::FromString("{", Ec);
    BOOST_TEST(Ec == std::make_error_code(std::errc::invalid_argument));
    BOOST_TEST(Invalid.Get<int>("count", 42) == 42);
}

BOOST_AUTO_TEST_CASE(PairConstructorPreservesValues)
{
    StandardCon::Pair<int, std::string> Value{7, "wan"};

    BOOST_TEST(Value.First == 7);
    BOOST_TEST(Value.Second == "wan");
}

BOOST_AUTO_TEST_CASE(AlgorithmCopyReturnsPastTheCopiedRange)
{
    const int Source[] = {1, 2, 3};
    int Target[] = {0, 0, 0};
    const auto End = StandardCon::Algorithm::Copy(
        Source, Source + 3, Target);

    BOOST_TEST(End == Target + 3);
    BOOST_TEST(Target[0] == 1);
    BOOST_TEST(Target[2] == 3);
}

BOOST_AUTO_TEST_CASE(StringHashDistinguishesReorderedBytes)
{
    StandardCon::HashImitationFunctions Hash;
    const StandardCon::String First{"ab"};
    const StandardCon::String Second{"ba"};

    BOOST_TEST(Hash(First) != Hash(Second));
}

BOOST_AUTO_TEST_CASE(VectorSizeAdjustAndErase)
{
    StandardCon::Vector<int> Values;
    Values.PushBack(1);
    Values.SizeAdjust(3, 7);

    BOOST_REQUIRE_EQUAL(Values.Size(), 3);
    BOOST_TEST(Values[0] == 1);
    BOOST_TEST(Values[1] == 7);
    BOOST_TEST(Values[2] == 7);

    auto Next = Values.Erase(Values.Begin() + 1);
    BOOST_REQUIRE(Next != Values.End());
    BOOST_TEST(*Next == 7);
    BOOST_TEST(Values.Size() == 2);
}

BOOST_AUTO_TEST_CASE(VectorSelfReferenceOperationsRemainValid)
{
    StandardCon::Vector<int> Values;
    for (int I = 0; I < 10; ++I)
    {
        Values.PushBack(I);
    }

    Values.PushBack(Values[0]);
    BOOST_TEST(Values.Back() == 0);

    Values.PushFront(Values[5]);
    BOOST_TEST(Values.Front() == 5);
    BOOST_TEST(Values[1] == 0);
}

BOOST_AUTO_TEST_CASE(StringMaintainsBoundsAndMoveState)
{
    StandardCon::String Value{"abc"};
    BOOST_TEST(Value.Reverse().CStr() == std::string{"cba"});
    BOOST_CHECK_THROW(Value[Value.Size()], CustomException::Fault);

    StandardCon::String Moved{std::move(Value)};
    BOOST_TEST(Moved.CStr() == std::string{"abc"});
    BOOST_TEST(Value.Empty()); // NOLINT(bugprone-use-after-move)
    BOOST_TEST(Value.CStr() == std::string{}); // NOLINT(bugprone-use-after-move)
    Value.Resize(0);
    BOOST_TEST(Value.Front() == '\0');

    StandardCon::String Aliased{"abc"};
    Aliased.Prepend(Aliased.CStr());
    BOOST_TEST(Aliased.CStr() == std::string{"abcabc"});
    StandardCon::String Inserted{"abc"};
    Inserted.InsertSubString(Inserted.CStr() + 1, 1);
    BOOST_TEST(Inserted.CStr() == std::string{"abcbc"});
    StandardCon::String Appended{"abc"};
    Appended.PushBack(Appended.CStr() + 1);
    BOOST_TEST(Appended.CStr() == std::string{"abcbc"});

    const std::string Payload(32 * 1024, 'p');
    StandardCon::String SelfAppended{Payload.c_str()};
    SelfAppended.PushBack(SelfAppended);
    BOOST_TEST(SelfAppended.Size() == Payload.size() * 2);
    BOOST_TEST(SelfAppended.CStr()
        == Payload + Payload);

    std::istringstream Input{"final-token"};
    StandardCon::String Read;
    Input >> Read;
    BOOST_TEST(Read.CStr() == std::string{"final-token"});

    StandardCon::String Resized{"a"};
    (void)Resized.Reserve(8);
    Resized.Resize(5, 'x');
    BOOST_TEST(Resized.CStr() == std::string{"axxxx"});
}

BOOST_AUTO_TEST_CASE(ListMoveAndEmptyOperations)
{
    StandardCon::List<int> Empty;
    Empty.PopBack();
    Empty.PopFront();

    StandardCon::List<int> Source{1, 2};
    StandardCon::List<int> Moved{std::move(Source)};

    BOOST_TEST(Moved.Size() == 2);
    BOOST_TEST(Source.Empty()); // NOLINT(bugprone-use-after-move)

    StandardCon::List<int> Assigned{9};
    Assigned = std::move(Moved);
    BOOST_TEST(Assigned.Size() == 2);
    BOOST_TEST(Moved.Empty()); // NOLINT(bugprone-use-after-move)
    auto Reverse = Assigned.Rbegin();
    BOOST_TEST(*Reverse == 2);
    ++Reverse;
    BOOST_TEST(*Reverse == 1);

    auto Begin = Assigned.Begin();
    StandardCon::List<int> EmptyRange{Begin, Begin};
    BOOST_TEST(EmptyRange.Empty());

    Assigned += Assigned;
    BOOST_TEST(Assigned.Size() == 4);
}

BOOST_AUTO_TEST_CASE(QueueStackAndAssociativeContainers)
{
    using HashMapIterator = StandardCon::HashMap<int, int>::Iterator;
    using TreeMapIterator = StandardCon::TreeMap<int, int>::Iterator;
    using HashSetIterator = StandardCon::HashSet<int>::Iterator;
    using TreeSetIterator = StandardCon::TreeSet<int>::Iterator;
    static_assert(std::is_const_v<std::remove_reference_t<
        decltype((std::declval<HashMapIterator>()->First))>>);
    static_assert(std::is_const_v<std::remove_reference_t<
        decltype((std::declval<TreeMapIterator>()->First))>>);
    static_assert(std::is_const_v<std::remove_reference_t<
        decltype(*std::declval<HashSetIterator>())>>);
    static_assert(std::is_const_v<std::remove_reference_t<
        decltype(*std::declval<TreeSetIterator>())>>);

    StandardCon::Queue<int> Queue;
    Queue.Push(1);
    Queue.Push(2);
    BOOST_TEST(Queue.Front() == 1);
    BOOST_TEST(Queue.Back() == 2);
    Queue.Pop();
    BOOST_TEST(Queue.Front() == 2);
    Queue.Pop();
    BOOST_CHECK_THROW((void)Queue.Front(), std::out_of_range);

    StandardCon::Stack<int> Stack;
    Stack.Push(3);
    BOOST_TEST(Stack.Top() == 3);
    Stack.Pop();
    BOOST_CHECK_THROW((void)Stack.Top(), std::out_of_range);

    StandardCon::HashMap<int, int> HashMap;
    BOOST_TEST(HashMap.Push(StandardCon::Pair<int, int>{1, 2}));
    BOOST_TEST(!HashMap.Push(StandardCon::Pair<int, int>{1, 3}));
    BOOST_TEST(HashMap.Size() == 1);

    StandardCon::HashMap<int, int> HashMapCopy{HashMap};
    BOOST_TEST(HashMapCopy.Size() == 1);
    StandardCon::HashMap<int, int> HashMapMoved{std::move(HashMapCopy)};
    BOOST_TEST(HashMapMoved.Size() == 1);
    HashMapCopy = HashMapMoved;
    BOOST_TEST(HashMapCopy.Size() == 1);

    StandardCon::HashSet<int> HashSet;
    BOOST_TEST(HashSet.Push(4));
    BOOST_TEST(HashSet.Size() == 1);

    StandardCon::BinaryTree<int> Tree;
    BOOST_TEST(Tree.Size() == 0);
    BOOST_TEST(Tree.Push(5));
    BOOST_REQUIRE(Tree.Find(5) != nullptr);
    StandardCon::BinaryTree<int> EmptyTree;
    StandardCon::BinaryTree<int> EmptyTreeCopy{EmptyTree};
    BOOST_TEST(EmptyTreeCopy.Size() == 0);

    StandardCon::TreeSet<int> OrderedSet;
    BOOST_TEST(OrderedSet.Push(6).Second);
    BOOST_CHECK(static_cast<bool>(OrderedSet.Find(6) != OrderedSet.End()));
    BOOST_TEST(OrderedSet.Pop(6).Second);
    BOOST_TEST(OrderedSet.Empty());

    StandardCon::TreeMap<int, int> OrderedMap;
    BOOST_TEST(OrderedMap.Push(StandardCon::Pair<int, int>{7, 8}).Second);
    BOOST_CHECK(static_cast<bool>(OrderedMap.Find(
        StandardCon::Pair<int, int>{7, 0}) != OrderedMap.End()));
}

BOOST_AUTO_TEST_CASE(BitSetTracksBitsInsteadOfOperations)
{
    StandardCon::BitSet Bits{64};
    Bits.Set(1);
    Bits.Set(1);
    BOOST_TEST(Bits.Test(1));
    BOOST_TEST(Bits.Size() == 1);

    Bits.Reset(1);
    BOOST_TEST(!Bits.Test(1));
    BOOST_TEST(Bits.Size() == 0);
}

BOOST_AUTO_TEST_CASE(SmartPointersRespectOwnership)
{
    auto Shared = StandardCon::Pointer::SharedPtr<int>{new int{42}};
    StandardCon::Pointer::WeakPtr<int> Weak{Shared};
    BOOST_TEST(!Weak.Expired());
    BOOST_TEST(Shared.GetCount() == 1);

    {
        auto Copy = Shared;
        BOOST_TEST(Shared.GetCount() == 2);
        BOOST_TEST(*Copy == 42);
    }

    BOOST_TEST(Shared.GetCount() == 1);
}

BOOST_AUTO_TEST_CASE(WeakPointerExpiresAfterOwnerDestruction)
{
    StandardCon::Pointer::WeakPtr<int> Weak;
    {
        StandardCon::Pointer::SharedPtr<int> Shared{new int{9}};
        Weak = Shared;
        BOOST_TEST(!Weak.Expired());
    }

    BOOST_TEST(Weak.Expired());
    BOOST_TEST(Weak.GetCount() == 0);

    StandardCon::Pointer::SharedPtr<int> Empty;
    BOOST_TEST(Empty.GetCount() == 0);
}

BOOST_AUTO_TEST_CASE(WeakPointerLockKeepsTheObjectAlive)
{
    auto Owner = StandardCon::Pointer::SharedPtr<int>{new int{17}};
    StandardCon::Pointer::WeakPtr<int> Weak{Owner};
    auto Locked = Weak.Lock();

    BOOST_REQUIRE(Locked.GetPtr() != nullptr);
    BOOST_TEST(*Locked == 17);

    Owner = decltype(Owner){};
    BOOST_TEST(!Weak.Expired());
    BOOST_TEST(*Locked == 17);

    Locked = decltype(Locked){};
    BOOST_TEST(Weak.Expired());
    BOOST_TEST(Weak.Lock().GetPtr() == nullptr);
}

BOOST_AUTO_TEST_CASE(WeakPointerLockIsSafeDuringOwnerDestruction)
{
    auto Owner = StandardCon::Pointer::SharedPtr<int>{new int{23}};
    StandardCon::Pointer::WeakPtr<int> Weak{Owner};
    std::atomic_bool ReleaseOwner{false};
    std::thread Releaser([Owner = std::move(Owner), &ReleaseOwner]() mutable
    {
        while (!ReleaseOwner.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        Owner = decltype(Owner){};
    });

    ReleaseOwner.store(true, std::memory_order_release);
    std::size_t SuccessfulLocks = 0;
    while (auto Locked = Weak.Lock())
    {
        BOOST_TEST(*Locked == 23);
        ++SuccessfulLocks;
    }
    Releaser.join();

    BOOST_TEST(SuccessfulLocks > 0);
    BOOST_TEST(Weak.Expired());
}

BOOST_AUTO_TEST_CASE(HttpAndProtocolJsonRoundTrip)
{
    Wan::Network::Http::Request<> HttpRequest;
    HttpRequest.Body() = "body";
    HttpRequest.PreparePayload();
    BOOST_TEST(HttpRequest.Body() == "body");

    Protocol::Request<> Request;
    Request.Header().SetMethod("GET");
    Request.Header().SetTarget("/");
    Request.SetMessage("payload");
    auto Json = Request.ToJson();

    Protocol::Request<> Restored;
    BOOST_TEST(Restored.FromJson(Json));
    BOOST_TEST(Restored.Body() == "payload");
}

BOOST_AUTO_TEST_CASE(ProtocolAndHttpRejectIncompleteInput)
{
    Protocol::RequestHeader Header;
    BOOST_TEST(!Header.FromString(
        "GET / 1 0 0 0\r\nTimestamp: invalid\r\n\r\n"));
    BOOST_TEST(!Header.FromString(
        "GET / 1 0 0 0\r\nBrokenHeader\r\n"));

    Wan::Network::Http::Request<> Request;
    BOOST_TEST(!Request.FromString("GET / HTTP/1.1\r\n"));
    BOOST_TEST(!Request.FromString(
        "GET / HTTP/1.1\r\nHost: localhost\r\n\r\ntrailing"));

    Protocol::RequestHeader ValidHeader;
    ValidHeader.SetMethod("GET");
    ValidHeader.SetTarget("/");
    ValidHeader.CalculateAndSetChecksum("payload");
    const auto ValidText = ValidHeader.ToString();
    BOOST_TEST(ValidHeader.FromString(ValidText));
    BOOST_TEST(!ValidHeader.FromString("invalid"));
    BOOST_TEST(ValidHeader.GetMethod() == "GET");

    const auto OversizedProtocol = std::string(
        Protocol::MaxProtocolMessageSize + 1, 'x');
    BOOST_TEST(!ValidHeader.FromString(OversizedProtocol));

    Protocol::ResponseHeader ResponseHeader;
    BOOST_TEST(!ResponseHeader.FromString(OversizedProtocol));
}

BOOST_AUTO_TEST_CASE(StaticFileRootBoundaryIsEnforced)
{
    const auto Root = std::filesystem::temp_directory_path()
        / "WanPublicApiRoot";
    const auto Sibling = std::filesystem::path{Root.string() + "2"};
    std::filesystem::remove_all(Root);
    std::filesystem::remove_all(Sibling);
    std::filesystem::create_directories(Root);
    std::filesystem::create_directories(Sibling);
    std::ofstream{Root / "index.html"} << "ok";
    std::ofstream{Sibling / "secret.txt"} << "secret";

    Wan::Web::StaticFileServer Files{Root};
    BOOST_TEST(Files.Resolve("/").has_value());
    BOOST_TEST(!Files.Resolve(("/../" + Sibling.filename().string()
        + "/secret.txt")).has_value());

    std::filesystem::remove_all(Root);
    std::filesystem::remove_all(Sibling);
}

BOOST_AUTO_TEST_CASE(AuthenticatedCipherRejectsMalformedInput)
{
    const auto Cipher =
        Wan::Network::Ciphertext::ArcaneSymmetric::AesGcmEncrypt(
            "payload", "test-passphrase");
    BOOST_TEST(
        Wan::Network::Ciphertext::ArcaneSymmetric::AesGcmDecrypt(
            Cipher, "test-passphrase") == "payload");

    BOOST_CHECK_THROW(
        Wan::Network::Ciphertext::ArcaneSymmetric::AesGcmDecrypt(
            Cipher.substr(0, Cipher.size() / 2), "test-passphrase"),
        std::runtime_error);
    BOOST_CHECK_THROW(
        Wan::Network::Ciphertext::ArcaneSymmetric::AesCtrDecrypt(
            "", "test-passphrase"),
        std::runtime_error);
}

BOOST_AUTO_TEST_CASE(CryptoRandomAndKdf)
{
    Encryption::AeonRandom Random;
    CryptoPP::SecByteBlock Salt(16);
    Random.Pool().GenerateBlock(Salt, Salt.size());
    const auto Key = Encryption::DeriveKeyFromPassword(
        "test-passphrase", Salt.BytePtr(), Salt.size(), 32);
    BOOST_TEST(Key.size() == 32);
    BOOST_CHECK_THROW(
        Wan::Network::Ciphertext::PenumbraAsymmetric::GenerateRSAKeypair(1024),
        std::invalid_argument);
    BOOST_CHECK_THROW(
        Wan::Network::Ciphertext::PenumbraAsymmetric::GenerateElGamalKeypair(1024),
        std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(RouterRejectsMalformedPatterns)
{
    Wan::Web::Router Router;
    const Wan::Web::Handler Handler =
        [](Wan::Web::Context&) -> boost::asio::awaitable<void>
        {
            co_return;
        };

    BOOST_CHECK_THROW(
        Router.Get("/broken/<string", Handler), std::invalid_argument);
    BOOST_CHECK_THROW(
        Router.Get("/user/<string:", Handler), std::invalid_argument);
    BOOST_CHECK_THROW(
        Router.Get("/user/<bool:id>", Handler), std::invalid_argument);
    BOOST_CHECK_THROW(
        Router.Get("/user/<string:id>/<int:id>", Handler), std::invalid_argument);
    BOOST_CHECK_THROW(
        Router.Get("/user/<path:rest>/tail", Handler), std::invalid_argument);
    Router.Get("/item/<string:id>", Handler);
    BOOST_CHECK_THROW(
        Router.Get("/item/<int:id>", Handler), std::invalid_argument);
    BOOST_TEST(!Router.Empty());
}

BOOST_AUTO_TEST_CASE(CoroutineLogWritesThroughPublicApi)
{
    const auto Root = std::filesystem::temp_directory_path()
        / "WanCoroutineLogTest";
    std::filesystem::remove_all(Root);

    boost::asio::io_context Ioc;
    PerformanceLog::CoroutineLog Log{Ioc.get_executor()};
    bool Completed = false;
    boost::asio::co_spawn(Ioc,
        [&]() -> boost::asio::awaitable<void>
        {
            co_await Log.SetOutputDirectory(Root.string());
            const auto Written = co_await Log.FileWrite(
                "application.log", std::string{"hello\n"});
            co_await Log.Shutdown();
            Completed = Written == 6;
        },
        boost::asio::detached);
    Ioc.run();

    std::string Contents;
    {
        std::ifstream Input{Root / "application.log"};
        Contents.assign(std::istreambuf_iterator<char>{Input}, {});
    }
    BOOST_TEST(Completed);
    BOOST_TEST(Contents == "hello\n");
    std::filesystem::remove_all(Root);
}
