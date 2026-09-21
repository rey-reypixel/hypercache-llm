#include "hypercache/cache/lru_cache_backend.hpp"
#include <gtest/gtest.h>

TEST(LruCacheBackend, PutGetRemove) {
    hypercache::cache::LruCacheBackend cache(10);
    cache.put("key1", "value1");
    auto result = cache.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value1");

    cache.remove("key1");
    result = cache.get("key1");
    EXPECT_FALSE(result.has_value());
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
    cache.get("key1");  // access key1 to make it recently used
    cache.put("key3", "value3");  // should evict key2
    EXPECT_TRUE(cache.exists("key1"));
    EXPECT_FALSE(cache.exists("key2"));
    EXPECT_TRUE(cache.exists("key3"));
}