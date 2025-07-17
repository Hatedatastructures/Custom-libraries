#include "smart_pointers.hpp"
#include <print>
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
int main()
{
    con::customize_exception ex("test","main", __LINE__);
    smart_ptr_test(); 
    unique_ptr_test();
    return 0;
}