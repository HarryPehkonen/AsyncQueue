#include <gtest/gtest.h>
#include "async_queue/async_queue.hpp"
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

// Test blocking behavior of push
TEST_F(AsyncQueueTest, PushBlocking) {
    AsyncQueue<int> bounded_queue(1);  // Queue with capacity 1

    // Fill the queue
    EXPECT_TRUE(bounded_queue.push(1));

    // Start a thread that will try to push to the full queue
    std::atomic<bool> push_completed{false};
    std::thread pusher([&]() {
        bounded_queue.push(2);  // This should block
        push_completed = true;
    });

    // Give the pusher thread time to block
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(push_completed); // Should still be blocked

    // Pop an item to make space
    auto item = bounded_queue.pop();

    // Wait for pusher to complete
    pusher.join();
    EXPECT_TRUE(push_completed);

    // Verify the item was pushed
    item = bounded_queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 2);
}

// Test non-blocking behavior of try_push
TEST_F(AsyncQueueTest, TryPushNonBlocking) {
    AsyncQueue<int> bounded_queue(1);  // Queue with capacity 1

    // Fill the queue
    EXPECT_TRUE(bounded_queue.push(1));

    // try_push should return quickly
    auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(bounded_queue.try_push(2, 100ms));
    auto duration = std::chrono::steady_clock::now() - start;

    // Should have waited approximately 100ms
    EXPECT_GE(duration, 90ms);
    EXPECT_LE(duration, 150ms);  // Give some margin for system scheduling
}

// Test blocking behavior of pop
TEST_F(AsyncQueueTest, PopBlocking) {
    std::atomic<bool> pop_completed{false};
    std::optional<int> popped_value;

    // Start a thread that will try to pop from empty queue
    std::thread popper([&]() {
        popped_value = queue.pop();  // This should block
        pop_completed = true;
    });

    // Give the popper thread time to block
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(pop_completed); // Should still be blocked

    // Push an item
    queue.push(42);

    // Wait for popper to complete
    popper.join();
    EXPECT_TRUE(pop_completed);

    // Verify the correct item was popped
    ASSERT_TRUE(popped_value.has_value());
    EXPECT_EQ(*popped_value, 42);
}

// Test non-blocking behavior of try_pop
TEST_F(AsyncQueueTest, TryPopNonBlocking) {
    // try_pop should return quickly on empty queue
    auto start = std::chrono::steady_clock::now();
    auto result = queue.try_pop(100ms);
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());

    // Should have waited approximately 100ms
    EXPECT_GE(duration, 90ms);
    EXPECT_LE(duration, 150ms);  // Give some margin for system scheduling
}

// Test try_push successful case
TEST_F(AsyncQueueTest, TryPushSuccessful) {
    AsyncQueue<int> bounded_queue(1);

    // Start a thread that will make space after a delay
    std::thread helper([&]() {
        std::this_thread::sleep_for(50ms);
        auto item = bounded_queue.pop();  // Make space
    });

    // Fill the queue
    EXPECT_TRUE(bounded_queue.push(1));

    // This try_push should succeed because the helper thread will make space
    EXPECT_TRUE(bounded_queue.try_push(2, 100ms));

    helper.join();
}

// Test try_pop successful case
TEST_F(AsyncQueueTest, TryPopSuccessful) {
    // Start a thread that will push after a delay
    std::thread helper([&]() {
        std::this_thread::sleep_for(50ms);
        queue.push(42);
    });

    // This try_pop should succeed because the helper thread will push an item
    auto result = queue.try_pop(100ms);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);

    helper.join();
}
