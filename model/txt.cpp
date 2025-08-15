#include "Concurrent_unordered_map.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <random>

// 测试配置
const int THREAD_COUNT = 16;         // 线程数量
const int OPERATIONS_PER_THREAD = 100000;  // 每个线程的操作次数
const int KEY_RANGE = 1000000;         // 键的范围

// 随机数生成器
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> key_dist(0, KEY_RANGE - 1);
std::uniform_int_distribution<int> op_dist(0, 3);  // 0:插入, 1:查找, 2:修改, 3:删除

// 全局计数器，用于验证操作的原子性
std::atomic<int> total_inserts(0);
std::atomic<int> total_updates(0);
std::atomic<int> total_deletes(0);
std::atomic<int> total_lookups(0);

// 线程函数：执行随机操作
void test_worker(con::concurrent_unordered_map<int, int>& map) {
    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        int key = key_dist(gen);
        int op = op_dist(gen);
        
        switch (op) {
            case 0: {  // 插入操作
                auto [it, inserted] = map.emplace(key, key * 10);
                if (inserted) {
                    total_inserts++;
                }
                break;
            }
            case 1: {  // 查找操作
                auto it = map.find(key);
                if (it != map.end()) {
                    // 验证值是否正确
                    assert(it->second == key * 10);
                }
                total_lookups++;
                break;
            }
            case 2: {  // 修改操作
                if (map.contains(key)) {
                    // 先查找再修改（哈希表不支持直接修改键）
                    map.erase(key);
                    map.emplace(key, key * 10);
                    total_updates++;
                }
                break;
            }
            case 3: {  // 删除操作
                size_t deleted = map.erase(key);
                if (deleted > 0) {
                    total_deletes++;
                }
                break;
            }
        }
    }
}

// 性能测试：批量插入
void performance_test_insert(con::concurrent_unordered_map<int, int>& map, int count) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < count; ++i) {
        map.emplace(i, i * 10);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    std::cout << "批量插入 " << count << " 个元素耗时: " << diff.count() << " 秒" << std::endl;
    std::cout << "插入性能: " << count / diff.count() << " 个/秒" << std::endl;
}

// 性能测试：批量查找
void performance_test_lookup(con::concurrent_unordered_map<int, int>& map, int count) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < count; ++i) {
        map.find(i % KEY_RANGE);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    std::cout << "批量查找 " << count << " 次耗时: " << diff.count() << " 秒" << std::endl;
    std::cout << "查找性能: " << count / diff.count() << " 次/秒" << std::endl;
}

int main() {
    con::concurrent_unordered_map<int, int> map;
    
    std::cout << "=== 开始并发操作测试 ===" << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 创建多个测试线程
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back(test_worker, std::ref(map));
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_time = end_time - start_time;
    
    // 输出测试统计
    std::cout << "=== 测试统计 ===" << std::endl;
    std::cout << "总线程数: " << THREAD_COUNT << std::endl;
    std::cout << "总操作次数: " << THREAD_COUNT * OPERATIONS_PER_THREAD << std::endl;
    std::cout << "插入次数: " << total_inserts << std::endl;
    std::cout << "查找次数: " << total_lookups << std::endl;
    std::cout << "修改次数: " << total_updates << std::endl;
    std::cout << "删除次数: " << total_deletes << std::endl;
    std::cout << "最终元素数量: " << map.size() << std::endl;
    std::cout << "总耗时: " << total_time.count() << " 秒" << std::endl;
    std::cout << "平均每秒操作数: " << (THREAD_COUNT * OPERATIONS_PER_THREAD) / total_time.count() << std::endl;
    
    // 验证最终数据的一致性
    std::cout << "\n=== 数据一致性验证 ===" << std::endl;
    auto snapshot = map.snapshot();
    bool data_valid = true;
    
    for (const auto& pair : snapshot) {
        if (pair.second != pair.first * 10) {
            std::cout << "数据不一致: 键 " << pair.first << " 的值应为 " 
                      << pair.first * 10 << "，实际为 " << pair.second << std::endl;
            data_valid = false;
        }
    }
    
    if (data_valid) {
        std::cout << "数据验证通过，所有键值对均符合预期" << std::endl;
    }
    
    // 性能测试
    std::cout << "\n=== 性能测试 ===" << std::endl;
    performance_test_insert(map, 100000);
    performance_test_lookup(map, 1000000);
    
    return 0;
}
