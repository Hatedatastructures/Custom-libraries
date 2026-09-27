// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE ContainerTests
#include <boost/test/included/unit_test.hpp>
#include "Wan/Container/Container.hpp"

#include <limits>
#include <map>
#include <random>
#include <stdexcept>

namespace
{
    struct MoveOnlyValue
    {
        int Value;

        MoveOnlyValue() = delete;
        explicit MoveOnlyValue(int InitialValue) : Value(InitialValue) {}
        MoveOnlyValue(const MoveOnlyValue&) = delete;
        auto operator=(const MoveOnlyValue&) -> MoveOnlyValue& = delete;
        MoveOnlyValue(MoveOnlyValue&&) noexcept = default;
        auto operator=(MoveOnlyValue&&) noexcept -> MoveOnlyValue& = default;
    };

    struct CountingLess
    {
        static inline std::size_t Calls = 0;

        bool operator()(int Left, int Right) const
        {
            ++Calls;
            return Left < Right;
        }
    };

    struct MoveTrackedValue
    {
        static inline std::size_t Moves = 0;
        int Value;

        explicit MoveTrackedValue(int InitialValue) : Value(InitialValue) {}
        MoveTrackedValue(const MoveTrackedValue&) = delete;
        auto operator=(const MoveTrackedValue&) -> MoveTrackedValue& = delete;
        MoveTrackedValue(MoveTrackedValue&& Other) noexcept : Value(Other.Value)
        {
            ++Moves;
            Other.Value = -1;
        }
        auto operator=(MoveTrackedValue&& Other) noexcept -> MoveTrackedValue&
        {
            ++Moves;
            Value = Other.Value;
            Other.Value = -1;
            return *this;
        }
    };

    struct ThrowingCopyValue
    {
        static inline int CopiesBeforeThrow = -1;
        static inline int ThrowOnValue = -1;
        int Value;

        explicit ThrowingCopyValue(int InitialValue) : Value(InitialValue) {}
        ThrowingCopyValue(const ThrowingCopyValue& Other) : Value(Other.Value)
        {
            if (CopiesBeforeThrow == 0 || Value == ThrowOnValue)
            {
                throw std::runtime_error("copy failure");
            }
            if (CopiesBeforeThrow > 0)
            {
                --CopiesBeforeThrow;
            }
        }
        ThrowingCopyValue(ThrowingCopyValue&& Other) noexcept(false)
            : Value(Other.Value)
        {
            Other.Value = -1;
        }
        auto operator=(const ThrowingCopyValue&) -> ThrowingCopyValue& = default;
        auto operator=(ThrowingCopyValue&&) -> ThrowingCopyValue& = default;
    };

    struct ConstantHash
    {
        std::uint64_t operator()(int) const noexcept
        {
            return 1;
        }
    };

    struct IdentityKey
    {
        int operator()(int Value) const noexcept
        {
            return Value;
        }
    };

    struct CopyBomb
    {
        static inline int Alive = 0;
        static inline int CopiesBeforeThrow = -1;
        int Value;

        explicit CopyBomb(int InitialValue) : Value(InitialValue)
        {
            ++Alive;
        }
        CopyBomb(const CopyBomb& Other) : Value(Other.Value)
        {
            if (CopiesBeforeThrow == 0)
            {
                throw std::runtime_error("copy failure");
            }
            if (CopiesBeforeThrow > 0)
            {
                --CopiesBeforeThrow;
            }
            ++Alive;
        }
        ~CopyBomb()
        {
            --Alive;
        }
        bool operator<(const CopyBomb& Other) const noexcept
        {
            return Value < Other.Value;
        }
    };

    struct SharedPtrArrowTarget
    {
        int Value;
    };
}

BOOST_AUTO_TEST_SUITE(SimulateVectorTests)

BOOST_AUTO_TEST_CASE(PushBackAndSize) {
    StandardCon::Vector<int> Vec;
    Vec.PushBack(1);
    Vec.PushBack(2);
    BOOST_CHECK_EQUAL(Vec.Size(), 2);
}

