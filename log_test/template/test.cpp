#include "./Smart_pointers.hpp"
#include "./Imitation_functions.hpp"
#include "./Algorithm.hpp"
#include "./Practicality.hpp"
#include "./String.hpp"
#include "./Vector.hpp"
#include "./List.hpp"
#include "./Stack.hpp"
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
void weak_ptr_test()
{
    con::smart_pointer::shared_ptr<ptr_test> sh (new ptr_test());
    con::smart_pointer::weak_ptr<ptr_test> wp(sh);
    std::cout << wp->value << std::endl;
    con::smart_pointer::weak_ptr<ptr_test> wp2 = wp;
    std::cout << wp2->value << std::endl;
    con::smart_pointer::weak_ptr<ptr_test> wp3(std::move(wp2));
    std::cout << wp3->value << std::endl;
}
void function_test()
{
    con::imitation_functions::less<int> less_test;
    std::cout << less_test(1, 2) << std::endl;
    // con::algorithm::hash_algorithm::hash_function<int> hash_test;
    std::string str = "hello";
    std::string res;
    std::cout << con::algorithm::copy(str.begin(), str.end(),res.begin()) << std::endl; 
    con::pair<int, int> p(1, 2);
    std::cout << p.first << " " << p.second << std::endl;
    std::cout << con::make_pair<int, int>(1, 2) << std::endl;
}
void vector_test()
{
    con::vector<int> v(10, 1);
    std::cout << v.size() << std::endl;
}
void list_test()
{
    con::list<int> l{1,2,3,4};
    std::cout << l.size() << std::endl;
}
void stack_test()
{
    con::stack<int> s;
    s.push(1);
    s.push(2);
    s.push(3);
    std::cout << s.top() << std::endl;
    s.pop();
    std::cout << s.top() << std::endl;
}
int main()
{
    try
    {
        throw con::customize_exception("test","main", __LINE__);
    }
    catch(const con::customize_exception& exc)
    {
        std::cerr << exc.what() << std::endl;
    }
    smart_ptr_test(); 
    unique_ptr_test();
    con::smart_pointer::shared_ptr<int> sp(new int(10));
    shared_ptr_test(sp);
    std::cout << sp.get_count() << std::endl;
    function_test();
    con::pair<int, int> p(1, 2);
    con::string str = "hello";
    // weak_ptr_test();
    con::string str2 = "world"; 
    std::cout << str + str2 << std::endl;   
    vector_test();   
    list_test(); 
    std::cout << std::endl;
    stack_test();
    return 0;
}