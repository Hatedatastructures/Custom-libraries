/**
 * @file Tree.hpp
 * @brief 二叉搜索树和 AVL 平衡树。
 */
#pragma once
#include "Stack.hpp"
#include "Algorithm.hpp"
#include "Utility.hpp"
#include "Exception.hpp"

#include <algorithm>
#include <utility>
namespace TreeContainer
{
/**
     * @brief 按比较器排序的二叉搜索树。
     * @tparam BinarySearchTreeType 元素类型。
     * @tparam ContainerImitateFunction 元素的比较器类型。
     * @note 操作复杂度取决于树高，最坏为 O(n)。
     */
    template <typename BinarySearchTreeType, typename ContainerImitateFunction = StandardCon::Less<BinarySearchTreeType>>
    class BinaryTree
    {
    private:
        /** @brief 保存二叉搜索树元素及左右子节点链接的节点。 */
        class BinarySearchTreeTypeNode
        {
        public:
            // 节点类
            /** @brief 左子节点；不存在时为空。 */
            BinarySearchTreeTypeNode *MemberLeft;
            /** @brief 右子节点；不存在时为空。 */
            BinarySearchTreeTypeNode *MemberRight;
            /** @brief 节点保存的元素。 */
            BinarySearchTreeType MemberData;

            /**
             * @brief 复制元素并创建节点。
             * @param BinarySearchTreeTypeData 节点中保存的元素。
             */
            explicit BinarySearchTreeTypeNode(const BinarySearchTreeType &BinarySearchTreeTypeData = BinarySearchTreeType())
                    : MemberLeft(nullptr), MemberRight(nullptr), MemberData(BinarySearchTreeTypeData)
            {
                ;
            }
            /** @brief 清空子节点链接；不递归释放子节点。 */
            ~BinarySearchTreeTypeNode()
            {
                MemberLeft = nullptr;
                MemberRight = nullptr;
            }
        };
        /** @brief 二叉搜索树内部节点类型。 */
        using ContainerNode = BinarySearchTreeTypeNode;
        /** @brief 根节点；空树时为空指针。 */
        ContainerNode *MemberRoot;
        /** @brief 比较树中元素的函数对象。 */
        ContainerImitateFunction FunctionPolicy;

        /**
         * @brief 将子树按键升序写入标准输出。
         * @param RootSubtreeNode 要遍历的子树根；为空时不输出内容。
         */
        void InteriorMiddleOrderTraversal(ContainerNode *RootSubtreeNode)
        {
            StandardCon::Stack<ContainerNode *> InteriorStack;
            while (RootSubtreeNode != nullptr || !InteriorStack.Empty())
            {
                while (RootSubtreeNode != nullptr)
                {
                    InteriorStack.Push(RootSubtreeNode);
                    // 压栈
                    RootSubtreeNode = RootSubtreeNode->MemberLeft;
                }
                // 访问栈顶节点
                RootSubtreeNode = InteriorStack.Top();
                // 移除已访问节点。
                InteriorStack.Pop();
                std::cout << RootSubtreeNode->MemberData << " ";
                RootSubtreeNode = RootSubtreeNode->MemberRight;
            }
        }
        /**
         * @brief 遍历子树并累计节点数。
         * @param RootSubtreeNode 要计数的子树根。
         * @param NodeCounter 累计计数器，函数会在其原值上增加节点数。
         * @return 更新后的累计计数。
         */
        uint64_t InteriorMiddleOrderTraversal(ContainerNode *RootSubtreeNode, uint64_t &NodeCounter)
        {
            StandardCon::Stack<ContainerNode *> InteriorStack;
            while (RootSubtreeNode != nullptr || !InteriorStack.Empty())
            {
                while (RootSubtreeNode != nullptr)
                {
                    InteriorStack.Push(RootSubtreeNode);
                    RootSubtreeNode = RootSubtreeNode->MemberLeft;
                }
                RootSubtreeNode = InteriorStack.Top();
                InteriorStack.Pop();
                NodeCounter++;
                RootSubtreeNode = RootSubtreeNode->MemberRight;
            }
            return NodeCounter;
        }
        /**
         * @brief 将子树按根、左、右顺序写入标准输出。
         * @param RootSubtreeNode 要遍历的子树根；为空时不输出内容。
         */
        void InteriorPreOrderTraversal(ContainerNode *RootSubtreeNode)
        {
            // 前序遍历，最外左子树全部压栈
            if (RootSubtreeNode == nullptr)
            {
                return;
            }
            ContainerNode *ReferenceNode = RootSubtreeNode;
            StandardCon::Stack<ContainerNode *> InteriorStack;
            InteriorStack.Push(ReferenceNode);
            while (!InteriorStack.Empty())
            {
                ReferenceNode = InteriorStack.Top();
                InteriorStack.Pop();

                std::cout << ReferenceNode->MemberData << " ";
                if (ReferenceNode->MemberRight != nullptr)
                {
                    InteriorStack.Push(ReferenceNode->MemberRight);
                }
                if (ReferenceNode->MemberLeft != nullptr)
                {
                    InteriorStack.Push(ReferenceNode->MemberLeft);
                } // 先压右子树，使左子树先出栈。
            }
        }
        /** @brief 迭代释放所有节点并将根指针置空。 */
        void Clear() noexcept
        {
            auto *Current = MemberRoot;
            while (Current != nullptr)
            {
                if (Current->MemberLeft != nullptr)
                {
                    auto *Left = Current->MemberLeft;
                    Current->MemberLeft = Left->MemberRight;
                    Left->MemberRight = Current;
                    Current = Left;
                }
                else
                {
                    auto *Right = Current->MemberRight;
                    delete Current;
                    Current = Right;
                }
            }
            MemberRoot = nullptr;
        }