BOOST_AUTO_TEST_CASE(VectorSupportsMoveOnlyNonDefaultConstructibleValues)
{
    StandardCon::Vector<MoveOnlyValue> Values;
    Values.PushBack(MoveOnlyValue{7});
    Values.PushBack(MoveOnlyValue{11});
    Values.PushFront(MoveOnlyValue{3});

    BOOST_REQUIRE_EQUAL(Values.Size(), 3);
    BOOST_TEST(Values[0].Value == 3);
    BOOST_TEST(Values[1].Value == 7);
    BOOST_TEST(Values[2].Value == 11);
    Values.PopFront();
    BOOST_REQUIRE_EQUAL(Values.Size(), 2);
    BOOST_TEST(Values[0].Value == 7);
    Values.Erase(Values.Begin());
    BOOST_REQUIRE_EQUAL(Values.Size(), 1);
    BOOST_TEST(Values[0].Value == 11);

    StandardCon::List<MoveOnlyValue> Nodes;
    Nodes.PushBack(MoveOnlyValue{13});
    Nodes.PushFront(MoveOnlyValue{17});
    BOOST_REQUIRE_EQUAL(Nodes.Size(), 2);
    BOOST_TEST(Nodes.Front().Value == 17);
    BOOST_TEST(Nodes.Back().Value == 13);
    Nodes.PopFront();
    BOOST_REQUIRE_EQUAL(Nodes.Size(), 1);
    BOOST_TEST(Nodes.Front().Value == 13);

    StandardCon::Vector<MoveOnlyValue> BackGrowth;
    StandardCon::Vector<MoveOnlyValue> FrontGrowth;
    for (int I = 0; I < 10; ++I)
    {
        BackGrowth.PushBack(MoveOnlyValue{I});
        FrontGrowth.PushBack(MoveOnlyValue{I});
    }
    BackGrowth.PushBack(MoveOnlyValue{10});
    FrontGrowth.PushFront(MoveOnlyValue{-1});
    BOOST_TEST(BackGrowth.Size() == 11);
    BOOST_TEST(BackGrowth.Back().Value == 10);
    BOOST_TEST(FrontGrowth.Size() == 11);
    BOOST_TEST(FrontGrowth.Front().Value == -1);
    BOOST_TEST(FrontGrowth.Back().Value == 9);
}

BOOST_AUTO_TEST_CASE(ListRejectsIteratorsFromAnotherContainer)
{
    StandardCon::List<int> First{1, 2};
    StandardCon::List<int> Second{9};
    auto ForeignPosition = Second.Begin();

    BOOST_CHECK_THROW(First.Insert(ForeignPosition, 7),
        std::invalid_argument);
    BOOST_CHECK_THROW(First.Erase(ForeignPosition),
        std::invalid_argument);
    BOOST_REQUIRE_EQUAL(First.Size(), 2);
    BOOST_REQUIRE_EQUAL(Second.Size(), 1);
    BOOST_TEST(First.Front() == 1);
    BOOST_TEST(Second.Front() == 9);
}

BOOST_AUTO_TEST_CASE(AvlDeletionUsesLogarithmicKeyComparisons)
{
    StandardCon::BalanceTree<int, int, CountingLess> Values;
    for (int Key = 0; Key < 2047; ++Key)
    {
        BOOST_REQUIRE(Values.Push(Key, Key * 2));
    }

    CountingLess::Calls = 0;
    Values.Pop(1023);

    BOOST_TEST(Values.Size() == 2046);
    BOOST_TEST(CountingLess::Calls < 100);
    std::size_t Visited = 0;
    for (auto It = Values.Begin(); It != Values.End(); ++It)
    {
        BOOST_TEST(It->First != 1023);
        ++Visited;
    }
    BOOST_TEST(Visited == 2046);
}

