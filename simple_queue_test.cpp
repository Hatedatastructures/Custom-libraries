#include "model/atomic_concurrent/Atomic_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

using namespace atomic_concurrent;

int main() {
    std::cout << "Testing basic atomic_queue functionality..." << std::endl;
    
    atomic_queue<int> q;
    
    // 基本测试
    std::cout << "1. Basic push/pop test..." << std::endl;
    q.push(1);
    q.push(2);
    q.push(3);
    
    int value;
    if (q.try_pop(value)) {
        std::cout << "Popped: " << value << std::endl;
    }
    if (q.try_pop(value)) {
        std::cout << "Popped: " << value << std::endl;
    }
    if (q.try_pop(value)) {
        std::cout << "Popped: " << value << std::endl;
    }
    
    std::cout << "Queue size: " << q.size() << std::endl;
    
    // 简单的多线程测试
    std::cout << "2. Simple multi-thread test..." << std::endl;
    
    std::atomic<int> produced(0);
    std::atomic<int> consumed(0);
    
    // 生产者线程
    std::thread producer([&q, &produced]() {
        for (int i = 0; i < 100; ++i) {
            q.push(i);
            produced.fetch_add(1);
        }
        std::cout << "Producer finished" << std::endl;
    });
    
    // 消费者线程
    std::thread consumer([&q, &consumed]() {
        int value;
        int count = 0;
        while (count < 100) {
            if (q.try_pop(value)) {
                consumed.fetch_add(1);
                count++;
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        std::cout << "Consumer finished" << std::endl;
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "Produced: " << produced.load() << ", Consumed: " << consumed.load() << std::endl;
    std::cout << "Final queue size: " << q.size() << std::endl;
    
    if (produced.load() == consumed.load() && q.size() == 0) {
        std::cout << "Simple test PASSED!" << std::endl;
    } else {
        std::cout << "Simple test FAILED!" << std::endl;
        return 1;
    }
    
    return 0;
}