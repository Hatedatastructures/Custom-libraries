/**
 * @file Pointer.hpp
 * @brief 独占、共享和弱引用指针包装器。
 */
#pragma once
#include <memory>
#include <type_traits>
#include "Exception.hpp"
namespace StandardCon
{
    /**
     * @brief 智能指针类型的命名空间。
     */
    namespace Pointer
    {
    }
}

namespace StandardCon::Pointer
{
    /**
     * @brief 通过删除器独占管理一个对象。
     * @tparam SmartPtrType 被管理的对象类型。
     * @tparam Deleter 释放对象的删除器类型。
     */
    template <typename SmartPtrType, typename Deleter = std::default_delete<SmartPtrType>>
    class SmartPtr
    {
    private:
        /** @brief 当前独占管理的原始指针；无对象时为空。 */
        SmartPtrType *MemberPtr;
        /** @brief 释放 MemberPtr 所指对象的删除器。 */
        Deleter MemberDeleter;
        /** @brief 被管理对象的引用类型。 */
        using Ref = SmartPtrType &;
        /** @brief 被管理对象的指针类型。 */
        using Ptr = SmartPtrType *;

    public:
        /**
         * @brief 构造 SmartPtr 对象。
         * @param Ptr 交由此指针管理的原始指针。
         */
        explicit SmartPtr(SmartPtrType *Ptr = nullptr)
            : MemberPtr(Ptr), MemberDeleter{}
        {
        }
        /**
         * @brief 构造 SmartPtr 对象。
         */
        SmartPtr(const SmartPtr &) = delete;
        /**
         * @brief 禁止从另一 SmartPtr 复制赋值。
         * @param SmartPtrData 复制赋值所用的源指针。
         * @return 当前对象的引用。
         */
        SmartPtr &operator=(const SmartPtr *&SmartPtrData) = delete;
        /**
         * @brief 移动构造并接管另一个指针的所有权。
         * @param SmartPtrData 要移出的指针；操作后不再管理对象。
         */
        SmartPtr(SmartPtr &&SmartPtrData)
            noexcept(std::is_nothrow_move_constructible_v<Deleter>)
            : MemberPtr(SmartPtrData.MemberPtr),
              MemberDeleter(std::move(SmartPtrData.MemberDeleter))
        {
            SmartPtrData.MemberPtr = nullptr;
        }
        /**
         * @brief 移动赋值并接管另一个指针的所有权。
         * @param SmartPtrData 要移出的指针；操作后不再管理对象。
         * @return 当前对象的引用。
         */
        SmartPtr &operator=(SmartPtr &&SmartPtrData)
            noexcept(std::is_nothrow_move_assignable_v<Deleter>
                && std::is_nothrow_invocable_v<Deleter&, SmartPtrType*>)
        {
            if (&SmartPtrData != this)
            {
                if (MemberPtr != nullptr)
                {
                    MemberDeleter(MemberPtr);
                }
                MemberPtr = SmartPtrData.MemberPtr;
                MemberDeleter = std::move(SmartPtrData.MemberDeleter);
                SmartPtrData.MemberPtr = nullptr;
            }
            return *this;
        }
        /**
         * @brief 销毁 SmartPtr 对象。
         */
        ~SmartPtr() noexcept
        {
            if (MemberPtr != nullptr)
            {
                MemberDeleter(MemberPtr);
                MemberPtr = nullptr;
            }
        }
        /**
         * @brief 返回被管理对象的指针。
         * @return 被管理对象的指针；未持有对象时为空指针。
         */
        Ptr operator->() const noexcept
        {
            return MemberPtr;
        }
        /**
         * @brief 解引用被管理对象。
         * @return 被管理对象的引用。
         * @note 调用前必须确认指针非空。
         */
        Ref operator*() noexcept
        {
            return *(MemberPtr);
        }
    };
    /**
     * @brief 独占管理一个对象并支持所有权转移。
     * @tparam UniquePtrType 被管理的对象类型。
     * @tparam Deleter 释放对象的删除器类型。
     */
    template <typename UniquePtrType, typename Deleter = std::default_delete<UniquePtrType>>
    class UniquePtr
    {
    private:
        /** @brief 当前独占管理的原始指针；无对象时为空。 */
        UniquePtrType *MemberPtr;
        /** @brief 释放 MemberPtr 所指对象的删除器。 */
        Deleter MemberDeleter;
        /** @brief 被管理对象的引用类型。 */
        using Ref = UniquePtrType &;
        /** @brief 被管理对象的指针类型。 */
        using Ptr = UniquePtrType *;

