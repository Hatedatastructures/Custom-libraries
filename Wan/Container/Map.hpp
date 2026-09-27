/**
 * @file Map.hpp
 * @brief 有序映射和哈希映射。
 */
#pragma once
#include "Base.hpp"

#include <concepts>
#include <utility>

namespace MapContainer
{
    /**
     * @brief 映射迭代器暴露的键值项，键不可修改。
     * @tparam Key 键类型。
     * @tparam Value 映射值类型。
     */
    template <typename Key, typename Value>
    struct MapEntry
    {
        /** @brief 键；不可通过该字段修改。 */
        const Key First;
        /** @brief 与键关联的可修改值。 */
        Value Second;

        /**
         * @brief 构造 MapEntry 对象。
         */
        MapEntry() requires std::default_initializable<Key>
            && std::default_initializable<Value>
            : First{}, Second{}
        {
        }

        /**
         * @brief 构造 MapEntry 对象。
         * @tparam KeyArgument 可用于构造键的类型。
         * @tparam ValueArgument 可用于构造映射值的类型。
         * @param KeyValue 要保存的键。
         * @param MappedValue 要保存的映射值。
         */
        template <typename KeyArgument, typename ValueArgument>
        MapEntry(KeyArgument&& KeyValue, ValueArgument&& MappedValue)
            : First(std::forward<KeyArgument>(KeyValue))
            , Second(std::forward<ValueArgument>(MappedValue))
        {
        }

        /**
         * @brief 构造 MapEntry 对象。
         * @param Entry 要复制的键值对。
         */
        MapEntry(const StandardCon::Pair<Key, Value>& Entry)
            : First(Entry.First), Second(Entry.Second)
        {
        }

        /**
         * @brief 构造 MapEntry 对象。
         * @param Entry 要移动的键值对。
         */
        MapEntry(StandardCon::Pair<Key, Value>&& Entry)
            : First(std::move(Entry.First)), Second(std::move(Entry.Second))
        {
        }

        /**
         * @brief 构造 MapEntry 对象。
         */
        MapEntry(const MapEntry&) = default;
        /**
         * @brief 构造 MapEntry 对象。
         */
        MapEntry(MapEntry&&) = default;
    };

    /**
     * @brief 按键排序的映射容器。
     * @tparam MapTypeK 键类型。
     * @tparam MapTypeV 映射值类型。
     * @tparam Comparators 键的比较器类型。
     */
    template <typename MapTypeK, typename MapTypeV, typename Comparators = StandardCon::Less<MapTypeK>>
    class TreeMap
    {
        /** @brief 红黑树中保存的键值项类型。 */
        using KeyValType = MapEntry<MapTypeK, MapTypeV>;
        /** @brief 构造映射项时使用的键值对类型。 */
        using InputPair = StandardCon::Pair<MapTypeK, MapTypeV>;
        /** @brief 从映射迭代器项中提取键供树排序。 */
        struct KeyVal
        {
            /** @brief 返回映射项的键。
             * @param KeyValue 要提取键的映射项。
             * @return 映射项中键的只读引用。
             */
            const MapTypeK &operator()(const KeyValType &KeyValue)
            {
                return KeyValue.First;
            }
        };
        /** @brief 按键维护顺序的底层红黑树类型。 */
        using InstanceRb = StandardCon::RedBlackTree<MapTypeK, KeyValType, KeyVal, Comparators>;
        /** @brief 保存所有有序映射项的红黑树。 */
        InstanceRb InstanceTreeMap;

    public:
        /** @brief 按键升序遍历的可修改迭代器。 */
        using Iterator = typename InstanceRb::Iterator;
        /** @brief 按键升序遍历的只读迭代器。 */
        using ConstIterator = typename InstanceRb::ConstIterator;
        /** @brief 按键降序遍历的可修改反向迭代器。 */
        using ReverseIterator = typename InstanceRb::ReverseIterator;
        /** @brief 按键降序遍历的只读反向迭代器。 */
        using ConstReverseIterator = typename InstanceRb::ConstReverseIterator;

