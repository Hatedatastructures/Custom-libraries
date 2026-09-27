/**
 * @file Queue.hpp
 * @brief 队列和优先队列适配器。
 */
#pragma once
#include "List.hpp"
#include "Vector.hpp"
#include "Algorithm.hpp"
#include "Exception.hpp"
#include "Imitate.hpp"

#include <stdexcept>
namespace QueueAdapter
{
    /**
     * @brief 先进先出的容器适配器。
     * @tparam QueueType 元素类型。
     * @tparam ListBasedQueue 底层序列容器类型。
     */
    template <typename QueueType, typename ListBasedQueue = StandardCon::List<QueueType>>
    class Queue
    {
        /** @brief 保存队列元素的底层序列容器。 */
        ListBasedQueue ListObject;

    public:
        /**
         * @brief 销毁 Queue 对象。
         */
        ~Queue() { ; }

        /**
         * @brief 插入元素。
         * @param QueueTypeData 要移动到队尾的元素。
         */
        void Push(QueueType &&QueueTypeData)
        {
            ListObject.PushBack(std::forward<QueueType>(QueueTypeData));
        }

        /**
         * @brief 插入元素。
         * @param QueueTypeData 要复制到队尾的元素。
         */
        void Push(const QueueType &QueueTypeData)
        {
            ListObject.PushBack(QueueTypeData);
        }

