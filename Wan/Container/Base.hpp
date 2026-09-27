/**
 * @file Base.hpp
 * @brief 红黑树、哈希表和位集合的基础实现。
 */
#pragma once
#include "Stack.hpp"
#include "Vector.hpp"
#include "Algorithm.hpp"
#include "Utility.hpp"
#include "Exception.hpp"
#include "Imitate.hpp"
#include "Hash.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace BaseContainer
{
    /**
     * @brief 按键排序的红黑树，查找、插入和删除的复杂度为 O(log n)。
     * @tparam RbTreeTypeKey 键类型。
     * @tparam RbTreeTypeValue 节点存储类型。
     * @tparam ContainerImitateFunctionVisit 从节点值提取键的函数对象。
     * @tparam ContainerImitateFunction 键比较器，默认为 `StandardCon::Less<RbTreeTypeKey>`。
     * @note 键必须唯一；遍历按键升序或降序进行。插入和旋转不会使现有节点迭代器失效，删除会使指向被删节点的迭代器失效。`Size()` 通过遍历计数，复杂度为 O(n)。
     */
    template <typename RbTreeTypeKey, typename RbTreeTypeValue,
        typename ContainerImitateFunctionVisit,
        typename ContainerImitateFunction = StandardCon::Less<RbTreeTypeKey>>
    class RedBlackTree
    {
    private:
        /** @brief 红黑树节点的颜色。 */
        enum class RbTreeColor
        {
            /** @brief 红色节点。 */
            Red,
            /** @brief 黑色节点。 */
            Black
        };

        /** @brief 红黑树节点，保存值、父子链接和颜色。 */
        class ContainerNode
        {
        public:
            /** @brief 节点保存的值。 */
            RbTreeTypeValue MemberData;
            /** @brief 左子节点；不存在时为空。 */
            ContainerNode* MemberLeft = nullptr;
            /** @brief 右子节点；不存在时为空。 */
            ContainerNode* MemberRight = nullptr;
            /** @brief 父节点；根节点的父链接为空。 */
            ContainerNode* MemberParent = nullptr;
            /** @brief 当前节点的颜色。 */
            RbTreeColor MemberColor = RbTreeColor::Red;

            /**
             * @brief 复制值并创建红色节点。
             * @param ValueData 要复制到节点中的值。
             */
            explicit ContainerNode(const RbTreeTypeValue& ValueData)
                : MemberData(ValueData)
            {
            }

            /**
             * @brief 移动值并创建红色节点。
             * @param ValueData 要移动到节点中的值。
             */
            explicit ContainerNode(RbTreeTypeValue&& ValueData)
                : MemberData(std::move(ValueData))
            {
            }
        };

        /**
         * @brief 沿红黑树中序遍历的双向迭代器。
         * @tparam T 节点值类型，可带 const 限定。
         * @tparam Ref 解引用返回的引用类型。
         * @tparam Ptr 箭头运算符返回的指针类型。
         * @note 空节点指针表示尾后位置；对尾后迭代器执行递增或递减均保持尾后位置，递减首元素也会到达尾后位置。
         * @note 插入和树旋转不会改变其他节点的地址；删除会使指向被删节点的迭代器失效。
         */
        template <typename T, typename Ref, typename Ptr>
        class RbTreeIterator
        {
        public:
            /** @brief 迭代器指向的内部节点类型。 */
            using IteratorNode = ContainerNode;
            /** @brief 当前迭代器类型。 */
            using Self = RbTreeIterator<T, Ref, Ptr>;
            /** @brief 箭头运算符返回的指针类型。 */
            using Pointer = Ptr;
            /** @brief 解引用返回的引用类型。 */
            using Reference = Ref;
            /** @brief 移除 const 限定后的节点值类型。 */
            using ValueType = std::remove_const_t<T>;
            /** @brief 指向同一节点的只读迭代器类型。 */
            using ConstSelf = RbTreeIterator<ValueType,
                const ValueType&, const ValueType*>;

            /**
             * @brief 从节点指针构造迭代器。
             * @param Node 当前节点；空指针表示尾后位置。
             */
            RbTreeIterator(IteratorNode* Node = nullptr) noexcept
                : MemberNode(Node)
            {
            }

            /**
             * @brief 访问当前节点的值。
             * @return 当前节点值的引用。
             * @pre 迭代器不得处于尾后位置。
             */
            [[nodiscard]] auto operator*() const -> Reference
            {
                return MemberNode->MemberData;
            }

            /**
             * @brief 取得当前节点值的指针。
             * @return 当前节点值的指针。
             * @pre 迭代器不得处于尾后位置。
             */
            [[nodiscard]] auto operator->() const -> Pointer
            {
                return &MemberNode->MemberData;
            }

            /**
             * @brief 将当前迭代器转换为只读迭代器。
             * @return 指向同一节点的只读迭代器。
             */
            [[nodiscard]] auto AsConst() const -> ConstSelf
            {
                return ConstSelf(MemberNode);
            }

            /**
             * @brief 前进到按键排序的下一个节点。
             * @return 当前迭代器的引用。
             */
            auto operator++() -> Self&
            {
                if (MemberNode == nullptr)
                {
                    return *this;
                }
                if (MemberNode->MemberRight != nullptr)
                {
                    MemberNode = MemberNode->MemberRight;
                    while (MemberNode->MemberLeft != nullptr)
                    {
                        MemberNode = MemberNode->MemberLeft;
                    }
                }
                else
                {
                    auto* Parent = MemberNode->MemberParent;
                    while (Parent != nullptr && MemberNode == Parent->MemberRight)
                    {
                        MemberNode = Parent;
                        Parent = Parent->MemberParent;
                    }
                    MemberNode = Parent;
                }
                return *this;
            }

            /**
             * @brief 后置递增。
             * @return 递增前的迭代器副本。
             */
            auto operator++(int) -> Self
            {
                auto Previous = *this;
                ++(*this);
                return Previous;
            }

            /**
             * @brief 后退到按键排序的前一个节点。
             * @return 当前迭代器的引用。
             */
            auto operator--() -> Self&
            {
                if (MemberNode == nullptr)
                {
                    return *this;
                }
                if (MemberNode->MemberLeft != nullptr)
                {
                    MemberNode = MemberNode->MemberLeft;
                    while (MemberNode->MemberRight != nullptr)
                    {
                        MemberNode = MemberNode->MemberRight;
                    }
                }
                else
                {
                    auto* Parent = MemberNode->MemberParent;
                    while (Parent != nullptr && MemberNode == Parent->MemberLeft)
                    {
                        MemberNode = Parent;
                        Parent = Parent->MemberParent;
                    }
                    MemberNode = Parent;
                }
                return *this;
            }

            /**
             * @brief 后置递减。
             * @return 递减前的迭代器副本。
             */
            auto operator--(int) -> Self
            {
                auto Previous = *this;
                --(*this);
                return Previous;
            }

            /**
             * @brief 比较两个迭代器是否指向同一节点。
             * @param Other 要比较的迭代器。
             * @return 指向同一节点时返回 true。
             */
            [[nodiscard]] auto operator==(const Self& Other) const noexcept -> bool
            {
                return MemberNode == Other.MemberNode;
            }

            /**
             * @brief 比较两个迭代器是否指向不同节点。
             * @param Other 要比较的迭代器。
             * @return 指向不同节点时返回 true。
             */
            [[nodiscard]] auto operator!=(const Self& Other) const noexcept -> bool
            {
                return !(*this == Other);
            }

        private:
            /** @brief 当前节点；空指针表示尾后位置。 */
            IteratorNode* MemberNode = nullptr;
        };

        /**
         * @brief 将树迭代器按降序移动的反向迭代器。
         * @tparam IteratorType 底层树迭代器类型。
         * @note 空底层迭代器表示反向尾后位置；递增沿底层迭代器递减，递减则相反。
         */
        template <typename IteratorType>
        class RbTreeReverseIterator
        {
        public:
            /** @brief 当前反向迭代器类型。 */
            using Self = RbTreeReverseIterator<IteratorType>;
            /** @brief 箭头运算符返回的指针类型。 */
            using Pointer = typename IteratorType::Pointer;
            /** @brief 解引用返回的引用类型。 */
            using Reference = typename IteratorType::Reference;

            /**
             * @brief 从底层迭代器构造反向迭代器。
             * @param IteratorValue 当前节点迭代器；默认值表示反向尾后位置。
             */
            RbTreeReverseIterator(IteratorType IteratorValue = {})
                : MemberIterator(std::move(IteratorValue))
            {
            }

            /**
             * @brief 访问当前节点的值。
             * @return 当前节点值的引用。
             * @pre 迭代器不得处于反向尾后位置。
             */
            [[nodiscard]] auto operator*() const -> Reference
            {
                return *MemberIterator;
            }

            /**
             * @brief 取得当前节点值的指针。
             * @return 当前节点值的指针。
             * @pre 迭代器不得处于反向尾后位置。
             */
            [[nodiscard]] auto operator->() const -> Pointer
            {
                return MemberIterator.operator->();
            }

            /**
             * @brief 前进到按键排序的前一个节点。
             * @return 当前反向迭代器的引用。
             */
            auto operator++() -> Self&
            {
                --MemberIterator;
                return *this;
            }

            /**
             * @brief 后置递增。
             * @return 递增前的迭代器副本。
             */
            auto operator++(int) -> Self
            {
                auto Previous = *this;
                --MemberIterator;
                return Previous;
            }

            /**
             * @brief 后退到按键排序的下一个节点。
             * @return 当前反向迭代器的引用。
             */
            auto operator--() -> Self&
            {
                ++MemberIterator;
                return *this;
            }

            /**
             * @brief 后置递减。
             * @return 递减前的迭代器副本。
             */
            auto operator--(int) -> Self
            {
                auto Previous = *this;
                ++MemberIterator;
                return Previous;
            }

            /**
             * @brief 比较两个反向迭代器是否指向同一节点。
             * @param Other 要比较的反向迭代器。
             * @return 指向同一节点时返回 true。
             */
            [[nodiscard]] auto operator==(const Self& Other) const -> bool
            {
                return MemberIterator == Other.MemberIterator;
            }

            /**
             * @brief 比较两个反向迭代器是否指向不同节点。
             * @param Other 要比较的反向迭代器。
             * @return 指向不同节点时返回 true。
             */
            [[nodiscard]] auto operator!=(const Self& Other) const -> bool
            {
                return !(*this == Other);
            }

        private:
            /** @brief 当前反向迭代器包装的树迭代器。 */
            IteratorType MemberIterator;
        };

    public:
        /** @brief 可修改的中序迭代器。 */
        using Iterator = RbTreeIterator<RbTreeTypeValue,
            RbTreeTypeValue&, RbTreeTypeValue*>;
        /** @brief 只读的中序迭代器。 */
        using ConstIterator = RbTreeIterator<RbTreeTypeValue,
            const RbTreeTypeValue&, const RbTreeTypeValue*>;
        /** @brief 可修改的反向迭代器。 */
        using ReverseIterator = RbTreeReverseIterator<Iterator>;
        /** @brief 只读的反向迭代器。 */
        using ConstReverseIterator = RbTreeReverseIterator<ConstIterator>;
        /** @brief 插入或删除结果，包含迭代器和操作是否成功。 */
        using ReturnPairValue = StandardCon::Pair<Iterator, bool>;

    private:
        /** @brief 从节点值中提取排序键的函数对象。 */
        ContainerImitateFunctionVisit Element;
        /** @brief 比较两个键的函数对象。 */
        ContainerImitateFunction FunctionPolicy;
        /** @brief 根节点；空树时为空指针。 */
        ContainerNode* MemberRoot = nullptr;

        /**
         * @brief 围绕子树根的右子节点执行左旋。
         * @param SubtreeNode 要旋转的子树根；调用前须有右子节点。
         */
        void LeftRevolve(ContainerNode* SubtreeNode) noexcept
        {
            auto* Pivot = SubtreeNode->MemberRight;
            SubtreeNode->MemberRight = Pivot->MemberLeft;
            if (Pivot->MemberLeft != nullptr)
            {
                Pivot->MemberLeft->MemberParent = SubtreeNode;
            }
            Pivot->MemberParent = SubtreeNode->MemberParent;
            if (SubtreeNode->MemberParent == nullptr)
            {
                MemberRoot = Pivot;
            }
            else if (SubtreeNode == SubtreeNode->MemberParent->MemberLeft)
            {
                SubtreeNode->MemberParent->MemberLeft = Pivot;
            }
            else
            {
                SubtreeNode->MemberParent->MemberRight = Pivot;
            }
            Pivot->MemberLeft = SubtreeNode;
            SubtreeNode->MemberParent = Pivot;
        }

        /**
         * @brief 围绕子树根的左子节点执行右旋。
         * @param SubtreeNode 要旋转的子树根；调用前须有左子节点。
         */
        void RightRevolve(ContainerNode* SubtreeNode) noexcept
        {
            auto* Pivot = SubtreeNode->MemberLeft;
            SubtreeNode->MemberLeft = Pivot->MemberRight;
            if (Pivot->MemberRight != nullptr)
            {
                Pivot->MemberRight->MemberParent = SubtreeNode;
            }
            Pivot->MemberParent = SubtreeNode->MemberParent;
            if (SubtreeNode->MemberParent == nullptr)
            {
                MemberRoot = Pivot;
            }
            else if (SubtreeNode == SubtreeNode->MemberParent->MemberRight)
            {
                SubtreeNode->MemberParent->MemberRight = Pivot;
            }
            else
            {
                SubtreeNode->MemberParent->MemberLeft = Pivot;
            }
            Pivot->MemberRight = SubtreeNode;
            SubtreeNode->MemberParent = Pivot;
        }

        /** @brief 迭代释放树中所有节点并将树置空。 */
        void Clear() noexcept
        {
            while (MemberRoot != nullptr)
            {
                if (MemberRoot->MemberLeft != nullptr)
                {
                    RightRevolve(MemberRoot);
                    continue;
                }
                auto* Node = MemberRoot;
                MemberRoot = Node->MemberRight;
                if (MemberRoot != nullptr)
                {
                    MemberRoot->MemberParent = nullptr;
                }
                delete Node;
            }
        }

        /**
         * @brief 将子树按键升序写入标准输出。
         * @param Node 要遍历的子树根；为空时不输出内容。
         */
        void InteriorMiddleOrderTraversal(ContainerNode* Node)
        {
            StandardCon::Stack<ContainerNode*> Pending;
            while (Node != nullptr || !Pending.Empty())
            {
                while (Node != nullptr)
                {
                    Pending.Push(Node);
                    Node = Node->MemberLeft;
                }
                Node = Pending.Top();
                Pending.Pop();
                std::cout << Node->MemberData << ' ';
                Node = Node->MemberRight;
            }
        }

        /**
         * @brief 将子树按根、左、右顺序写入标准输出。
         * @param Node 要遍历的子树根；为空时不输出内容。
         */
        void InteriorPreOrderTraversal(ContainerNode* Node)
        {
            if (Node == nullptr)
            {
                return;
            }
            StandardCon::Stack<ContainerNode*> Pending;
            Pending.Push(Node);
            while (!Pending.Empty())
            {
                Node = Pending.Top();
                Pending.Pop();
                std::cout << Node->MemberData << ' ';
                if (Node->MemberRight != nullptr)
                {
                    Pending.Push(Node->MemberRight);
                }
                if (Node->MemberLeft != nullptr)
                {
                    Pending.Push(Node->MemberLeft);
                }
            }
        }

        /**
         * @brief 取得节点颜色；空节点按红黑树规则视为黑色。
         * @param Node 要检查的节点；空指针表示叶子位置。
         * @return 节点颜色；空指针对应黑色。
         */
        [[nodiscard]] static auto GetColor(const ContainerNode* Node) noexcept
            -> RbTreeColor
        {
            return Node == nullptr ? RbTreeColor::Black : Node->MemberColor;
        }

        /**
         * @brief 判断节点是否为红色。
         * @param Node 要检查的节点；空指针表示叶子位置。
         * @return 节点为红色时返回 `true`；空指针返回 `false`。
         */
        [[nodiscard]] static auto RedGet(const ContainerNode* Node) noexcept -> bool
        {
            return GetColor(Node) == RbTreeColor::Red;
        }

        /**
         * @brief 判断节点是否为黑色。
         * @param Node 要检查的节点；空指针表示叶子位置。
         * @return 节点为黑色时返回 `true`；空指针返回 `true`。
         */
        [[nodiscard]] static auto BlackGet(const ContainerNode* Node) noexcept -> bool
        {
            return GetColor(Node) == RbTreeColor::Black;
        }

        /**
         * @brief 遍历计数，取得树中节点数。
         * @return 树中的节点数。
         * @note 需要访问每个节点，复杂度为 O(n)。
         */
        [[nodiscard]] auto InternalSize() const -> std::uint64_t
        {
            if (MemberRoot == nullptr)
            {
                return 0;
            }
            std::uint64_t Count = 0;
            StandardCon::Stack<const ContainerNode*> Pending;
            Pending.Push(MemberRoot);
            while (!Pending.Empty())
            {
                const auto* Node = Pending.Top();
                Pending.Pop();
                ++Count;
                if (Node->MemberRight != nullptr)
                {
                    Pending.Push(Node->MemberRight);
                }
                if (Node->MemberLeft != nullptr)
                {
                    Pending.Push(Node->MemberLeft);
                }
            }
            return Count;
        }

        /**
         * @brief 修复插入节点后可能违反的红黑树颜色规则。
         * @param Node 新插入的红色节点。
         */
        void InsertAdjust(ContainerNode* Node) noexcept
        {
            while (Node != MemberRoot
                && Node->MemberParent != nullptr
                && RedGet(Node->MemberParent))
            {
                auto* Parent = Node->MemberParent;
                auto* Grandparent = Parent->MemberParent;
                if (Grandparent == nullptr)
                {
                    break;
                }
                if (Parent == Grandparent->MemberLeft)
                {
                    auto* Uncle = Grandparent->MemberRight;
                    if (RedGet(Uncle))
                    {
                        Parent->MemberColor = RbTreeColor::Black;
                        Uncle->MemberColor = RbTreeColor::Black;
                        Grandparent->MemberColor = RbTreeColor::Red;
                        Node = Grandparent;
                    }
                    else
                    {
                        if (Node == Parent->MemberRight)
                        {
                            Node = Parent;
                            LeftRevolve(Node);
                            Parent = Node->MemberParent;
                            Grandparent = Parent->MemberParent;
                        }
                        Parent->MemberColor = RbTreeColor::Black;
                        Grandparent->MemberColor = RbTreeColor::Red;
                        RightRevolve(Grandparent);
                    }
                }
                else
                {
                    auto* Uncle = Grandparent->MemberLeft;
                    if (RedGet(Uncle))
                    {
                        Parent->MemberColor = RbTreeColor::Black;
                        Uncle->MemberColor = RbTreeColor::Black;
                        Grandparent->MemberColor = RbTreeColor::Red;
                        Node = Grandparent;
                    }
                    else
                    {
                        if (Node == Parent->MemberLeft)
                        {
                            Node = Parent;
                            RightRevolve(Node);
                            Parent = Node->MemberParent;
                            Grandparent = Parent->MemberParent;
                        }
                        Parent->MemberColor = RbTreeColor::Black;
                        Grandparent->MemberColor = RbTreeColor::Red;
                        LeftRevolve(Grandparent);
                    }
                }
            }
            if (MemberRoot != nullptr)
            {
                MemberRoot->MemberColor = RbTreeColor::Black;
            }
        }

        /**
         * @brief 按值类别插入节点并修复红黑树平衡。
         * @tparam ValueType 插入值的类型和值类别。
         * @param ValueData 要插入的节点值。
         * @return 指向已有或新节点的迭代器，以及是否实际插入。
         */
        template <typename ValueType>
        auto InsertValue(ValueType&& ValueData) -> ReturnPairValue
        {
            ContainerNode* Parent = nullptr;
            auto* Node = MemberRoot;
            while (Node != nullptr)
            {
                Parent = Node;
                const auto& NodeKey = Element(Node->MemberData);
                const auto& ValueKey = Element(ValueData);
                if (!FunctionPolicy(NodeKey, ValueKey)
                    && !FunctionPolicy(ValueKey, NodeKey))
                {
                    return ReturnPairValue(Iterator(Node), false);
                }
                Node = FunctionPolicy(NodeKey, ValueKey)
                    ? Node->MemberRight : Node->MemberLeft;
            }

            auto NewNode = std::make_unique<ContainerNode>(
                std::forward<ValueType>(ValueData));
            auto* Inserted = NewNode.get();
            Inserted->MemberParent = Parent;
            if (Parent == nullptr)
            {
                MemberRoot = Inserted;
            }
            else if (FunctionPolicy(Element(Parent->MemberData),
                         Element(Inserted->MemberData)))
            {
                Parent->MemberRight = Inserted;
            }
            else
            {
                Parent->MemberLeft = Inserted;
            }
            NewNode.release();
            InsertAdjust(Inserted);
            return ReturnPairValue(Iterator(Inserted), true);
        }

    public:
        /** @brief 创建空红黑树。 */
        RedBlackTree() = default;

        /**
         * @brief 创建仅含一个元素的红黑树。
         * @param ValueData 根节点值。
         */
        explicit RedBlackTree(const RbTreeTypeValue& ValueData)
            : MemberRoot(new ContainerNode(ValueData))
        {
            MemberRoot->MemberColor = RbTreeColor::Black;
        }

        /**
         * @brief 移动值并创建仅含一个元素的红黑树。
         * @param ValueData 根节点值。
         */
        explicit RedBlackTree(RbTreeTypeValue&& ValueData)
            : MemberRoot(new ContainerNode(std::move(ValueData)))
        {
            MemberRoot->MemberColor = RbTreeColor::Black;
        }

        /**
         * @brief 移动构造红黑树。
         * @param Other 源树。
         */
        RedBlackTree(RedBlackTree&& Other) noexcept(
            std::is_nothrow_move_constructible_v<ContainerImitateFunctionVisit>
            && std::is_nothrow_move_constructible_v<ContainerImitateFunction>)
            : Element(std::move(Other.Element))
            , FunctionPolicy(std::move(Other.FunctionPolicy))
            , MemberRoot(std::exchange(Other.MemberRoot, nullptr))
        {
        }

        /**
         * @brief 深拷贝红黑树。
         * @param Other 源树。
         * @note 复制比较器或节点值时发生的异常会向调用方传播；部分克隆节点会先被释放。
         */
        RedBlackTree(const RedBlackTree& Other)
            : Element(Other.Element)
            , FunctionPolicy(Other.FunctionPolicy)
        {
            if (Other.MemberRoot == nullptr)
            {
                return;
            }
            try
            {
                MemberRoot = new ContainerNode(Other.MemberRoot->MemberData);
                MemberRoot->MemberColor = Other.MemberRoot->MemberColor;
                std::vector<std::pair<const ContainerNode*, ContainerNode*>> Pending;
                Pending.emplace_back(Other.MemberRoot, MemberRoot);
                while (!Pending.empty())
                {
                    const auto [Source, Destination] = Pending.back();
                    Pending.pop_back();
                    if (Source->MemberLeft != nullptr)
                    {
                        auto Child = std::make_unique<ContainerNode>(
                            Source->MemberLeft->MemberData);
                        Child->MemberParent = Destination;
                        Child->MemberColor = Source->MemberLeft->MemberColor;
                        auto* ChildNode = Child.get();
                        Pending.emplace_back(Source->MemberLeft, ChildNode);
                        Destination->MemberLeft = ChildNode;
                        Child.release();
                    }
                    if (Source->MemberRight != nullptr)
                    {
                        auto Child = std::make_unique<ContainerNode>(
                            Source->MemberRight->MemberData);
                        Child->MemberParent = Destination;
                        Child->MemberColor = Source->MemberRight->MemberColor;
                        auto* ChildNode = Child.get();
                        Pending.emplace_back(Source->MemberRight, ChildNode);
                        Destination->MemberRight = ChildNode;
                        Child.release();
                    }
                }
            }
            catch (...)
            {
                Clear();
                throw;
            }
        }

        /**
         * @brief 复制赋值红黑树。
         * @param Other 源树。
         * @return 当前对象的引用。
         */
        auto operator=(const RedBlackTree& Other) -> RedBlackTree&
        {
            if (this != &Other)
            {
                RedBlackTree Copy(Other);
                Swap(Copy);
            }
            return *this;
        }

        /**
         * @brief 移动赋值红黑树。
         * @param Other 源树。
         * @return 当前对象的引用。
         */
        auto operator=(RedBlackTree&& Other) -> RedBlackTree&
        {
            if (this != &Other)
            {
                Clear();
                Element = std::move(Other.Element);
                FunctionPolicy = std::move(Other.FunctionPolicy);
                MemberRoot = std::exchange(Other.MemberRoot, nullptr);
            }
            return *this;
        }

        /** @brief 释放树中所有节点。 */
        ~RedBlackTree() noexcept
        {
            Clear();
        }

        /**
         * @brief 插入元素；已存在相同键时不修改树。
         * @param ValueData 要插入的值。
         * @return 指向现有或新元素的迭代器，以及是否插入成功。
         */
        auto Push(const RbTreeTypeValue& ValueData) -> ReturnPairValue
        {
            return InsertValue(ValueData);
        }

        /**
         * @brief 移动插入元素；已存在相同键时不修改树。
         * @param ValueData 要插入的值。
         * @return 指向现有或新元素的迭代器，以及是否插入成功。
         */
        auto Push(RbTreeTypeValue&& ValueData) -> ReturnPairValue
        {
            return InsertValue(std::move(ValueData));
        }

    private:
        /**
         * @brief 交换根节点、键提取器和比较器。
         * @param Other 要交换内容的红黑树。
         */
        void Swap(RedBlackTree& Other)
        {
            using std::swap;
            swap(Element, Other.Element);
            swap(FunctionPolicy, Other.FunctionPolicy);
            swap(MemberRoot, Other.MemberRoot);
        }

        /**
         * @brief 用新节点替换旧节点在父节点中的链接。
         * @param OldNode 要替换的节点。
         * @param NewNode 接替旧节点的节点；可以为空。
         */
        void Transplant(ContainerNode* OldNode, ContainerNode* NewNode) noexcept
        {
            auto* Parent = OldNode->MemberParent;
            if (Parent == nullptr)
            {
                MemberRoot = NewNode;
            }
            else if (Parent->MemberLeft == OldNode)
            {
                Parent->MemberLeft = NewNode;
            }
            else
            {
                Parent->MemberRight = NewNode;
            }
            if (NewNode != nullptr)
            {
                NewNode->MemberParent = Parent;
            }
        }

        /**
         * @brief 按键查找树节点。
         * @param KeyValue 要查找的键。
         * @return 匹配节点；未找到时返回空指针。
         */
        [[nodiscard]] auto FindNodeByKey(const RbTreeTypeKey& KeyValue)
            -> ContainerNode*
        {
            auto* Node = MemberRoot;
            while (Node != nullptr)
            {
                const auto& NodeKey = Element(Node->MemberData);
                if (!FunctionPolicy(NodeKey, KeyValue)
                    && !FunctionPolicy(KeyValue, NodeKey))
                {
                    return Node;
                }
                Node = FunctionPolicy(NodeKey, KeyValue)
                    ? Node->MemberRight : Node->MemberLeft;
            }
            return nullptr;
        }

        /**
         * @brief 删除树节点。
         * @param Node 要从树中移除的节点。
         */
        void EraseNode(ContainerNode* Node)
        {
            auto* MovedNode = Node;
            auto MovedColor = MovedNode->MemberColor;
            ContainerNode* FixNode = nullptr;
            ContainerNode* FixParent = nullptr;
            bool FixIsLeft = false;

            if (Node->MemberLeft == nullptr)
            {
                FixNode = Node->MemberRight;
                FixParent = Node->MemberParent;
                FixIsLeft = FixParent != nullptr
                    && FixParent->MemberLeft == Node;
                Transplant(Node, Node->MemberRight);
            }
            else if (Node->MemberRight == nullptr)
            {
                FixNode = Node->MemberLeft;
                FixParent = Node->MemberParent;
                FixIsLeft = FixParent != nullptr
                    && FixParent->MemberLeft == Node;
                Transplant(Node, Node->MemberLeft);
            }
            else
            {
                MovedNode = Node->MemberRight;
                while (MovedNode->MemberLeft != nullptr)
                {
                    MovedNode = MovedNode->MemberLeft;
                }
                MovedColor = MovedNode->MemberColor;
                FixNode = MovedNode->MemberRight;

                if (MovedNode->MemberParent == Node)
                {
                    FixParent = MovedNode;
                    FixIsLeft = false;
                    if (FixNode != nullptr)
                    {
                        FixNode->MemberParent = MovedNode;
                    }
                }
                else
                {
                    FixParent = MovedNode->MemberParent;
                    FixIsLeft = FixParent->MemberLeft == MovedNode;
                    Transplant(MovedNode, MovedNode->MemberRight);
                    MovedNode->MemberRight = Node->MemberRight;
                    MovedNode->MemberRight->MemberParent = MovedNode;
                }

                Transplant(Node, MovedNode);
                MovedNode->MemberLeft = Node->MemberLeft;
                MovedNode->MemberLeft->MemberParent = MovedNode;
                MovedNode->MemberColor = Node->MemberColor;
            }

            delete Node;
            if (MovedColor == RbTreeColor::Black)
            {
                DeleteAdjust(FixNode, FixParent, FixIsLeft);
            }
            if (MemberRoot != nullptr)
            {
                MemberRoot->MemberColor = RbTreeColor::Black;
            }
        }

        /**
         * @brief 调整删除后的树结构。
         * @param CurrentNode 当前修复节点；可以为空。
         * @param Parent 当前修复节点的父节点；可以为空。
         * @param CurrentIsLeft 当前节点为空时，指出它是否位于父节点左侧。
         */
        void DeleteAdjust(ContainerNode *CurrentNode, ContainerNode *Parent,
            bool CurrentIsLeft)
        {
            while (CurrentNode != MemberRoot && (CurrentNode == nullptr || BlackGet(CurrentNode)))
            {
                if (Parent == nullptr)
                {
                    break;
                }
                const bool IsLeft = CurrentNode != nullptr
                    ? Parent->MemberLeft == CurrentNode
                    : CurrentIsLeft;
                if (IsLeft)
                {
                    ContainerNode *Brother = Parent->MemberRight;
                    if (Brother == nullptr)
                    {
                        CurrentNode = Parent;
                        Parent = CurrentNode->MemberParent;
                        CurrentIsLeft = Parent != nullptr
                            && Parent->MemberLeft == CurrentNode;
                        continue;
                    }
                    if (RedGet(Brother))
                    {
                        // 情况1：兄弟节点为红
                        Brother->MemberColor = RbTreeColor::Black;
                        Parent->MemberColor = RbTreeColor::Red;
                        LeftRevolve(Parent);
                        // 调整后，兄弟节点为黑
                        // 继续向下调整
                        Brother = Parent->MemberRight;
                    }
                    if ((Brother != nullptr && BlackGet(Brother)) && (Brother->MemberLeft == nullptr || BlackGet(Brother->MemberLeft)) &&
                            (Brother->MemberRight == nullptr || BlackGet(Brother->MemberRight)))
                    {
                        // 情况2：兄弟节点为黑，且兄弟节点两个子节点都为黑
                        Brother->MemberColor = RbTreeColor::Red;
                        CurrentNode = Parent;
                        Parent = CurrentNode->MemberParent;
                        CurrentIsLeft = Parent != nullptr
                            && Parent->MemberLeft == CurrentNode;
                        if (CurrentNode->MemberColor == RbTreeColor::Red)
                        {
                            CurrentNode->MemberColor = RbTreeColor::Black;
                            break;
                        }
                    }
                    else if ((Brother != nullptr && BlackGet(Brother)) && (Brother->MemberRight == nullptr || BlackGet(Brother->MemberRight)) &&
                                      (Brother->MemberLeft != nullptr && RedGet(Brother->MemberLeft)))
                    {
                        // 情况3：兄弟节点为黑，兄弟节点左节点为红，右节点为黑
                        Brother->MemberLeft->MemberColor = RbTreeColor::Black;
                        Brother->MemberColor = RbTreeColor::Red;
                        RightRevolve(Brother);
                        // 调整后，兄弟节点为黑，兄弟节点右节点为红
                        // 继续向下调整
                        Brother = Parent->MemberRight;
                    }
                    else if ((Brother != nullptr && BlackGet(Brother)) && (Brother->MemberRight != nullptr && RedGet(Brother->MemberRight)))
                    {
                        // 情况4：兄弟节点为黑，兄弟节点右节点为红
                        Brother->MemberColor = Parent->MemberColor;
                        Parent->MemberColor = RbTreeColor::Black;
                        Brother->MemberRight->MemberColor = RbTreeColor::Black;
                        LeftRevolve(Parent);
                        CurrentNode = MemberRoot;
                        Parent = CurrentNode->MemberParent;
                    }
                }
                else
                {
                    ContainerNode *Brother = Parent->MemberLeft;
                    if (Brother == nullptr)
                    {
                        CurrentNode = Parent;
                        Parent = CurrentNode->MemberParent;
                        CurrentIsLeft = Parent != nullptr
                            && Parent->MemberLeft == CurrentNode;
                        continue;
                    }
                    if (RedGet(Brother))
                    {
                        // 情况1：兄弟节点为红
                        Brother->MemberColor = RbTreeColor::Black;
                        Parent->MemberColor = RbTreeColor::Red;
                        RightRevolve(Parent);
                        // 调整后，兄弟节点为黑
                        Brother = Parent->MemberLeft;
                    }
                    if (Brother != nullptr && BlackGet(Brother) && (Brother->MemberLeft == nullptr || BlackGet(Brother->MemberLeft)) &&
                            (Brother->MemberRight == nullptr || BlackGet(Brother->MemberRight)))
                    {
                        // 情况2：兄弟节点为黑，且兄弟节点两个子节点都为黑
                        Brother->MemberColor = RbTreeColor::Red;
                        CurrentNode = Parent;
                        Parent = CurrentNode->MemberParent;
                        CurrentIsLeft = Parent != nullptr
                            && Parent->MemberLeft == CurrentNode;
                        if (CurrentNode->MemberColor == RbTreeColor::Red)
                        {
                            CurrentNode->MemberColor = RbTreeColor::Black;
                            break;
                        }
                    }
                    else if (Brother != nullptr && BlackGet(Brother) && (Brother->MemberRight != nullptr && RedGet(Brother->MemberRight)) &&
                                      (Brother->MemberLeft == nullptr || BlackGet(Brother->MemberLeft)))
                    {
                        // 情况3：兄弟节点为黑，兄弟节点左节点为红，右节点为黑
                        Brother->MemberRight->MemberColor = RbTreeColor::Black;
                        Brother->MemberColor = RbTreeColor::Red;
                        LeftRevolve(Brother);
                        // 调整后，兄弟节点为黑，兄弟节点右节点为红
                        // 继续向下调整
                        Brother = Parent->MemberLeft;
                    }
                    else if (Brother != nullptr && BlackGet(Brother) && Brother->MemberLeft != nullptr && RedGet(Brother->MemberLeft))
                    {
                        // 情况4：兄弟节点为黑，兄弟节点右节点为红
                        Brother->MemberColor = Parent->MemberColor;
                        Parent->MemberColor = RbTreeColor::Black;
                        Brother->MemberLeft->MemberColor = RbTreeColor::Black;
                        RightRevolve(Parent);
                        CurrentNode = MemberRoot;
                        Parent = CurrentNode->MemberParent;
                    }
                }
            }
            if (CurrentNode != nullptr)
            {
                CurrentNode->MemberColor = RbTreeColor::Black;
            }
        }
    public:
        /**
         * @brief 删除指定元素。
         * @param RbTreeData 要删除的值。
         * @return 第二项表示是否删除成功；第一项始终为尾后迭代器。
         */
        ReturnPairValue Pop(const RbTreeTypeValue &RbTreeData)
        {
            auto* Target = FindNodeByKey(Element(RbTreeData));
            if (Target == nullptr)
            {
                return ReturnPairValue(Iterator(nullptr), false);
            }
            EraseNode(Target);
            return ReturnPairValue(Iterator(nullptr), true);
        }

        /**
         * @brief 按键删除元素。
         * @param KeyValue 要删除的键。
         * @return 第二项表示是否删除成功；第一项始终为尾后迭代器。
         */
        ReturnPairValue PopKey(const RbTreeTypeKey& KeyValue)
        {
            auto* Target = FindNodeByKey(KeyValue);
            if (Target == nullptr)
            {
                return ReturnPairValue(Iterator(nullptr), false);
            }
            EraseNode(Target);
            return ReturnPairValue(Iterator(nullptr), true);
        }

        /**
         * @brief 按键查找元素。
         * @param KeyValue 要查找的键。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        [[nodiscard]] auto FindKey(const RbTreeTypeKey& KeyValue) -> Iterator
        {
            return Iterator(FindNodeByKey(KeyValue));
        }
        /**
         * @brief 查找指定元素。
         * @param ValData 用于提取查找键的值。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const RbTreeTypeValue &ValData)
        {
            if (MemberRoot == nullptr)
            {
                return Iterator(nullptr);
            }
            else
            {
                ContainerNode *RootFindNode = MemberRoot;
                while (RootFindNode != nullptr)
                {
                    if (!FunctionPolicy(Element(RootFindNode->MemberData), Element(ValData))
                        && !FunctionPolicy(Element(ValData), Element(RootFindNode->MemberData)))
                    {
                        return Iterator(RootFindNode);
                    }
                    else if (FunctionPolicy(Element(RootFindNode->MemberData), Element(ValData)))
                    {
                        RootFindNode = RootFindNode->MemberRight;
                    }
                    else
                    {
                        RootFindNode = RootFindNode->MemberLeft;
                    }
                }
                return Iterator(nullptr);
            }
        }
        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        uint64_t Size()
        {
            return InternalSize();
        }
        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return InternalSize();
        }
        /**
         * @brief 判断容器是否为空。
         * @return 树中没有元素时返回 true。
         */
        bool Empty()
        {
            return MemberRoot == nullptr;
        }
        /**
         * @brief 按中序输出元素。
         */
        void MiddleOrderTraversal()
        {
            InteriorMiddleOrderTraversal(MemberRoot);
        }
        /**
         * @brief 按前序输出元素。
         */
        void PreOrderTraversal()
        {
            InteriorPreOrderTraversal(MemberRoot);
        }
        /**
         * @brief 取得首元素迭代器。
         * @return 指向最小键元素的迭代器；空树返回尾后迭代器。
         */
        Iterator Begin()
        {
            ContainerNode *RootNode = MemberRoot;
            while (RootNode != nullptr && RootNode->MemberLeft != nullptr)
            {
                RootNode = RootNode->MemberLeft;
            }
            return Iterator(RootNode);
        }

        /**
         * @brief 取得尾后迭代器。
         * @return 尾后迭代器。
         */
        static Iterator End()
        {
            return Iterator(nullptr);
        }
        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向最小键元素的只读迭代器；空树返回尾后迭代器。
         */
        ConstIterator Cbegin() const
        {
            ContainerNode *RootNode = MemberRoot;
            while (RootNode != nullptr && RootNode->MemberLeft != nullptr)
            {
                RootNode = RootNode->MemberLeft;
            }
            return ConstIterator(RootNode);
        }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 只读尾后迭代器。
         */
        static ConstIterator Cend()
        {
            return ConstIterator(nullptr);
        }
        /**
         * @brief 取得反向首元素迭代器。
         * @return 指向最大键元素的反向迭代器；空树返回反向尾后迭代器。
         */
        ReverseIterator Rbegin()
        {
            ContainerNode *RootNode = MemberRoot;
            while (RootNode != nullptr && RootNode->MemberRight != nullptr)
            {
                RootNode = RootNode->MemberRight;
            }
            return ReverseIterator(Iterator(RootNode));
        }

        /**
         * @brief 取得反向尾后迭代器。
         * @return 反向尾后迭代器。
         */
        static ReverseIterator Rend()
        {
            return ReverseIterator(nullptr);
        }
        /**
         * @brief 取得只读反向首元素迭代器。
         * @return 指向最大键元素的只读反向迭代器；空树返回反向尾后迭代器。
         */
        ConstReverseIterator Crbegin() const
        {
            ContainerNode *RootNode = MemberRoot;
            while (RootNode != nullptr && RootNode->MemberRight != nullptr)
            {
                RootNode = RootNode->MemberRight;
            }
            return ConstReverseIterator(ConstIterator(RootNode));
        }

        /**
         * @brief 取得只读反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        static ConstReverseIterator Crend()
        {
            return ConstReverseIterator(nullptr);
        }
        /**
         * @brief 按键查找元素。
         * @param RbTreeData 用于提取查找键的值。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator operator[](const RbTreeTypeValue &RbTreeData)
        {
            return Find(RbTreeData);
        }
    };
/**
 * @brief 按键哈希分桶，并按插入顺序遍历的哈希表。
 * @tparam HashTableTypeKey 键类型。
 * @tparam HashTableTypeValue 节点存储的值类型。
 * @tparam ContainerImitateFunction 从值中提取键的函数对象类型。
 * @tparam HashFunction 计算哈希值的函数对象类型。
 * @note 平均查找、插入和删除复杂度为 O(1)，最坏为 O(n)。默认负载系数为 7（0.7）。
 */
    template <typename HashTableTypeKey, typename HashTableTypeValue, typename ContainerImitateFunction,
                        typename HashFunction = StandardCon::HashImitationFunctions>
    class HashTable
    {
        /** @brief 保存哈希表元素及两种遍历所需链接的节点。 */
        class HashTableNode
        {
        public:
            /** @brief 节点保存的键值。 */
            HashTableTypeValue MemberData;
            /** @brief 同一哈希桶中的下一个节点。 */
            HashTableNode *MemberNext;
            /** @brief 插入顺序链表中的前一个节点。 */
            HashTableNode *OverallListPrev;
            // 全局链表指针，方便按照插入的顺序有序遍历哈希表
            /** @brief 插入顺序链表中的后一个节点。 */
            HashTableNode *OverallListNext;

            /** @brief 复制值并创建未链接的节点。
             * @param HashTableValueData 要复制到节点中的值。
             */
            explicit HashTableNode(const HashTableTypeValue &HashTableValueData)
                : MemberData(HashTableValueData), MemberNext(nullptr),
                  OverallListPrev(nullptr), OverallListNext(nullptr)
            {
            }
            /**
             * @brief 移动值并创建未链接的节点。
             * @param HashTableValueData 要移动到节点中的值。
             */
            explicit HashTableNode(HashTableTypeValue &&HashTableValueData)
                : MemberData(std::move(HashTableValueData)), MemberNext(nullptr),
                  OverallListPrev(nullptr), OverallListNext(nullptr)
            {
            }
        };
        /** @brief 哈希表内部节点类型。 */
        using ContainerNode = HashTableNode;
        /** @brief 从存储值中提取键的函数对象。 */
        ContainerImitateFunction ValueImitationFunctions;

        /** @brief 当前元素数量。 */
        uint64_t MemberSize;

        /** @brief 负载系数，以十分之一为单位，取值范围为 1 到 10。 */
        uint64_t LoadFactor;

        /** @brief 当前桶数。 */
        uint64_t HashCapacity;

        /** @brief 存放各桶头节点的数组。 */
        StandardCon::Vector<ContainerNode *> VectorHashTable;

        /** @brief 计算节点键哈希值的函数对象。 */
        HashFunction HashFunctionObject;

        /** @brief 插入顺序链表的尾节点；空表时为空。 */
        ContainerNode *OverallListBeforeNode = nullptr;

        /** @brief 插入顺序链表的首节点；空表时为空。 */
        ContainerNode *OverallListHeadNode = nullptr;

        /**
         * @brief 按插入顺序遍历哈希表的单向迭代器。
         * @tparam IteratorTypeKey 键类型，可带 const 限定。
         * @tparam IteratorTypeVal 节点值类型，可带 const 限定。
         * @note 空节点指针表示尾后位置；插入不会移动已有节点，删除会使指向被删节点的迭代器失效。
         */
        template <typename IteratorTypeKey, typename IteratorTypeVal>
        class HashIterator
        {
            /** @brief 哈希表内部节点类型。 */
            using IteratorNode = ContainerNode;
            /** @brief 解引用返回的引用类型。 */
            using Ref = IteratorTypeVal &;
            /** @brief 箭头运算符返回的指针类型。 */
            using Ptr = IteratorTypeVal *;
            /** @brief 当前迭代器类型。 */
            using Self = HashIterator<IteratorTypeKey, IteratorTypeVal>;
            /** @brief 当前节点；空指针表示尾后位置。 */
            IteratorNode *HashTableIteratorNode;

        public:
            template <typename, typename>
            friend class HashIterator;

            /**
             * @brief 从节点指针构造迭代器。
             * @param IteratorPtrNode 当前节点；空指针表示尾后位置。
             */
            HashIterator(IteratorNode *IteratorPtrNode) { HashTableIteratorNode = IteratorPtrNode; }

            /**
             * @brief 访问当前元素。
             * @return 当前元素的引用。
             * @pre 迭代器不得处于尾后位置。
             */
            Ref operator*() { return HashTableIteratorNode->MemberData; }

            /**
             * @brief 取得当前元素的指针。
             * @return 当前元素的指针。
             * @pre 迭代器不得处于尾后位置。
             */
            Ptr operator->() { return &HashTableIteratorNode->MemberData; }

            /**
             * @brief 比较两个迭代器是否不同。
             * @param IteratorData 要比较的迭代器。
             * @return 指向不同节点时返回 true。
             */
            bool operator!=(const Self &IteratorData) { return HashTableIteratorNode != IteratorData.HashTableIteratorNode; }

            /**
             * @brief 比较两个迭代器是否相同。
             * @param IteratorData 要比较的迭代器。
             * @return 指向同一节点时返回 true。
             */
            bool operator==(const Self &IteratorData) { return HashTableIteratorNode == IteratorData.HashTableIteratorNode; }

            /**
             * @brief 后置递增到下一个插入元素。
             * @return 递增前的迭代器副本。
             */
            Self operator++(int)
            {
                Self IteratorData = *this;
                HashTableIteratorNode = HashTableIteratorNode->OverallListNext;
                return IteratorData;
            }
            /**
             * @brief 前进到下一个插入元素。
             * @return 前进后的迭代器。
             */
            Self operator++()
            {
                HashTableIteratorNode = HashTableIteratorNode->OverallListNext;
                return *this;
            }
            /**
             * @brief 取得当前内部节点。
             * @return 当前节点；尾后迭代器返回空指针。
             */
            IteratorNode *GetNode()
            {
                return HashTableIteratorNode;
            }

            /**
             * @brief 将当前迭代器转换为只读迭代器。
             * @return 指向同一节点的只读迭代器。
             */
            [[nodiscard]] auto AsConst() const
                -> HashIterator<const IteratorTypeKey,
                    const IteratorTypeVal>
            {
                return HashIterator<const IteratorTypeKey,
                    const IteratorTypeVal>(HashTableIteratorNode);
            }
        };
        /**
         * @brief 按新桶数重建桶数组并重新链接现有节点。
         * @param NewCapacity 新桶数，必须大于零。
         * @throws std::length_error 桶数为零或元素规模超过可处理范围时抛出。
         * @note 节点和插入顺序链不重建；哈希函数或分配操作抛出的异常会传播。
         */
        void Rehash(uint64_t NewCapacity)
        {
            if (NewCapacity == 0)
            {
                throw std::length_error("hash table capacity cannot be zero");
            }

            StandardCon::Vector<ContainerNode *> NewBuckets;
            NewBuckets.SizeAdjust(NewCapacity, nullptr);
            std::vector<uint64_t> BucketIndices;
            if (MemberSize > BucketIndices.max_size())
            {
                throw std::length_error("hash table is too large");
            }
            BucketIndices.reserve(static_cast<std::size_t>(MemberSize));
            for (auto *Node = OverallListHeadNode; Node != nullptr;
                 Node = Node->OverallListNext)
            {
                BucketIndices.push_back(
                    HashFunctionObject(Node->MemberData) % NewCapacity);
            }

            auto *Node = OverallListHeadNode;
            for (const auto Bucket : BucketIndices)
            {
                auto *Next = Node->OverallListNext;
                Node->MemberNext = NewBuckets[Bucket];
                NewBuckets[Bucket] = Node;
                Node = Next;
            }
            VectorHashTable.Swap(NewBuckets);
            HashCapacity = NewCapacity;
        }

        /**
         * @brief 根据桶数和负载系数计算可容纳的最大元素数。
         * @param Capacity 桶数。
         * @param Factor 以十分之一为单位的负载系数。
         * @return 不超过 `Capacity * Factor / 10` 的最大整数。
         */
        [[nodiscard]] static auto MaximumElementsFor(
            uint64_t Capacity, uint64_t Factor) noexcept -> uint64_t
        {
            return (Capacity / 10) * Factor
                + ((Capacity % 10) * Factor) / 10;
        }

        /**
         * @brief 确保哈希表可以再插入一个元素，必要时扩大桶数。
         * @throws std::logic_error 负载系数不在 1 到 10 之间时抛出。
         * @throws std::length_error 元素数或桶数增长溢出时抛出。
         */
        void EnsureCapacityForOneMore()
        {
            if (LoadFactor < 1 || LoadFactor > 10)
            {
                throw std::logic_error("hash table load factor must be in [1, 10]");
            }
            if (MemberSize == std::numeric_limits<uint64_t>::max())
            {
                throw std::length_error("hash table size overflow");
            }

            while (HashCapacity == 0
                || MemberSize + 1 > MaximumElementsFor(
                    HashCapacity, LoadFactor))
            {
                uint64_t NewCapacity = 10;
                if (HashCapacity != 0)
                {
                    if (HashCapacity
                        > std::numeric_limits<uint64_t>::max() / 2)
                    {
                        throw std::length_error(
                            "hash table capacity overflow");
                    }
                    NewCapacity = HashCapacity * 2;
                }
                Rehash(NewCapacity);
            }
        }

        /**
         * @brief 在目标哈希桶中按键查找节点。
         * @param KeyValue 要查找的键。
         * @return 匹配节点；未找到时返回空指针。
         */
        [[nodiscard]] auto FindNodeByKey(
            const HashTableTypeKey& KeyValue) -> ContainerNode*
        {
            if (MemberSize == 0 || HashCapacity == 0)
            {
                return nullptr;
            }
            const auto Bucket = HashFunctionObject(KeyValue) % HashCapacity;
            auto* Node = VectorHashTable[Bucket];
            while (Node != nullptr)
            {
                if (ValueImitationFunctions(Node->MemberData) == KeyValue)
                {
                    return Node;
                }
                Node = Node->MemberNext;
            }
            return nullptr;
        }

        /**
         * @brief 从桶链和插入顺序链中移除并销毁节点。
         * @param Target 要删除的节点。
         * @param Bucket 节点所在的桶索引。
         * @return 节点在指定桶中找到并删除时返回 true。
         */
        auto EraseNode(ContainerNode* Target, uint64_t Bucket) -> bool
        {
            auto* PreviousBucketNode = static_cast<ContainerNode*>(nullptr);
            auto* BucketNode = VectorHashTable[Bucket];
            while (BucketNode != nullptr && BucketNode != Target)
            {
                PreviousBucketNode = BucketNode;
                BucketNode = BucketNode->MemberNext;
            }
            if (BucketNode == nullptr)
            {
                return false;
            }

            if (PreviousBucketNode == nullptr)
            {
                VectorHashTable[Bucket] = BucketNode->MemberNext;
            }
            else
            {
                PreviousBucketNode->MemberNext = BucketNode->MemberNext;
            }
            if (BucketNode->OverallListPrev != nullptr)
            {
                BucketNode->OverallListPrev->OverallListNext =
                    BucketNode->OverallListNext;
            }
            else
            {
                OverallListHeadNode = BucketNode->OverallListNext;
            }
            if (BucketNode->OverallListNext != nullptr)
            {
                BucketNode->OverallListNext->OverallListPrev =
                    BucketNode->OverallListPrev;
            }
            else
            {
                OverallListBeforeNode = BucketNode->OverallListPrev;
            }
            delete BucketNode;
            --MemberSize;
            return true;
        }

        /** @brief 释放所有桶中的节点并重置插入顺序链。 */
        void ClearNodes() noexcept
        {
            for (uint64_t I = 0; I < VectorHashTable.Size(); ++I)
            {
                ContainerNode *Node = VectorHashTable[I];
                while (Node != nullptr)
                {
                    ContainerNode *Next = Node->MemberNext;
                    delete Node;
                    Node = Next;
                }
                VectorHashTable[I] = nullptr;
            }
            MemberSize = 0;
            OverallListBeforeNode = nullptr;
            OverallListHeadNode = nullptr;
        }

        /**
         * @brief 交换哈希表的节点、桶、策略和容量设置。
         * @param Other 要交换内容的哈希表。
         */
        void Swap(HashTable &Other)
        {
            StandardCon::Algorithm::Swap(ValueImitationFunctions,
                Other.ValueImitationFunctions);
            StandardCon::Algorithm::Swap(MemberSize, Other.MemberSize);
            StandardCon::Algorithm::Swap(LoadFactor, Other.LoadFactor);
            StandardCon::Algorithm::Swap(HashCapacity, Other.HashCapacity);
            VectorHashTable.Swap(Other.VectorHashTable);
            StandardCon::Algorithm::Swap(HashFunctionObject,
                Other.HashFunctionObject);
            StandardCon::Algorithm::Swap(OverallListBeforeNode,
                Other.OverallListBeforeNode);
            StandardCon::Algorithm::Swap(OverallListHeadNode,
                Other.OverallListHeadNode);
        }

    public:
        /** @brief 可修改的哈希表迭代器。 */
        using Iterator = HashIterator<HashTableTypeKey, HashTableTypeValue>;
        /** @brief 只读哈希表迭代器。 */
        using ConstIterator = HashIterator<const HashTableTypeKey, const HashTableTypeValue>;
        /**
         * @brief 创建默认容量为 10、负载系数为 7 的空表。
         */
        HashTable()
        {
            MemberSize = 0;
            LoadFactor = 7;
            HashCapacity = 10;
            VectorHashTable.SizeAdjust(HashCapacity, nullptr);
        }

        /**
         * @brief 创建指定桶数的空表。
         * @param NewHashTableCapacity 桶数；传入 0 时首次插入会分配容量。
         */
        explicit HashTable(const uint64_t NewHashTableCapacity)
        {
            MemberSize = 0;
            LoadFactor = 7;
            HashCapacity = NewHashTableCapacity;
            VectorHashTable.SizeAdjust(HashCapacity, nullptr);
        }
        /**
         * @brief 按插入顺序深拷贝另一张表。
         * @param HashTableData 要复制的表。
         * @note 复制元素或计算哈希值失败时，对应异常会传播给调用方。
         */
        HashTable(const HashTable &HashTableData)
                : ValueImitationFunctions(HashTableData.ValueImitationFunctions), MemberSize(0), LoadFactor(HashTableData.LoadFactor),
                    HashCapacity(HashTableData.HashCapacity), HashFunctionObject(HashTableData.HashFunctionObject),
                    OverallListBeforeNode(nullptr), OverallListHeadNode(nullptr)
        {
            if (HashCapacity == 0)
            {
                return;
            }
            VectorHashTable.SizeAdjust(HashCapacity, nullptr);
            try
            {
                // 沿插入顺序复制，保持副本的遍历顺序。
                for (ContainerNode *Source = HashTableData.OverallListHeadNode;
                     Source != nullptr; Source = Source->OverallListNext)
                {
                    auto *Copy = new ContainerNode(Source->MemberData);
                    const auto Bucket = HashFunctionObject(Copy->MemberData)
                        % HashCapacity;
                    Copy->MemberNext = VectorHashTable[Bucket];
                    VectorHashTable[Bucket] = Copy;
                    Copy->OverallListPrev = OverallListBeforeNode;
                    if (OverallListBeforeNode != nullptr)
                    {
                        OverallListBeforeNode->OverallListNext = Copy;
                    }
                    else
                    {
                        OverallListHeadNode = Copy;
                    }
                    OverallListBeforeNode = Copy;
                    ++MemberSize;
                }
            }
            catch (...)
            {
                ClearNodes();
                throw;
            }
        }
        /**
         * @brief 移动构造哈希表。
         * @param HashTableData 要移出的表；操作后为空表。
         */
        HashTable(HashTable &&HashTableData)
        {
            VectorHashTable = std::move(HashTableData.VectorHashTable);
            MemberSize = HashTableData.MemberSize;
            LoadFactor = HashTableData.LoadFactor;
            HashCapacity = HashTableData.HashCapacity;
            HashFunctionObject = std::move(HashTableData.HashFunctionObject);
            OverallListBeforeNode = std::move(HashTableData.OverallListBeforeNode);
            OverallListHeadNode = std::move(HashTableData.OverallListHeadNode);
            ValueImitationFunctions = std::move(HashTableData.ValueImitationFunctions);
            HashTableData.MemberSize = 0;
            HashTableData.LoadFactor = 7;
            HashTableData.HashCapacity = 0;
            HashTableData.OverallListBeforeNode = nullptr;
            HashTableData.OverallListHeadNode = nullptr;
        }

        /**
         * @brief 深拷贝赋值。
         * @param HashTableData 要复制的表。
         * @return 当前对象的引用。
         */
        HashTable &operator=(const HashTable &HashTableData)
        {
            if (this != &HashTableData)
            {
                HashTable Copy(HashTableData);
                Swap(Copy);
            }
            return *this;
        }

        /**
         * @brief 移动赋值。
         * @param HashTableData 要移出的表；操作后为空表。
         * @return 当前对象的引用。
         */
        HashTable &operator=(HashTable &&HashTableData)
        {
            if (this != &HashTableData)
            {
                ClearNodes();
                VectorHashTable = std::move(HashTableData.VectorHashTable);
                MemberSize = HashTableData.MemberSize;
                LoadFactor = HashTableData.LoadFactor;
                HashCapacity = HashTableData.HashCapacity;
                HashFunctionObject = std::move(HashTableData.HashFunctionObject);
                ValueImitationFunctions = std::move(HashTableData.ValueImitationFunctions);
                OverallListBeforeNode = HashTableData.OverallListBeforeNode;
                OverallListHeadNode = HashTableData.OverallListHeadNode;

                HashTableData.MemberSize = 0;
                HashTableData.LoadFactor = 7;
                HashTableData.HashCapacity = 0;
                HashTableData.OverallListBeforeNode = nullptr;
                HashTableData.OverallListHeadNode = nullptr;
            }
            return *this;
        }

        /**
         * @brief 销毁 HashTable 对象。
         */
        ~HashTable() noexcept
        {
            ClearNodes();
        }
        /**
         * @brief 设置扩容负载系数，必要时重建桶数组。
         * @param NewLoadFactor 负载系数，范围为 1 到 10；例如 7 表示 0.7。
         * @return 取值有效时返回 true，否则返回 false。
         * @throws std::length_error 容量增长溢出时抛出。
         */
        bool ChangeLoadFactor(const uint64_t &NewLoadFactor)
        {
            if (NewLoadFactor < 1 || NewLoadFactor > 10)
            {
                return false;
            }

            auto NewCapacity = HashCapacity;
            if (MemberSize != 0 && NewCapacity == 0)
            {
                NewCapacity = 10;
            }
            while (NewCapacity != 0 && MemberSize
                > MaximumElementsFor(NewCapacity, NewLoadFactor))
            {
                if (NewCapacity
                    > std::numeric_limits<uint64_t>::max() / 2)
                {
                    throw std::length_error(
                        "hash table capacity overflow");
                }
                NewCapacity *= 2;
            }
            if (NewCapacity != HashCapacity)
            {
                Rehash(NewCapacity);
            }
            LoadFactor = NewLoadFactor;
            return true;
        }
        /**
         * @brief 按键查找元素。
         * @param KeyValue 要查找的键。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator operator[](const HashTableTypeKey &KeyValue)
        {
            return FindKey(KeyValue);
        }
        /**
         * @brief 取得首元素迭代器。
         * @return 指向最早插入元素的迭代器；空表返回尾后迭代器。
         */
        Iterator Begin() { return Iterator(OverallListHeadNode); }

        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向最早插入元素的只读迭代器；空表返回尾后迭代器。
         */
        ConstIterator Cbegin() const { return ConstIterator(OverallListHeadNode); }

        /**
         * @brief 取得尾后迭代器。
         * @return 尾后迭代器。
         */
        static Iterator End()
        {
            return Iterator(nullptr);
        }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 只读尾后迭代器。
         */
        static ConstIterator Cend()
        {
            return ConstIterator(nullptr);
        }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        uint64_t Size() { return MemberSize; }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return MemberSize;
        }

        /**
         * @brief 判断容器是否为空。
         * @return 表中没有元素时返回 true。
         */
        [[nodiscard]] bool Empty() const
        {
            return MemberSize == 0;
        }

        /**
         * @brief 取得当前容量。
         * @return 当前桶数。
         */
        uint64_t Capacity() { return HashCapacity; }

        /**
         * @brief 取得当前容量。
         * @return 当前桶数。
         */
        [[nodiscard]] uint64_t Capacity() const
        {
            return HashCapacity;
        }

        /**
         * @brief 插入键尚不存在的值。
         * @param HashTableValueData 要插入的值。
         * @return 插入成功时返回 true；键已存在时返回 false。
         */
        bool Push(const HashTableTypeValue &HashTableValueData)
        {
            if (Find(HashTableValueData).GetNode() != nullptr)
            {
                return false;
            }
            const auto FullHash = HashFunctionObject(HashTableValueData);
            auto NewNode = std::make_unique<ContainerNode>(HashTableValueData);
            EnsureCapacityForOneMore();

            const auto Bucket = FullHash % HashCapacity;
            NewNode->MemberNext = VectorHashTable[Bucket];
            VectorHashTable[Bucket] = NewNode.get();
            NewNode->OverallListPrev = OverallListBeforeNode;
            if (OverallListBeforeNode != nullptr)
            {
                OverallListBeforeNode->OverallListNext = NewNode.get();
            }
            else
            {
                OverallListHeadNode = NewNode.get();
            }
            OverallListBeforeNode = NewNode.release();
            ++MemberSize;
            return true;
        }
        /**
         * @brief 移动插入键尚不存在的值。
         * @param HashTableValueData 要插入的值。
         * @return 插入成功时返回 true；键已存在时返回 false。
         */
        bool Push(HashTableTypeValue &&HashTableValueData)
        {
            if (Find(HashTableValueData).GetNode() != nullptr)
            {
                return false;
            }
            const auto FullHash = HashFunctionObject(HashTableValueData);
            auto NewNode = std::make_unique<ContainerNode>(
                std::forward<HashTableTypeValue>(HashTableValueData));
            EnsureCapacityForOneMore();

            const auto Bucket = FullHash % HashCapacity;
            NewNode->MemberNext = VectorHashTable[Bucket];
            VectorHashTable[Bucket] = NewNode.get();
            NewNode->OverallListPrev = OverallListBeforeNode;
            if (OverallListBeforeNode != nullptr)
            {
                OverallListBeforeNode->OverallListNext = NewNode.get();
            }
            else
            {
                OverallListHeadNode = NewNode.get();
            }
            OverallListBeforeNode = NewNode.release();
            ++MemberSize;
            return true;
        }
        /**
         * @brief 按值提取出的键删除元素。
         * @param HashTableValueData 用于提取待删除键的值。
         * @return 找到并删除元素时返回 true。
         */
        bool Pop(const HashTableTypeValue &HashTableValueData)
        {
            auto* Target = Find(HashTableValueData).GetNode();
            if (Target == nullptr || HashCapacity == 0)
            {
                return false;
            }
            const auto Bucket = HashFunctionObject(HashTableValueData)
                % HashCapacity;
            return EraseNode(Target, Bucket);
        }

        /**
         * @brief 按键删除元素。
         * @param KeyValue 要删除的键。
         * @return 找到并删除元素时返回 true。
         */
        bool PopKey(const HashTableTypeKey& KeyValue)
        {
            if (HashCapacity == 0)
            {
                return false;
            }
            auto* Target = FindNodeByKey(KeyValue);
            if (Target == nullptr)
            {
                return false;
            }
            const auto Bucket = HashFunctionObject(KeyValue) % HashCapacity;
            return EraseNode(Target, Bucket);
        }
        /**
         * @brief 按值提取出的键查找元素。
         * @param HashTableValueData 用于提取查找键的值。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator Find(const HashTableTypeValue &HashTableValueData)
        {
            if (MemberSize == 0)
            {
                return Iterator(nullptr);
            }
            else
            {
                uint64_t HashMappingValue = HashFunctionObject(HashTableValueData);
                uint64_t HashMapLocation = HashMappingValue % HashCapacity;
                // 找到映射位置
                ContainerNode *HashBucketNode = VectorHashTable[HashMapLocation];
                while (HashBucketNode != nullptr)
                {
                    if (ValueImitationFunctions(HashBucketNode->MemberData) == ValueImitationFunctions(HashTableValueData))
                    {
                        return Iterator(HashBucketNode);
                    }
                    HashBucketNode = HashBucketNode->MemberNext;
                }
                return Iterator(nullptr);
            }
        }

        /**
         * @brief 按键查找元素。
         * @param KeyValue 要查找的键。
         * @return 匹配元素的迭代器；未找到时返回尾后迭代器。
         */
        Iterator FindKey(const HashTableTypeKey& KeyValue)
        {
            return Iterator(FindNodeByKey(KeyValue));
        }
    };
/** @brief 按位保存布尔状态的动态位集合。 */
    class BitSet
    {
        /** @brief 保存每 32 个布尔位的无符号整数字。 */
        StandardCon::Vector<std::uint32_t> VectorBitSet;
        /** @brief 当前已设置为 1 的位数。 */
        uint64_t MemberSize;
        /** @brief 可访问的位索引数量。 */
        uint64_t MemberCapacity = 0;

    public:
        /**
         * @brief 创建空位集合。
         */
        BitSet() : MemberSize(0) { ; }
        /**
         * @brief 创建指定容量的位集合。
         * @param NewCapacity 可用位数。
         */
        explicit BitSet(const uint64_t &NewCapacity)
        {
            MemberSize = 0;
            MemberCapacity = NewCapacity;
            VectorBitSet.SizeAdjust((NewCapacity / 32) + 1, 0);
            // 预留一个额外的 32 位字。
        }
        /**
         * @brief 调整位容量并清空所有位。
         * @param NewCapacity 新的可用位数；调整后所有位均清零。
         */
        void Resize(const uint64_t &NewCapacity)
        {
            VectorBitSet.SizeAdjust((NewCapacity / 32) + 1, 0);
            for (uint64_t Index = 0; Index < VectorBitSet.Size(); ++Index)
            {
                VectorBitSet[Index] = 0;
            }
            MemberSize = 0;
            MemberCapacity = NewCapacity;
        }
        /**
         * @brief 复制位集合。
         * @param BitSetData 要复制的位集合。
         */
        BitSet(const BitSet &BitSetData)
        {
            VectorBitSet = BitSetData.VectorBitSet;
            MemberSize = BitSetData.MemberSize;
            MemberCapacity = BitSetData.MemberCapacity;
        }
        /**
         * @brief 复制另一个位集合。
         * @param BitSetData 要复制的位集合。
         * @return 当前对象的引用。
         */
        BitSet &operator=(const BitSet &BitSetData)
        {
            if (this != &BitSetData)
            {
                VectorBitSet = BitSetData.VectorBitSet;
                MemberSize = BitSetData.MemberSize;
                MemberCapacity = BitSetData.MemberCapacity;
            }
            return *this;
        }
        /**
         * @brief 设置指定位。
         * @param ValueData 要设置的位索引。
         * @throws std::out_of_range 索引超出容量时抛出。
         */
        void Set(const uint64_t &ValueData)
        {
            // 将索引拆分为字索引和字内位偏移。
            if (ValueData >= MemberCapacity)
            {
                throw std::out_of_range("BitSet index out of range");
            }
            uint64_t MappingBit = ValueData / 32; // 定位到BitSet的位置在哪个int上
            uint64_t ValueBit = ValueData % 32;   // 定位到BitSet的位置在哪个int上的第几位
            const auto Mask = std::uint32_t{1} << ValueBit;
            if ((VectorBitSet[MappingBit] & Mask) == 0)
            {
                VectorBitSet[MappingBit] |= Mask;
                ++MemberSize;
            }
        }
        /**
         * @brief 清除指定位。
         * @param ValueData 要清除的位索引。
         * @throws std::out_of_range 索引超出容量时抛出。
         */
        void Reset(const uint64_t &ValueData)
        {
            // 将索引拆分为字索引和字内位偏移。
            if (ValueData >= MemberCapacity)
            {
                throw std::out_of_range("BitSet index out of range");
            }
            uint64_t MappingBit = ValueData / 32;
            uint64_t ValueBit = ValueData % 32;
            const auto Mask = std::uint32_t{1} << ValueBit;
            if ((VectorBitSet[MappingBit] & Mask) != 0)
            {
                VectorBitSet[MappingBit] &= ~Mask;
                --MemberSize;
            }
        }
        /**
         * @brief 取得元素数量。
         * @return 已置位的 bit 数。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return MemberSize;
        }
        /**
         * @brief 检查指定位是否已设置。
         * @param ValueData 要检查的位索引。
         * @return 该位已设置时返回 true；超出范围时返回 false。
         */
        bool Test(const uint64_t &ValueData)
        {
            if (ValueData >= MemberCapacity)
            {
                return false;
            }
            uint64_t MappingBit = ValueData / 32;
            uint64_t ValueBit = ValueData % 32;
            bool ReturnBitSet = (VectorBitSet[MappingBit]
                & (std::uint32_t{1} << ValueBit)) != 0;
            // 如果[]里对应的位是1那么就返回true，否则返回false
            return ReturnBitSet;
        }
    };
}
namespace StandardCon
{
    using BaseContainer::BitSet;
    using BaseContainer::HashTable;
    using BaseContainer::RedBlackTree;
}
