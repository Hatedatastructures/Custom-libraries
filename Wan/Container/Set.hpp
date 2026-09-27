/**
 * @file Set.hpp
 * @brief 有序集合和哈希集合。
 */
#pragma once
#include "Base.hpp"
namespace SetContainer
{
    /**
     * @brief 按比较器排序且不保存重复键的集合。
     * @tparam SetType 元素类型。
     * @tparam Comparators 元素的比较器类型。
     */
    template <typename SetType, typename Comparators = StandardCon::Less<SetType>>
    class TreeSet
    {
        /** @brief 集合中保存的元素类型。 */
        using KeyValType = SetType; // Comparators 用户自定义比较器，用于比较两个元素的大小，方便存储
        /** @brief 将集合元素直接作为树键。 */
        struct KeyVal
        {
            /** @brief 返回元素本身作为排序键。
             * @param KeyValue 要提取排序键的元素。
             * @return 元素值。
             */
            SetType operator()(const KeyValType &KeyValue)
            {
                return KeyValue;
            }
        };
        /** @brief 按比较器维护元素顺序的底层红黑树类型。 */
        using InstanceRb = StandardCon::RedBlackTree<SetType, KeyValType, KeyVal, Comparators>;
        /** @brief 保存有序集合元素的红黑树。 */
        InstanceRb InstanceTreeSet;

    public:
        /** @brief 按元素升序遍历的只读迭代器。 */
        using ConstIterator = typename InstanceRb::ConstIterator;
        /** @brief 按元素降序遍历的只读反向迭代器。 */
        using ConstReverseIterator = typename InstanceRb::ConstReverseIterator;
        /** @brief 与 ConstIterator 相同，集合元素不可通过迭代器修改。 */
        using Iterator = ConstIterator;
        /** @brief 与 ConstReverseIterator 相同的只读反向迭代器。 */
        using ReverseIterator = ConstReverseIterator;

        /** @brief 插入操作的结果：迭代器和是否新增。 */
        using SetIterator = StandardCon::Pair<Iterator, bool>;
        /**
         * @brief 深拷贝赋值。
         * @param SetData 要复制的集合。
         * @return 当前对象的引用。
         */
        TreeSet &operator=(const TreeSet &SetData)
        {
            if (this != &SetData)
            {
                InstanceTreeSet = SetData.InstanceTreeSet;
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param SetData 要移出的集合。
         * @return 当前对象的引用。
         */
        TreeSet &operator=(TreeSet &&SetData)
        {
            if (this != &SetData)
            {
                InstanceTreeSet = std::move(SetData.InstanceTreeSet);
            }
            return *this;
        }
        /**
         * @brief 从初始化列表构造有序集合。
         * @param LightweightContainer 要插入的元素列表。
         */
        TreeSet(std::initializer_list<KeyValType> LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                InstanceTreeSet.Push(std::move(ChainedValues));
            }
        }
        /**
         * @brief 将初始化列表中的元素插入集合，不清除已有元素。
         * @param LightweightContainer 要插入的元素列表。
         * @return 当前对象的引用。
         */
        TreeSet &operator=(std::initializer_list<KeyValType> LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                InstanceTreeSet.Push(std::move(ChainedValues));
            }
            return *this;
        }
        /**
         * @brief 移动插入元素；相等元素已存在时不插入。
         * @param SetData 要插入的元素。
         * @return 指向已有或新元素的只读迭代器，以及是否新增。
         */
        SetIterator Push(KeyValType &&SetData)
        {
            auto Result = InstanceTreeSet.Push(
                std::forward<KeyValType>(SetData));
            return SetIterator{Result.First.AsConst(), Result.Second};
        }
        /**
         * @brief 用一个元素构造有序集合。
         * @param SetTypeData 要插入的元素。
         */
        explicit TreeSet(KeyValType &&SetTypeData)
        {
            InstanceTreeSet.Push(std::forward<KeyValType>(SetTypeData));
        }

        /**
         * @brief 构造 TreeSet 对象。
         */
        TreeSet() { ; }

        /**
         * @brief 销毁 TreeSet 对象。
         */
        ~TreeSet() = default;

        /**
         * @brief 深拷贝另一个有序集合。
         * @param SetData 要复制的集合。
         */
        TreeSet(const TreeSet &SetData) { InstanceTreeSet = SetData.InstanceTreeSet; }

        /**
         * @brief 移动另一个有序集合。
         * @param SetData 要移出的集合。
         */
        TreeSet(TreeSet &&SetData) { InstanceTreeSet = std::move(SetData.InstanceTreeSet); }

        /**
         * @brief 用一个元素构造有序集合。
         * @param SetTypeData 要复制并插入的元素。
         */
        explicit TreeSet(const KeyValType &SetTypeData) { InstanceTreeSet.Push(SetTypeData); }

