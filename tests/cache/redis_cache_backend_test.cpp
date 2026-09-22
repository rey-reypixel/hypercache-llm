#include "hypercache/cache/redis_cache_backend.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

TEST(RedisCacheBackend, ConnectAndOperations) {
    hypercache::cache::RedisCacheBackend cache("127.0.0.1", 6379);
    if (!cache.connect()) {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }

    EXPECT_TRUE(cache.is_connected());

    cache.put("test_key", "test_value");
    auto result = cache.get("test_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "test_value");

    EXPECT_TRUE(cache.exists("test_key"));
    EXPECT_FALSE(cache.exists("nonexistent"));

    cache.remove("test_key");
    result = cache.get("test_key");
    EXPECT_FALSE(result.has_value());

    cache.disconnect();
    EXPECT_FALSE(cache.is_connected());
}

TEST(RedisCacheBackend, Size) {
    hypercache::cache::RedisCacheBackend cache("127.0.0.1", 6379);
    if (!cache.connect()) {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }

    auto size_before = cache.size();
    cache.put("size_key", "size_value");
    auto size_after = cache.size();
    EXPECT_GT(size_after, size_before);
}

TEST(RedisCacheBackend, ConcurrentReadWrite) {
    hypercache::cache::RedisCacheBackend cache("127.0.0.1", 6379);
    if (!cache.connect()) {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }

    const int num_threads = 8;
    const int ops_per_thread = 100;
    std::atomic<int> successful_gets{0};
    std::atomic<int> successful_puts{0};

    for (int i = 0; i < 50; ++i) {
        cache.put("init_" + std::to_string(i), "val_" + std::to_string(i));
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &successful_gets, &successful_puts, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                int key = i % 50;
                std::string key_str = "init_" + std::to_string(key);

                if (i % 2 == 0) {
                    auto result = cache.get(key_str);
                    if (result.has_value()) successful_gets++;
                } else {
                    cache.put(key_str, "updated_" + std::to_string(t) + "_" + std::to_string(i));
                    successful_puts++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successful_gets.load(), 0);
    EXPECT_EQ(successful_puts.load(), num_threads * ops_per_thread / 2);
}

TEST(RedisCacheBackend, ConnectionPoolStress) {
    hypercache::cache::RedisCacheBackend::Config config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.pool_min = 4;
    config.pool_max = 8;

    hypercache::cache::RedisCacheBackend cache(config);
    if (!cache.connect()) {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }

    const int num_threads = 16;
    const int ops_per_thread = 50;
    std::atomic<int> total_ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &total_ops, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "stress_" + std::to_string(t) + "_" + std::to_string(i);
                cache.put(key, "value_" + std::to_string(i));
                auto result = cache.get(key);
                if (result.has_value()) total_ops++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_ops.load(), num_threads * ops_per_thread);
    EXPECT_GE(cache.pool_size(), 4);
    EXPECT_LE(cache.pool_size(), 8);
}