        /**
         * @brief 移除队首元素；队列为空时不执行操作。
         */
        void Pop()
        {
            ListObject.PopFront();
        }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const noexcept
        {
            return ListObject.Size();
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        [[nodiscard]] bool Empty() const noexcept
        {
            return ListObject.Empty();
        }

        /**
         * @brief 访问首元素。
         * @return 队首元素的可写引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] QueueType &Front()
        {
            if (Empty())
            {
                throw std::out_of_range("Queue::Front called on an empty queue");
            }
            return ListObject.Front();
        }

        /**
         * @brief 访问首元素。
         * @return 队首元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] const QueueType &Front() const
        {
            if (Empty())
            {
                throw std::out_of_range("Queue::Front called on an empty queue");
            }
            return ListObject.Front();
        }

        /**
         * @brief 访问末元素。
         * @return 队尾元素的可写引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] QueueType &Back()
        {
            if (Empty())
            {
                throw std::out_of_range("Queue::Back called on an empty queue");
            }
            return ListObject.Back();
        }

        /**
         * @brief 访问末元素。
         * @return 队尾元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] const QueueType &Back() const
        {
            if (Empty())
            {
                throw std::out_of_range("Queue::Back called on an empty queue");
            }
            return ListObject.Back();
        }

        /**
         * @brief 复制另一个队列。
         * @param QueueData 要复制的队列。
         */
        explicit Queue(const Queue<QueueType> &QueueData)
        {
            ListObject = QueueData.ListObject;
        }

        /**
         * @brief 移动另一个队列。
         * @param QueueTypeData 要移出的队列。
         */
        Queue(Queue<QueueType> &&QueueTypeData)
        {
            // 移动构造
            ListObject = std::forward<ListBasedQueue>(QueueTypeData.ListObject);
        }
        /**
         * @brief 从初始化列表构造队列。
         * @param QueueTypeData 按列表顺序加入队列的元素。
         */
        Queue(std::initializer_list<QueueType> QueueTypeData)
        {
            // 链式构造
            for (auto &ChainedValues : QueueTypeData)
            {
                ListObject.PushBack(std::move(ChainedValues));
            }
        }
        /**
         * @brief 用一个元素构造队列。
         * @param QueueTypeData 要复制到队尾的元素。
         */
        explicit Queue(const QueueType &QueueTypeData)
        {
            ListObject.PushBack(QueueTypeData);
        }
        /**
         * @brief 构造 Queue 对象。
         */
        Queue() = default;
        /**
         * @brief 复制赋值。
         * @param QueueData 要复制的队列。
         * @return 当前对象的引用。
         */
        Queue &operator=(const Queue<QueueType> &QueueData)
        {
            if (this != &QueueData)
            {
                ListObject = QueueData.ListObject;
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param QueueData 要移出的队列。
         * @return 当前对象的引用。
         */
        Queue &operator=(Queue<QueueType> &&QueueData)
        {
            if (this != &QueueData)
            {
                ListObject = std::forward<ListBasedQueue>(QueueData.ListObject);
            }
            return *this;
        }
    };
    /**
     * @brief 按比较器维护堆顶的优先队列。
     * @tparam PriorityQueueType 元素类型。
     * @tparam ContainerImitateFunction 元素的比较器类型。
     * @tparam VectorBasedPriorityQueue 底层序列容器类型。
     */
    template <typename PriorityQueueType, typename ContainerImitateFunction = StandardCon::Less<PriorityQueueType>,
                        typename VectorBasedPriorityQueue = StandardCon::Vector<PriorityQueueType>>
    class PriorityQueue
    {
        // 创建容器对象
        /** @brief 按堆布局保存元素的底层向量。 */
        VectorBasedPriorityQueue VectorContainerObject;
        /** @brief 决定堆顶优先级的比较器。 */
        ContainerImitateFunction FunctionPolicy; // 仿函数：数据类型比较器，可自定义
        // 仿函数对象

        /**
         * @brief 从新插入的子节点向上恢复堆序。
         * @param AdjustUpwardsChild 新节点在底层向量中的索引。
         */
        void PriorityQueueAdjustUpwards(int AdjustUpwardsChild)
        {
            // 向上调整算法
            int AdjustUpwardsParent = (AdjustUpwardsChild - 1) / 2;
            while (AdjustUpwardsChild > 0)
            {
                if (FunctionPolicy(VectorContainerObject[AdjustUpwardsParent], VectorContainerObject[AdjustUpwardsChild]))
                {
                    StandardCon::Algorithm::Swap(VectorContainerObject[AdjustUpwardsParent], VectorContainerObject[AdjustUpwardsChild]);
                    AdjustUpwardsChild = AdjustUpwardsParent;
                    AdjustUpwardsParent = (AdjustUpwardsChild - 1) / 2;
                }
                else
                {
                    break;
                }
            }
        }
        /**
         * @brief 从指定父节点向下恢复堆序。
         * @param AdjustDownwardsParent 开始调整的父节点索引；默认从堆根开始。
         */
        void PriorityQueueAdjustDownwards(int AdjustDownwardsParent = 0)
        {
            int AdjustDownwardsChild = (AdjustDownwardsParent * 2) + 1;
            while (AdjustDownwardsChild < static_cast<int>(VectorContainerObject.Size()))
            {
                int AdjustDownwardsLeft = AdjustDownwardsChild;
                int AdjustDownwardsRight = AdjustDownwardsLeft + 1;
                if (AdjustDownwardsRight < static_cast<int>(VectorContainerObject.Size()) &&
                        FunctionPolicy(VectorContainerObject[AdjustDownwardsLeft], VectorContainerObject[AdjustDownwardsRight]))
                {
                    // 大堆找出左右节点哪个孩子大
                    AdjustDownwardsChild = AdjustDownwardsRight;
                }
                if (FunctionPolicy(VectorContainerObject[AdjustDownwardsParent], VectorContainerObject[AdjustDownwardsChild]))
                {
                    // 建大堆把小的换下去，建小堆把大的换下去
                    StandardCon::Algorithm::Swap(VectorContainerObject[AdjustDownwardsParent], VectorContainerObject[AdjustDownwardsChild]);

                    // 换完之后如果是大堆，则父亲节点是较大的值，需要更新孩子节点继续向下找比孩子节点大的值，如果有继续交换
                    AdjustDownwardsParent = AdjustDownwardsChild;
                    AdjustDownwardsChild = (AdjustDownwardsParent * 2) + 1;
                }
                else
                {
                    break;
                }
            }
        }

    public:
        /**
         * @brief 销毁 PriorityQueue 对象。
         */
        ~PriorityQueue() noexcept = default;
        /**
         * @brief 插入元素。
         * @param PrioityQueueTypeData 要插入堆中的元素。
         */
        void Push(const PriorityQueueType &PrioityQueueTypeData)
        {
            VectorContainerObject.PushBack(PrioityQueueTypeData);
            PriorityQueueAdjustUpwards(static_cast<int>(VectorContainerObject.Size() - 1));
        }
        /**
         * @brief 访问堆顶元素。
         * @return 堆顶元素的引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        PriorityQueueType &Top()
        {
            if (Empty())
            {
                throw std::out_of_range("PriorityQueue::Top called on an empty queue");
            }
            return VectorContainerObject.Front();
        }
        /**
         * @brief 访问堆顶元素。
         * @return 堆顶元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        const PriorityQueueType &Top() const
        {
            if (Empty())
            {
                throw std::out_of_range("PriorityQueue::Top called on an empty queue");
            }
            return VectorContainerObject.Front();
        }
        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        bool Empty() const noexcept
        {
            return VectorContainerObject.Empty();
        }
        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        uint64_t Size() const noexcept
        {
            return VectorContainerObject.Size();
        }
        /**
         * @brief 移除堆顶元素；队列为空时不执行操作。
         */
        void Pop()
        {
            if (Empty())
            {
                return;
            }
            StandardCon::Algorithm::Swap(VectorContainerObject[0], VectorContainerObject[VectorContainerObject.Size() - static_cast<uint64_t>(1)]);
            VectorContainerObject.PopBack();
            PriorityQueueAdjustDownwards();
        }
        /**
         * @brief 构造 PriorityQueue 对象。
         */
        PriorityQueue()
        {
            ;
        }
        /**
         * @brief 从初始化列表构造优先队列。
         * @param LightweightContainer 要插入的元素列表。
         */
        PriorityQueue(std::initializer_list<PriorityQueueType> LightweightContainer)
        {
            // 通过初始化列表构建一个list
            for (auto &ChainedValues : LightweightContainer)
            {
                Push(std::move(ChainedValues));
            }
        }
        /**
         * @brief 复制另一个优先队列。
         * @param PriorityQueueData 要复制的优先队列。
         */
        PriorityQueue(const PriorityQueue &PriorityQueueData)
        {
            // 拷贝构造
            VectorContainerObject = PriorityQueueData.VectorContainerObject;
        }
        /**
         * @brief 移动另一个优先队列。
         * @param PriorityQueueData 要移出的优先队列。
         */
        PriorityQueue(PriorityQueue &&PriorityQueueData)
                : FunctionPolicy(PriorityQueueData.FunctionPolicy)
        {
            // 移动构造
            VectorContainerObject = std::move(PriorityQueueData.VectorContainerObject);
        }
        /**
         * @brief 用一个元素构造优先队列。
         * @param PriorityQueueTypeData 要插入堆中的元素。
         */
        explicit PriorityQueue(const PriorityQueueType &PriorityQueueTypeData)
        {
            VectorContainerObject.PushBack(PriorityQueueTypeData);
            PriorityQueueAdjustUpwards((static_cast<int>(VectorContainerObject.Size() - 1)));
        }
        /**
         * @brief 移动赋值。
         * @param PriorityQueueData 要移出的优先队列。
         * @return 当前对象的引用。
         */
        PriorityQueue &operator=(PriorityQueue &&PriorityQueueData)
        {
            // 移动赋值
            if (this != &PriorityQueueData)
            {
                VectorContainerObject = std::move(PriorityQueueData.VectorContainerObject);
                FunctionPolicy = PriorityQueueData.FunctionPolicy;
            }
            return *this;
        }
        /**
         * @brief 复制赋值。
         * @param PriorityQueueData 要复制的优先队列。
         * @return 当前对象的引用。
         */
        PriorityQueue &operator=(const PriorityQueue &PriorityQueueData)
        {
            // 拷贝赋值
            if (this != &PriorityQueueData)
            {
                VectorContainerObject = PriorityQueueData.VectorContainerObject;
                FunctionPolicy = PriorityQueueData.FunctionPolicy;
            }
            return *this;
        }
    };
}
namespace StandardCon
{
    using QueueAdapter::PriorityQueue;
    using QueueAdapter::Queue;
}