        /**
         * @brief 插入元素；相等元素已存在时不插入。
         * @param SetTypeData 要插入的元素。
         * @return 指向已有或新元素的只读迭代器，以及是否新增。
         */
        SetIterator Push(const KeyValType &SetTypeData)
        {
            auto Result = InstanceTreeSet.Push(SetTypeData);
            return SetIterator{Result.First.AsConst(), Result.Second};
        }

        /**
         * @brief 删除指定元素。
         * @param SetTypeData 要删除的元素。
         * @return 第二项表示是否删除成功；第一项为尾后迭代器。
         */
        SetIterator Pop(const KeyValType &SetTypeData)
        {
            auto Result = InstanceTreeSet.Pop(SetTypeData);
            return SetIterator{Result.First.AsConst(), Result.Second};
        }

        /**
         * @brief 查找指定元素。
         * @param SetTypeData 要查找的元素。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const KeyValType &SetTypeData)
        {
            return InstanceTreeSet.Find(SetTypeData).AsConst();
        }

        /**
         * @brief 按中序输出元素。
         */
        void MiddleOrderTraversal() { InstanceTreeSet.MiddleOrderTraversal(); }

        /**
         * @brief 按前序输出元素。
         */
        void PreOrderTraversal() { InstanceTreeSet.PreOrderTraversal(); }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return InstanceTreeSet.Size();
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        bool Empty() { return InstanceTreeSet.Empty(); }

        /**
         * @brief 取得首元素迭代器。
         * @return 指向最小元素的只读迭代器；空集合返回尾后迭代器。
         */
        Iterator Begin() { return InstanceTreeSet.Cbegin(); }

        /**
         * @brief 取得尾后迭代器。
         * @return 只读尾后迭代器。
         */
        Iterator End() { return InstanceTreeSet.Cend(); }

        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向最小元素的只读迭代器；空集合返回尾后迭代器。
         */
        ConstIterator Cbegin() { return InstanceTreeSet.Cbegin(); }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 只读尾后迭代器。
         */
        ConstIterator Cend() { return InstanceTreeSet.Cend(); }

        /**
         * @brief 取得反向首元素迭代器。
         * @return 指向最大元素的只读反向迭代器；空集合返回反向尾后迭代器。
         */
        ReverseIterator Rbegin() { return InstanceTreeSet.Crbegin(); }

        /**
         * @brief 取得反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        ReverseIterator Rend() { return InstanceTreeSet.Crend(); }

        /**
         * @brief 取得只读反向首元素迭代器。
         * @return 指向最大元素的只读反向迭代器；空集合返回反向尾后迭代器。
         */
        ConstReverseIterator Crbegin() { return InstanceTreeSet.Crbegin(); }

        /**
         * @brief 取得只读反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        ConstReverseIterator Crend() { return InstanceTreeSet.Crend(); }

        /**
         * @brief 按键查找元素。
         * @param SetTypeData 要查找的元素。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator operator[](const KeyValType &SetTypeData)
        {
            return InstanceTreeSet[SetTypeData].AsConst();
        }
    };
/**
     * @brief 按哈希值分桶且不保存重复键的集合。
     * @tparam SetTypeVal 元素类型。
     * @tparam ExternalHashFunctions 元素的哈希函数对象类型。
     */
    template <typename SetTypeVal, typename ExternalHashFunctions = StandardCon::HashImitationFunctions>
    class HashSet
    {
        /** @brief 哈希集合中保存的元素类型。 */
        using KeyValType = SetTypeVal;
        /** @brief 通过外部哈希器计算集合元素的桶哈希值。 */
        class InbuiltSetHashFunctor
        {
        private:
            /** @brief 用于计算元素哈希值的外部函数对象。 */
            ExternalHashFunctions HashFunctionsObject;

        public:
            /** @brief 计算集合元素的哈希值。
             * @param KeyValue 要计算哈希值的元素。
             * @return 基于元素计算的哈希值。
             */
            uint64_t operator()(const KeyValType &KeyValue)
            {
                return HashFunctionsObject(KeyValue) * 131;
            }
        };
        /** @brief 将哈希集合元素直接作为哈希键。 */
        class KeyVal
        {
        public:
            /** @brief 返回元素本身作为哈希键。
             * @param KeyValue 要提取哈希键的元素。
             * @return 元素值。
             */
            KeyValType operator()(const KeyValType &KeyValue)
            {
                return KeyValue;
            }
        };
        /** @brief 按元素哈希分桶的底层表类型。 */
        using HashTable = StandardCon::HashTable<SetTypeVal, KeyValType, KeyVal, InbuiltSetHashFunctor>;
        /** @brief 保存哈希集合元素的底层表。 */
        HashTable InstanceHashSet;

    public:
        /** @brief 按插入顺序遍历的只读迭代器。 */
        using ConstIterator = typename HashTable::ConstIterator;
        /** @brief 与 ConstIterator 相同的只读迭代器。 */
        using Iterator = ConstIterator;
        /**
         * @brief 构造 HashSet 对象。
         */
        HashSet() { ; }

