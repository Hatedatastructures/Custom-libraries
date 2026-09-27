/**
 * @file Stack.hpp
 * @brief 栈容器适配器。
 */
#pragma once
#include "Vector.hpp"
#include "Algorithm.hpp"
#include "Exception.hpp"

#include <stdexcept>
namespace StackAdapter
{
    /**
     * @brief 后进先出的容器适配器。
     * @tparam StackType 元素类型。
     * @tparam VectorBasedStack 底层序列容器类型。
     */
    template <typename StackType, typename VectorBasedStack = StandardCon::Vector<StackType>>
    class Stack
    {
    private:
        /** @brief 按后进先出顺序保存栈元素的底层向量。 */
        VectorBasedStack VectorObject;

    public:
        /**
         * @brief 销毁 Stack 对象。
         */
        ~Stack() { ; }

        /**
         * @brief 插入元素。
         * @param StackTypeData 要移动到栈顶的元素。
         */
        void Push(StackType &&StackTypeData)
        {
            VectorObject.PushBack(std::forward<StackType>(StackTypeData));
        }

        /**
         * @brief 插入元素。
         * @param StackTypeData 要复制到栈顶的元素。
         */
        void Push(const StackType &StackTypeData)
        {
            VectorObject.PushBack(StackTypeData);
        }

        /**
         * @brief 移除栈顶元素；栈为空时不执行操作。
         */
        void Pop()
        {
            VectorObject.PopBack();
        }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        uint64_t Size() const noexcept
        {
            return VectorObject.Size();
        }

        /**
         * @brief 访问栈顶元素。
         * @return 栈顶元素的引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] StackType &Top()
        {
            if (Empty())
            {
                throw std::out_of_range("Stack::Top called on an empty stack");
            }
            return VectorObject.Back();
        }

        /**
         * @brief 访问栈顶元素。
         * @return 栈顶元素的只读引用。
         * @throws std::out_of_range 容器为空时抛出。
         */
        [[nodiscard]] const StackType &Top() const
        {
            if (Empty())
            {
                throw std::out_of_range("Stack::Top called on an empty stack");
            }
            return VectorObject.Back();
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        [[nodiscard]] bool Empty() const noexcept
        {
            return VectorObject.Empty();
        }

        /**
         * @brief 复制另一个栈。
         * @param StackData 要复制的栈。
         */
        explicit Stack(const Stack<StackType> &StackData)
        {
            VectorObject = StackData.VectorObject;
        }

        /**
         * @brief 访问栈顶元素。
         * @return 栈顶元素的引用；语义与 `Top()` 相同。
         * @throws std::out_of_range 容器为空时抛出。
         */
        StackType &Footer()
        {
            return Top();
        }

        /**
         * @brief 移动另一个栈。
         * @param StackData 要移出的栈。
         */
        explicit Stack(Stack<StackType> &&StackData) noexcept
        {
            VectorObject = std::move(StackData.VectorObject); // std::move将对象转换为右值引用
        }
        /**
         * @brief 从初始化列表构造栈。
         * @param StackTypeData 按列表顺序压入栈的元素。
         */
        Stack(std::initializer_list<StackType> StackTypeData)
        {
            for (auto &ChainedValues : StackTypeData)
            {
                VectorObject.PushBack(ChainedValues);
            }
        }
        /**
         * @brief 用一个元素构造栈。
         * @param StackTypeData 要复制到栈顶的元素。
         */
        explicit Stack(const StackType &StackTypeData)
        {
            VectorObject.PushBack(StackTypeData);
        }
        /**
         * @brief 复制赋值。
         * @param StackData 要复制的栈。
         * @return 当前对象的引用。
         */
        Stack &operator=(const Stack<StackType> &StackData)
        {
            if (this != &StackData)
            {
                VectorObject = StackData.VectorObject;
            }
            return *this;
        }
        /**
         * @brief 移动赋值。
         * @param StackData 要移出的栈。
         * @return 当前对象的引用。
         */
        Stack &operator=(Stack<StackType> &&StackData) noexcept
        {
            if (this != &StackData)
            {
                VectorObject = std::move(StackData.VectorObject);
            }
            return *this;
        }
        /**
         * @brief 构造 Stack 对象。
         */
        Stack() = default;
    };
}
namespace StandardCon
{
    using StackAdapter::Stack;
}
