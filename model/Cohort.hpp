#pragma once
#include <iostream>
#include <vector>
#include "Task.hpp"
#include <typeinfo>
#include <stdexcept>
#include <functional>
#include <unordered_map>

/**
 * @brief 任务队列类型的安全转换
 * @tparam originally_type 要转换的类型
 * @tparam function 转换函数
 * @tparam downgrade_function 降级调用函数 
 * @param pointer 队列 指针
 * @param conversion_call 转换函数值
 * @param downgrade  降级调用函数值
 * @warning 转换函数和失败调用函数的参数需要用智能指针来维护内存安全
 */
template<class originally_type,class function,class downgrade_function>
bool automatic_derivation(task_ptr pointer, function&& conversion_call, downgrade_function&& downgrade)
{
  if(pointer.get() != nullptr)
  {
    if(auto concrete_queue = std::dynamic_pointer_cast<originally_type>(pointer))
    {
      std::invoke(conversion_call, concrete_queue);
      return true;
    }
    else
    {
      std::invoke(downgrade, pointer);
    }
  }
  return false;
}