    public:
        /**
         * @brief 构造 UniquePtr 对象。
         * @param Ptr 交由此指针管理的原始指针。
         */
        explicit UniquePtr(UniquePtrType *Ptr = nullptr)
            noexcept(std::is_nothrow_default_constructible_v<Deleter>)
                : MemberDeleter(Deleter())
        {
            MemberPtr = Ptr;
        }
        /**
         * @brief 销毁 UniquePtr 对象。
         */
        ~UniquePtr() noexcept
        {
            if (MemberPtr != nullptr)
            {
                MemberDeleter(MemberPtr);
                MemberPtr = nullptr;
            }
        }
        /**
         * @brief 解引用被管理对象。
         * @return 被管理对象的引用。
         * @note 调用前必须确认指针非空。
         */
        Ref operator*() noexcept
        {
            return *(MemberPtr);
        }
        /**
         * @brief 返回被管理对象的指针。
         * @return 被管理对象的指针；未持有对象时为空指针。
         */
        Ptr operator->() noexcept
        {
            return MemberPtr;
        }
        /**
         * @brief 取得原始指针。
         * @return 被管理对象的原始指针，不转移所有权。
         */
        UniquePtrType *GetPtr() const noexcept
        {
            return MemberPtr;
        }
        /**
         * @brief 禁止复制构造。
         * @param UniquePtrData 复制构造所用的源指针。
         */
        UniquePtr(const UniquePtr &UniquePtrData) noexcept = delete;
        /**
         * @brief 禁止复制赋值。
         * @param UniquePtrData 复制赋值所用的源指针。
         * @return 当前对象的引用。
         */
        UniquePtr &operator=(const UniquePtr &UniquePtrData) noexcept = delete;
        /**
         * @brief 移动赋值并接管另一个指针的所有权。
         * @param UniquePtrData 要移出的指针；操作后不再管理对象。
         * @return 当前对象的引用。
         */
        UniquePtr &operator=(UniquePtr &&UniquePtrData)
            noexcept(std::is_nothrow_move_assignable_v<Deleter>
                && std::is_nothrow_invocable_v<Deleter&, UniquePtrType*>)
        {
            if (&UniquePtrData != this)
            {
                if (MemberPtr != nullptr)
                {
                    MemberDeleter(MemberPtr);
                    MemberPtr = nullptr;
                }
                MemberPtr = UniquePtrData.MemberPtr;
                MemberDeleter = std::move(UniquePtrData.MemberDeleter);
                UniquePtrData.MemberPtr = nullptr;
            }
            return *this;
        }
        /**
         * @brief 移动构造并接管另一个指针的所有权。
         * @param UniquePtrData 要移出的指针；操作后不再管理对象。
         */
        UniquePtr(UniquePtr &&UniquePtrData)
            noexcept(std::is_nothrow_move_constructible_v<Deleter>)
            : MemberPtr(UniquePtrData.MemberPtr),
              MemberDeleter(std::move(UniquePtrData.MemberDeleter))
        {
            UniquePtrData.MemberPtr = nullptr;
        }
    };
    /**
     * @brief 共享对象所有权的指针。
     * @tparam SharedPtrType 被管理的对象类型。
     * @tparam Deleter 释放对象的删除器类型。
     */
    template <typename SharedPtrType, typename Deleter = std::default_delete<SharedPtrType>>
    class SharedPtr
    {
    private:
        /** @brief 共享管理对象生命周期的标准共享指针。 */
        std::shared_ptr<SharedPtrType> MemberPtr;
        template <typename>
        friend class WeakPtr;

        /**
         * @brief 从标准共享指针构造包装器。
         * @param Ptr 已取得共享所有权的标准指针。
         */
        explicit SharedPtr(std::shared_ptr<SharedPtrType> Ptr) noexcept
            : MemberPtr(std::move(Ptr))
        {
        }

        /** @brief 被管理对象的引用类型。 */
        using Ref = SharedPtrType &;
        /** @brief 被管理对象的指针类型。 */
        using Ptr = SharedPtrType *;