        /** @brief 插入操作的结果：迭代器和是否新增。 */
        using MapIterator = StandardCon::Pair<Iterator, bool>;
        /**
         * @brief 销毁 TreeMap 对象。
         */
        ~TreeMap() = default;
        /**
         * @brief 深拷贝赋值。
         * @param TreeMapData 要复制的映射。
         * @return 当前对象的引用。
         */
        TreeMap &operator=(const TreeMap &TreeMapData)
        {
            if (this != &TreeMapData)
            {
                InstanceTreeMap = TreeMapData.InstanceTreeMap;
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param TreeMapData 要移出的映射。
         * @return 当前对象的引用。
         */
        TreeMap &operator=(TreeMap &&TreeMapData)
        {
            if (this != &TreeMapData)
            {
                InstanceTreeMap = std::move(TreeMapData.InstanceTreeMap);
            }
            return *this;
        }
        /**
         * @brief 移动插入键值对。
         * @param TreeMapData 要插入的键值对。
         * @return 指向已有或新元素的迭代器，以及是否新增。
         */
        MapIterator Push(InputPair&& TreeMapData)
        {
            return InstanceTreeMap.Push(KeyValType{std::move(TreeMapData)});
        }
        /**
         * @brief 将初始化列表中尚不存在的键值对插入映射。
         * @param LightweightContainer 要插入的键值对列表。
         * @return 当前对象的引用。
         */
        TreeMap &operator=(std::initializer_list<InputPair> LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                (void)Push(std::move(ChainedValues));
            }
            return *this;
        }
        /**
         * @brief 从初始化列表构造有序映射。
         * @param LightweightContainer 要插入的键值对列表。
         */
        TreeMap(const std::initializer_list<InputPair> &LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                (void)Push(std::move(ChainedValues));
            }
        }
        /**
         * @brief 用一个键值对构造有序映射。
         * @param TreeMapData 要插入的键值对。
         */
        explicit TreeMap(InputPair&& TreeMapData)
        {
            (void)Push(std::move(TreeMapData));
        }
        /**
         * @brief 构造 TreeMap 对象。
         */
        TreeMap() { ; }

        /**
         * @brief 深拷贝另一个有序映射。
         * @param TreeMapData 要复制的映射。
         */
        TreeMap(const TreeMap &TreeMapData) { InstanceTreeMap = TreeMapData.InstanceTreeMap; }

        /**
         * @brief 移动另一个有序映射。
         * @param TreeMapData 要移出的映射。
         */
        TreeMap(TreeMap &&TreeMapData) { InstanceTreeMap = std::move(TreeMapData.InstanceTreeMap); }

        /**
         * @brief 用一个键值对构造有序映射。
         * @param TreeMapData 要复制并插入的键值对。
         */
        explicit TreeMap(const InputPair& TreeMapData)
        {
            (void)Push(TreeMapData);
        }

        /**
         * @brief 插入键值对；相同键已存在时保留原值。
         * @param TreeMapData 要插入的键值对。
         * @return 指向已有或新元素的迭代器，以及是否新增。
         */
        MapIterator Push(const InputPair& TreeMapData)
        {
            return InstanceTreeMap.Push(KeyValType{TreeMapData});
        }

        /**
         * @brief 按键值对中的键删除元素。
         * @param TreeMapData 用于指定待删除键的键值对。
         * @return 第二项表示是否删除成功；第一项为尾后迭代器。
         */
        MapIterator Pop(const InputPair& TreeMapData)
        {
            return InstanceTreeMap.PopKey(TreeMapData.First);
        }

        /**
         * @brief 按键值对中的键查找元素。
         * @param TreeMapData 用于指定查找键的键值对。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const InputPair& TreeMapData)
        {
            return InstanceTreeMap.FindKey(TreeMapData.First);
        }

        /**
         * @brief 按键查找元素。
         * @param KeyValue 要查找的键。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const MapTypeK& KeyValue)
        {
            return InstanceTreeMap.FindKey(KeyValue);
        }

        /**
         * @brief 按键删除元素。
         * @param KeyValue 要删除的键。
         * @return 第二项表示是否删除成功；第一项为尾后迭代器。
         */
        MapIterator Pop(const MapTypeK& KeyValue)
        {
            return InstanceTreeMap.PopKey(KeyValue);
        }

        /**
         * @brief 按中序输出元素。
         */
        void MiddleOrderTraversal() { InstanceTreeMap.MiddleOrderTraversal(); }

        /**
         * @brief 按前序输出元素。
         */
        void PreOrderTraversal() { InstanceTreeMap.PreOrderTraversal(); }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return InstanceTreeMap.Size();
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        bool Empty() { return InstanceTreeMap.Empty(); }

        /**
         * @brief 取得首元素迭代器。
         * @return 指向最小键元素的迭代器；空映射返回尾后迭代器。
         */
        Iterator Begin() { return InstanceTreeMap.Begin(); }

        /**
         * @brief 取得尾后迭代器。
         * @return 尾后迭代器。
         */
        Iterator End() { return InstanceTreeMap.End(); }

        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向最小键元素的只读迭代器；空映射返回尾后迭代器。
         */
        ConstIterator Cbegin() { return InstanceTreeMap.Cbegin(); }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 只读尾后迭代器。
         */
        ConstIterator Cend() { return InstanceTreeMap.Cend(); }

