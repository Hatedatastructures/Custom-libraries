#include "smart_pointers.hpp"
#include "imitation_functions.hpp"
#include "algorithm.hpp"
// #include <print>
using namespace con::smart_pointer;
struct ptr_test
{
    int value = 43;
};
void smart_ptr_test()
{
    con::smart_pointer::smart_ptr<int> sp(new int(10));
    std::cout << *sp << std::endl;
    smart_ptr<ptr_test> sp2(new ptr_test());
    std::cout << sp2->value << std::endl;
}
void unique_ptr_test()
{
    con::smart_pointer::unique_ptr<int>  up(new int(10));
    std::cout << *up << std::endl;
    con::smart_pointer::unique_ptr<char> up2(new char('a'));
    std::cout << *up << std::endl;
    con::smart_pointer::unique_ptr<char> up3(std::move(up2));
    std::cout << *up3 << std::endl;
    std::printf("up3 is %p ", up3.get_ptr());
    con::smart_pointer::unique_ptr<ptr_test> up4(new ptr_test());
    // std::println("hello,C++23!");
}
void shared_ptr_test(con::smart_pointer::shared_ptr<int> sp)
{
    std::cout << *sp << std::endl;
    std::cout << sp.get_count() << std::endl;
    con::smart_pointer::shared_ptr<int> sp2 = sp;
    std::cout << sp.get_count() << std::endl;
    con::smart_pointer::shared_ptr<int> sp3 = std::move(sp2);
    std::cout << sp.get_count() << std::endl;
}
void function_test()
{
    con::imitation_functions::less<int> less_test;
    std::cout << less_test(1, 2) << std::endl;
    con::algorithm::hash_algorithm::hash_function<int> hash_test;
}
int main()
{
    con::customize_exception ex("test","main", __LINE__);
    smart_ptr_test(); 
    unique_ptr_test();
    con::smart_pointer::shared_ptr<int> sp(new int(10));
    shared_ptr_test(sp);
    std::cout << sp.get_count() << std::endl;
    return 0;
}