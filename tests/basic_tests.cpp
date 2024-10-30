#include <gtest/gtest.h>
#include "async_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace async_queue;
using namespace std::chrono_literals;

class AsyncQueueTest : public ::testing::Test {
protected:
    AsyncQueue<int> queue;
    AsyncQueue<int> bounded_queue{2}; // Queue with capacity 2
};

// Basic Operations Tests
TEST_F(AsyncQueueTest, PushPopBasic) {
    EXPECT_TRUE(queue.push(42));
    EXPECT_EQ(queue.size(), 1);
    
    auto result = queue.pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
    EXPECT_TRUE(queue.empty());
}

TEST_F(AsyncQueueTest, CapacityRespected) {
    EXPECT_TRUE(bounded_queue.push(1));
    EXPECT_TRUE(bounded_queue.push(2));
    
    // This should timeout as queue is full
    EXPECT_FALSE(bounded_queue.try_push(3, 100ms));
    EXPECT_EQ(bounded_queue.size(), 2);
}

// Timeout Tests
TEST_F(AsyncQueueTest, PushPopTimeout) {
    EXPECT_TRUE(queue.try_push(1, 100ms));
    EXPECT_EQ(queue.size(), 1);
    
    auto result = queue.try_pop(100ms);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
    
    // Should timeout on empty queue
    result = queue.try_pop(100ms);
    EXPECT_FALSE(result.has_value());
}

// Closing Behavior Tests
TEST_F(AsyncQueueTest, CloseBehavior) {
    queue.push(1);
    queue.close();
    
    EXPECT_TRUE(queue.is_closed());
    EXPECT_FALSE(queue.push(2)); // Should fail after closing
    
    auto result = queue.pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
    
    result = queue.pop();
    EXPECT_FALSE(result.has_value()); // Should return nullopt after emptying closed queue
}

// Concurrent Operation Tests
TEST_F(AsyncQueueTest, ConcurrentOperations) {
    constexpr int NUM_PRODUCERS = 3;
    constexpr int NUM_CONSUMERS = 2;
    constexpr int ITEMS_PER_PRODUCER = 1000;
    
    std::atomic<int> items_consumed{0};
    std::atomic<int> sum_consumed{0};
    
    // Start producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                queue.push(i * ITEMS_PER_PRODUCER + j);
            }
        });
    }
    
    // Start consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back([&]() {
            while (items_consumed < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
                auto item = queue.try_pop(100ms);
                if (item) {
                    items_consumed++;
                    sum_consumed += *item;
                }
            }
        });
    }
    
    // Join all threads
    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();
    
    // Verify results
    EXPECT_EQ(items_consumed, NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    
    // Calculate expected sum
    int expected_sum = 0;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
            expected_sum += i * ITEMS_PER_PRODUCER + j;
        }
    }
    EXPECT_EQ(sum_consumed, expected_sum);
}

// Move Semantics Tests
TEST_F(AsyncQueueTest, MoveSemantics) {
    queue.push(1);
    queue.push(2);
    
    AsyncQueue<int> moved_queue = std::move(queue);
    EXPECT_EQ(moved_queue.size(), 2);
    
    auto item = moved_queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 1);
}
