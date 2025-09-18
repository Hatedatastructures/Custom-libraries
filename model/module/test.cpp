#include "Thread_pool.hpp"

int main()
{
    auto pool = con::make_lightweight_pool();
    pool->is_rank_empty();
    return 0;
}