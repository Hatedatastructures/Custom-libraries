/**
  * @file rank.hpp
  * @brief 任务队列定义
  * @details 提供任务队列的定义与操作，包括任务的添加、移除、执行等功能
  */
#pragma once
#include "unit.hpp"
#include "integration.hpp"
#include <set>
#include <queue>
#include <deque>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include <typeinfo>
#include <shared_mutex>
#include <unordered_map>

#define parameter_discard(parameter) (void)(parameter)
#define macro_statement throw operation_exception("The current derived class has not overridden the function.")

namespace internals
{
    namespace structure_r {}
}
namespace internals::structure_r
{
    using namespace internals::structure_u;
    using safety_unit_pointer = std::shared_ptr<unit_ordinary>;

    using internals_clk    = std::chrono::system_clock;
    using internals_time_t = std::chrono::system_clock::time_point;
    using internals_time   = std::shared_ptr<internals_time_t>;

    /**
      * @brief 任务队列基类
      * @details 任务队列基类，定义了任务队列的基本接口，以及任务队列的基本属性。
      * @warning 该类需重载内部函数版本来消除运行时异常
      */
    class rank_ordinary
    {
    protected:

    // 计算执行单元默认超时时间点
    internals_time internal_calculation_deadline()
    {
        if(!unit_time_limit_)
        {
            return nullptr;
        }
        internals_time_t now_time = std::chrono::system_clock::now() + default_function_timeout_;
        return std::make_shared<internals_time_t>(now_time);
    }

    protected:

        std::atomic<bool> closed_{false}; //关闭标识
        std::atomic<bool> unit_time_limit_{false}; //执行单元时间限制
        std::atomic<std::size_t> max_storage_capacity_{0}; //最大队列大小
        std::chrono::milliseconds default_function_timeout_{1000}; //默认延时时间 

