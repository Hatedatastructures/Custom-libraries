/**
 * @file List.hpp
 * @brief 支持双向遍历的链表。
 */
#pragma once
#include "Exception.hpp"
#include "Algorithm.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>
namespace ListContainer
{
/**
     * @brief 带哨兵节点的双向链表。
     * @tparam ListType 元素类型。
     * @note 已知位置的插入和删除为 O(1)；插入不使其他迭代器失效，删除只使被删节点的迭代器失效。
     */
    template <typename ListType>
    class List
    {
        /** @brief 双向链表节点的链接基类；哨兵节点不保存元素。 */
        struct ListNodeBase
        {
            /** @brief 前一个节点。 */
            ListNodeBase *MemberPrev = nullptr;
            /** @brief 后一个节点。 */
            ListNodeBase *MemberNext = nullptr;
        };

        /** @brief 保存元素值的链表节点。 */
        template <typename ListTypeFunctionNode>
        struct ListContainerNode : ListNodeBase
        {
            /** @brief 节点保存的元素。 */
            ListTypeFunctionNode MemberData;

            /**
             * @brief 复制元素并创建节点。
             * @param ListTypeData 要复制的元素；省略时默认构造元素。
             */
            explicit ListContainerNode(const ListTypeFunctionNode &ListTypeData = ListTypeFunctionNode())
                    : MemberData(ListTypeData)
            {
                // 列表初始化
            }
            /**
             * @brief 移动元素并创建节点。
             * @param Data 要移动到节点中的元素。
             */
            explicit ListContainerNode(ListTypeFunctionNode &&Data)
                    : MemberData(std::move(Data))
            {
            }
        };
        /**
         * @brief 携带所属链表信息的双向迭代器。
         * @tparam ListNodeTypeIterator 元素类型。
         * @tparam Ref 解引用返回的引用类型。
         * @tparam Ptr 箭头运算符返回的指针类型。
         * @note 迭代器相等比较同时检查节点和所属链表。
         */
        template <typename ListNodeTypeIterator, typename Ref, typename Ptr>
        class ListIterator
        {
        public:
            /** @brief 链表节点基类。 */
            using ContainerNode = ListNodeBase;
            /** @brief 保存当前元素的节点类型。 */
            using ValueNode = ListContainerNode<ListNodeTypeIterator>;
            /** @brief 当前迭代器类型。 */
            using Iterator = ListIterator<ListNodeTypeIterator, ListNodeTypeIterator &, ListNodeTypeIterator *>;
            /** @brief 解引用返回的引用类型。 */
            using Reference = Ref;
            /** @brief 箭头运算符返回的指针类型。 */
            using Pointer = Ptr;
            /** @brief 当前节点；默认构造时为空。 */
            ContainerNode *MemberNode = nullptr;
            /** @brief 所属链表的哨兵节点。 */
            ContainerNode *MemberOwner = nullptr;

            /** @brief 创建未绑定的迭代器。 */
            ListIterator() noexcept = default;

