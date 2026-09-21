#include "hypercache/cache/redis_cache_backend.hpp"
#include <gtest/gtest.h>

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