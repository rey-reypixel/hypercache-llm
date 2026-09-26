#include "hypercache/cache/lru_cache_backend.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

TEST(LruCacheBackend, PutGetRemove) {
    hypercache::cache::LruCacheBackend cache(10);
    cache.put("key1", "value1");
    auto result = cache.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value1");

    cache.remove("key1");
    result = cache.get("key1");
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(cache.exists("key1"));
    EXPECT_EQ(cache.size(), 0);
}

TEST(LruCacheBackend, RemoveFreesSlot) {
    hypercache::cache::LruCacheBackend cache(2);
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.remove("key1");
    cache.put("key3", "value3");
    // key2 must survive: removing key1 freed a slot, so nothing gets evicted.
    EXPECT_TRUE(cache.exists("key2"));
    EXPECT_TRUE(cache.exists("key3"));
    EXPECT_EQ(cache.size(), 2);
}

TEST(LruCacheBackend, RemoveMissingKeyIsNoop) {
    hypercache::cache::LruCacheBackend cache(2);
    cache.put("key1", "value1");
    cache.remove("nope");
    EXPECT_EQ(cache.size(), 1);
}

TEST(LruCacheBackend, Exists) {
    hypercache::cache::LruCacheBackend cache(10);
    EXPECT_FALSE(cache.exists("key1"));
    cache.put("key1", "value1");
    EXPECT_TRUE(cache.exists("key1"));
}

TEST(LruCacheBackend, Size) {
    hypercache::cache::LruCacheBackend cache(10);
    EXPECT_EQ(cache.size(), 0);
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    EXPECT_EQ(cache.size(), 2);
}

TEST(LruCacheBackend, Eviction) {
    hypercache::cache::LruCacheBackend cache(2);
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.get("key1");
    cache.put("key3", "value3");
    EXPECT_TRUE(cache.exists("key1"));
    EXPECT_FALSE(cache.exists("key2"));
    EXPECT_TRUE(cache.exists("key3"));
}

TEST(LruCacheBackend, ConcurrentReadWrite) {
    hypercache::cache::LruCacheBackend cache(1000);
    const int num_threads = 8;
    const int ops_per_thread = 1000;
    std::atomic<int> successful_gets{0};
    std::atomic<int> successful_puts{0};

    for (int i = 0; i < 100; ++i) {
        cache.put("init_" + std::to_string(i), "val_" + std::to_string(i));
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &successful_gets, &successful_puts, t, ops_per_thread]() {
            std::mt19937 rng(t + 42);
            std::uniform_int_distribution<int> key_dist(0, 99);
            std::uniform_int_distribution<int> op_dist(0, 1);

            for (int i = 0; i < ops_per_thread; ++i) {
                int key = key_dist(rng);
                std::string key_str = "init_" + std::to_string(key);

                if (op_dist(rng) == 0) {
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

    // Keys are never evicted (100 keys, capacity 1000), so every get must hit.
    EXPECT_EQ(successful_gets.load() + successful_puts.load(), num_threads * ops_per_thread);
    EXPECT_GT(successful_puts.load(), 0);
    EXPECT_EQ(cache.size(), 100);
}

TEST(LruCacheBackend, ConcurrentEvictionUnderLoad) {
    hypercache::cache::LruCacheBackend cache(100);
    const int num_threads = 16;
    const int ops_per_thread = 500;
    std::atomic<int> total_ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &total_ops, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                cache.put(key, "value_" + std::to_string(i));
                auto result = cache.get(key);
                if (result.has_value()) total_ops++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Other threads can evict a key between our put and get, so not every get hits.
    EXPECT_GT(total_ops.load(), 0);
    EXPECT_LE(total_ops.load(), num_threads * ops_per_thread);
    EXPECT_EQ(cache.size(), 100);
}

TEST(LruCacheBackend, HighContentionSameKeys) {
    hypercache::cache::LruCacheBackend cache(10);
    const int num_threads = 16;
    const int ops_per_thread = 1000;
    std::atomic<int> gets{0}, puts{0};

    for (int i = 0; i < 10; ++i) {
        cache.put("hot_" + std::to_string(i), "init");
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &gets, &puts, t, ops_per_thread]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> dist(0, 9);
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "hot_" + std::to_string(dist(rng));
                if (rng() % 2 == 0) {
                    auto result = cache.get(key);
                    if (result.has_value()) gets++;
                } else {
                    cache.put(key, "update_" + std::to_string(t));
                    puts++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(gets.load(), 0);
    EXPECT_GT(puts.load(), 0);
    EXPECT_LE(cache.size(), 10);
}

TEST(LruCacheBackend, StressTestNoCrash) {
    hypercache::cache::LruCacheBackend cache(500);
    const int num_threads = 32;
    const int ops_per_thread = 2000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, t, ops_per_thread]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> key_dist(0, 999);
            for (int i = 0; i < ops_per_thread; ++i) {
                int key = key_dist(rng);
                std::string key_str = "k_" + std::to_string(key);
                if (rng() % 3 == 0) {
                    cache.get(key_str);
                } else if (rng() % 3 == 1) {
                    cache.put(key_str, "v_" + std::to_string(i));
                } else {
                    cache.remove(key_str);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_LE(cache.size(), 500);
}