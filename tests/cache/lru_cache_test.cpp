#include "hypercache/cache/lru_cache.hpp"
#include <gtest/gtest.h>

TEST(LruCache, EvictsLeastRecentlyUsedEntry) {
    hypercache::cache::LruCache<int, int> cache(2);
    cache.put(1, 10); cache.put(2, 20);
    ASSERT_TRUE(cache.get(1).has_value());
    cache.put(3, 30);
    EXPECT_FALSE(cache.get(2).has_value());
    EXPECT_EQ(cache.get(1), 10);
    EXPECT_EQ(cache.get(3), 30);
}

TEST(LruCache, ZeroCapacityStoresNothing) {
    hypercache::cache::LruCache<int, int> cache(0);
    cache.put(1, 10);
    EXPECT_FALSE(cache.get(1).has_value());
}