BOOST_AUTO_TEST_CASE(AvlMatchesMapAcrossDeterministicRandomOperations)
{
    StandardCon::BalanceTree<int, int> Values;
    std::map<int, int> Expected;
    std::mt19937 Generator{0x57414E};
    std::uniform_int_distribution<int> KeyDistribution{0, 511};
    std::uniform_int_distribution<int> OperationDistribution{0, 2};

    for (int Operation = 0; Operation < 3000; ++Operation)
    {
        const auto Key = KeyDistribution(Generator);
        if (OperationDistribution(Generator) == 0)
        {
            BOOST_TEST(Values.Push(Key, Key * 3)
                == Expected.emplace(Key, Key * 3).second);
        }
        else
        {
            Values.Pop(Key);
            Expected.erase(Key);
        }

        if (Operation % 50 == 0)
        {
            BOOST_REQUIRE_EQUAL(Values.Size(), Expected.size());
            auto ExpectedIt = Expected.begin();
            for (auto It = Values.Begin(); It != Values.End(); ++It)
            {
                BOOST_REQUIRE(ExpectedIt != Expected.end());
                BOOST_TEST(It->First == ExpectedIt->first);
                BOOST_TEST(It->Second == ExpectedIt->second);
                ++ExpectedIt;
            }
            BOOST_CHECK(ExpectedIt == Expected.end());
        }
    }
}

BOOST_AUTO_TEST_CASE(TreeCopyFailuresReleaseAllClonedNodes)
{
    {
        StandardCon::BinaryTree<CopyBomb> Source;
        for (int Value = 0; Value < 15; ++Value)
        {
            CopyBomb Key{Value};
            BOOST_REQUIRE(Source.Push(Key));
        }
        const auto AliveBeforeCopy = CopyBomb::Alive;
        CopyBomb::CopiesBeforeThrow = 2;
        BOOST_CHECK_THROW((void)StandardCon::BinaryTree<CopyBomb>{Source},
            std::runtime_error);
        CopyBomb::CopiesBeforeThrow = -1;
        BOOST_TEST(CopyBomb::Alive == AliveBeforeCopy);

        StandardCon::BinaryTree<CopyBomb> Target;
        CopyBomb TargetKey{-1};
        BOOST_REQUIRE(Target.Push(TargetKey));
        const auto AliveBeforeAssignment = CopyBomb::Alive;
        CopyBomb::CopiesBeforeThrow = 2;
        BOOST_CHECK_THROW(Target = Source, std::runtime_error);
        CopyBomb::CopiesBeforeThrow = -1;
        BOOST_TEST(Target.Size() == 1);
        BOOST_CHECK(Target.Find(TargetKey) != nullptr);
        BOOST_TEST(CopyBomb::Alive == AliveBeforeAssignment);
    }
    {
        StandardCon::TreeSet<CopyBomb> Source;
        for (int Value = 0; Value < 15; ++Value)
        {
            CopyBomb Key{Value};
            BOOST_REQUIRE(Source.Push(Key).Second);
        }
        const auto AliveBeforeCopy = CopyBomb::Alive;
        CopyBomb::CopiesBeforeThrow = 2;
        BOOST_CHECK_THROW((void)StandardCon::TreeSet<CopyBomb>{Source},
            std::runtime_error);
        CopyBomb::CopiesBeforeThrow = -1;
        BOOST_TEST(CopyBomb::Alive == AliveBeforeCopy);

        StandardCon::TreeSet<CopyBomb> Target;
        CopyBomb TargetKey{-1};
        BOOST_REQUIRE(Target.Push(TargetKey).Second);
        const auto AliveBeforeAssignment = CopyBomb::Alive;
        CopyBomb::CopiesBeforeThrow = 2;
        BOOST_CHECK_THROW(Target = Source, std::runtime_error);
        CopyBomb::CopiesBeforeThrow = -1;
        BOOST_TEST(Target.Size() == 1);
        BOOST_CHECK(Target.Find(TargetKey) != Target.End());
        BOOST_TEST(CopyBomb::Alive == AliveBeforeAssignment);
    }
    {
        StandardCon::BalanceTree<CopyBomb, int> Source;
        for (int Value = 0; Value < 15; ++Value)
        {
            CopyBomb Key{Value};
            BOOST_REQUIRE(Source.Push(Key, Value));
        }
        const auto AliveBeforeCopy = CopyBomb::Alive;
        CopyBomb::CopiesBeforeThrow = 2;
        BOOST_CHECK_THROW(
            ((void)StandardCon::BalanceTree<CopyBomb, int>{Source}),
            std::runtime_error);
        CopyBomb::CopiesBeforeThrow = -1;
        BOOST_TEST(CopyBomb::Alive == AliveBeforeCopy);

        StandardCon::BalanceTree<CopyBomb, int> Target;
        CopyBomb TargetKey{-1};
        BOOST_REQUIRE(Target.Push(TargetKey, 1));
        const auto AliveBeforeAssignment = CopyBomb::Alive;
        CopyBomb::CopiesBeforeThrow = 2;
        BOOST_CHECK_THROW(Target = Source, std::runtime_error);
        CopyBomb::CopiesBeforeThrow = -1;
        BOOST_TEST(Target.Size() == 1);
        BOOST_CHECK(Target.Find(TargetKey) != nullptr);
        BOOST_TEST(CopyBomb::Alive == AliveBeforeAssignment);
    }
    BOOST_TEST(CopyBomb::Alive == 0);
}