            /**
             * @brief 创建指向指定节点的迭代器。
             * @param Node 当前节点；尾后位置使用所属链表的哨兵节点。
             * @param Owner 所属链表的哨兵节点。
             */
            ListIterator(ContainerNode *Node, ContainerNode *Owner) noexcept
                : MemberNode(Node), MemberOwner(Owner)
            {
            }
            /**
             * @brief 访问当前元素。
             * @return 当前元素的引用。
             * @pre 迭代器必须指向元素节点，而不是哨兵或空节点。
             */
            Ref operator*() noexcept
            {
                return static_cast<ValueNode*>(MemberNode)->MemberData;
            }
            /**
             * @brief 移动到下一个节点。
             * @return 当前迭代器的引用。
             */
            ListIterator &operator++() noexcept
            {
                if (MemberNode != nullptr)
                {
                    MemberNode = MemberNode->MemberNext;
                }
                return *this;
            }
            /**
             * @brief 后置递增。
             * @return 递增前的迭代器副本。
             */
            ListIterator operator++(int) noexcept
            {
                ListIterator ReturnSelf(*this);
                ++(*this);
                return ReturnSelf;
            }
            /**
             * @brief 移动到上一个节点。
             * @return 当前迭代器的引用。
             */
            ListIterator &operator--() noexcept
            {
                if (MemberNode != nullptr)
                {
                    MemberNode = MemberNode->MemberPrev;
                }
                return *this;
            }
            /**
             * @brief 后置递减。
             * @return 递减前的迭代器副本。
             */
            ListIterator operator--(int) noexcept
            {
                ListIterator ReturnSelf(*this);
                --(*this);
                return ReturnSelf;
            }
            /**
             * @brief 比较两个迭代器是否不同。
             * @param IteratorTemp 要比较的迭代器。
             * @return 节点或所属链表不同时返回 true。
             */
            bool operator!=(const ListIterator &IteratorTemp) noexcept
            {
                return MemberNode != IteratorTemp.MemberNode
                    || MemberOwner != IteratorTemp.MemberOwner;
            }
            /**
             * @brief 比较两个迭代器是否相同。
             * @param IteratorTemp 要比较的迭代器。
             * @return 节点和所属链表都相同时返回 true。
             */
            bool operator==(const ListIterator &IteratorTemp) noexcept
            {
                return MemberNode == IteratorTemp.MemberNode
                    && MemberOwner == IteratorTemp.MemberOwner;
            }
            /**
             * @brief 取得当前元素的指针。
             * @return 当前元素的指针。
             * @pre 迭代器必须指向元素节点，而不是哨兵或空节点。
             */
            Ptr operator->() noexcept
            {
                return &static_cast<ValueNode*>(MemberNode)->MemberData;
            }
        };
        /** @brief 将双向迭代器包装为反向遍历迭代器。 */
        template <typename Iterator>
        class ReverseListIterator
        {
            // 创建反向迭代器
            /** @brief 解引用返回的引用类型。 */
            using Ref = typename Iterator::Reference;
            /** @brief 箭头运算符返回的指针类型。 */
            using Ptr = typename Iterator::Pointer;
            /** @brief 当前底层迭代器对应的反向迭代器类型。 */
            using ConstReverseListIterator = ReverseListIterator<Iterator>;

        public:
            /** @brief 反向迭代器当前保存的正向迭代器。 */
            Iterator MemberIt;
            /**
             * @brief 从正向迭代器构造反向迭代器。
             * @param It 反向迭代器的基准位置。
             */
            ReverseListIterator(Iterator It) noexcept
                    : MemberIt(It)
            {
                ;
            }
            /**
             * @brief 访问当前反向元素。
             * @return 当前元素的引用。
             * @pre 反向迭代器必须指向元素，而不是反向尾后位置。
             */
            Ref &operator*() noexcept
            {
                // 因为反向迭代器起始位置在哨兵节点所以通过指向上一个来找到准确位置
                // 正好到rend位置停下来的时候已经遍历到rend位置
                Iterator ReturnSelf(MemberIt);
                --(ReturnSelf);
                return *ReturnSelf;
            }
            /**
             * @brief 取得当前反向元素的指针。
             * @return 当前元素的指针。
             * @pre 反向迭代器必须指向元素，而不是反向尾后位置。
             */
            Ptr operator->() noexcept
            {
                // 两者函数差不多可直接调用
                return &(operator*());
            }
            /**
             * @brief 反向前进一个元素。
             * @return 当前反向迭代器的引用。
             */
            ReverseListIterator &operator++() noexcept
            {
                --MemberIt;
                return *this;
            }
            /**
             * @brief 后置递增。
             * @return 递增前的迭代器副本。
             */
            ReverseListIterator operator++(int) noexcept
            {
                ReverseListIterator Temp(MemberIt);
                --MemberIt;
                return Temp;
            }
            /**
             * @brief 反向后退一个元素。
             * @return 当前反向迭代器的引用。
             */
            ReverseListIterator &operator--() noexcept
            {
                ++MemberIt;
                return *this;
            }
            /**
             * @brief 后置递减。
             * @return 递减前的迭代器副本。
             */
            ReverseListIterator operator--(int) noexcept
            {
                ReverseListIterator Temp(MemberIt);
                ++MemberIt;
                return Temp;
            }
            /**
             * @brief 比较两个反向迭代器是否不同。
             * @param Temp 要比较的迭代器。
             * @return 底层迭代器不同时返回 true。
             */
            bool operator!=(const ConstReverseListIterator &Temp) noexcept
            {
                return MemberIt != Temp.MemberIt;
            }
        };
        /** @brief 保存链表元素的节点类型。 */
        using ContainerNode = ListContainerNode<ListType>;