    protected:
        // 内部推送任务接口
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
        internals_time deadline  = nullptr)
        {
            parameter_discard(pointer);  parameter_discard(mode);
            parameter_discard(deadline); macro_statement;
            return false;
        }
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode)
        {
            parameter_discard(pointer); parameter_discard(mode); macro_statement;
            return false;
        }
        // 内部批量推送任务接口
        virtual std::size_t internal_push_batch(std::vector<safety_unit_pointer>&& pointers,
              backpressure mode)
        {
            parameter_discard(pointers); parameter_discard(mode); macro_statement;
            return std::size_t(0);
        }
        // 内部弹出任务接口
        virtual safety_unit_pointer internal_pop()
        {
            macro_statement;
            return nullptr;
        }
        // 内部批量弹出任务接口
        virtual std::vector<safety_unit_pointer> internal_pop_batch(const std::size_t count)
        {
            parameter_discard(count); macro_statement;
            return {};
        }
        // 内部尝试弹出任务接口
        virtual safety_unit_pointer internal_try_pop()
        {
            macro_statement;
            return nullptr;
        }
        // 内部尝试弹出任务接口（带超时）
        virtual safety_unit_pointer internal_try_pop_for(const std::chrono::milliseconds& timeout)
        {
            parameter_discard(timeout); macro_statement;
            return nullptr;
        }
        // 内部获取队列大小接口
        virtual std::size_t internal_size() const
        {
            macro_statement;
            return 0;
        }
        // 内部判断队列是否为空接口
        virtual bool internal_empty() const
        {
            macro_statement;
            return true;
        }
        // 内部清空队列接口
        virtual void internal_clear()
        {
            macro_statement;
            return;
        }
        // 内部关闭接口
        virtual void internal_close()
        {
            macro_statement;
            return;
        }
        // 内部获取子队列数量接口
        virtual std::size_t internal_get_sub_queue_count() const
        {
            macro_statement;
            return 0;
        }
        // 内部获取延迟执行单元数量接口
        virtual std::size_t internal_get_delay_uint_count() const
        {
            macro_statement;
            return 0;
        }
        // 内部获取调度策略接口
        virtual rank_strategy internal_strategy() const
        {
            macro_statement;
            return rank_strategy::fifo;
        }
    public:
        rank_ordinary(const std::size_t size) :max_storage_capacity_(size) {} 

        virtual ~rank_ordinary() = default;

        rank_strategy strategy() const 
        { 
            return internal_strategy(); 
        }

        bool push(safety_unit_pointer pointer, backpressure mode = backpressure::block) 
        {
            if(strategy() == rank_strategy::delay) 
            {
                return internal_push(std::move(pointer), mode, internal_calculation_deadline());
            }
            return internal_push(std::move(pointer), mode,nullptr);
        }

        bool push(safety_unit_pointer pointer, std::chrono::system_clock::time_point deadline,
        backpressure mode = backpressure::block)
        {
            internals_time time_point = std::make_shared<std::chrono::system_clock::time_point>(deadline);
            return internal_push(std::move(pointer), mode, time_point);
        }

        std::size_t push_batch(std::vector<safety_unit_pointer> pointers, backpressure mode = backpressure::block)
        {
            return internal_push_batch(std::move(pointers), mode);
        }

        safety_unit_pointer pop()
        {
            return internal_pop();
        }

        std::vector<safety_unit_pointer> pop_batch(const std::size_t count)
        {
            return internal_pop_batch(count);
        }

        safety_unit_pointer try_pop()
        {
            return internal_try_pop();
        }
        template<typename rep, typename period>
        safety_unit_pointer try_pop_for(const std::chrono::duration<rep, period>& timeout)
        {
            return internal_try_pop_for(convert_time::to_milliseconds(timeout));
        }

        std::size_t size() const
        {
            return internal_size();
        }

        bool empty() const 
        { 
            return internal_empty(); 
        }

        void clear() 
        {
            internal_clear();
        }
        // 关闭提交，拒绝新任务提交
        virtual void close() 
        {
            internal_close();
        }
        
        bool closed() const 
        { 
            return closed_.load(std::memory_order_acquire); 
        }
        bool set_max_size(const std::size_t max_size)
        {
            max_storage_capacity_.store(max_size, std::memory_order_relaxed);
            return true;
        }
        std::size_t get_max_size()const  
        {
            return max_storage_capacity_.load();
        }

        std::size_t get_sub_queue_count()  const 
        { 
            return internal_get_sub_queue_count(); 
        }

        std::size_t get_delay_uint_count() const 
        { 
            return internal_get_delay_uint_count(); 
        }

    };
    /**
      * @brief 标准任务队列
      * @details 线程安全的标准任务队列，支持阻塞、覆盖、异常三种背压策略
      * @note 底层容器为`std::deque`
      */
    class rank_standard : public rank_ordinary
    {
    protected:

        std::deque<safety_unit_pointer> rank_unit_standard_;

        std::condition_variable_any judge_full_cv_;
        std::condition_variable_any judge_empty_cv_;

        mutable std::shared_mutex rank_standard_mutex_;

    public:
        explicit rank_standard(std::size_t max_size = 0) : rank_ordinary(max_size) {}

        virtual ~rank_standard() = default;

    private:
        bool enqueue_with_backpressure(safety_unit_pointer pointer, backpressure mode)
        {
            std::size_t current_size = 0;
            
            std::unique_lock<std::shared_mutex> lock(rank_standard_mutex_);
            current_size = rank_unit_standard_.size();
            
            if((max_storage_capacity_ != 0 && current_size >= max_storage_capacity_) == false)
            {
                rank_unit_standard_.push_back(std::move(pointer));
                lock.unlock();
                judge_empty_cv_.notify_one();
                return true;
            }
            switch(mode)
            {
                case backpressure::block:
                {
                    auto block_func = [this]()
                    {
                        return this->rank_unit_standard_.size() < this->max_storage_capacity_
                        || this->closed_.load(std::memory_order_acquire);
                    };
                    judge_full_cv_.wait(lock, block_func);
                    if(closed_.load(std::memory_order_acquire)) return false;
                    rank_unit_standard_.push_back(std::move(pointer));
                    lock.unlock();
                    judge_empty_cv_.notify_one();
                    return true;
                }
                case backpressure::overwrite:
                {
                    if(!rank_unit_standard_.empty())  rank_unit_standard_.pop_back();
                    rank_unit_standard_.push_back(std::move(pointer));
                    lock.unlock();
                    judge_empty_cv_.notify_one();
                    return true;
                }
                case backpressure::exception:
                    lock.unlock();
                    throw operation_exception("The queue is full, please check the overflow policy.");
                case backpressure::drop:
                    lock.unlock();
                    return false;
                default:
                    lock.unlock();
                    throw operation_exception("Unknown backpressure mode.");
            }
        }
    protected:
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode) override
        {
            if(closed_.load(std::memory_order_acquire)) return false;
            if(pointer == nullptr) return false;
            return enqueue_with_backpressure(std::move(pointer), mode);
        }
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
        internals_time timeout_pointer) override
        {
            internals_time_t now_time = std::chrono::system_clock::now();
            if(!timeout_pointer || now_time < *timeout_pointer)
            {
                return internal_push(std::move(pointer), mode);
            }
            return false;
        }
        virtual std::size_t internal_push_batch(std::vector<safety_unit_pointer>&& pointers, 
            backpressure mode) override
        {
            if(closed_.load(std::memory_order_acquire)) return 0;
            if(pointers.empty()) throw operation_exception("The vector pointers is empty.");
            std::size_t complete_push_unit_counter = 0;
            for(auto& unit_pointers : pointers)
            {
                if (internal_push(std::move(unit_pointers), mode))
                {
                    complete_push_unit_counter++;
                }
            }
            return complete_push_unit_counter;
        }
        virtual safety_unit_pointer internal_pop() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_standard_mutex_);
            auto  check_units_func = [this]()
            {
                return !this->rank_unit_standard_.empty() || this->closed_.load(std::memory_order_acquire);
            };
            judge_empty_cv_.wait(lock, check_units_func);
            if(closed_.load(std::memory_order_acquire) && rank_unit_standard_.empty()) return nullptr;
            safety_unit_pointer pointer = std::move(rank_unit_standard_.front());
            rank_unit_standard_.pop_front();
            judge_full_cv_.notify_one();
            return pointer;
        }
        virtual std::vector<safety_unit_pointer> internal_pop_batch(const std::size_t count) override
        {
            std::vector<safety_unit_pointer> pointers;

            std::unique_lock<std::shared_mutex> lock(rank_standard_mutex_);
            pointers.reserve(count);
            auto  popup_func = [this]()
            {
                return !this->rank_unit_standard_.empty() || this->closed_.load(std::memory_order_acquire);
            };
            judge_empty_cv_.wait(lock, popup_func);
            if(closed_.load(std::memory_order_acquire) && this->rank_unit_standard_.empty()) return pointers;
            const std::size_t safety_count = std::min(count, rank_unit_standard_.size());
            auto last_iterator = std::next(rank_unit_standard_.begin(), safety_count);
            auto first = std::make_move_iterator(rank_unit_standard_.begin());
            auto last  = std::make_move_iterator(last_iterator);
            pointers.assign(first, last);
            rank_unit_standard_.erase(rank_unit_standard_.begin(), last_iterator);
            lock.unlock();
            if(count > safety_count)
            {
                //log funtion
            }
            if (safety_count > 0) judge_full_cv_.notify_one();
            return pointers;
        }
        virtual safety_unit_pointer internal_try_pop() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_standard_mutex_);

            if(rank_unit_standard_.empty()) return nullptr;
            auto pointer = std::move(rank_unit_standard_.front());
            rank_unit_standard_.pop_front();

            judge_full_cv_.notify_one();
            return pointer;
        }
        virtual safety_unit_pointer internal_try_pop_for(const std::chrono::milliseconds& timeout) override
        {
            std::unique_lock<std::shared_mutex> lock(rank_standard_mutex_);
            auto  popup_func = [this]()
            {
                return !this->rank_unit_standard_.empty() || this->closed_.load(std::memory_order_acquire);
            };
            if(judge_empty_cv_.wait_for(lock, timeout, popup_func))
            {
                if(closed_.load(std::memory_order_acquire) && rank_unit_standard_.empty()) return nullptr;

                auto pointer = std::move(rank_unit_standard_.front());
                rank_unit_standard_.pop_front();
                lock.unlock();
                judge_full_cv_.notify_one();
                return pointer;
            }
            return nullptr;
        }
        virtual std::size_t internal_size()const override
        {
            std::shared_lock<std::shared_mutex> lock(rank_standard_mutex_);
            return rank_unit_standard_.size();
        }
        virtual bool internal_empty()const override
        {
            std::shared_lock<std::shared_mutex> lock(rank_standard_mutex_);
            return rank_unit_standard_.empty();
        }
        virtual void internal_clear() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_standard_mutex_);
            closed_.store(false, std::memory_order_release);
            max_storage_capacity_.store(0, std::memory_order_release);
            rank_unit_standard_.clear();
        }
        virtual void internal_close() override
        {
            closed_.store(true, std::memory_order_release);
            judge_empty_cv_.notify_all();
            judge_full_cv_.notify_all();
        }
        virtual rank_strategy internal_strategy()const override
        {
            return rank_strategy::fifo;
        }
        virtual std::size_t internal_get_sub_queue_count()const override
        {
            return 0;
        }
        virtual std::size_t internal_get_delay_uint_count()const override
        {
            return 0;
        }
    };
    /**
      * @brief 优先级队列
      * @details 优先级队列，根据优先级排序，优先级高的先出队
      * @note 底层容器为`std::multiset`
      */
    class rank_priority : public rank_ordinary
    {
    public:
        explicit rank_priority(std::size_t max_size = 0) : rank_ordinary(max_size) {}
        
        virtual ~rank_priority() = default;
        
    protected:
        class comparator
        {
        public:
            bool operator()(const safety_unit_pointer& first, const safety_unit_pointer& second) const
            {
                return first->getpriority_() < second->getpriority_();
            }
        };
    protected: 
        std::multiset<safety_unit_pointer,comparator> rank_unit_priority_;

        std::condition_variable_any judge_empty_cv_;
        std::condition_variable_any judge_full_cv_;

        mutable std::shared_mutex rank_priority_mutex_;
    private:
        bool enqueue_with_backpressure(safety_unit_pointer pointer, backpressure mode)
        {
            std::size_t current_size = 0;
            
            std::unique_lock<std::shared_mutex> lock(rank_priority_mutex_);
            current_size = rank_unit_priority_.size();
            
            if((max_storage_capacity_ != 0 && current_size >= max_storage_capacity_) == false)
            {
                rank_unit_priority_.insert(std::move(pointer));
                lock.unlock();
                judge_empty_cv_.notify_one();
                return true;
            }
            switch(mode)
            {
                case backpressure::block:
                {
                    auto block_func = [this]()
                    {
                        return this->rank_unit_priority_.size() < this->max_storage_capacity_
                        || this->closed_.load(std::memory_order_acquire);
                    };
                    judge_full_cv_.wait(lock, block_func);
                    if(closed_.load(std::memory_order_acquire)) return false;
                    rank_unit_priority_.insert(std::move(pointer));
                    lock.unlock();
                    judge_empty_cv_.notify_one();
                    return true;
                }
                case backpressure::overwrite:
                { 
                    if(!rank_unit_priority_.empty())
                    {
                        auto replace_iterator = std::prev(rank_unit_priority_.end());
                        rank_unit_priority_.erase(replace_iterator);
                    }
                    rank_unit_priority_.insert(std::move(pointer));
                    lock.unlock();
                    judge_empty_cv_.notify_one();
                    return true;
                }
                case backpressure::exception:
                    lock.unlock();
                    throw operation_exception("The queue is full, please check the overflow policy.");
                case backpressure::drop:
                    lock.unlock();
                    return false;
                default:
                    lock.unlock();
                    throw operation_exception("Unknown backpressure mode.");
            }
        }
    protected:
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode) override
        {
            if(closed_.load(std::memory_order_acquire)) return false;
            if(pointer == nullptr) return false;
            return enqueue_with_backpressure(std::move(pointer), mode);
        }
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
        internals_time timeout_pointer) override
        {
            internals_time_t now_time = std::chrono::system_clock::now();
            if(!timeout_pointer || now_time < *timeout_pointer)
            {
                return internal_push(std::move(pointer), mode);
            }
            return false;
        }
        virtual std::size_t internal_push_batch(std::vector<safety_unit_pointer>&& pointers, 
            backpressure mode) override
        {
            if(closed_.load(std::memory_order_acquire)) return 0;
            if(pointers.empty()) throw operation_exception("The vector pointers is empty.");
            std::size_t complete_push_unit_counter = 0;
            for(auto& unit_pointers : pointers)
            {
                if (internal_push(std::move(unit_pointers), mode))
                {
                    complete_push_unit_counter++;
                }
            }
            return complete_push_unit_counter;
        }
        virtual safety_unit_pointer internal_pop() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_priority_mutex_);
            auto check_units_func = [this]()
            {
                return !this->rank_unit_priority_.empty() || this->closed_.load(std::memory_order_acquire);
            }; 
            judge_empty_cv_.wait(lock, check_units_func);
            if(closed_.load(std::memory_order_acquire) && rank_unit_priority_.empty()) return nullptr;
            
            auto it = rank_unit_priority_.begin();
            if(it == rank_unit_priority_.end()) return nullptr;
            
            safety_unit_pointer pointer = *it;
            rank_unit_priority_.erase(it);
            lock.unlock();
            judge_full_cv_.notify_one();
            return pointer;
        }
        virtual std::vector<safety_unit_pointer> internal_pop_batch(const std::size_t count) override
        {
            std::vector<safety_unit_pointer> pointers;
            std::unique_lock<std::shared_mutex> lock(rank_priority_mutex_);
            auto popup_func = [this]()
            {
                return !this->rank_unit_priority_.empty() || this->closed_.load(std::memory_order_acquire);
            };
            judge_empty_cv_.wait(lock, popup_func);
            if(closed_.load(std::memory_order_acquire) && rank_unit_priority_.empty()) return pointers;
            const std::size_t safety_count = std::min(count, rank_unit_priority_.size());
            pointers.reserve(safety_count);
            for(std::size_t i = 0; i < safety_count; ++i)
            {
                safety_unit_pointer high_level_value = const_cast<safety_unit_pointer&>(*rank_unit_priority_.begin());
                safety_unit_pointer pointer = std::move(high_level_value);
                pointers.push_back(std::move(pointer));
                rank_unit_priority_.erase(rank_unit_priority_.begin());
            }
            lock.unlock();
            if(count > safety_count)
            {
                //log funtion
            }
            if (safety_count > 0) judge_full_cv_.notify_one();
            return pointers;
        }
        virtual safety_unit_pointer internal_try_pop() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_priority_mutex_);
            if(rank_unit_priority_.empty()) return nullptr;
            
            auto it = rank_unit_priority_.begin();
            if(it == rank_unit_priority_.end()) return nullptr;
            
            safety_unit_pointer pointer = *it;
            rank_unit_priority_.erase(it);
            judge_full_cv_.notify_one();
            return pointer;
        }
        virtual safety_unit_pointer internal_try_pop_for(const std::chrono::milliseconds& timeout) override
        {
            std::unique_lock<std::shared_mutex> lock(rank_priority_mutex_);
            auto  popup_func = [this]()
            {
                return !this->rank_unit_priority_.empty() || this->closed_.load(std::memory_order_acquire);
            };
            if(judge_empty_cv_.wait_for(lock, timeout, popup_func))
            {
                auto it = rank_unit_priority_.begin();
                if(it == rank_unit_priority_.end()) return nullptr;
                
                safety_unit_pointer pointer = *it;
                rank_unit_priority_.erase(it);

                lock.unlock();
                judge_full_cv_.notify_one();
                return pointer;
            }
            return nullptr;
        }
        virtual std::size_t internal_size()const override
        {
            std::shared_lock<std::shared_mutex> lock(rank_priority_mutex_);
            return rank_unit_priority_.size();
        }
        virtual bool internal_empty()const override
        {
            std::shared_lock<std::shared_mutex> lock(rank_priority_mutex_);
            return rank_unit_priority_.empty();
        }
        virtual void internal_clear() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_priority_mutex_);
            closed_.store(false, std::memory_order_release);
            max_storage_capacity_.store(0, std::memory_order_release);
            rank_unit_priority_.clear();
        }
        virtual void internal_close() override
        {
            closed_.store(true, std::memory_order_release);
            max_storage_capacity_.store(0, std::memory_order_release);
            judge_empty_cv_.notify_all();
            judge_full_cv_.notify_all();
        }
        virtual rank_strategy internal_strategy()const override
        {
            return rank_strategy::priority;
        }
        virtual std::size_t internal_get_sub_queue_count()const override
        {
            return 0;
        }
        virtual std::size_t internal_get_delay_uint_count()const override
        {
            return 0;
        }
    };
    /**
      * @brief 延迟队列
      */
    class rank_deferred : public rank_ordinary
    {
    protected:
        class delay_unit
        {
        public:
            safety_unit_pointer safety_unit_pointer_;
            internals_time_t delay_time_;
            delay_unit(safety_unit_pointer safety_unit_pointer,internals_time_t delay_time = internals_clk::now())
            :safety_unit_pointer_(std::move(safety_unit_pointer)),delay_time_(delay_time) {}
        };
        struct comparator
        {
            bool operator()(const std::shared_ptr<delay_unit>& first, const std::shared_ptr<delay_unit>& second)const
            {
                return first->delay_time_ > second->delay_time_;
            }
        };
    protected:
        std::jthread background_detection_;

        std::condition_variable_any judge_empty_cv_;
        std::condition_variable_any judge_full_cv_;

        mutable std::shared_mutex rank_deferred_mutex_; 
        std::multiset <std::shared_ptr<delay_unit>,comparator> rank_unit_deferred_;
    private:
        bool enqueue_with_backpressure(std::shared_ptr<delay_unit> struct_pointer, backpressure mode)
        {
            if(struct_pointer == nullptr) 
                throw operation_exception("The incoming pointer is null, please check the parameters passed from the upper layer.");

            std::size_t current_size = 0;
            std::unique_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            current_size = rank_unit_deferred_.size();
            if((max_storage_capacity_ != 0 && current_size >= max_storage_capacity_) == false)
            {
                rank_unit_deferred_.insert(std::move(struct_pointer));
                lock.unlock();
                judge_empty_cv_.notify_one();
                return true;
            }
            switch(mode)
            {
                case backpressure::block:
                {
                    auto block_func = [this]()
                    {
                        return this->rank_unit_deferred_.size() < this->max_storage_capacity_
                        || this->closed_.load(std::memory_order_acquire);
                    };
                    judge_full_cv_.wait(lock, block_func);
                    if(closed_.load(std::memory_order_acquire)) return false;
                    rank_unit_deferred_.insert(std::move(struct_pointer));
                    lock.unlock();
                    judge_empty_cv_.notify_one();
                    return true;
                }
                case backpressure::overwrite:
                {
                    
                    if(!rank_unit_deferred_.empty())
                    {
                        auto replace_iterator = std::prev(rank_unit_deferred_.end());
                        rank_unit_deferred_.erase(replace_iterator);
                    }
                    rank_unit_deferred_.insert(std::move(struct_pointer));
                    lock.unlock();
                    judge_empty_cv_.notify_one();
                    return true;
                }
                case backpressure::exception:
                    throw operation_exception("The queue is full, please check the overflow policy.");
                case backpressure::drop:
                    return false;
                default:
                    throw operation_exception("Unknown backpressure mode.");
            }
        }
        void background_detection()
        {
            // 后台检测线程
            while (!closed_.load(std::memory_order_acquire))
            {
                bool has_expired = false;
                std::chrono::system_clock::time_point next_check_time;
                
                {
                    std::unique_lock<std::shared_mutex> lock(rank_deferred_mutex_);
                    if (!rank_unit_deferred_.empty())
                    {
                        auto now = std::chrono::system_clock::now();
                        auto earliest_task = *rank_unit_deferred_.begin();
                        
                        if (earliest_task->delay_time_ <= now)
                        {
                            has_expired = true;
                        }
                        else
                        {
                            next_check_time = earliest_task->delay_time_;
                        }
                    }
                }
                
                if (has_expired)
                {
                    judge_empty_cv_.notify_one();          // 有元素到期，叫醒消费者
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else if (!rank_unit_deferred_.empty())
                {
                    // 智能等待：等待到下一个任务到期时间，但最多等待10ms
                    auto now = std::chrono::system_clock::now();
                    auto wait_time = std::min(
                        std::chrono::duration_cast<std::chrono::milliseconds>(next_check_time - now),
                        std::chrono::milliseconds(10)
                    );
                    if (wait_time > std::chrono::milliseconds(0))
                    {
                        std::this_thread::sleep_for(wait_time);
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
                else
                {
                    // 队列为空时等待更长时间
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
    public:
        rank_deferred() = default;
        rank_deferred(std::size_t max_storage_capacity = 0) :rank_ordinary(max_storage_capacity)
        {
            background_detection_ = std::jthread(&rank_deferred::background_detection, this);
        }
        ~rank_deferred()
        {
            internal_close();
            if(background_detection_.joinable())
                background_detection_.join();
        }
    protected:
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode) override
        {
            if(closed_.load(std::memory_order_acquire)) return false;
            if(pointer == nullptr) return false;
            std::shared_ptr<delay_unit> small_unit = std::make_shared<delay_unit>(std::move(pointer));
            return enqueue_with_backpressure(small_unit, mode);
        }
        virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
            internals_time delay_time) override
        {
            if(closed_.load(std::memory_order_acquire)) return false;
            if(pointer == nullptr) return false;
            std::shared_ptr<delay_unit> small_unit = std::make_shared<delay_unit>(std::move(pointer), *delay_time);
            return enqueue_with_backpressure(small_unit, mode);
        }
        virtual std::size_t internal_push_batch(std::vector<safety_unit_pointer>&& pointer, backpressure mode) override
        {
            if(closed_.load(std::memory_order_acquire)) return 0;
            if(pointer.empty())  throw operation_exception("The vector pointers is empty.");
            std::size_t complete_push_unit_counter = 0;
            for(auto& unit : pointer)
            {
                if (internal_push(unit, mode))
                {
                    complete_push_unit_counter++;
                }
            }
            return complete_push_unit_counter; 
        }
        virtual safety_unit_pointer internal_pop() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            if(rank_unit_deferred_.empty() && closed_.load(std::memory_order_acquire)) return nullptr;
            judge_empty_cv_.wait(lock);
            auto it = rank_unit_deferred_.begin();
            safety_unit_pointer pointer = (*it)->safety_unit_pointer_;
            rank_unit_deferred_.erase(it);
            lock.unlock();
            judge_full_cv_.notify_one();
            return pointer;
        }
        virtual std::vector<safety_unit_pointer> internal_pop_batch(std::size_t count) override
        {
            std::vector<safety_unit_pointer> pointer;
            std::unique_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            const std::size_t safety_count = std::min(count, rank_unit_deferred_.size());
            for(std::size_t i = 0; i < safety_count; i++)
            {
                if(rank_unit_deferred_.empty()) break;
                if((*rank_unit_deferred_.begin())->delay_time_ <= internals_clk::now())
                {
                    auto& delay_ptr = const_cast<std::shared_ptr<delay_unit>&>(*rank_unit_deferred_.begin());
                    pointer.push_back(std::move(delay_ptr->safety_unit_pointer_));
                    rank_unit_deferred_.erase(rank_unit_deferred_.begin());
                }
                else
                {
                    judge_empty_cv_.wait(lock);
                }
            }
            lock.unlock();
            if (safety_count > 0) judge_full_cv_.notify_one();
            return pointer;
        }
        virtual safety_unit_pointer internal_try_pop() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            if(rank_unit_deferred_.empty()) return nullptr;
            auto it = rank_unit_deferred_.begin();
            if((*it)->delay_time_ < internals_clk::now())
            {
                safety_unit_pointer pointer = (*it)->safety_unit_pointer_;
                rank_unit_deferred_.erase(it);
                lock.unlock();
                judge_full_cv_.notify_one();
                return pointer;
            }
            return nullptr;
        }
        virtual safety_unit_pointer internal_try_pop_for(const std::chrono::milliseconds& timeout) override
        {
            std::unique_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            auto  popup_func = [this]()
            {
                return !this->rank_unit_deferred_.empty() || this->closed_.load(std::memory_order_acquire);
            };
            if(judge_empty_cv_.wait_for(lock, timeout, popup_func))
            {
                auto it = rank_unit_deferred_.begin();
                safety_unit_pointer pointer = (*it)->safety_unit_pointer_;
                rank_unit_deferred_.erase(it);

                lock.unlock();
                judge_full_cv_.notify_one();
                return pointer;
            }
            return nullptr;
        }
        virtual std::size_t internal_size()const override
        {
            std::shared_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            return rank_unit_deferred_.size();
        }
        virtual bool internal_empty()const override
        {
            std::shared_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            return rank_unit_deferred_.empty();
        }
        virtual void internal_clear() override
        {
            std::unique_lock<std::shared_mutex> lock(rank_deferred_mutex_);
            closed_.store(false, std::memory_order_release);
            max_storage_capacity_.store(0, std::memory_order_release);
            rank_unit_deferred_.clear();
        }
        virtual void internal_close() override
        {
            closed_.store(true, std::memory_order_release);
            max_storage_capacity_.store(0, std::memory_order_release);
            judge_empty_cv_.notify_all();
            judge_full_cv_.notify_all();
        }
        virtual rank_strategy internal_strategy()const override
        {
            return rank_strategy::delay;
        }
        virtual std::size_t internal_get_sub_queue_count()const override
        {
            return 0;
        }
        virtual std::size_t internal_get_delay_uint_count()const override
        {
            return 0;
        }
    };
    /**
      * @brief 任务队列工厂函数 - 创建`FIFO`队列
      * @param max_capacity 最大队列容量
      * @return 队列智能指针
      */
    inline std::shared_ptr<rank_standard> make_rank_standard(std::size_t max_capacity = 0)
    {
        return std::make_shared<rank_standard>(max_capacity);
    }
    /**
      * @brief 任务队列工厂函数 - 创建优先级队列
      * @param max_capacity 最大队列容量
      * @return 队列智能指针
      */
    inline std::shared_ptr<rank_priority> make_rank_priority(std::size_t max_capacity = 0)
    {
        return std::make_shared<rank_priority>(max_capacity);
    }
    /**
      * @brief 任务队列工厂函数 - 创建延迟队列
      * @param max_capacity 最大队列容量
      * @return 队列智能指针
      */
    inline std::shared_ptr<rank_deferred> make_rank_deferred(std::size_t max_capacity = 0)
    {
        return std::make_shared<rank_deferred>(max_capacity);
    }
    /**
      * @brief 任务队列工厂函数 - 根据策略创建队列
      * @param strategy 队列策略
      * @param max_capacity 最大队列容量
      * @return 队列智能指针
      */
    inline std::shared_ptr<rank_ordinary> make_rank(rank_strategy strategy, std::size_t max_capacity = 0)
    {
        switch(strategy)
        {
            case rank_strategy::fifo:
                return make_rank_standard(max_capacity);
            case rank_strategy::priority:
                return make_rank_priority(max_capacity);
            case rank_strategy::delay:
                return make_rank_deferred(max_capacity);
            default:
                return make_rank_standard(max_capacity);
        }
    }
}