        /**
         * @brief 取得反向首元素迭代器。
         * @return 指向最大键元素的反向迭代器；空映射返回反向尾后迭代器。
         */
        ReverseIterator Rbegin() { return InstanceTreeMap.Rbegin(); }

        /**
         * @brief 取得反向尾后迭代器。
         * @return 反向尾后迭代器。
         */
        ReverseIterator Rend() { return InstanceTreeMap.Rend(); }

        /**
         * @brief 取得只读反向首元素迭代器。
         * @return 指向最大键元素的只读反向迭代器；空映射返回反向尾后迭代器。
         */
        ConstReverseIterator Crbegin() { return InstanceTreeMap.Crbegin(); }

        /**
         * @brief 取得只读反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        ConstReverseIterator Crend() { return InstanceTreeMap.Crend(); }

        /**
         * @brief 按键查找元素。
         * @param KeyValue 要查找的键。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator operator[](const MapTypeK& KeyValue)
        {
            return InstanceTreeMap.FindKey(KeyValue);
        }
    };
    /**
     * @brief 按键哈希值分桶的映射容器。
     * @tparam HashMapTypeKey 键类型。
     * @tparam HashMapTypeValue 映射值类型。
     * @tparam FirstExternalHashFunctions 键的哈希函数对象类型。
     * @tparam SecondExternalHashFunctions 保留的第二哈希函数对象类型。
     */
    template <typename HashMapTypeKey, typename HashMapTypeValue,
                        typename FirstExternalHashFunctions = StandardCon::HashImitationFunctions,
                        typename SecondExternalHashFunctions = StandardCon::HashImitationFunctions> // 两个对应的hash函数
    class HashMap
    {
        /** @brief 哈希表中保存的键值项类型。 */
        using KeyValType = MapEntry<HashMapTypeKey, HashMapTypeValue>;
        /** @brief 构造映射项时使用的键值对类型。 */
        using InputPair = StandardCon::Pair<HashMapTypeKey, HashMapTypeValue>;
        /** @brief 从映射项中提取哈希键。 */
        struct KeyVal
        {
            /** @brief 返回映射项的键。
             * @param KeyValue 要提取键的映射项。
             * @return 映射项中键的只读引用。
             */
            const HashMapTypeKey &operator()(const KeyValType &KeyValue)
            {
                return KeyValue.First;
            }
        };
        /** @brief 只根据映射键计算桶哈希值。 */
        class InbuiltMapHashFunctor
        {
        private:
            /** @brief 计算键哈希值的外部函数对象。 */
            FirstExternalHashFunctions FirstHashFunctionsObject;

        public:
            /** @brief 计算映射项键的哈希值。
             * @param KeyValue 要计算哈希值的映射项。
             * @return 基于映射项键计算的哈希值。
             */
            uint64_t operator()(const KeyValType &KeyValue)
            {
                // A map's bucket identity must depend only on its key.
                // Including the value lets the same key bypass duplicate
                // detection by landing in a different bucket.
                return FirstHashFunctionsObject(KeyValue.First) * 31;
            }

            /** @brief 计算键的哈希值。
             * @param KeyValue 要计算哈希值的键。
             * @return 基于键计算的哈希值。
             */
            uint64_t operator()(const HashMapTypeKey& KeyValue)
            {
                return FirstHashFunctionsObject(KeyValue) * 31;
            }
        };
        /** @brief 按键哈希分桶并维护插入顺序的底层表类型。 */
        using HashTable = StandardCon::HashTable<HashMapTypeKey, KeyValType, KeyVal, InbuiltMapHashFunctor>;
        /** @brief 保存所有映射项的底层哈希表。 */
        HashTable InstanceHashMap;

    public:
        /** @brief 按插入顺序遍历的可修改迭代器。 */
        using Iterator = typename HashTable::Iterator;
        /** @brief 按插入顺序遍历的只读迭代器。 */
        using ConstIterator = typename HashTable::ConstIterator; // 单向迭代器
        /**
         * @brief 构造 HashMap 对象。
         */
        HashMap() { ; }

        /**
         * @brief 销毁 HashMap 对象。
         */
        ~HashMap() = default;

        /**
         * @brief 用一个键值对构造哈希映射。
         * @param KeyValue 要插入的键值对。
         */
        explicit HashMap(const InputPair& KeyValue)
        {
            (void)Push(KeyValue);
        }

        /**
         * @brief 深拷贝另一个哈希映射。
         * @param HashMapData 要复制的映射。
         */
        HashMap(const HashMap &HashMapData) { InstanceHashMap = HashMapData.InstanceHashMap; }

        /**
         * @brief 移动另一个哈希映射。
         * @param HashMapData 要移出的映射。
         */
        HashMap(HashMap &&HashMapData) { InstanceHashMap = std::move(HashMapData.InstanceHashMap); }