        /** @brief 循环双向链表的哨兵节点。 */
        ListNodeBase *MemberHead = nullptr;
        /** @brief 当前元素数量，不包含哨兵节点。 */
        std::uint64_t MemberSize = 0;
        //MemberHead为哨兵位
        /** @brief 创建并初始化空链表的循环哨兵。
         * @throws std::bad_alloc 哨兵节点分配失败时抛出。
         */
        void CreateHead()
        {
            try
            {
                MemberHead = new ListNodeBase;
                MemberHead->MemberPrev = MemberHead;
                MemberHead->MemberNext = MemberHead;
                MemberSize = 0;
            }
            catch (const std::bad_alloc &Process)
            {
                MemberHead = nullptr;
                std::cerr << Process.what() << std::endl;
                throw;
            }
        }

        /** @brief 构造失败时释放已创建的元素节点和哨兵。 */
        void CleanupFailedConstruction() noexcept
        {
            Clear();
            delete MemberHead;
            MemberHead = nullptr;
        }

    public:
        /** @brief 可修改的双向迭代器。 */
        using Iterator = ListIterator<ListType, ListType &, ListType *>;
        /** @brief 只读双向迭代器。 */
        using ConstIterator = ListIterator<ListType, const ListType &, const ListType *>;

        // 拿正向迭代器构造反向迭代器，可以直接调用 Iterator 已经重载的运算符和函数，相当于在封装一层类
        /** @brief 可修改的反向迭代器。 */
        using ReverseIterator = ReverseListIterator<Iterator>;
        /** @brief 只读反向迭代器。 */
        using ReverseConstIterator = ReverseListIterator<ConstIterator>;
        /**
         * @brief 构造 List 对象。
         */
        List() { CreateHead(); }
        /**
         * @brief 销毁 List 对象。
         */
        ~List() noexcept
        {
            if (MemberHead == nullptr)
            {
                return;
            }
            Clear();
            delete MemberHead;
            MemberHead = nullptr;
        }
        /**
         * @brief 复制迭代器范围中的元素，创建链表。
         * @param First 指向范围首元素的迭代器。
         * @param Last 指向范围尾后位置的迭代器。
         */
        List(Iterator First, Iterator Last)
        {
            if (First.MemberOwner == nullptr
                || First.MemberOwner != Last.MemberOwner)
            {
                throw std::invalid_argument(
                    "list range iterators must have the same owner");
            }
            CreateHead();
            try
            {
                while (First != Last)
                {
                    PushBack(*First);
                    ++First;
                }
            }
            catch (...)
            {
                CleanupFailedConstruction();
                throw;
            }
        }
        /**
         * @brief 从初始化列表复制元素，创建链表。
         * @param LightweightContainer 要复制的元素列表。
         */
        List(std::initializer_list<ListType> LightweightContainer)
        {
            CreateHead();
            try
            {
                for (const auto& Value : LightweightContainer)
                {
                    PushBack(Value);
                }
            }
            catch (...)
            {
                CleanupFailedConstruction();
                throw;
            }
        }
        /**
         * @brief 复制只读迭代器范围中的元素，创建链表。
         * @param First 指向范围首元素的只读迭代器。
         * @param Last 指向范围尾后位置的只读迭代器。
         */
        List(ConstIterator First, ConstIterator Last)
        {
            if (First.MemberOwner == nullptr
                || First.MemberOwner != Last.MemberOwner)
            {
                throw std::invalid_argument(
                    "list range iterators must have the same owner");
            }
            CreateHead();
            try
            {
                while (First != Last)
                {
                    PushBack(*First);
                    ++First;
                }
            }
            catch (...)
            {
                CleanupFailedConstruction();
                throw;
            }
        }
        /**
         * @brief 深拷贝另一个链表。
         * @param ListData 要复制的链表。
         */
        List(const List<ListType> &ListData)
        {
            CreateHead();
            try
            {
                for (auto It = ListData.Cbegin(); It != ListData.Cend(); ++It)
                {
                    PushBack(*It);
                }
            }
            catch (...)
            {
                CleanupFailedConstruction();
                throw;
            }
        }
        /**
         * @brief 移动另一个链表的节点。
         * @param ListData 要移出的链表；操作后为空。
         */
        List(List<ListType> &&ListData)
        {
            auto *ReplacementHead = new ListNodeBase;
            ReplacementHead->MemberPrev = ReplacementHead;
            ReplacementHead->MemberNext = ReplacementHead;
            MemberHead = ListData.MemberHead;
            MemberSize = ListData.MemberSize;
            ListData.MemberHead = ReplacementHead;
            ListData.MemberSize = 0;
        }
        /**
         * @brief 交换两个对象的内容。
         * @param SwapTarget 要交换内容的链表。
         */
        void Swap(ListContainer::List<ListType> &SwapTarget) noexcept
        {
            StandardCon::Algorithm::Swap(MemberHead, SwapTarget.MemberHead);
            StandardCon::Algorithm::Swap(MemberSize, SwapTarget.MemberSize);
        }
        /**
         * @brief 取得首元素迭代器。
         * @return 指向首元素的迭代器；空链表返回尾后迭代器。
         */
        [[nodiscard]] Iterator Begin() noexcept
        {
            return Iterator(MemberHead->MemberNext, MemberHead);
        }