    public:
        /**
         * @brief 销毁 BinaryTree 对象。
         */
        ~BinaryTree() noexcept { Clear(); }
        // 构造函数，使用初始化列表来初始化二叉搜索树
        /**
         * @brief 从初始化列表构造二叉搜索树。
         * @param LightweightContainer 按顺序插入的元素列表。
         */
        BinaryTree(std::initializer_list<BinarySearchTreeType> LightweightContainer)
        {
            MemberRoot = nullptr;
            for (auto &ChainedValues : LightweightContainer)
            {
                Push(ChainedValues);
            }
        }
        /**
         * @brief 构造 BinaryTree 对象。
         */
        BinaryTree()
                : MemberRoot(nullptr)
        {
        }
        /**
         * @brief 用一个元素构造二叉搜索树。
         * @param BsttNode 根节点的值。
         */
        explicit BinaryTree(const BinarySearchTreeType &BsttNode)
                : MemberRoot(new ContainerNode(BsttNode))
        {
        }
        /**
         * @brief 移动另一个二叉搜索树。
         * @param BinarySearchTreeObject 要移出的树；操作后为空。
         */
        BinaryTree(BinaryTree &&BinarySearchTreeObject)
                : MemberRoot(nullptr), FunctionPolicy(BinarySearchTreeObject.FunctionPolicy)
        {
            MemberRoot = std::move(BinarySearchTreeObject.MemberRoot);
            BinarySearchTreeObject.MemberRoot = nullptr;
        }
        /**
         * @brief 深拷贝另一个二叉搜索树。
         * @param BinarySearchTreeObject 要复制的树。
         */
        BinaryTree(const BinaryTree &BinarySearchTreeObject)
                : MemberRoot(nullptr), FunctionPolicy(BinarySearchTreeObject.FunctionPolicy)
        {
            // 拷贝构造，时间复杂度为O(n)
            ContainerNode *ReferenceNode = BinarySearchTreeObject.MemberRoot;
            if (ReferenceNode == nullptr)
            {
                return;
            }
            try
            {
            StandardCon::Stack<StandardCon::Pair<ContainerNode *, ContainerNode **>> InteriorStack;
            // 每项记录源节点和副本中需要填充的链接位置。
                InteriorStack.Push(StandardCon::Pair<ContainerNode *, ContainerNode **>(ReferenceNode, &MemberRoot));
            while (!InteriorStack.Empty())
            {
                auto PairNode = InteriorStack.Top();
                InteriorStack.Pop();
                *(PairNode.Second) = new ContainerNode(PairNode.First->MemberData);
                if (PairNode.First->MemberRight != nullptr)
                {
                    InteriorStack.Push(StandardCon::Pair<ContainerNode *, ContainerNode **>(PairNode.First->MemberRight, &((*PairNode.Second)->MemberRight)));
                }
                if (PairNode.First->MemberLeft != nullptr)
                {
                    InteriorStack.Push(StandardCon::Pair<ContainerNode *, ContainerNode **>(PairNode.First->MemberLeft, &((*PairNode.Second)->MemberLeft)));
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
         * @brief 插入元素；比较器判定为等价时不插入。
         * @param BinarySearchTreeTypeData 要插入的元素。
         * @return 插入成功时返回 true，否则返回 false。
         */
        bool Push(const BinarySearchTreeType &BinarySearchTreeTypeData)
        {
            if (MemberRoot == nullptr)
            {
                MemberRoot = new ContainerNode(BinarySearchTreeTypeData);
                return true;
            }
            else
            {
                ContainerNode *ReferenceNode = MemberRoot;
                ContainerNode *SubtreeNode = nullptr;
                while (ReferenceNode != nullptr)
                {
                    SubtreeNode = ReferenceNode;
                    if (!FunctionPolicy(BinarySearchTreeTypeData, ReferenceNode->MemberData) &&
                            !FunctionPolicy(ReferenceNode->MemberData, BinarySearchTreeTypeData))
                    {
                        // 改用仿函数特性，判断是否有重复元素,防止自定义类型没有重载==运算符
                        return false;
                    }
                    else if (FunctionPolicy(BinarySearchTreeTypeData, ReferenceNode->MemberData))
                    {
                        ReferenceNode = ReferenceNode->MemberLeft;
                    }
                    else
                    {
                        ReferenceNode = ReferenceNode->MemberRight;
                    }
                }
                // 新开节点链接
                auto *NewElementNode = new ContainerNode(BinarySearchTreeTypeData);
                // 链接节点
                if (FunctionPolicy(BinarySearchTreeTypeData, SubtreeNode->MemberData))
                {
                    SubtreeNode->MemberLeft = NewElementNode;
                }
                else
                {
                    SubtreeNode->MemberRight = NewElementNode;
                }
                return true;
            }
        }
        /**
         * @brief 删除与给定值等价的元素。
         * @param BinarySearchTreeTypeData 要删除的元素。
         * @return 当前树的引用；未找到时保持不变。
         */
        BinaryTree &Pop(const BinarySearchTreeType &BinarySearchTreeTypeData)
        {
            // 删除节点
            ContainerNode *ReferenceNode = MemberRoot;
            ContainerNode *SubtreeNode = nullptr;
            while (ReferenceNode != nullptr)
            {
                if (!FunctionPolicy(BinarySearchTreeTypeData, ReferenceNode->MemberData)
                    && !FunctionPolicy(ReferenceNode->MemberData,
                        BinarySearchTreeTypeData))
                {
                    // 找到节点
                    if (ReferenceNode->MemberLeft == nullptr)
                    {
                        // 左子树为空,下面判断要删除的节点是父节点的左子树还是右子树，防止多删和误删
                        if (SubtreeNode == nullptr)
                        {
                            // 当前节点是根节点，直接更新 MemberRoot
                            MemberRoot = ReferenceNode->MemberRight;
                        }
                        else
                        {
                            if (SubtreeNode->MemberLeft == ReferenceNode)
                            {
                                // 根节点
                                SubtreeNode->MemberLeft = ReferenceNode->MemberRight;
                            }
                            else
                            {
                                // 非根节点
                                SubtreeNode->MemberRight = ReferenceNode->MemberRight;
                            }
                        }
                        delete ReferenceNode;
                        ReferenceNode = nullptr;
                        return *this;
                    }
                    else if (ReferenceNode->MemberRight == nullptr)
                    {
                        if (SubtreeNode == nullptr)
                        {
                            // 防止当前节点是根节点，无法解引用，直接更新 MemberRoot
                            MemberRoot = ReferenceNode->MemberLeft;
                        }
                        else
                        {
                            if (SubtreeNode->MemberLeft == ReferenceNode)
                            {
                                SubtreeNode->MemberLeft = ReferenceNode->MemberLeft;
                            }
                            else
                            {
                                SubtreeNode->MemberRight = ReferenceNode->MemberLeft;
                            }
                        }
                        delete ReferenceNode;
                        ReferenceNode = nullptr;
                        return *this;
                    }
                    else
                    {
                        // 左右子树都不为空，找右子树的最左节点
                        ContainerNode *RightSubtreeLeastNode = ReferenceNode->MemberRight;
                        ContainerNode *SubtreeParentNode = ReferenceNode;
                        while (RightSubtreeLeastNode->MemberLeft != nullptr)
                        {
                            SubtreeParentNode = RightSubtreeLeastNode;
                            RightSubtreeLeastNode = RightSubtreeLeastNode->MemberLeft;
                        }
                        // 找到最左节点
                        StandardCon::Algorithm::Swap(ReferenceNode->MemberData, RightSubtreeLeastNode->MemberData);
                        // 因为右树最左节点已经被删，但是还需要把被删的上一节点的左子树指向被删节点的右子树，不管右子树有没有节点都要连接上
                        if (SubtreeParentNode == ReferenceNode)
                        {
                            // 说明右子树没有左子树最小节点就是右子树的第一个根，如同上面判断条件：要删除的根节点等于右子树最小节点的父亲节点
                            SubtreeParentNode->MemberRight = RightSubtreeLeastNode->MemberRight;
                            // 这俩交换指针指向位置就行，上面已经完成值的替换
                        }
                        else
                        {
                            // 情况2：说明要删除的数据的右子树的最左节点如果有数据，就把数据连接到右子树的最左节点的父亲节点的左子树指向最左子树的右子树
                            SubtreeParentNode->MemberLeft = RightSubtreeLeastNode->MemberRight;
                        }
                        delete RightSubtreeLeastNode;
                        RightSubtreeLeastNode = nullptr;
                        return *this;
                    }
                }
                else if (FunctionPolicy(BinarySearchTreeTypeData, ReferenceNode->MemberData))
                {
                    SubtreeNode = ReferenceNode;
                    ReferenceNode = ReferenceNode->MemberLeft;
                }
                else
                {
                    SubtreeNode = ReferenceNode;
                    ReferenceNode = ReferenceNode->MemberRight;
                }
            }
            return *this;
        }
        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        uint64_t Size()
        {
            uint64_t SizeCount = 0;
            return InteriorMiddleOrderTraversal(MemberRoot, SizeCount);
        }
        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const
        {
            uint64_t NodeNumberCounter = 0;
            return InteriorMiddleOrderTraversal(MemberRoot, NodeNumberCounter);
        }
        /**
         * @brief 查找与给定值等价的节点。
         * @param FindNode 要查找的元素。
         * @return 匹配节点的指针；未找到时返回空指针。
         */
        ContainerNode *Find(const BinarySearchTreeType &FindNode)
        {
            // 查找函数
            ContainerNode *ReferenceNode = MemberRoot;
            while (ReferenceNode != nullptr)
            {
                if (!FunctionPolicy(FindNode, ReferenceNode->MemberData)
                    && !FunctionPolicy(ReferenceNode->MemberData, FindNode))
                {
                    return ReferenceNode;
                }
                else if (FunctionPolicy(FindNode, ReferenceNode->MemberData))
                {
                    ReferenceNode = ReferenceNode->MemberLeft;
                }
                else
                {
                    ReferenceNode = ReferenceNode->MemberRight;
                }
            }
            return nullptr;
        }
        /**
         * @brief 确认锚点存在后，按比较器插入新元素。
         * @param ExistingValue 必须已存在于树中的锚点值。
         * @param NewValue 要插入的元素。
         * @throws CustomException::Fault 锚点不在树中时抛出。
         */
        void Insert(const BinarySearchTreeType &ExistingValue, const BinarySearchTreeType &NewValue)
        {
            // 在ExistingValue后面插入NewValue
            ContainerNode *ExistingValueNode = Find(ExistingValue);
            // 插入节点
            if (ExistingValueNode == nullptr)
            {
                throw CustomException::Fault("传入值未找到！", "insert::find", __LINE__);
            }
            else
            {
                (void)ExistingValueNode;
                Push(NewValue);
            }
        }
        /**
         * @brief 深拷贝赋值。
         * @param BinarySearchTreeObject 要复制的树。
         * @return 当前对象的引用。
         */
        BinaryTree &operator=(const BinaryTree &BinarySearchTreeObject)
        {
            if (this != &BinarySearchTreeObject)
            {
                BinaryTree Copy(BinarySearchTreeObject);
                using std::swap;
                swap(MemberRoot, Copy.MemberRoot);
                swap(FunctionPolicy, Copy.FunctionPolicy);
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param BinarySearchTreeObject 要移出的树；操作后为空。
         * @return 当前对象的引用。
         */
        BinaryTree &operator=(BinaryTree &&BinarySearchTreeObject)
        {
            // 移动赋值运算符重载
            if (this != &BinarySearchTreeObject)
            {
                Clear();
                FunctionPolicy = BinarySearchTreeObject.FunctionPolicy;
                MemberRoot = std::move(BinarySearchTreeObject.MemberRoot);
                BinarySearchTreeObject.MemberRoot = nullptr;
            }
            return *this;
        }
    };
/**
     * @brief 按键排序的 AVL 平衡树。
     * @tparam AvlTreeTypeK 键类型。
     * @tparam AvlTreeTypeV 值类型。
     * @tparam ContainerImitateFunction 键的比较器类型。
     * @tparam AvlTreeNodePair 节点存储的键值对类型。
     * @note 查找、插入和删除的复杂度为 O(log n)；`Size()` 通过遍历计数，复杂度为 O(n)。
     */
    template <typename AvlTreeTypeK, typename AvlTreeTypeV, typename ContainerImitateFunction = StandardCon::Less<AvlTreeTypeK>,
                        typename AvlTreeNodePair = StandardCon::Pair<AvlTreeTypeK, AvlTreeTypeV>>
    class BalanceTree
    {
    private:
        /** @brief AVL 树节点，保存键值对、父子链接及平衡信息。 */
        class AvlTreeTypeNode
        {
        public:
            /** @brief 节点保存的键值对。 */
            AvlTreeNodePair MemberData;

            /** @brief 左子节点；不存在时为空。 */
            AvlTreeTypeNode *MemberLeft;
            /** @brief 右子节点；不存在时为空。 */
            AvlTreeTypeNode *MemberRight;
            /** @brief 父节点；根节点的父链接为空。 */
            AvlTreeTypeNode *MemberParent;
            /** @brief 右子树高度减左子树高度。 */
            int MemberBalanceFactor;
            /** @brief 以当前节点为根的子树高度。 */
            int MemberHeight;
            /**
             * @brief 用键和值创建叶节点。
             * @param AvlTtKData 节点键。
             * @param AvlTtVData 节点值。
             */
            explicit AvlTreeTypeNode(const AvlTreeTypeK &AvlTtKData = AvlTreeTypeK(), const AvlTreeTypeV &AvlTtVData = AvlTreeTypeV())
                    : MemberData(AvlTtKData, AvlTtVData), MemberLeft(nullptr), MemberRight(nullptr), MemberParent(nullptr), MemberBalanceFactor(0), MemberHeight(1)
            {
                ;
            }
            /**
             * @brief 用键值对创建叶节点。
             * @param PairTypeData 要复制到节点中的键值对。
             */
            explicit AvlTreeTypeNode(const AvlTreeNodePair &PairTypeData)
                    : MemberData(PairTypeData), MemberLeft(nullptr), MemberRight(nullptr), MemberParent(nullptr), MemberBalanceFactor(0), MemberHeight(1)
            {
                ;
            }
        };
        /**
         * @brief 沿 AVL 树中序遍历的双向迭代器。
         * @tparam T 节点值类型，可带 const 限定。
         * @tparam Ref 解引用返回的引用类型。
         * @tparam Ptr 箭头运算符返回的指针类型。
         * @note 空节点指针表示尾后位置；解引用、递增或递减尾后迭代器无效。
         * @note 删除会使指向被删节点的迭代器失效；旋转不会移动其他节点。
         */
        template <typename T, typename Ref, typename Ptr>
        class AvlTreeIterator
        {
        public:
            /** @brief 迭代器指向的 AVL 节点类型。 */
            using IteratorNode = AvlTreeTypeNode;
            /** @brief 当前迭代器类型。 */
            using Self = AvlTreeIterator<T, Ref, Ptr>;
            /** @brief 箭头运算符返回的指针类型。 */
            using Pointer = Ptr;
            /** @brief 解引用返回的引用类型。 */
            using Reference = Ref;
            /** @brief 当前节点；空指针表示尾后位置。 */
            IteratorNode *MemberNodeIteratorPtr;

            /**
             * @brief 从节点指针构造迭代器。
             * @param IteratorPtrData 当前节点；空指针表示尾后位置。
             */
            explicit AvlTreeIterator(IteratorNode *IteratorPtrData)
                    : MemberNodeIteratorPtr(IteratorPtrData) { ; }

            /**
             * @brief 取得当前键值对的指针。
             * @return 当前键值对的指针。
             * @pre 迭代器不得处于尾后位置。
             */
            Ptr operator->() { return &(MemberNodeIteratorPtr->MemberData); }

            /**
             * @brief 访问当前键值对。
             * @return 当前键值对的引用。
             * @pre 迭代器不得处于尾后位置。
             */
            Ref &operator*() { return MemberNodeIteratorPtr->MemberData; }

            /**
             * @brief 比较两个迭代器是否不同。
             * @param AnotherIterator 要比较的迭代器。
             * @return 指向不同节点时返回 true。
             */
            bool operator!=(const Self &AnotherIterator) { return MemberNodeIteratorPtr != AnotherIterator.MemberNodeIteratorPtr; }

            /**
             * @brief 比较两个迭代器是否相同。
             * @param AnotherIterator 要比较的迭代器。
             * @return 指向同一节点时返回 true。
             */
            bool operator==(const Self &AnotherIterator) { return MemberNodeIteratorPtr == AnotherIterator.MemberNodeIteratorPtr; }

            /**
             * @brief 前进到按键排序的下一个节点。
             * @return 当前迭代器的引用。
             */
            Self &operator++()
            {
                if (MemberNodeIteratorPtr->MemberRight != nullptr)
                {
                    MemberNodeIteratorPtr = MemberNodeIteratorPtr->MemberRight;
                    while (MemberNodeIteratorPtr->MemberLeft != nullptr)
                    {
                        MemberNodeIteratorPtr = MemberNodeIteratorPtr->MemberLeft;
                    }
                }
                else
                {
                    ContainerNode *IteratorSelfNode = MemberNodeIteratorPtr;
                    while (IteratorSelfNode->MemberParent != nullptr && IteratorSelfNode == IteratorSelfNode->MemberParent->MemberRight)
                    {
                        IteratorSelfNode = IteratorSelfNode->MemberParent;
                    }
                    MemberNodeIteratorPtr = IteratorSelfNode->MemberParent;
                }
                return *this;
            }
            /**
             * @brief 后置递增。
             * @return 递增前的迭代器副本。
             */
            Self operator++(int)
            {
                Self ReturnSelf = *this;
                ++(*this);
                return ReturnSelf;
            }
            /**
             * @brief 后退到按键排序的前一个节点。
             * @return 当前迭代器的引用。
             */
            Self &operator--()
            {
                if (MemberNodeIteratorPtr->MemberLeft != nullptr)
                {
                    MemberNodeIteratorPtr = MemberNodeIteratorPtr->MemberLeft;
                    while (MemberNodeIteratorPtr->MemberRight != nullptr)
                    {
                        MemberNodeIteratorPtr = MemberNodeIteratorPtr->MemberRight;
                    }
                }
                else
                {
                    ContainerNode *IteratorSelfNode = MemberNodeIteratorPtr;
                    while (IteratorSelfNode->MemberParent != nullptr && IteratorSelfNode == IteratorSelfNode->MemberParent->MemberLeft)
                    {
                        IteratorSelfNode = IteratorSelfNode->MemberParent;
                    }
                    MemberNodeIteratorPtr = IteratorSelfNode->MemberParent;
                }
                return *this;
            }
            /**
             * @brief 后置递减。
             * @return 递减前的迭代器副本。
             */
            Self operator--(int)
            {
                Self ReturnSelf = *this;
                --(*this);
                return ReturnSelf;
            }
        };
        /** @brief 将 AVL 树迭代器按降序移动的反向迭代器。 */
        template <typename Iterator>
        class AvlTreeReverseIterator
        {
        public:
            /** @brief 当前反向迭代器类型。 */
            using Self = AvlTreeReverseIterator<Iterator>;
            /** @brief 当前节点的底层树迭代器。 */
            Iterator MemberIt;
            /** @brief 箭头运算符返回的指针类型。 */
            using Ptr = typename Iterator::Pointer;
            /** @brief 解引用返回的引用类型。 */
            using Ref = typename Iterator::Reference;

            /**
             * @brief 从底层树迭代器构造反向迭代器。
             * @param IteratorData 当前节点迭代器。
             */
            explicit AvlTreeReverseIterator(Iterator IteratorData)
                    : MemberIt(IteratorData) { ; }

            /**
             * @brief 取得当前键值对的指针。
             * @return 当前键值对的指针。
             * @pre 迭代器不得处于反向尾后位置。
             */
            Ptr operator->() { return &(*this); }

            /**
             * @brief 访问当前键值对。
             * @return 当前键值对的引用。
             * @pre 迭代器不得处于反向尾后位置。
             */
            Ref &operator*() { return *MemberIt; }

            /**
             * @brief 比较两个反向迭代器是否不同。
             * @param AnotherIterator 要比较的反向迭代器。
             * @return 指向不同节点时返回 true。
             */
            bool operator!=(const Self &AnotherIterator) { return MemberIt != AnotherIterator.MemberIt; }

            /**
             * @brief 比较两个反向迭代器是否相同。
             * @param AnotherIterator 要比较的反向迭代器。
             * @return 指向同一节点时返回 true。
             */
            bool operator==(const Self &AnotherIterator) { return MemberIt == AnotherIterator.MemberIt; }

            /**
             * @brief 反向前进到前一个键。
             * @return 当前反向迭代器的引用。
             */
            Self &operator++()
            {
                --MemberIt;
                return *this;
            }
            /**
             * @brief 后置递增。
             * @return 递增前的迭代器副本。
             */
            Self operator++(int)
            {
                Self ReturnSelf = *this;
                --(*this);
                return ReturnSelf;
            }
            /**
             * @brief 反向后退到后一个键。
             * @return 当前反向迭代器的引用。
             */
            Self &operator--()
            {
                ++MemberIt;
                return *this;
            }
            /**
             * @brief 后置递减。
             * @return 递减前的迭代器副本。
             */
            Self operator--(int)
            {
                Self ReturnSelf = *this;
                ++(*this);
                return ReturnSelf;
            }
        };
        /** @brief AVL 树内部节点类型。 */
        using ContainerNode = AvlTreeTypeNode;
        /** @brief 根节点；空树时为空指针。 */
        ContainerNode *MemberRoot;

        /** @brief 比较 AVL 树中键的函数对象。 */
        ContainerImitateFunction FunctionPolicy;

        /**
         * @brief 取得子树高度。
         * @param Node 子树根节点；空指针表示空子树。
         * @return 子树高度；空子树高度为零。
         */
        static auto Height(const ContainerNode *Node) noexcept -> int
        {
            return Node == nullptr ? 0 : Node->MemberHeight;
        }

        /**
         * @brief 根据左右子树高度更新节点高度和平衡因子。
         * @param Node 要更新的节点。
         */
        static void UpdateHeightAndBalance(ContainerNode *Node) noexcept
        {
            const auto LeftHeight = Height(Node->MemberLeft);
            const auto RightHeight = Height(Node->MemberRight);
            Node->MemberHeight = std::max(LeftHeight, RightHeight) + 1;
            Node->MemberBalanceFactor = RightHeight - LeftHeight;
        }

        /**
         * @brief 对节点执行左旋并更新高度和平衡因子。
         * @param Node 要旋转的子树根；必须存在右子节点。
         * @return 旋转后的子树根。
         */
        auto RotateLeft(ContainerNode *Node) noexcept -> ContainerNode *
        {
            auto *Pivot = Node->MemberRight;
            auto *Parent = Node->MemberParent;
            Node->MemberRight = Pivot->MemberLeft;
            if (Node->MemberRight != nullptr)
            {
                Node->MemberRight->MemberParent = Node;
            }
            Pivot->MemberLeft = Node;
            Node->MemberParent = Pivot;
            Pivot->MemberParent = Parent;
            if (Parent == nullptr)
            {
                MemberRoot = Pivot;
            }
            else if (Parent->MemberLeft == Node)
            {
                Parent->MemberLeft = Pivot;
            }
            else
            {
                Parent->MemberRight = Pivot;
            }
            UpdateHeightAndBalance(Node);
            UpdateHeightAndBalance(Pivot);
            return Pivot;
        }

        /**
         * @brief 对节点执行右旋并更新高度和平衡因子。
         * @param Node 要旋转的子树根；必须存在左子节点。
         * @return 旋转后的子树根。
         */
        auto RotateRight(ContainerNode *Node) noexcept -> ContainerNode *
        {
            auto *Pivot = Node->MemberLeft;
            auto *Parent = Node->MemberParent;
            Node->MemberLeft = Pivot->MemberRight;
            if (Node->MemberLeft != nullptr)
            {
                Node->MemberLeft->MemberParent = Node;
            }
            Pivot->MemberRight = Node;
            Node->MemberParent = Pivot;
            Pivot->MemberParent = Parent;
            if (Parent == nullptr)
            {
                MemberRoot = Pivot;
            }
            else if (Parent->MemberLeft == Node)
            {
                Parent->MemberLeft = Pivot;
            }
            else
            {
                Parent->MemberRight = Pivot;
            }
            UpdateHeightAndBalance(Node);
            UpdateHeightAndBalance(Pivot);
            return Pivot;
        }

        /**
         * @brief 从指定节点向根方向更新高度并恢复 AVL 平衡。
         * @param Node 开始检查的节点；可以为空。
         */
        void RebalanceFrom(ContainerNode *Node) noexcept
        {
            while (Node != nullptr)
            {
                UpdateHeightAndBalance(Node);
                ContainerNode *SubtreeRoot = Node;
                if (Node->MemberBalanceFactor < -1)
                {
                    if (Node->MemberLeft->MemberBalanceFactor > 0)
                    {
                        (void)RotateLeft(Node->MemberLeft);
                    }
                    SubtreeRoot = RotateRight(Node);
                }
                else if (Node->MemberBalanceFactor > 1)
                {
                    if (Node->MemberRight->MemberBalanceFactor < 0)
                    {
                        (void)RotateRight(Node->MemberRight);
                    }
                    SubtreeRoot = RotateLeft(Node);
                }
                Node = SubtreeRoot->MemberParent;
            }
        }

        /**
         * @brief 按键插入键值对并恢复 AVL 平衡。
         * @param Value 要插入的键值对。
         * @return 新键插入成功时返回 true；等价键已存在时返回 false。
         */
        bool Insert(const AvlTreeNodePair &Value)
        {
            ContainerNode *Parent = nullptr;
            auto *Current = MemberRoot;
            while (Current != nullptr)
            {
                Parent = Current;
                if (!FunctionPolicy(Value.First, Current->MemberData.First)
                    && !FunctionPolicy(Current->MemberData.First, Value.First))
                {
                    return false;
                }
                Current = FunctionPolicy(Value.First, Current->MemberData.First)
                    ? Current->MemberLeft : Current->MemberRight;
            }

            auto *NewNode = new ContainerNode(Value);
            NewNode->MemberParent = Parent;
            if (Parent == nullptr)
            {
                MemberRoot = NewNode;
            }
            else if (FunctionPolicy(Value.First, Parent->MemberData.First))
            {
                Parent->MemberLeft = NewNode;
            }
            else
            {
                Parent->MemberRight = NewNode;
            }
            RebalanceFrom(Parent);
            return true;
        }

        /**
         * @brief 对指定子树执行左单旋并更新链接和平衡因子。
         * @param SubtreeNode 子树根指针的引用；调用前须非空且有右子节点。
         * @throws CustomException::Fault 子树根为空时抛出。
         */
        void LeftRevolve(ContainerNode *&SubtreeNode)
        {
            /*                                                                                                              左单旋情况：简化图
                    传进来的值是发现该树平衡性被破坏的节点地址                                                                     SubtreeNode(10)(当前平衡因子为-2，触发调整)
                    大致思想：因为这是左单旋，所以找传进来的父亲节点的右根节点来当调整节点                                                         \
                    然后把调整节点的左根节点赋值给传进来的父亲节点的右根节点 (刚才已经用节点保存过调整节点，所以这里直接赋值)，                   SubTreeRightNode(20)(当前平衡因子为-1，不触发调整)
                    再把父亲节点赋值给调整节点的左根节点，！！注意：在旋转的过程中还要处理每个调整节点的父亲节点的指向和平衡因子                      /         \
                    {                                                                                            SubRightLeftNode(nullptr)  SubTreeRightRightNode(30)
                            ContainerNode* SubTreeRightNode = SubtreeNode->MemberRight;                                  /############             分割线             ############/
                            SubtreeNode->MemberRight = SubTreeRightNode->MemberLeft;                                                          SubTreeRightNode(20)(当前平衡因子为0)
                            SubTreeRightNode->MemberLeft = SubtreeNode;                                                                        /       \
                    }                                                                                                                 \
                    if(SubtreeNode == nullptr || SubtreeNode->MemberRight == nullptr)                                 SubRightLeftNode(nullptr)
                    {
                            std::cout <<"left "<< "空指针"  <<std::endl;
                            return ;
                    }
            */
            try
            {
                if (SubtreeNode == nullptr)
                {
                    throw CustomException::Fault("左单旋传进来的节点为空！", "left_revolve", __LINE__);
                }
            }
            catch (const CustomException::Fault &Exception)
            {
                std::cerr << Exception.what() << Exception.FunctionNameGet() << Exception.LineNumberGet() << std::endl;
                throw;
            }
            ContainerNode *SubTreeRightNode = SubtreeNode->MemberRight;
            ContainerNode *SubRightLeftNode = (SubTreeRightNode->MemberLeft) ? SubTreeRightNode->MemberLeft : nullptr;
            // 防止空指针解引用
            SubtreeNode->MemberRight = SubRightLeftNode; // 因为调整后ParentNode的位置是SubRightLeftNode的位置
            if (SubRightLeftNode)                    // 把ParentNode的右根节点赋值给SubRightLeftNode
            {
                SubRightLeftNode->MemberParent = SubtreeNode;
                // 如果SubRightLeftNode(调整节点的左根节点)不等于空，还需要调整SubRightLeftNode它的父亲节点
            }
            SubTreeRightNode->MemberLeft = SubtreeNode;
            // 这里先保存一下ParentNode的父亲地址，防止到下面else比较的时候丢失
            ContainerNode *ParentNode = SubtreeNode->MemberParent;
            SubtreeNode->MemberParent = SubTreeRightNode;
            // 更新ParentNode节点指向正确的地址

            if (MemberRoot == SubtreeNode)
            {
                // 如果要调整的节点是根节点，直接把调整节点赋值给根节点，然后把调整节点的父亲节点置空
                MemberRoot = SubTreeRightNode;
                SubTreeRightNode->MemberParent = nullptr;
            }
            else
            {
                // 调整前ParentNode是这个树的根现在是SubTreeRightNode是这个树的根
                if (ParentNode->MemberLeft == SubtreeNode)
                {
                    ParentNode->MemberLeft = SubTreeRightNode;
                }
                else
                {
                    ParentNode->MemberRight = SubTreeRightNode;
                }
                SubTreeRightNode->MemberParent = ParentNode;
            }
            SubtreeNode->MemberBalanceFactor = SubTreeRightNode->MemberBalanceFactor = 0;
        }

        /**
         * @brief 对指定子树执行右单旋并更新链接和平衡因子。
         * @param SubtreeNode 子树根指针的引用；调用前须非空且有左子节点。
         * @throws CustomException::Fault 子树根为空时抛出。
         */
        void RightRevolve(ContainerNode *&SubtreeNode)
        {
            // 思路同左单旋思路差不多,实现相反，图略
            try
            {
                if (SubtreeNode == nullptr)
                {
                    throw CustomException::Fault("右单旋传进来的节点为空！", "right_revolve", __LINE__);
                }
            }
            catch (const CustomException::Fault &Exception)
            {
                std::cerr << Exception.what() << Exception.FunctionNameGet() << Exception.LineNumberGet() << std::endl;
                throw;
            }
            ContainerNode *SubTreeLeftNode = SubtreeNode->MemberLeft;
            ContainerNode *SubLeftRightNode = (SubTreeLeftNode->MemberRight) ? SubTreeLeftNode->MemberRight : nullptr;
            // 防止空指针解引用
            SubtreeNode->MemberLeft = SubLeftRightNode;
            if (SubLeftRightNode)
            {
                SubLeftRightNode->MemberParent = SubtreeNode;
            }
            SubTreeLeftNode->MemberRight = SubtreeNode;
            // 保存ParentTempNode的父亲节点
            ContainerNode *ParentNode = SubtreeNode->MemberParent;
            SubtreeNode->MemberParent = SubTreeLeftNode;

            if (MemberRoot == SubtreeNode)
            {
                MemberRoot = SubTreeLeftNode;
                SubTreeLeftNode->MemberParent = nullptr;
            }
            else
            {
                if (ParentNode->MemberLeft == SubtreeNode)
                {
                    ParentNode->MemberLeft = SubTreeLeftNode;
                }
                else
                {
                    ParentNode->MemberRight = SubTreeLeftNode;
                }
                SubTreeLeftNode->MemberParent = ParentNode;
            }
            SubtreeNode->MemberBalanceFactor = SubTreeLeftNode->MemberBalanceFactor = 0;
        }
        /**
         * @brief 对指定子树执行先右后左的双旋。
         * @param SubtreeNode 子树根指针的引用；调用前须有右子节点及其左子节点。
         * @throws CustomException::Fault 子树根为空时抛出。
         */
        void RightLeftRevolve(ContainerNode *&SubtreeNode)
        {
            try
            {
                if (SubtreeNode == nullptr)
                {
                    throw CustomException::Fault("右左双旋传进来的节点为空！", "right_left_revolve", __LINE__);
                }
            }
            catch (const CustomException::Fault &Exception)
            {
                std::cerr << Exception.what() << Exception.FunctionNameGet() << Exception.LineNumberGet() << std::endl;
                throw;
            }
            ContainerNode *SubTreeRightNode = SubtreeNode->MemberRight;
            ContainerNode *SubRightLeftNode = SubTreeRightNode->MemberLeft;
            int SeparateBalanceFactor = SubRightLeftNode->MemberBalanceFactor;

            RightRevolve(SubtreeNode->MemberRight);
            // 右旋
            LeftRevolve(SubtreeNode);
            // 左旋
            if (SeparateBalanceFactor == -1)
            {
                SubtreeNode->MemberBalanceFactor = 0;
                SubTreeRightNode->MemberBalanceFactor = 1;
                SubRightLeftNode->MemberBalanceFactor = 0;
            }
            else if (SeparateBalanceFactor == 1)
            {
                SubtreeNode->MemberBalanceFactor = -1;
                SubTreeRightNode->MemberBalanceFactor = 0;
                SubRightLeftNode->MemberBalanceFactor = 0;
            }
            else
            {
                SubtreeNode->MemberBalanceFactor = 0;
                SubTreeRightNode->MemberBalanceFactor = 0;
                SubRightLeftNode->MemberBalanceFactor = 0;
            }
        }
        /**
         * @brief 对指定子树执行先左后右的双旋。
         * @param SubtreeNode 子树根指针的引用；调用前须有左子节点及其右子节点。
         * @throws CustomException::Fault 子树根为空时抛出。
         */
        void LeftRightRevolve(ContainerNode *&SubtreeNode)
        {
            try
            {
                if (SubtreeNode == nullptr)
                {
                    throw CustomException::Fault("左右单旋传进来的节点为空！", "left_right_revolve", __LINE__);
                }
            }
            catch (const CustomException::Fault &Exception)
            {
                std::cerr << Exception.what() << Exception.FunctionNameGet() << Exception.LineNumberGet() << std::endl;
                throw;
            }
            ContainerNode *SubTreeLeftNode = SubtreeNode->MemberLeft;
            ContainerNode *SubLeftRightNode = SubTreeLeftNode->MemberRight;
            int SeparateBalanceFactor = SubLeftRightNode->MemberBalanceFactor;

            LeftRevolve(SubtreeNode->MemberLeft);
            // 左旋
            RightRevolve(SubtreeNode);
            // 右旋
            if (SeparateBalanceFactor == -1)
            {
                SubtreeNode->MemberBalanceFactor = 0;
                SubTreeLeftNode->MemberBalanceFactor = 1;
                SubLeftRightNode->MemberBalanceFactor = 0;
            }
            else if (SeparateBalanceFactor == 1)
            {
                SubtreeNode->MemberBalanceFactor = -1;
                SubTreeLeftNode->MemberBalanceFactor = 0;
                SubLeftRightNode->MemberBalanceFactor = 0;
            }
            else
            {
                SubtreeNode->MemberBalanceFactor = 0;
                SubTreeLeftNode->MemberBalanceFactor = 0;
                SubLeftRightNode->MemberBalanceFactor = 0;
            }
        }
        /** @brief 迭代释放 AVL 树的所有节点并将根指针置空。 */
        void Clear() noexcept
        {
            auto *Current = MemberRoot;
            while (Current != nullptr)
            {
                if (Current->MemberLeft != nullptr)
                {
                    auto *Left = Current->MemberLeft;
                    Current->MemberLeft = Left->MemberRight;
                    Left->MemberRight = Current;
                    Current = Left;
                }
                else
                {
                    auto *Right = Current->MemberRight;
                    delete Current;
                    Current = Right;
                }
            }
            MemberRoot = nullptr;
        }
        /**
         * @brief 将子树按根、左、右顺序写入标准输出。
         * @param RootSubtreeNode 要遍历的子树根；为空时不输出内容。
         */
        void InteriorPreOrderTraversal(ContainerNode *RootSubtreeNode)
        {
            // 前序遍历，最外左子树全部压栈
            if (RootSubtreeNode == nullptr)
            {
                return;
            }
            ContainerNode *ReferenceNode = RootSubtreeNode;
            StandardCon::Stack<ContainerNode *> InteriorStack;
            InteriorStack.Push(ReferenceNode);
            while (!InteriorStack.Empty())
            {
                ReferenceNode = InteriorStack.Top();
                InteriorStack.Pop();

                std::cout << ReferenceNode->MemberData << " ";
                if (ReferenceNode->MemberRight != nullptr)
                {
                    InteriorStack.Push(ReferenceNode->MemberRight);
                }
                if (ReferenceNode->MemberLeft != nullptr)
                {
                    InteriorStack.Push(ReferenceNode->MemberLeft);
                }
            }
        }
        /**
         * @brief 将子树按键升序写入标准输出。
         * @param RootSubtreeNode 要遍历的子树根；为空时不输出内容。
         */
        void InteriorMiddleOrderTraversal(ContainerNode *RootSubtreeNode)
        {
            // 中序遍历函数
            StandardCon::Stack<ContainerNode *> InteriorStack;
            while (RootSubtreeNode != nullptr || !InteriorStack.Empty())
            {
                while (RootSubtreeNode != nullptr)
                {
                    InteriorStack.Push(RootSubtreeNode);
                    // 压栈
                    RootSubtreeNode = RootSubtreeNode->MemberLeft;
                }
                // 访问栈顶节点
                RootSubtreeNode = InteriorStack.Top();
                // 弹出栈顶元素，刷新栈顶元素，栈顶元素会变成之前压入栈的节点的父节点

                InteriorStack.Pop();
                std::cout << RootSubtreeNode->MemberData << " ";
                RootSubtreeNode = RootSubtreeNode->MemberRight;
            }
        }
        /**
         * @brief 遍历计数并取得 AVL 树节点数。
         * @return 当前树中的节点数。
         * @note 需要访问每个节点，复杂度为 O(n)。
         */
        uint64_t InternalSize()
        {
            uint64_t AvlTreeNodeCounters = 0;
            if (MemberRoot == nullptr)
            {
                return AvlTreeNodeCounters;
            }
            else
            {
                ContainerNode *ReferenceNode = MemberRoot;
                StandardCon::Stack<ContainerNode *> InteriorStack;
                InteriorStack.Push(ReferenceNode);
                while (!InteriorStack.Empty())
                {
                    ReferenceNode = InteriorStack.Top();
                    InteriorStack.Pop();

                    AvlTreeNodeCounters++;

                    if (ReferenceNode->MemberRight != nullptr)
                    {
                    InteriorStack.Push(ReferenceNode->MemberRight);
                    }
                    if (ReferenceNode->MemberLeft != nullptr)
                    {
                    InteriorStack.Push(ReferenceNode->MemberLeft);
                    }
                }
            }
            return AvlTreeNodeCounters;
        }

    public:
        /** @brief 按键升序遍历的可修改迭代器。 */
        using Iterator = AvlTreeIterator<AvlTreeNodePair, AvlTreeNodePair &, AvlTreeNodePair *>;
        /** @brief 按键升序遍历的只读迭代器。 */
        using ConstIterator = AvlTreeIterator<AvlTreeNodePair, const AvlTreeNodePair &, const AvlTreeNodePair *>;

        /** @brief 按键降序遍历的可修改反向迭代器。 */
        using ReverseIterator = AvlTreeReverseIterator<Iterator>;
        /** @brief 按键降序遍历的只读反向迭代器。 */
        using ConstReverseIterator = AvlTreeReverseIterator<ConstIterator>;

        /**
         * @brief 取得首元素迭代器。
         * @return 指向最小键元素的迭代器；空树返回尾后迭代器。
         */
        Iterator Begin()
        {
            ContainerNode *StartPositionNode = MemberRoot;
            while (StartPositionNode != nullptr && StartPositionNode->MemberLeft != nullptr)
            {
                StartPositionNode = StartPositionNode->MemberLeft;
            }
            return Iterator(StartPositionNode);
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
            ContainerNode *StartPositionNode = MemberRoot;
            while (StartPositionNode != nullptr && StartPositionNode->MemberLeft != nullptr)
            {
                StartPositionNode = StartPositionNode->MemberLeft;
            }
            return ConstIterator(StartPositionNode);
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
            ContainerNode *ReverseStartPosition = MemberRoot;
            while (ReverseStartPosition != nullptr && ReverseStartPosition->MemberRight != nullptr)
            {
                ReverseStartPosition = ReverseStartPosition->MemberRight;
            }
            return ReverseIterator(Iterator(ReverseStartPosition));
        }

        /**
         * @brief 取得反向尾后迭代器。
         * @return 反向尾后迭代器。
         */
        static ReverseIterator Rend()
        {
            return ReverseIterator(Iterator(nullptr));
        }
        /**
         * @brief 取得只读反向首元素迭代器。
         * @return 指向最大键元素的只读反向迭代器；空树返回反向尾后迭代器。
         */
        ConstReverseIterator Crbegin() const
        {
            ContainerNode *ReverseStartPosition = MemberRoot;
            while (ReverseStartPosition != nullptr && ReverseStartPosition->MemberRight != nullptr)
            {
                ReverseStartPosition = ReverseStartPosition->MemberRight;
            }
            return ConstReverseIterator(ConstIterator(ReverseStartPosition));
        }

        /**
         * @brief 取得只读反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        static ConstReverseIterator Crend()
        {
            return ConstReverseIterator(ConstIterator(nullptr));
        }
        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        bool Empty()
        {
            return MemberRoot == nullptr;
        }
        /**
         * @brief 构造 BalanceTree 对象。
         */
        BalanceTree()
        {
            MemberRoot = nullptr;
        }
        /**
         * @brief 用一个键值对和指定比较器构造 AVL 树。
         * @param KeyData 根节点的键。
         * @param ValData 根节点的值。
         * @param ComValue 比较键的函数对象。
         */
        explicit BalanceTree(const AvlTreeTypeK &KeyData, const AvlTreeTypeV &ValData = AvlTreeTypeV(),
                                                                                ContainerImitateFunction ComValue = ContainerImitateFunction())
                : MemberRoot(nullptr), FunctionPolicy(ComValue)
        {
            MemberRoot = new ContainerNode(KeyData, ValData);
        }
        /**
         * @brief 用一个键值对对象和指定比较器构造 AVL 树。
         * @param PairTypeData 根节点的键值对。
         * @param ComValue 比较键的函数对象。
         */
        explicit BalanceTree(const AvlTreeNodePair &PairTypeData,
                                                                                ContainerImitateFunction ComValue = ContainerImitateFunction())
                : MemberRoot(nullptr), FunctionPolicy(ComValue)
        {
            MemberRoot = new ContainerNode(PairTypeData.First, PairTypeData.Second);
        }
        /**
         * @brief 深拷贝另一个 AVL 树。
         * @param AvlTreeData 要复制的树。
         */
        BalanceTree(const BalanceTree &AvlTreeData)
                : MemberRoot(nullptr), FunctionPolicy(AvlTreeData.FunctionPolicy)
        {
            if (AvlTreeData.MemberRoot == nullptr)
            {
                return;
            }

            try
            {
            // 使用单栈，存储源节点和目标父节点（均为一级指针）
            StandardCon::Stack<StandardCon::Pair<ContainerNode *, ContainerNode *>> Stack;

            // 创建根节点
            MemberRoot = new ContainerNode(AvlTreeData.MemberRoot->MemberData);
            MemberRoot->MemberBalanceFactor = AvlTreeData.MemberRoot->MemberBalanceFactor;
            MemberRoot->MemberHeight = AvlTreeData.MemberRoot->MemberHeight;
            MemberRoot->MemberParent = nullptr; // 根节点的父节点为nullptr

            // 初始化栈，将根节点的子节点压入（注意：这里父节点是 Root，一级指针）
            if (AvlTreeData.MemberRoot->MemberRight != nullptr)
            {
                Stack.Push(StandardCon::Pair<ContainerNode *, ContainerNode *>(AvlTreeData.MemberRoot->MemberRight, MemberRoot));
            }
            if (AvlTreeData.MemberRoot->MemberLeft != nullptr)
            {
                Stack.Push(StandardCon::Pair<ContainerNode *, ContainerNode *>(AvlTreeData.MemberRoot->MemberLeft, MemberRoot));
            }

            // 遍历并复制剩余节点
            while (!Stack.Empty())
            {
                auto [FirstNode, SecondNode] = Stack.Top();
                Stack.Pop();

                // 创建新节点并复制数据
                auto *NewStructureNode = new ContainerNode(FirstNode->MemberData);
                NewStructureNode->MemberBalanceFactor = FirstNode->MemberBalanceFactor;
                NewStructureNode->MemberHeight = FirstNode->MemberHeight;

                // 设置父节点关系（注意：SecondNode 是一级指针）
                NewStructureNode->MemberParent = SecondNode;

                // 判断源节点在原树中是左子还是右子
                bool IsleftChild = false;
                if (FirstNode->MemberParent != nullptr)
                {
                    IsleftChild = (FirstNode->MemberParent->MemberLeft == FirstNode);
                }

                // 将新节点链接到父节点的正确位置（注意：直接使用 SecondNode）
                if (IsleftChild)
                {
                    SecondNode->MemberLeft = NewStructureNode;
                }
                else
                {
                    SecondNode->MemberRight = NewStructureNode;
                }

                // 处理子节点（注意：压栈时父节点是 NewStructureNode，一级指针）
                if (FirstNode->MemberRight != nullptr)
                {
                    Stack.Push(StandardCon::Pair<ContainerNode *, ContainerNode *>(FirstNode->MemberRight, NewStructureNode));
                }
                if (FirstNode->MemberLeft != nullptr)
                {
                    Stack.Push(StandardCon::Pair<ContainerNode *, ContainerNode *>(FirstNode->MemberLeft, NewStructureNode));
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
         * @brief 移动另一个 AVL 树。
         * @param AvlTreeData 要移出的树；操作后为空。
         */
        BalanceTree(BalanceTree &&AvlTreeData)
                : MemberRoot(nullptr), FunctionPolicy(AvlTreeData.FunctionPolicy)
        {
            MemberRoot = std::move(AvlTreeData.MemberRoot);
            AvlTreeData.MemberRoot = nullptr;
        }
        /**
         * @brief 移动赋值。
         * @param AvlTreeData 要移出的树；操作后为空。
         * @return 当前对象的引用。
         */
        BalanceTree &operator=(BalanceTree &&AvlTreeData)
        {
            if (this != &AvlTreeData)
            {
                Clear();
                MemberRoot = std::move(AvlTreeData.MemberRoot);
                FunctionPolicy = std::move(AvlTreeData.FunctionPolicy);
                AvlTreeData.MemberRoot = nullptr;
            }
            return *this;
        }
        /**
         * @brief 赋值另一棵 AVL 树。
         * @param AvlTreeData 要复制或移动的树。
         * @return 当前对象的引用。
         */
        BalanceTree &operator=(BalanceTree AvlTreeData)
        {
            using std::swap;
            swap(MemberRoot, AvlTreeData.MemberRoot);
            swap(FunctionPolicy, AvlTreeData.FunctionPolicy);
            return *this;
        }
        /**
         * @brief 销毁 BalanceTree 对象。
         */
        ~BalanceTree() noexcept
        {
            Clear();
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
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size()
        {
            return InternalSize();
        }

        /**
         * @brief 按前序输出元素。
         */
        void PreOrderTraversal()
        {
            InteriorPreOrderTraversal(MemberRoot);
        }

        /**
         * @brief 按中序输出元素。
         */
        void MiddleOrderTraversal()
        {
            InteriorMiddleOrderTraversal(MemberRoot);
        }

        /**
         * @brief 插入键值对；等价键已存在时不修改树。
         * @param KeyData 要插入的键。
         * @param ValData 与键关联的值。
         * @return 插入成功时返回 true，否则返回 false。
         */
        bool Push(const AvlTreeTypeK &KeyData, const AvlTreeTypeV &ValData = AvlTreeTypeV())
        {
            return Insert(AvlTreeNodePair{KeyData, ValData});
        }
        /**
         * @brief 插入键值对；等价键已存在时不修改树。
         * @param PairTypeData 要插入的键值对。
         * @return 插入成功时返回 true，否则返回 false。
         */
        bool Push(const AvlTreeNodePair &PairTypeData)
        {
            return Insert(PairTypeData);
        }
        /**
         * @brief 按键查找节点。
         * @param KeyData 要查找的键。
         * @return 匹配节点的指针；未找到时返回空指针。
         */
        ContainerNode *Find(const AvlTreeTypeK &KeyData)
        {
            ContainerNode *ReferenceNode = MemberRoot;
            while (ReferenceNode != nullptr)
            {
                const auto &NodeKey = ReferenceNode->MemberData.First;
                if (!FunctionPolicy(NodeKey, KeyData)
                    && !FunctionPolicy(KeyData, NodeKey))
                {
                    return ReferenceNode;
                }
                else if (FunctionPolicy(ReferenceNode->MemberData.First, KeyData))
                {
                    ReferenceNode = ReferenceNode->MemberRight;
                }
                else
                {
                    ReferenceNode = ReferenceNode->MemberLeft;
                }
            }
            return ReferenceNode;
        }
        /**
         * @brief 按键删除节点。
         * @param KeyData 要删除的键。
         * @return 当前树的引用；未找到时保持不变。
         */
        BalanceTree &Pop(const AvlTreeTypeK &KeyData)
        {
            auto *Target = Find(KeyData);
            if (Target == nullptr)
            {
                return *this;
            }

            auto ReplaceNode = [this](ContainerNode *OldNode,
                                   ContainerNode *NewNode) noexcept
            {
                if (OldNode->MemberParent == nullptr)
                {
                    MemberRoot = NewNode;
                }
                else if (OldNode->MemberParent->MemberLeft == OldNode)
                {
                    OldNode->MemberParent->MemberLeft = NewNode;
                }
                else
                {
                    OldNode->MemberParent->MemberRight = NewNode;
                }
                if (NewNode != nullptr)
                {
                    NewNode->MemberParent = OldNode->MemberParent;
                }
            };

            ContainerNode *RebalanceStart = nullptr;
            if (Target->MemberLeft == nullptr)
            {
                RebalanceStart = Target->MemberParent != nullptr
                    ? Target->MemberParent : Target->MemberRight;
                ReplaceNode(Target, Target->MemberRight);
            }
            else if (Target->MemberRight == nullptr)
            {
                RebalanceStart = Target->MemberParent != nullptr
                    ? Target->MemberParent : Target->MemberLeft;
                ReplaceNode(Target, Target->MemberLeft);
            }
            else
            {
                auto *Successor = Target->MemberRight;
                while (Successor->MemberLeft != nullptr)
                {
                    Successor = Successor->MemberLeft;
                }

                if (Successor->MemberParent != Target)
                {
                    RebalanceStart = Successor->MemberParent;
                    ReplaceNode(Successor, Successor->MemberRight);
                    Successor->MemberRight = Target->MemberRight;
                    Successor->MemberRight->MemberParent = Successor;
                }
                else
                {
                    RebalanceStart = Successor;
                }

                ReplaceNode(Target, Successor);
                Successor->MemberLeft = Target->MemberLeft;
                Successor->MemberLeft->MemberParent = Successor;
                UpdateHeightAndBalance(Successor);
            }

            delete Target;
            RebalanceFrom(RebalanceStart);
            return *this;
        }
    };
}
namespace StandardCon
{
    using TreeContainer::BalanceTree;
    using TreeContainer::BinaryTree;
}