        /**
         * @brief 用一个元素构造哈希集合。
         * @param SetTypeData 要插入的元素。
         */
        explicit HashSet(const SetTypeVal &SetTypeData)
        {
            InstanceHashSet.Push(SetTypeData);
        }

        /**
         * @brief 深拷贝另一个哈希集合。
         * @param HashSetData 要复制的集合。
         */
        HashSet(const HashSet &HashSetData) { InstanceHashSet = HashSetData.InstanceHashSet; }

        /**
         * @brief 销毁 HashSet 对象。
         */
        ~HashSet() = default;

        /**
         * @brief 移动另一个哈希集合。
         * @param HashSetData 要移出的集合。
         */
        HashSet(HashSet &&HashSetData) { InstanceHashSet = std::move(HashSetData.InstanceHashSet); }

        /**
         * @brief 插入元素；相等元素已存在时不插入。
         * @param SetTypeData 要插入的元素。
         * @return 插入成功时返回 true；元素已存在时返回 false。
         */
        bool Push(const KeyValType &SetTypeData) { return InstanceHashSet.Push(SetTypeData); }

        /**
         * @brief 删除指定元素。
         * @param SetTypeData 要删除的元素。
         * @return 找到并删除元素时返回 true。
         */
        bool Pop(const KeyValType &SetTypeData) { return InstanceHashSet.Pop(SetTypeData); }

        /**
         * @brief 查找指定元素。
         * @param SetTypeData 要查找的元素。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const KeyValType &SetTypeData)
        {
            return InstanceHashSet.Find(SetTypeData).AsConst();
        }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        uint64_t Size() { return InstanceHashSet.Size(); }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        bool Empty() { return InstanceHashSet.Empty(); }

        /**
         * @brief 取得当前容量。
         * @return 当前桶数量。
         */
        uint64_t Capacity() { return InstanceHashSet.Capacity(); }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return InstanceHashSet.Size();
        }

        /**
         * @brief 取得当前容量。
         * @return 当前桶数量。
         */
        [[nodiscard]] uint64_t Capacity() const
        {
            return InstanceHashSet.Capacity();
        }

        /**
         * @brief 取得首元素迭代器。
         * @return 指向最早插入元素的只读迭代器；空集合返回尾后迭代器。
         */
        Iterator Begin() { return InstanceHashSet.Cbegin(); }

        /**
         * @brief 取得尾后迭代器。
         * @return 只读尾后迭代器。
         */
        Iterator End() { return InstanceHashSet.Cend(); }

        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向最早插入元素的只读迭代器；空集合返回尾后迭代器。
         */
        ConstIterator Cbegin() { return InstanceHashSet.Cbegin(); }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 只读尾后迭代器。
         */
        ConstIterator Cend() { return InstanceHashSet.Cend(); }

        /**
         * @brief 按键查找元素。
         * @param SetTypeData 要查找的元素。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator operator[](const KeyValType &SetTypeData)
        {
            return InstanceHashSet[SetTypeData].AsConst();
        }

        /**
         * @brief 移动插入一个元素，构造哈希集合。
         * @param SetTypeData 要插入的元素。
         */
        explicit HashSet(SetTypeVal &&SetTypeData)
        {
            InstanceHashSet.Push(std::forward<SetTypeVal>(SetTypeData));
        }
        /**
         * @brief 移动插入元素；相等元素已存在时不插入。
         * @param SetTypeData 要插入的元素。
         * @return 插入成功时返回 true；元素已存在时返回 false。
         */
        bool Push(SetTypeVal &&SetTypeData)
        {
            return InstanceHashSet.Push(std::forward<SetTypeVal>(SetTypeData));
        }
        /**
         * @brief 从初始化列表构造哈希集合。
         * @param LightweightContainer 要插入的元素列表。
         */
        HashSet(std::initializer_list<KeyValType> LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                InstanceHashSet.Push(std::move(ChainedValues));
            }
        }
        /**
         * @brief 将初始化列表中的元素插入集合，不清除已有元素。
         * @param LightweightContainer 要插入的元素列表。
         * @return 当前对象的引用。
         */
        HashSet &operator=(const std::initializer_list<KeyValType> &LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                InstanceHashSet.Push(std::move(ChainedValues));
            }
            return *this;
        }
        /**
         * @brief 深拷贝赋值。
         * @param HashSetData 要复制的集合。
         * @return 当前对象的引用。
         */
        HashSet &operator=(const HashSet &HashSetData) = default;
        /**
         * @brief 移动赋值。
         * @param HashSetData 要移出的集合。
         * @return 当前对象的引用。
         */
        HashSet &operator=(HashSet &&HashSetData)
        {
            InstanceHashSet = std::move(HashSetData.InstanceHashSet);
            return *this;
        }
    };
}
namespace StandardCon
{
    using SetContainer::HashSet;
    using SetContainer::TreeSet;
}