        /**
         * @brief 取得尾后迭代器。
         * @return 尾后迭代器。
         */
        [[nodiscard]] Iterator End() noexcept
        {
            return Iterator(MemberHead, MemberHead);
        }

        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向首元素的只读迭代器；空链表返回尾后迭代器。
         */
        [[nodiscard]] ConstIterator Cbegin() const noexcept
        {
            return ConstIterator(MemberHead->MemberNext, MemberHead);
        }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 只读尾后迭代器。
         */
        [[nodiscard]] ConstIterator Cend() const noexcept
        {
            return ConstIterator(MemberHead, MemberHead);
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        [[nodiscard]] bool Empty() const noexcept
        {
            return MemberHead->MemberNext == MemberHead;
        }

        /**
         * @brief 获取链表中的元素数量。
         * @return 当前元素数，时间复杂度为 O(1)。
         */
        [[nodiscard]] auto Size() const noexcept -> std::uint64_t
        {
            return MemberSize;
        }

        /**
         * @brief 取得反向首元素迭代器。
         * @return 指向末元素的反向迭代器；空链表返回反向尾后迭代器。
         */
        [[nodiscard]] ReverseIterator Rbegin() noexcept
        {
            return ReverseIterator(End());
        }

        /**
         * @brief 取得反向尾后迭代器。
         * @return 反向尾后迭代器。
         */
        [[nodiscard]] ReverseIterator Rend() noexcept
        {
            return ReverseIterator(Begin());
        }

        /**
         * @brief 取得只读反向首元素迭代器。
         * @return 指向末元素的只读反向迭代器；空链表返回反向尾后迭代器。
         */
        [[nodiscard]] ReverseConstIterator Rcbegin() const noexcept
        {
            return ReverseConstIterator(Cend());
        }

        /**
         * @brief 取得只读反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        [[nodiscard]] ReverseConstIterator Rcend() const noexcept
        {
            return ReverseConstIterator(Cbegin());
        }

        /**
         * @brief 访问首元素。
         * @return 首元素的只读引用。
         * @pre 链表非空。
         */
        [[nodiscard]] const ListType& Front() const noexcept
        {
            return static_cast<ContainerNode*>(MemberHead->MemberNext)->MemberData;
        }

        /**
         * @brief 访问首元素。
         * @return 首元素的可写引用。
         * @pre 链表非空。
         */
        [[nodiscard]] ListType& Front() noexcept
        {
            return static_cast<ContainerNode*>(MemberHead->MemberNext)->MemberData;
        }

        /**
         * @brief 访问末元素。
         * @return 末元素的只读引用。
         * @pre 链表非空。
         */
        [[nodiscard]] const ListType& Back() const noexcept
        {
            return static_cast<ContainerNode*>(MemberHead->MemberPrev)->MemberData;
        }

        /**
         * @brief 访问末元素。
         * @return 末元素的可写引用。
         * @pre 链表非空。
         */
        [[nodiscard]] ListType& Back() noexcept
        {
            return static_cast<ContainerNode*>(MemberHead->MemberPrev)->MemberData;
        }

        /**
         * @brief 在末尾追加元素。
         * @param PushBackData 要添加的元素。
         */
        void PushBack(const ListType &PushBackData)
        {
            Insert(End(), PushBackData);
        }

        /**
         * @brief 在开头插入元素。
         * @param PushFrontData 要复制到链表开头的元素。
         */
        void PushFront(const ListType &PushFrontData)
        {
            Insert(Begin(), PushFrontData);
        }

        /**
         * @brief 在末尾追加元素。
         * @param PushBackData 要移动到链表末尾的元素。
         */
        void PushBack(ListType &&PushBackData)
        {
            Insert(End(), std::forward<ListType>(PushBackData));
        }

        /**
         * @brief 在开头插入元素。
         * @param PushFrontData 要移动到链表开头的元素。
         */
        void PushFront(ListType &&PushFrontData)
        {
            Insert(Begin(), std::forward<ListType>(PushFrontData));
        }

        /**
         * @brief 删除末尾元素。
         */
        void PopBack()
        {
            if (!Empty())
            {
                Erase(--End());
            }
        }

        /**
         * @brief 删除首元素。
         * @return 被删位置之后的迭代器。
         */
        Iterator PopFront()
        {
            if (Empty())
            {
                return End();
            }

            ListNodeBase *DeleteNode = MemberHead->MemberNext;
            auto *NextNode = DeleteNode->MemberNext;
            MemberHead->MemberNext = NextNode;
            NextNode->MemberPrev = MemberHead;
            --MemberSize;
            delete static_cast<ContainerNode*>(DeleteNode);
            return Iterator(NextNode, MemberHead);
        }

        /**
         * @brief 在指定位置插入元素。
         * @param IteratorPosition 属于本链表的插入或删除位置。
         * @param ListTypeData 要插入的元素。
         * @return 指向新元素的迭代器。
         * @throws std::invalid_argument 位置不属于本链表或未绑定时抛出。
         */
        Iterator Insert(Iterator IteratorPosition, const ListType &ListTypeData)
        {
            try
            {
                if (IteratorPosition.MemberNode == nullptr
                    || IteratorPosition.MemberOwner != MemberHead)
                {
                    throw std::invalid_argument(
                        "list insertion iterator does not belong to this list");
                }
                auto *NewContainerNode = new ContainerNode(ListTypeData);
                ListNodeBase *IteratorCurrentNode =
                    IteratorPosition.MemberNode;
                NewContainerNode->MemberPrev = IteratorCurrentNode->MemberPrev;
                NewContainerNode->MemberNext = IteratorCurrentNode;
                NewContainerNode->MemberPrev->MemberNext = NewContainerNode;
                IteratorCurrentNode->MemberPrev = NewContainerNode;
                ++MemberSize;
                return Iterator(NewContainerNode, MemberHead);
            }
            catch (const std::bad_alloc &Process)
            {
                std::cerr << Process.what() << std::endl;
                throw;
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
        }
        /**
         * @brief 在指定位置插入元素。
         * @param IteratorPosition 属于本链表的插入或删除位置。
         * @param ListTypeData 要移动插入的元素。
         * @return 指向新元素的迭代器。
         * @throws std::invalid_argument 位置不属于本链表或未绑定时抛出。
         */
        Iterator Insert(Iterator IteratorPosition, ListType &&ListTypeData)
        {
            try
            {
                if (IteratorPosition.MemberNode == nullptr
                    || IteratorPosition.MemberOwner != MemberHead)
                {
                    throw std::invalid_argument(
                        "list insertion iterator does not belong to this list");
                }
                auto *NewContainerNode = new ContainerNode(std::forward<ListType>(ListTypeData));
                ListNodeBase *IteratorCurrentNode = IteratorPosition.MemberNode;
                NewContainerNode->MemberPrev = IteratorCurrentNode->MemberPrev;
                NewContainerNode->MemberNext = IteratorCurrentNode;
                NewContainerNode->MemberPrev->MemberNext = NewContainerNode;
                IteratorCurrentNode->MemberPrev = NewContainerNode;
                ++MemberSize;
                return Iterator(NewContainerNode, MemberHead);
            }
            catch (const std::bad_alloc &Process)
            {
                std::cerr << Process.what() << "插入时内存不足" << std::endl;
                throw;
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
        }
        /**
         * @brief 删除指定位置的元素。
         * @param IteratorPosition 属于本链表的插入或删除位置。
         * @return 被删位置之后的迭代器。
         * @throws std::invalid_argument 位置属于其他链表时抛出。
         */
        Iterator Erase(Iterator IteratorPosition)
        {
            try
            {
                if (IteratorPosition.MemberOwner != MemberHead
                    || IteratorPosition.MemberNode == nullptr)
                {
                    throw std::invalid_argument(
                        "list erase iterator does not belong to this list");
                }
                if (IteratorPosition.MemberNode == MemberHead)
                {
                    return End();
                }
                auto *IteratorDeleteNode = static_cast<ContainerNode*>(
                    IteratorPosition.MemberNode);
                ListNodeBase *NextElementNode = IteratorDeleteNode->MemberNext;

                IteratorDeleteNode->MemberPrev->MemberNext = IteratorDeleteNode->MemberNext; // 将该节点从链表中拆下来并删除
                IteratorDeleteNode->MemberNext->MemberPrev = IteratorDeleteNode->MemberPrev;
                delete IteratorDeleteNode;
                --MemberSize;
                return Iterator(NextElementNode, MemberHead);
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
        }
        /**
         * @brief 调整元素数量；新增位置用指定值填充。
         * @param NewContainerSize 调整后的元素数量。
         * @param ListTypeData 新增元素的填充值。
         */
        void Resize(const uint64_t NewContainerSize, const ListType &ListTypeData = ListType())
        {
            if (uint64_t ContainerSize = Size(); NewContainerSize <= ContainerSize)
            {
                // 有效元素个数减少到NewContainerSize
                while (NewContainerSize < ContainerSize)
                {
                    PopBack();
                    --ContainerSize;
                }
            }
            else
            {
                while (ContainerSize < NewContainerSize)
                {
                    PushBack(ListTypeData);
                    ++ContainerSize;
                }
            }
        }
        /**
         * @brief 清空容器。
         */
        void Clear() noexcept
        {
            if (MemberHead == nullptr)
            {
                return;
            }
            // 循环释放资源
            ListNodeBase *CurrentNode = MemberHead->MemberNext;
            // 采用头删除
            while (CurrentNode != MemberHead)
            {
                MemberHead->MemberNext = CurrentNode->MemberNext;
                delete static_cast<ContainerNode*>(CurrentNode);
                CurrentNode = MemberHead->MemberNext;
            }
            MemberHead->MemberNext = MemberHead->MemberPrev = MemberHead;
            MemberSize = 0;
        }
        /**
         * @brief 深拷贝赋值。
         * @param ListData 要复制的链表。
         * @return 当前对象的引用。
         */
        List &operator=(const List<ListType> &ListData)
        {
            // 拷贝赋值
            if (this != &ListData)
            {
                List<ListType> CopyListObject(ListData);
                Swap(CopyListObject);
            }
            return *this;
        }
        /**
         * @brief 用初始化列表替换链表内容。
         * @param LightweightContainer 要复制的元素列表。
         * @return 当前对象的引用。
         */
        List &operator=(std::initializer_list<ListType> LightweightContainer)
        {
            Clear();
            for (auto &ChainedValues : LightweightContainer)
            {
                PushBack(std::move(ChainedValues));
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param ListData 要移出的链表；操作后为空。
         * @return 当前对象的引用。
         */
        List &operator=(List<ListType> &&ListData)
        {
            if (this != &ListData)
            {
                auto *ReplacementHead = new ListNodeBase;
                ReplacementHead->MemberPrev = ReplacementHead;
                ReplacementHead->MemberNext = ReplacementHead;

                Clear();
                delete MemberHead;
                MemberHead = ListData.MemberHead;
                MemberSize = ListData.MemberSize;
                ListData.MemberHead = ReplacementHead;
                ListData.MemberSize = 0;
            }
            return *this;
        }
        /**
         * @brief 创建由当前链表和另一链表拼接而成的新链表。
         * @param ListData 要追加到当前链表副本末尾的链表。
         * @return 拼接后的新对象。
         */
        List operator+(const List<ListType> &ListData)
        {
            List<ListType> ReturnListObject(Cbegin(), Cend());
            ConstIterator StartPositionIterator = ListData.Cbegin();
            ConstIterator EndPositionIterator = ListData.Cend();
            while (StartPositionIterator != EndPositionIterator)
            {
                ReturnListObject.PushBack(*StartPositionIterator);
                ++StartPositionIterator;
            }
            return ReturnListObject;
        }
        /**
         * @brief 将另一链表中的元素依次追加到末尾。
         * @param ListData 要追加的链表。
         * @return 当前链表的引用。
         */
        List &operator+=(const List<ListType> &ListData)
        {
            if (this == &ListData)
            {
                List<ListType> Copy(ListData);
                return operator+=(Copy);
            }

            ConstIterator StartPositionIterator = ListData.Cbegin();
            ConstIterator EndPositionIterator = ListData.Cend();
            while (StartPositionIterator != EndPositionIterator)
            {
                PushBack(*StartPositionIterator);
                ++StartPositionIterator;
            }
            return *this;
        }
        /**
         * @brief 向流写入对象。
         * @tparam ConstListOutputTemplates 输出链表中元素的类型。
         * @param ListOstream 输出流。
         * @param DynamicArraysData 要写入流的链表。
         * @return 输入或输出流的引用。
         */
        template <typename ConstListOutputTemplates>
        friend std::ostream &operator<<(std::ostream &ListOstream, const List<ConstListOutputTemplates> &DynamicArraysData);
    };
    template <typename ConstListOutputTemplates>
    std::ostream &operator<<(std::ostream &ListOstream, const List<ConstListOutputTemplates> &DynamicArraysData)
    {
        typename List<ConstListOutputTemplates>::ConstIterator It = DynamicArraysData.Cbegin();
        while (It != DynamicArraysData.Cend())
        {
            ListOstream << *It << " ";
            ++It;
        }
        return ListOstream;
    }
}
namespace StandardCon
{
    using ListContainer::List;
}
