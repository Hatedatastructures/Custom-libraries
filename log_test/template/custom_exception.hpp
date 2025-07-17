#pragma once
#define CRT_SECURE_NO_WARNINGS
#include <cstring>
#include <iostream>
#include <mutex>
namespace custom_exception
{
    /*
     * @brief  #### `customize_exception` 类

    *   - 用于抛出带消息、函数名、行号信息的异常 

     * 带参构造参数: `message_target`, `function_name_target` , `line_number_target`

     * * - `message_target` : 异常信息

     * * - `function_name_target` : 异常出现所在的函数

     * * - `line_number_target` : 异常发生行数

     * `what()`函数:返回异常信息

     * `function_name_get()`函数:返回异常发生函数

     * `line_number_get()`函数:返回异常发生行数

     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 

     * 不支持移动赋值，移动构造，拷贝赋值，拷贝构造

     * 不可被继承

     * 该类自动清理资源
    */
    class  customize_exception final : public std::exception
    {
    private:
        char* message;
        char* function_name;
        size_t line_number;
    public:
        customize_exception(const char* message_target,const char* function_name_target,const size_t& line_number_target) noexcept 
        {
            message = new char [std::strlen(message_target) + 1];
            std::strcpy(message,message_target);
            function_name = new char [std::strlen(function_name_target) + 1];
            std::strcpy(function_name,function_name_target);
            line_number = line_number_target;
        }
        [[nodiscard]] const char* what() const noexcept override
        {
            return message;
        }
        [[nodiscard]] const char* function_name_get() const noexcept
        {
            return function_name;
        }
        [[nodiscard]] size_t line_number_get() const noexcept
        {
            return line_number;
        }
        ~customize_exception() noexcept override
        {
            delete [] message;
            delete [] function_name;
        }
        customize_exception(const customize_exception&) = delete;
        customize_exception(customize_exception&&) = delete;
        customize_exception& operator=(const customize_exception&) = delete;
        customize_exception& operator=(customize_exception&&) = delete;
    };
}
namespace con
{
    using namespace custom_exception;
}