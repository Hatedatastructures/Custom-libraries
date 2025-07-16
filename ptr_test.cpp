#include "./template_container.hpp"
#include <iostream>

using namespace con;
using namespace con::smart_pointer;
vector<unique_ptr<int>*> v1;
void test()
{
    unique_ptr<int> p1(new int(10));
    unique_ptr<int> p2(std::move(p1));
    v1.push_back(&p1);
    v1.push_back(&p2);
    // std::cout <<  *p1  << std::endl;
    std::cout <<  *p2  << std::endl;
    //std::cout <<  *v1[0]  << std::endl;
    //std::cout <<  *v1[1]  << std::endl;
}
int main()
{
    std::cout << v1.size() << std::endl;
    test();
    if(v1[0] == nullptr)
    {
        std::cout << v1.size() << std::endl;
        std::cout << "nullptr" << std::endl;
        // std::cout << v1[0]->return_ptr() << std::endl;
    }
    std::cout << v1.size() << std::endl;
    con::hash_function<int,con::hash_imitation_functions> hf;
    std::cout << hf.hash_djbhash(328746) << std::endl;
    con::make_pair<int,int>(1,2);
    // std::cout << v1[0]->return_ptr() << std::endl;
    return 0;
}