    public:
        /**
         * @brief 构造 SharedPtr 对象。
         * @param Ptr 交由此指针管理的原始指针。
         */
        explicit SharedPtr(SharedPtrType *Ptr = nullptr)
        {
            if (Ptr != nullptr)
            {
                MemberPtr = std::shared_ptr<SharedPtrType>(Ptr, Deleter{});
            }
        }
        /**
         * @brief 构造 SharedPtr 对象。
         */
        SharedPtr(const SharedPtr&) noexcept = default;
        /**
         * @brief 构造 SharedPtr 对象。
         */
        SharedPtr(SharedPtr&&) noexcept = default;
        /**
         * @brief 赋值当前对象。
         * @return 当前对象的引用。
         */
        SharedPtr& operator=(const SharedPtr&) noexcept = default;
        /**
         * @brief 赋值当前对象。
         * @return 当前对象的引用。
         */
        SharedPtr& operator=(SharedPtr&&) noexcept = default;
        /**
         * @brief 销毁 SharedPtr 对象。
         */
        ~SharedPtr() noexcept = default;

        /**
         * @brief 取得共享所有者数量。
         * @return 共享所有者数量。
         */
        [[nodiscard]] int GetCount() const noexcept
        {
            return static_cast<int>(MemberPtr.use_count());
        }

        /**
         * @brief 判断是否持有对象。
         * @return 持有对象时返回 true。
         */
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(MemberPtr);
        }

        /**
         * @brief 解引用被管理对象。
         * @return 被管理对象的引用。
         * @note 调用前必须确认指针非空。
         */
        Ref operator*() noexcept
        {
            return *(MemberPtr);
        }
        /**
         * @brief 返回被管理对象的指针。
         * @return 被管理对象的指针；未持有对象时为空指针。
         */
        Ptr operator->() noexcept
        {
            return MemberPtr.get();
        }
        /**
         * @brief 取得原始指针。
         * @return 被管理对象的原始指针，不转移所有权。
         */
        Ptr GetPtr() const noexcept
        {
            return MemberPtr.get();
        }
    };
    /**
     * @brief 观察共享对象但不延长其生命周期。
     * @tparam WeakPtrType 被观察的对象类型。
     */
    template <typename WeakPtrType>
    class WeakPtr
    {
    private:
        /** @brief 不延长对象生命周期的标准弱引用。 */
        std::weak_ptr<WeakPtrType> MemberWeak;
        /** @brief 区分默认未绑定状态和已绑定后过期的状态。 */
        bool MemberBound = false;

    public:
        /**
         * @brief 构造 WeakPtr 对象。
         */
        WeakPtr() noexcept = default;
        /**
         * @brief 从共享指针创建弱引用。
         * @param WeakPtrData 要观察的共享指针。
         */
        explicit WeakPtr(const SharedPtr<WeakPtrType> &WeakPtrData) noexcept
            : MemberWeak(WeakPtrData.MemberPtr),
              MemberBound(true)
        {
        }
        /**
         * @brief 构造 WeakPtr 对象。
         */
        WeakPtr(const WeakPtr&) noexcept = default;
        /**
         * @brief 构造 WeakPtr 对象。
         */
        WeakPtr(WeakPtr&&) noexcept = default;
        /**
         * @brief 构造 WeakPtr 对象。
         */
        WeakPtr(SharedPtr<WeakPtrType> &&) = delete;
        /**
         * @brief 赋值当前对象。
         * @return 当前对象的引用。
         */
        WeakPtr &operator=(const WeakPtr&) noexcept = default;
        /**
         * @brief 赋值当前对象。
         * @return 当前对象的引用。
         */
        WeakPtr &operator=(WeakPtr&&) noexcept = default;
        /**
         * @brief 将弱引用改为观察指定共享指针。
         * @param SharedPtrData 要观察的共享指针。
         * @return 当前对象的引用。
         */
        WeakPtr &operator=(const SharedPtr<WeakPtrType> &SharedPtrData) noexcept
        {
            MemberWeak = SharedPtrData.MemberPtr;
            MemberBound = true;
            return *this;
        }
        /**
         * @brief 销毁 WeakPtr 对象。
         */
        ~WeakPtr() noexcept = default;

        /**
         * @brief 尝试取得共享所有权。
         * @return 对象仍存在时返回共享指针，否则返回空指针。
         */
        [[nodiscard]] auto Lock() const noexcept -> SharedPtr<WeakPtrType>
        {
            return SharedPtr<WeakPtrType>{MemberWeak.lock()};
        }

        /**
         * @brief 判断观察的对象是否已失效。
         * @return 对象已失效或未绑定时返回 true。
         */
        bool Expired() const noexcept
        {
            return !MemberBound || MemberWeak.expired();
        }
        /**
         * @brief 取得共享所有者数量。
         * @return 共享所有者数量；未绑定时返回 -1。
         */
        int GetCount() const noexcept
        {
            return MemberBound ? static_cast<int>(MemberWeak.use_count()) : -1;
        }
    };
}