BOOST_AUTO_TEST_CASE(HashMapRehashDoesNotMoveExistingValues)
{
    StandardCon::HashMap<int, MoveTrackedValue> Values;
    for (int Key = 1; Key <= 7; ++Key)
    {
        BOOST_REQUIRE(Values.Push(
            StandardCon::Pair<int, MoveTrackedValue>{
                int{Key}, MoveTrackedValue{Key * 10}}));
    }

    const auto MovesBeforeRehash = MoveTrackedValue::Moves;
    BOOST_REQUIRE(Values.Push(
        StandardCon::Pair<int, MoveTrackedValue>{8, MoveTrackedValue{80}}));

    BOOST_TEST(MoveTrackedValue::Moves - MovesBeforeRehash == 3);
    for (int Key = 1; Key <= 8; ++Key)
    {
        auto It = Values.Find(Key);
        BOOST_REQUIRE(It != Values.End());
        BOOST_TEST(It->Second.Value == Key * 10);
    }
}

BOOST_AUTO_TEST_CASE(VectorGrowthPreservesValuesWhenCopyThrows)
{
    StandardCon::Vector<ThrowingCopyValue> Values;
    for (int I = 0; I < 10; ++I)
    {
        Values.PushBack(ThrowingCopyValue{I});
    }

    ThrowingCopyValue::CopiesBeforeThrow = 2;
    const ThrowingCopyValue Next{10};
    BOOST_CHECK_THROW(Values.PushBack(Next), std::runtime_error);
    ThrowingCopyValue::CopiesBeforeThrow = -1;

    BOOST_REQUIRE_EQUAL(Values.Size(), 10);
    BOOST_REQUIRE_EQUAL(Values.Capacity(), 10);
    for (int I = 0; I < 10; ++I)
    {
        BOOST_TEST(Values[static_cast<std::uint64_t>(I)].Value == I);
    }

    ThrowingCopyValue::ThrowOnValue = 99;
    const ThrowingCopyValue ThrowingValue{99};
    BOOST_CHECK_THROW(Values.PushBack(ThrowingValue), std::runtime_error);
    ThrowingCopyValue::ThrowOnValue = -1;
    BOOST_TEST(Values.Size() == 10);
    BOOST_TEST(Values.Capacity() == 10);
    for (int I = 0; I < 10; ++I)
    {
        BOOST_TEST(Values[static_cast<std::uint64_t>(I)].Value == I);
    }

    StandardCon::Vector<ThrowingCopyValue> Other;
    Other.PushBack(ThrowingCopyValue{77});
    ThrowingCopyValue::ThrowOnValue = 77;
    const ThrowingCopyValue ThrowingFront{77};
    BOOST_CHECK_THROW(Values.PushFront(ThrowingFront), std::runtime_error);
    BOOST_TEST(Values.Capacity() == 10);
    BOOST_CHECK_THROW(Values.SizeAdjust(20, ThrowingCopyValue{77}),
        std::runtime_error);
    BOOST_TEST(Values.Capacity() == 10);
    BOOST_CHECK_THROW(Values += Other, std::runtime_error);
    BOOST_TEST(Values.Size() == 10);
    BOOST_TEST(Values.Capacity() == 10);
    for (int I = 0; I < 10; ++I)
    {
        BOOST_TEST(Values[static_cast<std::uint64_t>(I)].Value == I);
    }
    ThrowingCopyValue::ThrowOnValue = -1;

    BOOST_CHECK_THROW(Values.SizeAdjust(
        std::numeric_limits<std::uint64_t>::max(),
        ThrowingCopyValue{0}), std::length_error);
}

