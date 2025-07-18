#pragma once
#include "custom_exception.hpp"
#include <memory>
namespace con
{
    /*
    * @brief  #### `smart_pointer`智能指针命名空间

    *   ##### 智能指针模板类

    *   - `smart_ptr`: 管理动态分配的资源，确保资源自动释放

    *   - `shared_ptr`: 共享所有权的智能指针，允许多个智能指针共享同一资源

    *   - `unique_ptr`: 独占所有权的智能指针，确保资源仅由一个智能指针管理

    *   - `weak_ptr`: 弱引用的智能指针，不拥有资源所有权，用于避免循环引用
    */
    namespace smart_pointer{}
}

namespace con::smart_pointer
{
    /*
     * @brief  #### `smart_ptr` 类

    *   - 智能指针，管理动态分配的资源，确保资源自动释放

    * 模板参数:

        - `smart_ptr_type`: 管理的对象类型

        - `deleter`: 定制删除器类型，默认为 `std::default_delete<smart_ptr_type>`

     * 构造函数:

     * * - 默认构造函数: 创建空智能指针

     * * - `explicit smart_ptr(const smart_ptr_type* ptr)`: 从原始指针构造，禁用隐式转换

     * 禁用的操作:

      - 拷贝构造、拷贝赋值: 不允许共享资源所有权

      - 移动赋值(右值引用赋值): 仅支持移动构造转移资源所有权

     * 提供的操作符:

     * * - `operator->()`: 访问管理对象的成员

     * * - `operator*()`: 解引用管理的对象

     * 资源管理:

     * * - 使用指定的删除器自动释放资源
     
     * * - 移动构造后原对象置空，避免双重释放

     * * - 析构时自动调用删除器释放资源

     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 

     * 线程安全性:

     * * - 不保证多线程环境下的安全性，操作时需外部同步

     * 注意事项:

     * * - 不要用同一个原始指针创建多个智能指针实例

     * * - 管理数组时需使用自定义删除器
    */
    template<typename smart_ptr_type,typename deleter = std::default_delete<smart_ptr_type>>
    class smart_ptr
    {
    private:
        smart_ptr_type* _ptr;
        deleter _deleter;
        using Ref = smart_ptr_type&;
        using Ptr = smart_ptr_type*;
    public:
        explicit smart_ptr(smart_ptr_type* ptr = nullptr)
        :_deleter(deleter())
        {
            _ptr = ptr;
        }
        smart_ptr(const smart_ptr&) = delete;
        smart_ptr& operator=(const smart_ptr*& smart_ptr_data) = delete;
        smart_ptr(smart_ptr&& smart_ptr_data) noexcept
        {
            _ptr = smart_ptr_data._ptr;
            smart_ptr_data._ptr = nullptr;
        }
        smart_ptr& operator=(smart_ptr&& smart_ptr_data) noexcept
        {
            if(&smart_ptr_data != this)
            {
                _ptr = smart_ptr_data._ptr;
                smart_ptr_data._ptr = nullptr;
            }
            return *this;
        }
        ~smart_ptr() noexcept
        {
            if(_ptr != nullptr)
            {
                _deleter(_ptr);
                _ptr = nullptr;
            }
        }
        Ptr operator->()const noexcept
        {
            return _ptr;
        }
        Ref operator*() noexcept
        {
            return *(_ptr);
        }
    };
    /*
     * @brief  #### `unique_ptr` 类

    *   - 实现独占式资源管理的智能指针，确保同一资源仅被一个指针拥有

    *   - 资源在指针析构时自动释放，防止内存泄漏

     * 模板参数:

     * * - `unique_ptr_type`: 管理的对象类型

     * * - `deleter`: 资源释放器类型，默认为 `std::default_delete<unique_ptr_type>`

     * 构造函数:

     * * - `explicit unique_ptr(unique_ptr_type* ptr = nullptr)`: 从原始指针构造，禁用隐式转换

     * * - 移动构造函数: 转移资源所有权，原指针置空

     * 禁用的操作:

     * * - 拷贝构造、拷贝赋值: 禁止资源共享，确保独占性
     * 
     * * - 拷贝赋值(常量右值引用): 防止意外的资源所有权转移

     * 提供的操作符:

     * * - `operator*()`: 解引用管理的对象

     * * - `operator->()`: 访问管理对象的成员

     * * - 移动赋值(右值引用赋值): 转移资源所有权

     * 关键方法:

     * * - `get_ptr()`: 返回原始指针（不释放所有权）


     * 资源管理:

     * * - 使用指定删除器自动释放资源

     * * - 移动操作后原指针置空，避免双重释放

     * * - 支持自定义删除器处理特殊资源（如文件句柄、网络连接）

     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 

     * 线程安全性:

     * * - 不保证多线程环境下的安全性，操作时需外部同步

     * 注意事项:

     * * - 不要用同一个原始指针创建多个智能指针实例

     * * - 管理数组时需使用自定义删除器

     * * - 避免存储由 `new[]` 分配的数组指针，除非使用匹配的删除器
    */
    template <typename unique_ptr_type,typename deleter = std::default_delete<unique_ptr_type>>
    class unique_ptr
    {    
    private:
        unique_ptr_type* _ptr;
        deleter _deleter;
        using Ref = unique_ptr_type&;
        using Ptr = unique_ptr_type*;
    public:
        explicit unique_ptr(unique_ptr_type* ptr = nullptr) noexcept
        :_deleter(deleter())
        {
            _ptr = ptr;
        }
        ~unique_ptr() noexcept
        {
            if( _ptr != nullptr)
            {
                _deleter(_ptr);
                _ptr = nullptr;
            }
        }
        Ref operator*() noexcept
        {
            return *(_ptr);
        }
        Ptr operator->() noexcept
        {
            return _ptr;
        }
        unique_ptr_type* get_ptr() const noexcept
        {
            return _ptr;
        }
        unique_ptr(const unique_ptr& unique_ptr_data) noexcept = delete;
        unique_ptr<unique_ptr_type>& operator= (const unique_ptr<unique_ptr_type,deleter>& unique_ptr_data) noexcept = delete;
        unique_ptr<unique_ptr_type>& operator= (unique_ptr<unique_ptr_type,deleter>&& unique_ptr_data) noexcept
        {
            if(&unique_ptr_data != this)
            {
                if( _ptr != nullptr)
                {
                    _deleter(_ptr);
                    _ptr = nullptr;
                }
                _ptr = unique_ptr_data._ptr;
                unique_ptr_data._ptr = nullptr;
            }
            return *this;
        }
        unique_ptr(unique_ptr&& unique_ptr_data) noexcept
        {
            _ptr = unique_ptr_data._ptr;
            unique_ptr_data._ptr = nullptr;
        }
    };
    /*
     * @brief  #### `shared_ptr` 类

    *   - 实现共享式资源管理的智能指针，通过引用计数实现资源的自动释放

    *   - 多个指针可共享同一资源，最后一个指针释放时资源才被销毁

     * 模板参数:

     * * - `shared_ptr_type`: 管理的对象类型

     * * - `deleter`: 资源释放器类型，默认为 `std::default_delete<shared_ptr_type>`

     * 构造函数:

     * * - `explicit shared_ptr(shared_ptr_type* ptr = nullptr)`: 从原始指针构造

     * * - 拷贝构造函数: 增加引用计数并共享资源

     * * - 移动构造函数: 转移资源所有权，原指针重置

     * 核心机制:

     * * - 引用计数(`shared_pcount`): 记录共享同一资源的指针数量

     * * - 线程安全: 使用互斥锁(`_pmutex`)保护引用计数操作

     * 提供的操作符:

     * * - `operator*()`: 解引用管理的对象

     * * - `operator->()`: 访问管理对象的成员

     * * - 拷贝赋值、移动赋值: 管理引用计数的增减

     * 关键方法:

     * * - `get_count()`: 返回当前引用计数

     * * - `get_ptr()`: 返回原始指针（不释放所有权）

     * 资源管理:

     * * - 最后一个指针释放时调用删除器销毁资源

     * * - 支持自定义删除器处理特殊资源（如文件句柄、网络连接）

     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 

     * 线程安全性:

     * * - 引用计数操作是线程安全的

     * * - 但不保证被管理对象的线程安全，需用户自行同步

     * 注意事项:

     * * - 避免循环引用（会导致内存泄漏，需配合 `weak_ptr` 使用）

     * * - 不要用同一个原始指针创建多个独立的 shared_ptr

     * * - 管理数组时需使用自定义删除器
    */
    template <typename shared_ptr_type,typename deleter = std::default_delete<shared_ptr_type>>
    class shared_ptr
    {
    private:
        shared_ptr_type* _ptr;
        deleter _deleter;
        int* shared_pcount;
        std::mutex* _pmutex;
        using Ref = shared_ptr_type&;
        using ptr = shared_ptr_type*;
        void release() noexcept
        {
            _pmutex->lock();
            bool flag = false;
            if(--(*shared_pcount) == 0 && _ptr != nullptr)
            {
                _deleter(_ptr);
                _ptr = nullptr;
                delete shared_pcount;
                shared_pcount = nullptr;
                flag = true;
            }
            _pmutex->unlock();
            if(flag == true)
            {
                delete _pmutex;
                _pmutex = nullptr;
            }
        }
        void swap(shared_ptr& deliver_value) noexcept
        {
            _ptr = deliver_value._ptr;
            shared_pcount = deliver_value.shared_pcount;
            _pmutex = deliver_value._pmutex;
            _deleter = deliver_value._deleter;

        }
    public:
        explicit shared_ptr(shared_ptr_type* ptr = nullptr)
        : _deleter(deleter())
        {
            _ptr = ptr;
            shared_pcount = new int(1);
            _pmutex = new std::mutex;
        }
        shared_ptr(shared_ptr& shared_ptr_data) noexcept
        {
            swap(shared_ptr_data);
            _pmutex->lock();
            (*shared_pcount)++;
            _pmutex->unlock();
        }
        shared_ptr(shared_ptr&& shared_ptr_data) noexcept
        {
            swap(shared_ptr_data);         
            shared_ptr_data._ptr = nullptr;
            shared_ptr_data.shared_pcount = new int(1);  
            shared_ptr_data._pmutex = new std::mutex;   
        }
        ~shared_ptr() noexcept
        {
           release();
        }
        shared_ptr<shared_ptr_type>& operator=(const shared_ptr& shared_ptr_data) noexcept
        {
            if(&shared_ptr_data != this)
            {
                if(_ptr != shared_ptr_data._ptr)
                {
                    release();
                    swap(shared_ptr_data);
                    _pmutex->lock();
                    (*shared_pcount)++;
                    _pmutex->unlock();
                }
            }
            return *this;
        }
        shared_ptr<shared_ptr_type>& operator=(shared_ptr&& shared_ptr_data) noexcept
        {
            if(&shared_ptr_data != this)
            {
                release();
                swap(shared_ptr_data);
                shared_ptr_data._ptr = nullptr;
                shared_ptr_data.shared_pcount = new int(1);
                shared_ptr_data._pmutex = new std::mutex;
            }
            return *this;
        }
        [[nodiscard]] int get_count() const noexcept
        {
            return *shared_pcount;
        }
        Ref operator*() noexcept
        {
            return *(_ptr);
        }
        ptr operator->() noexcept
        {
            return _ptr;
        }
        ptr get_ptr() const noexcept
        {
            return _ptr;
        }
    };
    template <typename weak_ptr_type>
    class weak_ptr
    {
    private:
        weak_ptr_type* _ptr;
        using Ref = weak_ptr_type&;
        using ptr = weak_ptr_type*;
    public:
        weak_ptr() = default;
        explicit weak_ptr(smart_pointer::shared_ptr<weak_ptr_type>& ptr) noexcept
        {
            _ptr = ptr.get_ptr();
        }
        weak_ptr(const weak_ptr& ptr_type_data) noexcept
        {
            _ptr = ptr_type_data._ptr;
        }
        Ref operator*() noexcept
        {
            return *(_ptr);
        }
        ptr operator->() noexcept
        {
            return _ptr;
        }
        weak_ptr<weak_ptr_type>& operator=(smart_pointer::shared_ptr<weak_ptr_type>& ptr) noexcept
        {
            _ptr = ptr.get_ptr();
            return *this;
        }
    };
}