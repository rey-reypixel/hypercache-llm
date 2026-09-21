#include "hypercache/cache/redis_cache.hpp"
#include <gtest/gtest.h>

TEST(RedisCache, ConnectAndDisconnect) {
    hypercache::cache::RedisCache cache("127.0.0.1", 6379);
    if (cache.connect()) {
        EXPECT_TRUE(cache.is_connected());
        cache.disconnect();
        EXPECT_FALSE(cache.is_connected());
    } else {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }
}

TEST(RedisCache, PutGetRemove) {
    hypercache::cache::RedisCache cache("127.0.0.1", 6379);
    if (!cache.connect()) {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }

    cache.put("test_key", "test_value");
    auto result = cache.get("test_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "test_value");

    cache.remove("test_key");
    result = cache.get("test_key");
    EXPECT_FALSE(result.has_value());
}

TEST(RedisCache, ExistsAndTTL) {
    hypercache::cache::RedisCache cache("127.0.0.1", 6379);
    if (!cache.connect()) {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }

    cache.put("ttl_key", "ttl_value");
    cache.set_ttl("ttl_key", std::chrono::seconds(10));

    EXPECT_TRUE(cache.exists("ttl_key"));
    auto ttl = cache.get_ttl("ttl_key");
    ASSERT_TRUE(ttl.has_value());
    EXPECT_GT(ttl->count(), 0);
    EXPECT_LE(ttl->count(), 10);
}

TEST(RedisCache, MoveSemantics) {
    hypercache::cache::RedisCache cache1("127.0.0.1", 6379);
    if (!cache1.connect()) {
        GTEST_SKIP() << "Redis server not available at 127.0.0.1:6379";
    }

    cache1.put("move_key", "move_value");

    hypercache::cache::RedisCache cache2 = std::move(cache1);
    EXPECT_TRUE(cache2.is_connected());
    EXPECT_FALSE(cache1.is_connected());

    auto result = cache2.get("move_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "move_value");
}