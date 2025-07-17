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
        smart_ptr& operator=(const smart_ptr*& other) = delete;
        smart_ptr(smart_ptr&& other) noexcept
        {
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        smart_ptr& operator=(smart_ptr&& other) noexcept
        {
            if(&other != this)
            {
                _ptr = other._ptr;
                other._ptr = nullptr;
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
        unique_ptr(const unique_ptr& ptr_type_data) noexcept = delete;
        unique_ptr<unique_ptr_type>& operator= (const unique_ptr<unique_ptr_type,deleter>& ptr_type_data) noexcept = delete;
        unique_ptr<unique_ptr_type>& operator= (unique_ptr<unique_ptr_type,deleter>&& ptr_type_data) noexcept
        {
            if(&ptr_type_data != this)
            {
                if( _ptr != nullptr)
                {
                    _deleter(_ptr);
                    _ptr = nullptr;
                }
                _ptr = ptr_type_data._ptr;
                ptr_type_data._ptr = nullptr;
            }
            return *this;
        }
        unique_ptr(unique_ptr&& ptr_type_data) noexcept
        {
            _ptr = ptr_type_data._ptr;
            ptr_type_data._ptr = nullptr;
        }
    };
    template <typename shared_ptr_type,typename deleter = std::default_delete<unique_ptr_type>>
    class shared_ptr
    {
    private:
        shared_ptr_type* _ptr;
        int* shared_pcount;
        std::mutex* _pmutex;
        using Ref = shared_ptr_type&;
        using ptr = shared_ptr_type*;
    public:
        explicit shared_ptr(shared_ptr_type* ptr = nullptr)
        {
            _ptr = ptr;
            shared_pcount = new int(1);
            _pmutex = new std::mutex;
        }
        shared_ptr(const shared_ptr& shared_ptr_data) noexcept
        {
            _ptr = shared_ptr_data._ptr;
            shared_pcount = shared_ptr_data.shared_pcount;
            _pmutex = shared_ptr_data._pmutex;
            //上锁
            _pmutex->lock();
            (*shared_pcount)++;
            _pmutex->unlock();
        }
        ~shared_ptr() noexcept
        {
           release();
        }
        void release() noexcept
        {
            _pmutex->lock();
            bool flag = false;
            if(--(*shared_pcount) == 0 && _ptr != nullptr)
            {
                delete _ptr;
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
        shared_ptr<shared_ptr_type>& operator=(const shared_ptr& shared_ptr_data) noexcept
        {
            if(&shared_ptr_data != this)
            {
                if(_ptr != shared_ptr_data._ptr)
                {
                    release();
                    _ptr = shared_ptr_data._ptr;
                    shared_pcount = shared_ptr_data.shared_pcount;
                    _pmutex = shared_ptr_data._pmutex;
                    //上锁
                    _pmutex->lock();
                    (*shared_pcount)++;
                    _pmutex->unlock();
                }
            }
            return *this;
        }
        [[nodiscard]] int get_sharedp_count() const noexcept
        {
            return *shared_pcount;
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
namespace con
{

}