BOOST_AUTO_TEST_CASE(HashMapHandlesConcentratedCollisions)
{
    StandardCon::HashMap<int, int, ConstantHash> Values;
    for (int Key = 0; Key < 100; ++Key)
    {
        BOOST_REQUIRE(Values.Push(
            StandardCon::Pair<int, int>{Key, Key * 2}));
    }
    for (int Key = 0; Key < 100; ++Key)
    {
        auto It = Values.Find(Key);
        BOOST_REQUIRE(It != Values.End());
        BOOST_TEST(It->Second == Key * 2);
    }
    BOOST_TEST(Values.Pop(50));
    BOOST_CHECK(Values.Find(50) == Values.End());
}

BOOST_AUTO_TEST_CASE(HashTableMaintainsConfiguredLoadFactor)
{
    StandardCon::HashTable<int, int, IdentityKey, std::hash<int>> Values{10};
    for (int Key = 0; Key < 7; ++Key)
    {
        BOOST_REQUIRE(Values.Push(Key));
    }

    BOOST_TEST(Values.ChangeLoadFactor(1));
    BOOST_TEST(Values.Capacity() == 80);
    BOOST_TEST(Values.Push(7));
    BOOST_TEST(Values.Capacity() == 80);
    BOOST_TEST(Values.Push(8));
    BOOST_TEST(Values.Capacity() == 160);
    BOOST_TEST(!Values.ChangeLoadFactor(0));
    BOOST_TEST(!Values.ChangeLoadFactor(11));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SmartPointerAndBitSetTests)

BOOST_AUTO_TEST_CASE(SharedPtrArrowReturnsManagedObject)
{
    StandardCon::Pointer::SharedPtr<SharedPtrArrowTarget> Value(
        new SharedPtrArrowTarget{42});

    BOOST_REQUIRE(static_cast<bool>(Value));
    BOOST_TEST(Value->Value == 42);
}

BOOST_AUTO_TEST_CASE(BitSetResizeClearsWordsAndCount)
{
    StandardCon::BitSet Bits{64};
    Bits.Set(4);
    Bits.Set(35);
    BOOST_TEST(Bits.Size() == 2);

    Bits.Resize(64);

    BOOST_TEST(Bits.Size() == 0);
    BOOST_CHECK(!Bits.Test(4));
    BOOST_CHECK(!Bits.Test(35));
    Bits.Reset(4);
    BOOST_TEST(Bits.Size() == 0);
    Bits.Set(4);
    BOOST_TEST(Bits.Size() == 1);
}

BOOST_AUTO_TEST_CASE(BitSetResizeClearsRetainedWordsWhenCapacityChanges)
{
    StandardCon::BitSet Bits{64};
    Bits.Set(4);
    Bits.Set(35);

    Bits.Resize(40);
    BOOST_TEST(Bits.Size() == 0);
    BOOST_CHECK(!Bits.Test(4));
    BOOST_CHECK(!Bits.Test(35));
    BOOST_CHECK(!Bits.Test(40));

    Bits.Set(35);
    Bits.Resize(96);
    BOOST_TEST(Bits.Size() == 0);
    BOOST_CHECK(!Bits.Test(35));
    Bits.Set(80);
    BOOST_TEST(Bits.Size() == 1);
    Bits.Reset(80);
    BOOST_TEST(Bits.Size() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