        /**
         * @brief 插入键值对；相同键已存在时不插入。
         * @param KeyValue 要插入的键值对。
         * @return 插入成功时返回 true；键已存在时返回 false。
         */
        bool Push(const InputPair& KeyValue)
        {
            return InstanceHashMap.Push(KeyValType{KeyValue});
        }

        /**
         * @brief 按键值对中的键删除元素。
         * @param KeyValue 用于指定待删除键的键值对。
         * @return 找到并删除元素时返回 true。
         */
        bool Pop(const InputPair& KeyValue)
        {
            return InstanceHashMap.PopKey(KeyValue.First);
        }

        /**
         * @brief 按键值对中的键查找元素。
         * @param KeyValue 用于指定查找键的键值对。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const InputPair& KeyValue)
        {
            return InstanceHashMap.FindKey(KeyValue.First);
        }

        /**
         * @brief 按键查找元素。
         * @param KeyValue 要查找的键。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const HashMapTypeKey& KeyValue)
        {
            return InstanceHashMap.FindKey(KeyValue);
        }

        /**
         * @brief 按键删除元素。
         * @param KeyValue 要删除的键。
         * @return 找到并删除元素时返回 true。
         */
        bool Pop(const HashMapTypeKey& KeyValue)
        {
            return InstanceHashMap.PopKey(KeyValue);
        }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        uint64_t Size() { return InstanceHashMap.Size(); }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return InstanceHashMap.Size();
        }

        /**
         * @brief 取得当前容量。
         * @return 当前桶数量。
         */
        [[nodiscard]] uint64_t Capacity() const
        {
            return InstanceHashMap.Capacity();
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        bool Empty() { return InstanceHashMap.Empty(); }

        /**
         * @brief 取得首元素迭代器。
         * @return 指向最早插入元素的迭代器；空映射返回尾后迭代器。
         */
        Iterator Begin() { return InstanceHashMap.Begin(); }

        /**
         * @brief 取得尾后迭代器。
         * @return 尾后迭代器。
         */
        Iterator End() { return InstanceHashMap.End(); }

        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向最早插入元素的只读迭代器；空映射返回尾后迭代器。
         */
        ConstIterator Cbegin() { return InstanceHashMap.Cbegin(); }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 只读尾后迭代器。
         */
        ConstIterator Cend() { return InstanceHashMap.Cend(); }

        /**
         * @brief 按键查找元素。
         * @param KeyValue 要查找的键。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator operator[](const HashMapTypeKey& KeyValue)
        {
            return InstanceHashMap[KeyValue];
        }

        /**
         * @brief 按键查找元素。
         * @param KeyValue 用于指定查找键的键值对。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator operator[](const InputPair& KeyValue)
        {
            return InstanceHashMap.FindKey(KeyValue.First);
        }

        /**
         * @brief 从初始化列表构造哈希映射。
         * @param LightweightContainer 要插入的键值对列表。
         */
        HashMap(const std::initializer_list<InputPair> &LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                (void)Push(std::move(ChainedValues));
            }
        }
        /**
         * @brief 移动插入一个键值对，构造哈希映射。
         * @param KeyValue 要插入的键值对。
         */
        explicit HashMap(InputPair&& KeyValue)
        {
            (void)Push(std::move(KeyValue));
        }
        /**
         * @brief 将初始化列表中尚不存在的键值对插入映射。
         * @param LightweightContainer 要插入的键值对列表。
         * @return 当前对象的引用。
         */
        HashMap &operator=(const std::initializer_list<InputPair> &LightweightContainer)
        {
            for (auto &ChainedValues : LightweightContainer)
            {
                (void)Push(std::move(ChainedValues));
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param HashMapData 要移出的映射。
         * @return 当前对象的引用。
         */
        HashMap &operator=(HashMap &&HashMapData)
        {
            if (this != &HashMapData)
            {
                InstanceHashMap = std::forward<HashTable>(HashMapData.InstanceHashMap);
            }
            return *this;
        }
        /**
         * @brief 移动插入键值对；相同键已存在时不插入。
         * @param KeyValue 要移动插入的键值对。
         * @return 插入成功时返回 true；键已存在时返回 false。
         */
        bool Push(InputPair&& KeyValue)
        {
            return InstanceHashMap.Push(KeyValType{std::move(KeyValue)});
        }
        /**
         * @brief 深拷贝赋值。
         * @param HashMapData 要复制的映射。
         * @return 当前对象的引用。
         */
        HashMap &operator=(const HashMap &HashMapData)
        {
            if (this != &HashMapData)
            {
                InstanceHashMap = HashMapData.InstanceHashMap;
            }
            return *this;
        }
    };
}
namespace StandardCon
{
    using MapContainer::HashMap;
    using MapContainer::TreeMap;
}
