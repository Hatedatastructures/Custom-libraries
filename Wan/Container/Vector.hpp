/**
 * @file Vector.hpp
 * @brief 连续存储的动态数组。
 */
#pragma once
#include "Exception.hpp"
#include "Algorithm.hpp"

#include <functional>
#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace VectorContainer
{
/**
     * @brief 管理连续元素存储的动态数组。
     * @tparam VectorType 元素类型。
     * @note 扩容会使所有指针、引用和迭代器失效；插删会使操作位置及其后的迭代器失效。
     */
    template <typename VectorType>
    class Vector
    {
    public:
        /** @brief 指向可修改元素的指针迭代器。 */
        using Iterator = VectorType *;
        /** @brief 指向只读元素的指针迭代器。 */
        using ConstIterator = const VectorType *;
        /** @brief 与 Iterator 同类型的别名。 */
        using ReverseIterator = Iterator;
        /** @brief 与 ConstIterator 同类型的别名。 */
        using ConstReverseIterator = ConstIterator;

    private:
        /** @brief 元素存储使用的标准分配器类型。 */
        using Allocator = std::allocator<VectorType>;
        /** @brief 调用分配器和对象生命周期操作的 traits 类型。 */
        using AllocatorTraits = std::allocator_traits<Allocator>;

        /** @brief 已分配存储区的起始位置；空向量时为空。 */
        Iterator MemberDataPointer = nullptr;
        /** @brief 尾后位置，同时标记已构造元素的范围终点。 */
        Iterator MemberSizePointer = nullptr;
        /** @brief 已分配容量的尾后位置。 */
        Iterator MemberCapacityPointer = nullptr;
        /** @brief 管理元素存储区的分配器实例。 */
        [[no_unique_address]] Allocator MemberAllocator;

        /**
         * @brief 销毁半开区间 `[First, Last)` 中已构造的元素。
         * @param First 区间首位置。
         * @param Last 区间尾后位置。
         */
        void DestroyRange(Iterator First, Iterator Last) noexcept
        {
            while (Last != First)
            {
                --Last;
                AllocatorTraits::destroy(MemberAllocator, Last);
            }
        }

        /** @brief 销毁所有元素、释放存储区并重置边界指针。 */
        void Release() noexcept
        {
            if (MemberDataPointer == nullptr)
            {
                return;
            }
            const auto CurrentCapacity = Capacity();
            DestroyRange(MemberDataPointer, MemberSizePointer);
            AllocatorTraits::deallocate(
                MemberAllocator, MemberDataPointer, CurrentCapacity);
            MemberDataPointer = nullptr;
            MemberSizePointer = nullptr;
            MemberCapacityPointer = nullptr;
        }

        /**
         * @brief 返回当前分配器允许的最大元素数。
         * @return 当前分配器最多可分配的元素数。
         */
        [[nodiscard]] auto MaxSize() const noexcept -> std::uint64_t
        {
            return static_cast<std::uint64_t>(
                AllocatorTraits::max_size(MemberAllocator));
        }

        /**
         * @brief 按几何增长策略计算满足需求的容量。
         * @param RequiredCapacity 所需的最小元素容量。
         * @return 不小于所需容量的新容量。
         * @throws std::length_error 所需容量超过最大值时抛出。
         */
        [[nodiscard]] auto GrowthCapacity(
            std::uint64_t RequiredCapacity) const -> std::uint64_t
        {
            const auto Maximum = MaxSize();
            if (RequiredCapacity > Maximum)
            {
                throw std::length_error("Vector capacity exceeds max_size");
            }

            const auto CurrentCapacity = Capacity();
            std::uint64_t NewCapacity = CurrentCapacity == 0
                ? std::min<std::uint64_t>(10, Maximum)
                : (CurrentCapacity > Maximum / 2
                    ? Maximum : CurrentCapacity * 2);
            if (NewCapacity < RequiredCapacity)
            {
                NewCapacity = RequiredCapacity;
            }
            return NewCapacity;
        }

        /**
         * @brief 确认重分配时可以安全迁移现有元素。
         * @throws std::logic_error 元素既不可复制、移动又可能抛异常且向量非空时抛出。
         */
        void EnsureRelocationIsSafe() const
        {
            if constexpr (!std::is_copy_constructible_v<VectorType>
                && !std::is_nothrow_move_constructible_v<VectorType>)
            {
                if (!Empty())
                {
                    throw std::logic_error(
                        "Vector growth requires copy construction or a non-throwing move");
                }
            }
        }

        /**
         * @brief 分配新存储区，构造新增尾部，再迁移现有元素。
         * @tparam BuildTail 在新存储区构造新增尾部元素的回调类型。
         * @param NewSize 重分配后的元素数量。
         * @param ConstructTail 接收新存储区、原元素尾后位置及已构造尾部计数的回调。
         * @note 构造或迁移抛出异常时会销毁新存储区中已构造的元素并传播异常。
         */
        template <typename BuildTail>
        void ReallocateWithTail(std::uint64_t NewSize,
            BuildTail&& ConstructTail)
        {
            const auto CurrentSize = Size();
            EnsureRelocationIsSafe();
            const auto NewCapacity = GrowthCapacity(NewSize);
            auto *NewData = AllocatorTraits::allocate(
                MemberAllocator, static_cast<std::size_t>(NewCapacity));
            std::uint64_t ConstructedOld = 0;
            std::uint64_t ConstructedTail = 0;
            try
            {
                ConstructTail(NewData, CurrentSize, ConstructedTail);
                for (; ConstructedOld < CurrentSize; ++ConstructedOld)
                {
                    AllocatorTraits::construct(MemberAllocator,
                        NewData + ConstructedOld,
                        std::move_if_noexcept(
                            MemberDataPointer[ConstructedOld]));
                }
            }
            catch (...)
            {
                DestroyRange(NewData, NewData + ConstructedOld);
                DestroyRange(NewData + CurrentSize,
                    NewData + CurrentSize + ConstructedTail);
                AllocatorTraits::deallocate(MemberAllocator, NewData,
                    static_cast<std::size_t>(NewCapacity));
                throw;
            }

            const auto CurrentCapacity = Capacity();
            if (MemberDataPointer != nullptr)
            {
                DestroyRange(MemberDataPointer, MemberSizePointer);
                AllocatorTraits::deallocate(MemberAllocator,
                    MemberDataPointer,
                    static_cast<std::size_t>(CurrentCapacity));
            }
            MemberDataPointer = NewData;
            MemberSizePointer = NewData + NewSize;
            MemberCapacityPointer = NewData + NewCapacity;
        }

        /**
         * @brief 在新存储区首位构造元素并迁移现有元素。
         * @tparam Value 新元素的值类别。
         * @param NewValue 要前置插入的元素。
         */
        template <typename Value>
        void ReallocateWithPrepend(Value&& NewValue)
        {
            const auto CurrentSize = Size();
            if (CurrentSize == MaxSize())
            {
                throw std::length_error("Vector size exceeds max_size");
            }
            EnsureRelocationIsSafe();
            const auto NewSize = CurrentSize + 1;
            const auto NewCapacity = GrowthCapacity(NewSize);
            auto *NewData = AllocatorTraits::allocate(
                MemberAllocator, static_cast<std::size_t>(NewCapacity));
            bool ConstructedFront = false;
            std::uint64_t ConstructedOld = 0;
            try
            {
                AllocatorTraits::construct(MemberAllocator, NewData,
                    std::forward<Value>(NewValue));
                ConstructedFront = true;
                for (; ConstructedOld < CurrentSize; ++ConstructedOld)
                {
                    AllocatorTraits::construct(MemberAllocator,
                        NewData + ConstructedOld + 1,
                        std::move_if_noexcept(
                            MemberDataPointer[ConstructedOld]));
                }
            }
            catch (...)
            {
                DestroyRange(NewData + 1,
                    NewData + ConstructedOld + 1);
                if (ConstructedFront)
                {
                    AllocatorTraits::destroy(MemberAllocator, NewData);
                }
                AllocatorTraits::deallocate(MemberAllocator, NewData,
                    static_cast<std::size_t>(NewCapacity));
                throw;
            }

            const auto CurrentCapacity = Capacity();
            if (MemberDataPointer != nullptr)
            {
                DestroyRange(MemberDataPointer, MemberSizePointer);
                AllocatorTraits::deallocate(MemberAllocator,
                    MemberDataPointer,
                    static_cast<std::size_t>(CurrentCapacity));
            }
            MemberDataPointer = NewData;
            MemberSizePointer = NewData + NewSize;
            MemberCapacityPointer = NewData + NewCapacity;
        }

        /**
         * @brief 扩大存储区至至少所需容量，不改变元素数量。
         * @param RequiredCapacity 所需的最小元素容量。
         * @note 重分配期间会迁移现有元素；迁移失败时会传播异常。
         */
        void EnsureCapacity(std::uint64_t RequiredCapacity)
        {
            if (RequiredCapacity <= Capacity())
            {
                return;
            }
            EnsureRelocationIsSafe();

            const auto CurrentSize = Size();
            const auto CurrentCapacity = Capacity();
            const auto NewCapacity = GrowthCapacity(RequiredCapacity);

            auto *NewData = AllocatorTraits::allocate(
                MemberAllocator, static_cast<std::size_t>(NewCapacity));
            std::uint64_t Constructed = 0;
            try
            {
                for (; Constructed < CurrentSize; ++Constructed)
                {
                    AllocatorTraits::construct(MemberAllocator,
                        NewData + Constructed,
                        std::move_if_noexcept(MemberDataPointer[Constructed]));
                }
            }
            catch (...)
            {
                DestroyRange(NewData, NewData + Constructed);
                AllocatorTraits::deallocate(MemberAllocator, NewData,
                    static_cast<std::size_t>(NewCapacity));
                throw;
            }

            if (MemberDataPointer != nullptr)
            {
                DestroyRange(MemberDataPointer, MemberSizePointer);
                AllocatorTraits::deallocate(MemberAllocator,
                    MemberDataPointer,
                    static_cast<std::size_t>(CurrentCapacity));
            }
            MemberDataPointer = NewData;
            MemberSizePointer = NewData + CurrentSize;
            MemberCapacityPointer = NewData + NewCapacity;
        }

        /**
         * @brief 复制构造一个元素并追加到末尾。
         * @param Value 要复制追加的元素。
         * @throws std::length_error 元素数量达到最大值时抛出。
         */
        void AppendConstructed(const VectorType& Value)
        {
            const auto CurrentSize = Size();
            if (CurrentSize == MaxSize())
            {
                throw std::length_error("Vector size exceeds max_size");
            }
            if (CurrentSize == Capacity())
            {
                ReallocateWithTail(CurrentSize + 1,
                    [this, &Value](Iterator NewData,
                        std::uint64_t TailStart,
                        std::uint64_t& ConstructedTail)
                    {
                        AllocatorTraits::construct(MemberAllocator,
                            NewData + TailStart, Value);
                        ++ConstructedTail;
                    });
                return;
            }
            AllocatorTraits::construct(MemberAllocator,
                MemberDataPointer + CurrentSize, Value);
            MemberSizePointer = MemberDataPointer + CurrentSize + 1;
        }

        /**
         * @brief 移动构造一个元素并追加到末尾。
         * @param Value 要移动追加的元素。
         * @throws std::length_error 元素数量达到最大值时抛出。
         */
        void AppendConstructed(VectorType&& Value)
        {
            const auto CurrentSize = Size();
            if (CurrentSize == MaxSize())
            {
                throw std::length_error("Vector size exceeds max_size");
            }
            if (CurrentSize == Capacity())
            {
                ReallocateWithTail(CurrentSize + 1,
                    [this, &Value](Iterator NewData,
                        std::uint64_t TailStart,
                        std::uint64_t& ConstructedTail)
                    {
                        AllocatorTraits::construct(MemberAllocator,
                            NewData + TailStart, std::move(Value));
                        ++ConstructedTail;
                    });
                return;
            }
            AllocatorTraits::construct(MemberAllocator,
                MemberDataPointer + CurrentSize, std::move(Value));
            MemberSizePointer = MemberDataPointer + CurrentSize + 1;
        }

        /**
         * @brief 检查地址是否指向当前向量中的已构造元素。
         * @param Address 要检查的地址。
         * @return 地址位于 `[Begin(), End())` 时返回 true。
         */
        [[nodiscard]] bool ContainsAddress(const VectorType *Address) const noexcept
        {
            if (MemberDataPointer == nullptr || Address == nullptr)
            {
                return false;
            }

            const std::less<const VectorType *> Less;
            return !Less(Address, MemberDataPointer)
                && Less(Address, MemberSizePointer);
        }
    public:
        /**
         * @brief 取得首元素迭代器。
         * @return 指向首元素的迭代器；空向量返回尾后迭代器。
         */
        [[nodiscard]] Iterator Begin() noexcept
        {
            return MemberDataPointer;
        }

        /**
         * @brief 取得尾后迭代器。
         * @return 尾后迭代器。
         */
        [[nodiscard]] Iterator End() noexcept
        {
            return MemberSizePointer;
        }
        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const noexcept
        {
            return MemberDataPointer ? (MemberSizePointer - MemberDataPointer) : 0;
        }

        /**
         * @brief 取得当前容量。
         * @return 当前容量。
         */
        [[nodiscard]] uint64_t Capacity() const noexcept
        {
            return MemberDataPointer ? (MemberCapacityPointer - MemberDataPointer) : 0;
        }

        /**
         * @brief 访问首元素。
         * @return 首元素的可写引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] VectorType &Front()
        {
            return Head();
        }

        /**
         * @brief 访问首元素。
         * @return 首元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] const VectorType &Front() const
        {
            return Head();
        }

        /**
         * @brief 访问末元素。
         * @return 末元素的可写引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] VectorType &Back()
        {
            return Tail();
        }

        /**
         * @brief 访问末元素。
         * @return 末元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] const VectorType &Back() const
        {
            return Tail();
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        [[nodiscard]] bool Empty() const noexcept
        {
            return Size() == 0;
        }

        /**
         * @brief 访问首元素。
         * @return 首元素的可写引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] VectorType &Head()
        {
            if (Empty())
            {
                throw std::out_of_range("Vector::Head called on an empty vector");
            }
            return *MemberDataPointer;
        }

        /**
         * @brief 访问首元素。
         * @return 首元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] const VectorType &Head() const
        {
            if (Empty())
            {
                throw std::out_of_range("Vector::Head called on an empty vector");
            }
            return *MemberDataPointer;
        }

        /**
         * @brief 访问末元素。
         * @return 末元素的可写引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] VectorType &Tail()
        {
            if (Empty())
            {
                throw std::out_of_range("Vector::Tail called on an empty vector");
            }
            return *(MemberSizePointer - 1);
        }

        /**
         * @brief 访问末元素。
         * @return 末元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] const VectorType &Tail() const
        {
            if (Empty())
            {
                throw std::out_of_range("Vector::Tail called on an empty vector");
            }
            return *(MemberSizePointer - 1);
        }

        /**
         * @brief 构造 Vector 对象。
         */
        Vector() noexcept = default;

        /**
         * @brief 构造 Vector 对象。
         * @param ContainerCapacity 初始元素数量。
         */
        explicit Vector(const uint64_t& ContainerCapacity)
            requires std::default_initializable<VectorType>
        {
            SizeAdjust(ContainerCapacity, VectorType{});
        }

        /**
         * @brief 创建指定元素数量并填充同一值的向量。
         * @param ContainerCapacity 初始元素数量。
         * @param VectorData 用于初始化每个元素的值。
         */
        Vector(const uint64_t& ContainerCapacity, const VectorType& VectorData)
        {
            SizeAdjust(ContainerCapacity, VectorData);
        }

        /**
         * @brief 从初始化列表复制元素，创建向量。
         * @param LightweightContainer 要复制的元素列表。
         */
        Vector(std::initializer_list<VectorType> LightweightContainer)
        {
            EnsureCapacity(LightweightContainer.size());
            try
            {
                for (const auto& Value : LightweightContainer)
                {
                    AllocatorTraits::construct(MemberAllocator,
                        MemberDataPointer + Size(), Value);
                    MemberSizePointer = MemberDataPointer + Size() + 1;
                }
            }
            catch (...)
            {
                Release();
                throw;
            }
        }
        /**
         * @brief 按索引访问元素。
         * @param FindSize 元素索引。
         * @return 指定索引处元素的引用。
         * @throws CustomException::Fault 索引超出元素范围时抛出。
         */
        VectorType &Find(const uint64_t &FindSize)
        {
            try
            {
                if (FindSize >= Size())
                {
                    throw CustomException::Fault("传入数据超出容器范围", "vector::find", __LINE__);
                }
                else
                {
                    return MemberDataPointer[FindSize];
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
        }
        /**
         * @brief 调整元素数量。
         * @param DataSize 调整后的元素数量。
         * @return 当前对象的引用。
         * @throws std::length_error 请求容量超过最大值时抛出。
         * @note 此重载默认构造填充值后复制到新增位置，因此元素类型还须可复制构造。
         */
        Vector<VectorType>& SizeAdjust(std::uint64_t DataSize)
            requires std::default_initializable<VectorType>
        {
            return SizeAdjust(DataSize, VectorType{});
        }

        /**
         * @brief 调整元素数量。
         * @param DataSize 调整后的元素数量。
         * @param PaddingTempData 新增元素的填充值。
         * @return 当前对象的引用。
         * @throws std::length_error 请求容量超过最大值时抛出。
         * @note 元素复制抛出的异常会向调用方传播。
         */
        Vector<VectorType>& SizeAdjust(std::uint64_t DataSize,
            const VectorType& PaddingTempData)
        {
            const auto CurrentSize = Size();
            if (DataSize > MaxSize())
            {
                throw std::length_error("Vector size exceeds max_size");
            }
            if (DataSize < CurrentSize)
            {
                DestroyRange(MemberDataPointer + DataSize,
                    MemberSizePointer);
                MemberSizePointer = MemberDataPointer + DataSize;
                return *this;
            }

            if (DataSize == CurrentSize)
            {
                return *this;
            }

            std::optional<std::uint64_t> PaddingIndex;
            if (ContainsAddress(std::addressof(PaddingTempData)))
            {
                PaddingIndex = static_cast<std::uint64_t>(
                    std::addressof(PaddingTempData) - MemberDataPointer);
            }

            const VectorType& FillValue = PaddingIndex
                ? MemberDataPointer[*PaddingIndex] : PaddingTempData;
            if (DataSize > Capacity())
            {
                ReallocateWithTail(DataSize,
                    [this, &FillValue, DataSize](Iterator NewData,
                        std::uint64_t TailStart,
                        std::uint64_t& ConstructedTail)
                    {
                        while (TailStart + ConstructedTail < DataSize)
                        {
                            AllocatorTraits::construct(MemberAllocator,
                                NewData + TailStart + ConstructedTail,
                                FillValue);
                            ++ConstructedTail;
                        }
                    });
                return *this;
            }

            const auto OldEnd = MemberSizePointer;
            try
            {
                while (MemberSizePointer < MemberDataPointer + DataSize)
                {
                    AllocatorTraits::construct(MemberAllocator,
                        MemberSizePointer, FillValue);
                    ++MemberSizePointer;
                }
            }
            catch (...)
            {
                DestroyRange(OldEnd, MemberSizePointer);
                MemberSizePointer = OldEnd;
                throw;
            }
            return *this;
        }
        /**
         * @brief 深拷贝另一个向量。
         * @param VectorData 要复制的向量。
         */
        Vector(const Vector<VectorType> &VectorData)
        {
            try
            {
                Resize(VectorData.Capacity());
                for (std::uint64_t I = 0; I < VectorData.Size(); ++I)
                {
                    AppendConstructed(VectorData.MemberDataPointer[I]);
                }
            }
            catch (...)
            {
                Release();
                throw;
            }
        }
        /**
         * @brief 移动另一个向量的存储。
         * @param VectorData 要移出的向量；操作后为空。
         */
        Vector(Vector<VectorType>&& VectorData) noexcept
            : MemberDataPointer(VectorData.MemberDataPointer)
            , MemberSizePointer(VectorData.MemberSizePointer)
            , MemberCapacityPointer(VectorData.MemberCapacityPointer)
        {
            VectorData.MemberDataPointer = nullptr;
            VectorData.MemberSizePointer = nullptr;
            VectorData.MemberCapacityPointer = nullptr;
        }
        /**
         * @brief 销毁 Vector 对象。
         */
        ~Vector() noexcept
        {
            Release();
        }
        /**
         * @brief 交换两个对象的内容。
         * @param VectorData 要交换内容的向量。
         */
        void Swap(Vector<VectorType> &VectorData) noexcept
        {
            using std::swap;
            swap(MemberDataPointer, VectorData.MemberDataPointer);
            swap(MemberSizePointer, VectorData.MemberSizePointer);
            swap(MemberCapacityPointer, VectorData.MemberCapacityPointer);
        }
        /**
         * @brief 删除指定位置的元素。
         * @param DeletePosition 待删除元素的迭代器。
         * @return 被删位置之后的迭代器。
         * @note 删除位置及其后的迭代器失效；无效位置不执行删除。
         */
        Iterator Erase(Iterator DeletePosition)
        {
            if (MemberDataPointer == nullptr || DeletePosition == nullptr
                || DeletePosition == MemberSizePointer)
            {
                return MemberSizePointer;
            }

            const std::less<Iterator> Less;
            if (Less(DeletePosition, MemberDataPointer)
                || !Less(DeletePosition, MemberSizePointer))
            {
                return MemberSizePointer;
            }

            for (Iterator Current = DeletePosition;
                 Current + 1 != MemberSizePointer; ++Current)
            {
                *Current = std::move(*(Current + 1));
            }
            --MemberSizePointer;
            AllocatorTraits::destroy(MemberAllocator, MemberSizePointer);
            return DeletePosition;
        }

        /**
         * @brief 预留至少指定容量，不改变元素数量。
         * @param NewContainerCapacity 要预留的最小元素容量。
         * @return 当前对象的引用。
         * @throws std::length_error 请求容量超过最大值时抛出。
         */
        Vector<VectorType>& Resize(const std::uint64_t& NewContainerCapacity)
        {
            EnsureCapacity(NewContainerCapacity);
            return *this;
        }

        /**
         * @brief 预留至少指定容量，不改变元素数量。
         * @param NewContainerCapacity 要预留的最小元素容量。
         * @return 当前对象的引用。
         * @throws std::length_error 请求容量超过最大值时抛出。
         */
        Vector<VectorType>& Resize(const std::uint64_t& NewContainerCapacity,
            const VectorType&)
        {
            return Resize(NewContainerCapacity);
        }

        /**
         * @brief 取得允许的最大容量。
         * @return 最大可用元素容量。
         */
        [[nodiscard]] auto MaxCapacity() const noexcept -> std::uint64_t
        {
            return MaxSize();
        }
        /**
         * @brief 在末尾追加元素。
         * @param VectorTypeData 要复制到末尾的元素。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &PushBack(const VectorType &VectorTypeData)
        {
            if (ContainsAddress(&VectorTypeData))
            {
                VectorType StagedData(VectorTypeData);
                return PushBack(std::move(StagedData));
            }
            AppendConstructed(VectorTypeData);
            return *this;
        }
        /**
         * @brief 在末尾追加元素。
         * @param VectorTypeData 要移动到末尾的元素。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &PushBack(VectorType &&VectorTypeData)
        {
            if (ContainsAddress(&VectorTypeData))
            {
                if constexpr (std::is_copy_constructible_v<VectorType>)
                {
                    VectorType StagedData(VectorTypeData);
                    return PushBack(std::move(StagedData));
                }
                else
                {
                    VectorType StagedData(std::move(VectorTypeData));
                    return PushBack(std::move(StagedData));
                }
            }
            AppendConstructed(std::move(VectorTypeData));
            return *this;
        }
        /**
         * @brief 删除末尾元素。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &PopBack()
        {
            if (!Empty())
            {
                --MemberSizePointer;
                AllocatorTraits::destroy(MemberAllocator, MemberSizePointer);
            }
            return *this;
        }
        /**
         * @brief 在开头插入元素。
         * @param VectorTypeData 要复制到开头的元素。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &PushFront(const VectorType &VectorTypeData)
        {
            if (ContainsAddress(&VectorTypeData))
            {
                VectorType StagedData(VectorTypeData);
                return PushFront(std::move(StagedData));
            }

            const auto CurrentSize = Size();
            if (CurrentSize == MaxSize())
            {
                throw std::length_error("Vector size exceeds max_size");
            }
            if (CurrentSize == Capacity())
            {
                ReallocateWithPrepend(VectorTypeData);
                return *this;
            }
            if (CurrentSize == 0)
            {
                AllocatorTraits::construct(MemberAllocator,
                    MemberDataPointer, VectorTypeData);
                MemberSizePointer = MemberDataPointer + 1;
                return *this;
            }

            AllocatorTraits::construct(MemberAllocator,
                MemberDataPointer + CurrentSize,
                std::move_if_noexcept(MemberDataPointer[CurrentSize - 1]));
            try
            {
                for (std::uint64_t I = CurrentSize - 1; I > 0; --I)
                {
                    MemberDataPointer[I] =
                        std::move_if_noexcept(MemberDataPointer[I - 1]);
                }
                MemberDataPointer[0] = VectorTypeData;
            }
            catch (...)
            {
                AllocatorTraits::destroy(MemberAllocator,
                    MemberDataPointer + CurrentSize);
                throw;
            }
            MemberSizePointer = MemberDataPointer + CurrentSize + 1;
            return *this;
        }

        /**
         * @brief 在开头插入元素。
         * @param VectorTypeData 要移动到开头的元素。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &PushFront(VectorType&& VectorTypeData)
        {
            if (ContainsAddress(std::addressof(VectorTypeData)))
            {
                if constexpr (std::is_copy_constructible_v<VectorType>)
                {
                    VectorType StagedData(VectorTypeData);
                    return PushFront(std::move(StagedData));
                }
                else
                {
                    VectorType StagedData(std::move(VectorTypeData));
                    return PushFront(std::move(StagedData));
                }
            }

            const auto CurrentSize = Size();
            if (CurrentSize == MaxSize())
            {
                throw std::length_error("Vector size exceeds max_size");
            }
            if (CurrentSize == Capacity())
            {
                ReallocateWithPrepend(std::move(VectorTypeData));
                return *this;
            }
            if (CurrentSize == 0)
            {
                AllocatorTraits::construct(MemberAllocator,
                    MemberDataPointer, std::move(VectorTypeData));
                MemberSizePointer = MemberDataPointer + 1;
                return *this;
            }

            AllocatorTraits::construct(MemberAllocator,
                MemberDataPointer + CurrentSize,
                std::move_if_noexcept(MemberDataPointer[CurrentSize - 1]));
            try
            {
                for (std::uint64_t I = CurrentSize - 1; I > 0; --I)
                {
                    MemberDataPointer[I] =
                        std::move_if_noexcept(MemberDataPointer[I - 1]);
                }
                MemberDataPointer[0] = std::move(VectorTypeData);
            }
            catch (...)
            {
                AllocatorTraits::destroy(MemberAllocator,
                    MemberDataPointer + CurrentSize);
                throw;
            }
            MemberSizePointer = MemberDataPointer + CurrentSize + 1;
            return *this;
        }

        /**
         * @brief 删除首元素。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &PopFront()
        {
            const auto CurrentSize = Size();
            if (CurrentSize > 0)
            {
                for (std::uint64_t I = 1; I < CurrentSize; ++I)
                {
                    MemberDataPointer[I - 1] =
                        std::move(MemberDataPointer[I]);
                }
                --MemberSizePointer;
                AllocatorTraits::destroy(MemberAllocator, MemberSizePointer);
            }
            return *this;
        }
        /**
         * @brief 按索引访问元素。
         * @param AccessLocation 元素索引。
         * @return 索引处元素的引用。
         * @throws CustomException::Fault 索引超出元素范围时抛出。
         */
        VectorType &operator[](const uint64_t &AccessLocation)
        {
            try
            {
                if (AccessLocation >= Size())
                {
                    throw CustomException::Fault("传入参数越界", "vector::operator[]", __LINE__);
                }
                else
                {
                    return MemberDataPointer[AccessLocation];
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
        }
        /**
         * @brief 按索引访问元素。
         * @param AccessLocation 元素索引。
         * @return 索引处元素的引用。
         * @throws CustomException::Fault 索引超出元素范围时抛出。
         */
        const VectorType &operator[](const uint64_t &AccessLocation) const
        {
            // return MemberDataPointer[AccessLocation];
            try
            {
                if (AccessLocation >= Size())
                {
                    throw CustomException::Fault("传入参数越界", "vector::operator[]", __LINE__);
                }
                else
                {
                    return MemberDataPointer[AccessLocation];
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
        }
        /**
         * @brief 深拷贝赋值。
         * @param VectorData 要复制的向量。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &operator=(const Vector<VectorType> &VectorData)
        {
            if (this != &VectorData)
            {
                Vector<VectorType> ReturnVectorObject(VectorData); // 拷贝构造
                Swap(ReturnVectorObject);                            // 交换资源，temp析构时会释放原资源
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param VectorMobileData 要移出的向量；操作后为空。
         * @return 当前对象的引用。
         */
        Vector<VectorType> &operator=(Vector<VectorType> &&VectorMobileData) noexcept
        {
            if (this != &VectorMobileData)
            {
                Release();
                MemberDataPointer = VectorMobileData.MemberDataPointer;
                MemberSizePointer = VectorMobileData.MemberSizePointer;
                MemberCapacityPointer = VectorMobileData.MemberCapacityPointer;
                VectorMobileData.MemberDataPointer = nullptr;
                VectorMobileData.MemberSizePointer = nullptr;
                VectorMobileData.MemberCapacityPointer = nullptr;
            }
            return *this;
        }
        /**
         * @brief 将另一向量的元素依次追加到末尾。
         * @param VectorData 要追加的向量。
         * @return 当前向量的引用。
         */
        Vector<VectorType> &operator+=(const Vector<VectorType> &VectorData)
        {
            if (VectorData.Empty())
            {
                return *this;
            }

            if (this == &VectorData)
            {
                Vector Copy{VectorData};
                return operator+=(Copy);
            }

            const auto OldSize = Size();
            const auto OtherSize = VectorData.Size();
            if (OtherSize > MaxSize() - OldSize)
            {
                throw std::length_error("Vector size exceeds max_size");
            }
            const auto NewSize = OldSize + OtherSize;
            if (NewSize > Capacity())
            {
                ReallocateWithTail(NewSize,
                    [this, &VectorData, OtherSize](Iterator NewData,
                        std::uint64_t TailStart,
                        std::uint64_t& ConstructedTail)
                    {
                        while (ConstructedTail < OtherSize)
                        {
                            AllocatorTraits::construct(MemberAllocator,
                                NewData + TailStart + ConstructedTail,
                                VectorData.MemberDataPointer[
                                    ConstructedTail]);
                            ++ConstructedTail;
                        }
                    });
                return *this;
            }
            EnsureCapacity(NewSize);
            std::uint64_t Constructed = 0;
            try
            {
                for (; Constructed < OtherSize; ++Constructed)
                {
                    AllocatorTraits::construct(MemberAllocator,
                        MemberDataPointer + OldSize + Constructed,
                        VectorData.MemberDataPointer[Constructed]);
                }
            }
            catch (...)
            {
                DestroyRange(MemberDataPointer + OldSize,
                    MemberDataPointer + OldSize + Constructed);
                throw;
            }
            MemberSizePointer = MemberDataPointer + NewSize;
            return *this;
        }
        /**
         * @brief 向流写入对象。
         * @tparam ConstVectorOutputTemplates 输出向量中元素的类型。
         * @param VectorOstream 输出流。
         * @param DynamicArraysData 要写入流的向量。
         * @return 输入或输出流的引用。
         */
        template <typename ConstVectorOutputTemplates>
        friend std::ostream &operator<<(std::ostream &VectorOstream, const Vector<ConstVectorOutputTemplates> &DynamicArraysData);
    };
    template <typename ConstVectorOutputTemplates>
    std::ostream &operator<<(std::ostream &VectorOstream, const Vector<ConstVectorOutputTemplates> &DynamicArraysData)
    {
        for (uint64_t InputTraversal = 0; InputTraversal < DynamicArraysData.Size(); InputTraversal++)
        {
            VectorOstream << DynamicArraysData[InputTraversal] << " ";
        }
        return VectorOstream;
    }
}
namespace StandardCon
{
    using VectorContainer::Vector;
}
