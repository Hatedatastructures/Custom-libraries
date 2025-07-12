#include "../template_container.hpp"
// #include "MY_Test.h"
#include <thread>
// import MY_Template;
#include <random>
#include <algorithm>
#include <atomic>
#include <windows.h>
// std::terminate() 异常抛出函数
int main()
{
    SetConsoleOutputCP(CP_UTF8);
    /*            string测试             */
    {
        //异常测试
        // template_container::string_container::string string_test1;
        // template_container::string_container::string string_test2 = string_test1.reverse_sub_string(10,20);
        // std::cout << string_test1 << std::endl << string_test2 << std::endl;
    }
    {
        std::cout << " string 测试 " << std::endl << std::endl;
        template_container::string_container::string string_test1("hello");
        template_container::string_container::string string_test2("world");
        
        template_container::string_container::string string_test3 = string_test1 + string_test2;
        std::cout << "string_test3: " << string_test3 << std::endl;
        string_test3.push_back('!');
        auto insert_str = "inserted";
        string_test3.prepend(insert_str);
        std::cout << "str3 after insertion: " << string_test3 << std::endl;

        size_t old_pos = strlen(insert_str);
        template_container::string_container::string string_test4 = string_test3.sub_string(old_pos);
        std::cout << "string_test4: " << string_test4 << std::endl;

        std::cout << string_test3.lowercase() << std::endl;
        std::cout << string_test3.lowercase() << std::endl;

        template_container::string_container::string string_test5 = string_test3.sub_string_from(5);
        std::cout << "string_test5: " << string_test5 << std::endl;

        template_container::string_container::string string_test6 = string_test3.sub_string(5, 10);
        std::cout << "string_test6: " << string_test6 << std::endl;

        std::cout << "str3 size: " << string_test3.size() << std::endl;
        std::cout << "str3 capacity: " << string_test3.capacity() << std::endl;
        std::cout << "string_test3 after resize: " << string_test3.resize(21, '*') << std::endl;

        std::cout << "string_test3 after reverse: " << string_test3.reverse() << std::endl;

        std::cout << "string_test3 after reverse_sub_string: " << string_test3.reverse_sub_string(5, 10) << std::endl;

        string_test3.string_print();
        string_test3.string_reverse_print();

        for(auto i :string_test3)
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;

        for(char i : string_test3)
        {
            std::cout << i << " ";
        }
        // std::cout << string_test3[30000000] << std::endl;
        std::cout << std::endl;
        {
            //异常测试
            template_container::string_container::string Ex;
            Ex.resize(10000000);
            Ex.push_back("异常测试");
        }
    }

    /*            vector测试             */
    {
        std::cout << " vector 测试 " << std::endl << std::endl;
        template_container::vector_container::vector<int> vector_test(5,1);
        for(auto i: vector_test)
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;
        template_container::vector_container::vector<int> vector_test1(vector_test);
        for(const auto& i  : vector_test1 )
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;
        template_container::vector_container::vector<int> test2 = vector_test1;
        for(const auto i : test2)
        {
            std::cout << i << " ";
        }
        template_container::string_container::string s2 = "name";
        std::cout << std::endl;
        template_container::vector_container::vector<template_container::string_container::string> name_test(10,s2);
        for(const auto& i : name_test )
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;
        template_container::vector_container::vector<template_container::string_container::string> name_test1 =name_test ;
        for(const auto& i : name_test1 )
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;
        template_container::string_container::string s3 = "hello word!";
        name_test1.push_back(s3);
        for(const auto& i : name_test1 )
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;

        name_test1.push_front(s3);
        for(const auto& i : name_test1 )
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;

        name_test1+=name_test;
        for(const auto& i : name_test1 )
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;

        std::cout << name_test1 << std::endl;
        std::cout << name_test1.pop_back() << std::endl;
    }


    /*            list测试             */
    {
        std::cout << " list 测试 " << std::endl << std::endl;
        template_container::list_container::list<int> list_test1;
        for(size_t i = 1; i < 10; i++)
        {
            list_test1.push_back(static_cast<int>(i));
        }
        template_container::list_container::list<int>::const_iterator it =list_test1.cbegin();
        while(it != list_test1.cend())
        {
            std::cout << *it  << " ";
            it++;
        }
        std::cout << std::endl;
        template_container::list_container::list<int>::reverse_const_iterator i = list_test1.rcbegin();
        while(i != list_test1.rcend())
        {
            std::cout << *i << " ";
            i++;
        }
        std::cout <<std::endl;

        list_test1.pop_back(); 
        template_container::list_container::list<int>::const_iterator j =list_test1.cbegin();
        while(j != list_test1.cend())
        {
            std::cout << *j  << " ";
            j++;
        }
        std::cout << std::endl;
        std::cout << list_test1.size() << std::endl;

        template_container::list_container::list<int> list_test2 = list_test1;
        template_container::list_container::list<int>::const_iterator p =list_test2.cbegin();
        while(p != list_test2.cend())
        {
            std::cout << *p  << " ";
            p++;
        }
        std::cout << std::endl;
        std::cout << list_test2.size() << std::endl;

        template_container::list_container::list<int> list_test3 = list_test2 + list_test1;
        template_container::list_container::list<int>::const_iterator k =list_test3.cbegin();
        while(k != list_test3.cend())
        {
            std::cout << *k  << " ";
            k++;
        }
        std::cout << std::endl;
        std::cout << list_test3.size() << std::endl;

        template_container::list_container::list<int> list_test4 = list_test3 + list_test1;
        template_container::list_container::list<int>::const_iterator kp =list_test4.cbegin();
        while(kp != list_test4.cend())
        {
            std::cout << *kp  << " ";
            kp++;
        }
        std::cout << std::endl;
        std::cout << list_test4.size() << std::endl;
        std::cout << list_test4 << std::endl;
    }
    {
        template_container::list_container::list<template_container::string_container::string> list_string_test;
        list_string_test.push_back("hello");
    }
    /*            stack测试             */
    {
        std::cout << " stack 测试 " << std::endl << std::endl;
        template_container::string_container::string staic_test_str1 = "hello";
        template_container::string_container::string staic_test_str2 = "word";
        template_container::string_container::string staic_test_str3 = "  ";
        template_container::stack_adapter::stack< template_container::string_container::string> staic_test1;

        staic_test1.push(staic_test_str1);
        staic_test1.push(staic_test_str3);
        staic_test1.push(staic_test_str2);

        std::cout << staic_test1.top() << std::endl;
        staic_test1.pop();
        std::cout << staic_test1.top() << std::endl;
        staic_test1.pop();
        std::cout << staic_test1.top() << std::endl;
        staic_test1.pop();
    }

    /*            queue测试             */
    {
        std::cout << " queue 测试 " << std::endl << std::endl;
        template_container::string_container::string queue_test_str1 = "hello";
        template_container::string_container::string queue_test_str2 = "word";
        template_container::string_container::string queue_test_str3 = "  ";
        template_container::queue_adapter::queue< template_container::string_container::string,template_container::list_container::list< template_container::string_container::string>>
        queue_test1;

        queue_test1.push(queue_test_str1);
        queue_test1.push(queue_test_str3);
        queue_test1.push(queue_test_str2);

        std::cout << queue_test1.front() << std::endl;
        std::cout << queue_test1.back()  << std::endl;

        std::cout << queue_test1.front() << " ";
        queue_test1.pop();
        std::cout << queue_test1.front() << " ";
        queue_test1.pop();
        std::cout << queue_test1.front() << " ";
        queue_test1.pop();
    }

    /*            priority_queue测试             */
    {
        std::cout << " priority_queue 测试 " << std::endl << std::endl;
        template_container::queue_adapter::priority_queue<int> priority_queue_test;
        int size = 10000;
        time_t num1 = clock();
        for(int i = 0; i < size ; i++)
        {
            priority_queue_test.push(i);
        }
        time_t num2 = clock();
        std::cout << priority_queue_test.size() << std::endl;
        for(int i = 0; i < size; i++)
        {
            // std::cout << priority_queue_test.top() << " ";
            priority_queue_test.pop();
        }
        std::cout << std::endl;
        std::cout << "priority_queue 测试插入 " << size << " 个数时间： " << num2-num1 << "毫秒" << std::endl;
    }

    /*            binary_search_tree 测试             */
    {
        //栈拷贝1855行
        time_t Binary_search_tree_num1 = clock();
        template_container::tree_container::binary_search_tree<int,template_container::imitation_functions::greater<int>> Binary_search_tree_test;
        for(size_t i = 100; i > 0; i--)
        {
            //相对来说这算是有序插入导致二叉树相乘时间复杂度为O(N)的链表
            Binary_search_tree_test.push(static_cast<int>(i));
        }
        time_t Binary_search_tree_num2 = clock();

        time_t Binary_search_tree_num3 = clock();
        std::cout << Binary_search_tree_test.find(58) << std::endl;
        time_t Binary_search_tree_num4 = clock();
        // Binary_search_tree_test.middle_order_traversal();
        std::cout << "退化链表插入时间" << Binary_search_tree_num2-Binary_search_tree_num1 << std::endl;
        std::cout << "退化链表查找时间" << Binary_search_tree_num4-Binary_search_tree_num3 << std::endl;
        std::cout << std::endl << std::endl;
    }

    {
        template_container::tree_container::binary_search_tree<int, template_container::imitation_functions::greater<int>> bst;
        bst.push(5);
        bst.push(4);
        bst.push(3);
        bst.push(2);
        bst.push(1);
        bst.middle_order_traversal(); 
        std::cout << std::endl << std::endl;
    }

    {
        constexpr size_t Binary_search_tree_arraySize = 10;
        template_container::vector_container::vector<int> Binary_search_tree_array(Binary_search_tree_arraySize);
        for (size_t i = 0; i < Binary_search_tree_arraySize; ++i) 
        {
            Binary_search_tree_array[i] = static_cast<int>(i);
        }

        // 创建随机数引擎和分布
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(Binary_search_tree_array.begin(), Binary_search_tree_array.end(), g);
        //输出打乱后的数组
        // for (auto& i : Binary_search_tree_array)
        // {
        //     std::cout << i << " ";
        // }

        //打乱数组元素顺序
        size_t size = 0;
        time_t Binary_search_tree_num1 = clock();
        template_container::tree_container::binary_search_tree<int,template_container::imitation_functions::greater<int>> Binary_search_tree_test;
        for(const auto& Binary_search_tree_for_test: Binary_search_tree_array)
        {
            if(Binary_search_tree_test.push(Binary_search_tree_for_test))
            {
                size++;
            }
        }
        time_t Binary_search_tree_num2 = clock();

        const int Binary_search_tree_find = Binary_search_tree_array[Binary_search_tree_arraySize/2];

        time_t Binary_search_tree_num3 = clock();
        Binary_search_tree_test.find(Binary_search_tree_find);
        time_t Binary_search_tree_num4 = clock();
        // Binary_search_tree_test.middle_order_traversal();
        std::cout << "插入个数" << size << std::endl;
        std::cout << "插入时间" << Binary_search_tree_num2-Binary_search_tree_num1 << std::endl;
        std::cout << "查找时间" << Binary_search_tree_num4-Binary_search_tree_num3 << std::endl;
        /*              查找数据时间不稳定时间复杂度是O(logN)        */
        std::cout << std::endl << std::endl;
    }

    {
        constexpr const size_t Binary_search_tree_arraySize = 20;
        template_container::vector_container::vector<int> Binary_search_tree_array(Binary_search_tree_arraySize);
        for (size_t i = 0; i < Binary_search_tree_arraySize; ++i) 
        {
            Binary_search_tree_array[i] = static_cast<int>(i);
        }

        // 创建随机数引擎和分布
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(Binary_search_tree_array.begin(), Binary_search_tree_array.end(), g);
        //输出打乱后的数组
        // for(auto& i : Binary_search_tree_array)
        // {
        //     std::cout << i << " ";
        // }

        //打乱数组元素顺序
        size_t size = 0;
        time_t Binary_search_tree_num1 = clock();
        template_container::tree_container::binary_search_tree<int,template_container::imitation_functions::greater<int>> Binary_search_tree_test;
        for(const auto& Binary_search_tree_for_test: Binary_search_tree_array)
        {
            if(Binary_search_tree_test.push(Binary_search_tree_for_test))
            {
                size++;
                std::cout << size << " ";
            }
        }
        std::cout << std::endl;
        time_t Binary_search_tree_num2 = clock();
        template_container::tree_container::binary_search_tree<int,template_container::imitation_functions::greater<int>> Binary_search_tree_test1 = Binary_search_tree_test;
        time_t Binary_search_tree_num3 = clock();
        std::cout << "拷贝构造没问题 " << std::endl;

        Binary_search_tree_test.pop(Binary_search_tree_array[2]);
        std::cout << "pop(1)函数没问题 " << std::endl;
        Binary_search_tree_test.pop(Binary_search_tree_array[0]);
        std::cout << "pop(2)函数没问题 " << std::endl;
        Binary_search_tree_test.pop(Binary_search_tree_array[1]);
        std::cout << "pop(3)函数没问题 " << std::endl;
        Binary_search_tree_test.pop(Binary_search_tree_array[3]);
        std::cout << "pop(4)函数没问题 " << std::endl;


        Binary_search_tree_test.middle_order_traversal();
        std::cout << std::endl;
        Binary_search_tree_test1.middle_order_traversal();

        std::cout << "前序遍历 "<< std::endl;
        Binary_search_tree_test.pre_order_traversal();
        std::cout << std::endl;
        Binary_search_tree_test1.pre_order_traversal();
        std::cout << std::endl;
        std::cout << "插入个数" << size << std::endl;
        std::cout << "插入时间" << Binary_search_tree_num2-Binary_search_tree_num1 << std::endl;
        std::cout << "拷贝时间" << Binary_search_tree_num3-Binary_search_tree_num2 << std::endl;
    }

    {
        template_container::string_container::string str1 = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
        std::cout << str1 << std::endl;
        template_container::vector_container::vector< template_container::string_container::string> vector_str = 
        {"西瓜","樱桃","苹果","西瓜","樱桃","苹果","樱桃","西瓜","樱桃","西瓜","樱桃","苹果","樱桃","苹果","樱桃"};

        template_container::tree_container::binary_search_tree< template_container::string_container::string> BST_temp;
        size_t BST_size = vector_str.size();
        for(size_t i = 0 ; i < BST_size;i++)
        {
            if(BST_temp.push(vector_str[i]))
            {
                std::cout << "插入成功" << std::endl;
            }
            else
            {
                //当前未实现累加功能
                std::cout << "插入失败" << std::endl;
            }
        }
        BST_temp.middle_order_traversal();
        std::cout << BST_temp.size() << std::endl;
    }
    /*            pair类 测试             */
    {
        constexpr const int i = 31;constexpr const int j = 28;
        template_container::practicality::pair<int,int> pair_test =template_container::practicality::make_pair(i,j);
        std::cout << pair_test << std::endl;
    }
    /*            avl_tree 测试             */
    {
        template_container::tree_container::avl_tree <template_container::practicality::pair<int,int>,int> AVL_Tree_test_pair(template_container::practicality::pair(9,0), 10);
        template_container::practicality::pair<template_container::practicality::pair<int,int>,int> pair_test_ (template_container::practicality::pair(9,0), 10);
        template_container::tree_container::avl_tree <template_container::practicality::pair<int,int>,int> AVL_Tree_test(pair_test_);
        //两个构造函数，根据传值调用来查看调用情况
        template_container::tree_container::avl_tree<template_container::string_container::string,int> AVL_Tree_test2;
        AVL_Tree_test2.~avl_tree();
    }
    {
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test_pair;
        template_container::vector_container::vector<template_container::practicality::pair<int,int>> AVL_Tree_array_pair = 
        {{22,0},{16,0},{13,0},{15,0},{11,0},{12,0},{14,0},{10,0},{2,0},{10,0}};
        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.push(i);
        }
        std::cout << "前序遍历 "<< std::endl;
        AVL_Tree_test_pair.pre_order_traversal();
        std::cout << std::endl;
        std::cout << "中序遍历 "<< std::endl;
        AVL_Tree_test_pair.middle_order_traversal();
        std::cout << std::endl;
    }
    {
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test_pair;
        template_container::vector_container::vector<template_container::practicality::pair<int,int>> AVL_Tree_array_pair = 
        {{22,0},{16,0},{13,0},{15,0},{11,0},{12,0},{14,0},{10,0},{2,0},{10,0}};

        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.push(i);
        }
        std::cout << "前序遍历 "<< std::endl;
        AVL_Tree_test_pair.pre_order_traversal();
        std::cout << std::endl;
        std::cout << "中序遍历 "<< std::endl;
        AVL_Tree_test_pair.middle_order_traversal();
        std::cout << std::endl; 
        template_container::tree_container::avl_tree<int,int>AVL_Tree_test_pair1(AVL_Tree_test_pair);
        std::cout << "前序遍历 "<< std::endl;
        AVL_Tree_test_pair1.pre_order_traversal();
        std::cout << std::endl;
        std::cout << "中序遍历 "<< std::endl;
        AVL_Tree_test_pair1.middle_order_traversal();
        std::cout << std::endl; 

        template_container::tree_container::binary_search_tree<char> BS_Tr;
        template_container::string_container::string str1 = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
        for(auto& i :str1)
        {
            BS_Tr.push(i);
        }
        BS_Tr.middle_order_traversal();
        std::cout << std::endl;
        template_container::tree_container::binary_search_tree<char> BS_TREE(BS_Tr);
        BS_TREE.middle_order_traversal();
        std::cout << std::endl;
    }
    {
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test_pair;
        template_container::vector_container::vector<template_container::practicality::pair<int,int>> AVL_Tree_array_pair = 
        {{22,0},{16,0},{13,0},{15,0},{11,0},{12,0},{14,0},{10,0},{2,0},{10,0}};

        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.push(i);
        }
        std::cout << std::endl;
        for ( const auto& i : AVL_Tree_test_pair)
        {
            std::cout << i << " ";
            // 该行会被推断为pair<int,int>类型
            //C++11的新特性,存在疑问？，Map容器是如何推导的？
        }
        std::cout << std::endl;
        for (auto it = AVL_Tree_test_pair.begin(); it != AVL_Tree_test_pair.end(); ++it)
        {
            std::cout << *it << " ";
        }
        std::cout << std::endl;
        for (auto const &p : AVL_Tree_test_pair) 
        {
            std::cout << p << " ";
        }
        
    }
    {
        //删除测试
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test_pair;
        template_container::vector_container::vector<template_container::practicality::pair<int,int>> AVL_Tree_array_pair = 
        {{22,0},{16,0},{13,0},{15,0},{11,0},{12,0},{14,0},{10,0},{2,0},{10,0}};
        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.push(i);
        }
        std::cout << "前序遍历 "<< std::endl;
        AVL_Tree_test_pair.pre_order_traversal();
        std::cout << std::endl;
        std::cout << "开始删除 "<< std::endl;
        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.pop(i.first);
            AVL_Tree_test_pair.pre_order_traversal();
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    {
        //性能测试
        /*                   pair 类型                */
        template_container::tree_container::avl_tree<size_t,int> AVL_Tree_test_pair;
        template_container::vector_container::vector<template_container::practicality::pair<size_t,int>> AVL_Tree_array_pair;
        size_t size = 100000;
        for(size_t i = 0; i < size; i++)
        {
            AVL_Tree_array_pair.push_back(template_container::practicality::pair<size_t,int>(i,0));
        }
        time_t AVL_Tree_num1 = clock();
        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.push(i);
        }
        time_t AVL_Tree_num2 = clock();
        std::cout << "插入个数:" << AVL_Tree_test_pair.size()  << " " << " 插入时间:" << AVL_Tree_num2 - AVL_Tree_num1 << std::endl;

        /*                  非pair 类型               */
        template_container::tree_container::avl_tree<size_t,int> AVL_Tree_test;
        template_container::vector_container::vector<size_t> AVL_Tree_array;
        for(size_t j = 0; j < size ; j++)
        {
            AVL_Tree_array.push_back(j);
        }
        time_t AVL_Tree_num3 = clock();
        for(auto& j : AVL_Tree_array)
        {
            AVL_Tree_test.push(j);
        }
        time_t AVL_Tree_num4 = clock();
        std::cout << "插入个数:" << AVL_Tree_test.size()  << " " << " 插入时间:" << AVL_Tree_num4 - AVL_Tree_num3 << std::endl;
    }
    /*            tree_map 测试             */
    {
        template_container::map_container::tree_map<size_t,size_t> Map_Test;
        size_t size = 10;
        template_container::vector_container::vector<template_container::practicality::pair<size_t,size_t>> arr;
        size_t l = 0;
        for(size_t i = 0 ; i < size; i++ ,l = i)
        {
            arr.push_back(template_container::practicality::pair<size_t,size_t>(i,l));
        }
        std::cout << arr << std::endl;
        for(auto& j : arr)
        {
            Map_Test.push(j);
            std::cout << "前序" << " ";
            Map_Test.pre_order_traversal();
            std::cout << "   " << "中序:"<< Map_Test.size() << " " << Map_Test.empty() << " ";
            Map_Test.middle_order_traversal();
            std::cout << std::endl;
        }
        std::cout << Map_Test.empty() << " ";
        std::cout << "正向" << std::endl;
        for(const auto& tree_map : Map_Test)
        {
            std::cout << tree_map << " ";
        }
        std::cout << "反向" << std::endl;
        for(auto it = Map_Test.crbegin(); it != Map_Test.crend(); it++)
        {
            std::cout << *it << " ";
        }
        for(auto& j : arr)
        {
            Map_Test.pop(j);
            // std::cout << "前序" << " ";
            // Map_Test.pre_order_traversal();
            std::cout << "   " << "中序:"<< Map_Test.size() << " " << Map_Test.empty() << " ";
            Map_Test.middle_order_traversal();
            std::cout << std::endl;
        }
    }
    /*            tree_set 测试             */
    {
        template_container::set_container::tree_set<size_t> Set_test;
        size_t size = 20;
        template_container::vector_container::vector<size_t> arr;
        for(size_t i = 0; i < size; i++ )
        {
            arr.push_back(i);
        }
         std::cout << arr << std::endl;
        for(auto& j : arr)
        {
            Set_test.push(j);
            std::cout << "前序" << " ";
            Set_test.pre_order_traversal();
            std::cout << "   " << "中序" << " ";
            Set_test.middle_order_traversal();
            std::cout << std::endl;
        }
        std::cout << "正向"<< std::endl;
        for(auto& j : Set_test)
        {
            std::cout << j << " ";
        }
        std::cout << std::endl << "反向"<< std::endl;
        for(auto j = Set_test.crbegin(); j != Set_test.crend(); j++)
        {
            std::cout << *j << " ";
        }
    }
    /*            hash_map 测试             */
    {
        template_container::map_container::hash_map<size_t,size_t> unordered_Map_test;
        size_t size = 23;
        template_container::vector_container::vector<template_container::practicality::pair<size_t,size_t>> arr;
        size_t l = 0;
        for(size_t i = 0 ; i < size; i++,l = i)
        {
            arr.push_back(template_container::practicality::pair<size_t,size_t>(i,l));
        }
        for(size_t i = 0; i < size; i++)
        {
            unordered_Map_test.push(arr[i]);
        }
        std::cout << std::endl;

        std::cout << arr << std::endl;
        std::cout << *unordered_Map_test.find(template_container::practicality::pair<size_t,size_t>(20,20)) << std::endl;
        for(size_t i = 0; i < size; i++)
        {
            std::cout << *unordered_Map_test.find(template_container::practicality::pair<size_t,size_t>(i,i)) << " ";
        }
        std::cout << std::endl;
        for(size_t i = 0; i < (size - 10); i++)
        {
            std::cout <<  unordered_Map_test.pop(template_container::practicality::pair<size_t,size_t>(i,i)) <<" ";
        }
        // auto it = unordered_Map_test.begin();//迭代器越界
        // for(size_t i = 0; i < size; i++)
        // {
        //     //
        //     std::cout << *it <<" ";
        //     ++it;
        // }
        std::cout << std::endl;
        // for(auto& i : unordered_Map_test)
        // {
        //     std::cout << i << " ";
        // }
        std::cout << std::endl;
    }
    /*            bloom_filter 测试             */
    {
        template_container::bloom_filter_container::bloom_filter<size_t> BloomFilter_test(3000000000);
        size_t size = 20;
        template_container::vector_container::vector<size_t> arr;
        for(size_t i = 0; i < size; i++ )
        {
            arr.push_back(i);
        }
        std::cout << arr << std::endl;
        for(auto& j : arr)
        {
            BloomFilter_test.set(j);
        }
        for(auto& j : arr)
        {
            std::cout << BloomFilter_test.test(j) << " ";
        }
        std::cout << BloomFilter_test.test(100) << " ";
        std::cout << std::endl;
    }
    {
        std::cout << "AVL_Tree移动构造测试" <<std::endl;
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test_pair;
        template_container::vector_container::vector<template_container::practicality::pair<int,int>> AVL_Tree_array_pair = 
        {{22,0},{16,0},{13,0},{15,0},{11,0},{12,0},{14,0},{10,0},{2,0},{10,0}};

        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.push(i);
        }

        std::cout << "移动前：" ;
        AVL_Tree_test_pair.middle_order_traversal();
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test = std::move(AVL_Tree_test_pair);
        std::cout << std::endl;
        std::cout << "移动构造：";
        AVL_Tree_test.middle_order_traversal();
        std::cout << std::endl;
        std::cout << "移动后：" ;
        AVL_Tree_test_pair.middle_order_traversal();
        //移动构造就是窃取资源
        std::cout << std::endl;
    }
    {
        std::cout << "AVL_Tree拷贝构造测试" <<std::endl;
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test_pair;
        template_container::vector_container::vector<template_container::practicality::pair<int,int>> AVL_Tree_array_pair = 
        {{22,0},{16,0},{13,0},{15,0},{11,0},{12,0},{14,0},{10,0},{2,0},{10,0}};

        for(auto& i : AVL_Tree_array_pair)
        {
            AVL_Tree_test_pair.push(i);
        }

        std::cout << "拷贝前：" ;
        AVL_Tree_test_pair.middle_order_traversal();
        template_container::tree_container::avl_tree<int,int> AVL_Tree_test = AVL_Tree_test_pair;
        std::cout << std::endl;
        std::cout << "移动构造：";
        AVL_Tree_test.middle_order_traversal();
        std::cout << std::endl;
        std::cout << "拷贝后：" ;
        AVL_Tree_test_pair.middle_order_traversal();
        std::cout << std::endl;
    }
    {
        //链式输出轻量化容器
        std::initializer_list<int> init_list = {1,2,3,4,5,6,7,8,9,10};
        std::initializer_list<int>::iterator it = init_list.begin();
        for(;it != init_list.end();it++)
        {
            std::cout << *it << " ";
        }
        std::cout << std::endl;
        for(auto& i : init_list)
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;
    }
    {
        // std::forward<int>(1);
        //完美转发，不会丢失左右值属性
        
    }
    // {
    //     std::atomic<size_t> sum(0);
    //     auto func = [&sum](){sum += 6;};
    //     MY_Template::vector_container::vector<std::thread> array_thread;
    //     array_thread.resize(30);//30个空线程
    //     for(auto& size_thread :array_thread)
    //     {
    //         size_thread = std::thread(func);
    //     }
    //     for(auto& size_join :array_thread)
    //     {
    //         std::cout << "线程：" << size_join.get_id() << " " << std::endl;
    //         size_join.join();
    //     }
    //     std::cout << sum << std::endl;
    // }
    // //问题vector容器resize函数问题啊大大。
    {
        //尝试构建线程池来测试给个容器性能开销
        using Vector_pair =  template_container::vector_container::vector<template_container::practicality::pair<size_t,size_t>>;
        template_container::vector_container::vector<Vector_pair> array_vector;
        size_t size = 20000;
        auto t1 = std::chrono::high_resolution_clock::now();
        for(size_t i = 0; i < size; i++)
        {
            Vector_pair temp;
            for(size_t j = 0; j < size; j++)
            {
                temp.push_back(template_container::practicality::pair<size_t,size_t>(j,j));
            }
            array_vector.push_back(std::move(temp));
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "push_back函数时间：" << std::chrono::duration<double, std::milli>(t2 - t1).count() << std::endl;
    }
    {
        // set::vector<set::pair<set::list,set::queue>> array_list_stack;
        template_container::algorithm::hash_algorithm::hash_function <int,template_container::imitation_functions::hash_imitation_functions> hash;
        int test_size = 792;
        auto t1 = std::chrono::high_resolution_clock::now();
        std::cout << "计算哈希值： " << hash.hash_aphash(test_size)   << std::endl; 
        std::cout << "计算哈希值： " << hash.hash_bkdrhash(test_size) << std::endl;
        std::cout << "计算哈希值： " << hash.hash_pjwhash(test_size)  << std::endl;
        std::cout << "计算哈希值： " << hash.hash_djbhash(test_size)  << std::endl;
        std::cout << "计算哈希值： " << hash.hash_sdmmhash(test_size) << std::endl;
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration<double, std::milli>(t2 - t1).count() << std::endl;
        {
            // collections::bloom_filter<size_t> BloomFilter_test(3000000000);
        }
    }
    {
        system("pause");
    }
    {
        template_container::map_container::tree_map<size_t,size_t> Map_Test = {{1,1},{2,2},{3,3},{4,4},{5,5}};
        Map_Test.middle_order_traversal();
        std::cout << std::endl;
        Map_Test.pop({1,1});
        std::cout << std::endl;
        Map_Test.middle_order_traversal();
    }
    {
        //结构化绑定测试
        template_container::map_container::tree_map<size_t,size_t> test_map;
        test_map.push(template_container::practicality::make_pair(345ull,3472ull));
        auto& [ key , value ] = *test_map.begin();
        std::cout << key << " " << value << std::endl;
    }
    return 0;
}