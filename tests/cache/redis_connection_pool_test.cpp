#include "hypercache/cache/redis_connection_pool.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(RedisConnectionPool, StartStop) {
    hypercache::cache::RedisConnectionPool::Config config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.min_connections = 2;
    config.max_connections = 5;

    hypercache::cache::RedisConnectionPool pool(config);
    pool.start();

    EXPECT_GE(pool.size(), 2);
    EXPECT_GE(pool.available(), 2);

    pool.stop();
    EXPECT_EQ(pool.size(), 0);
}

TEST(RedisConnectionPool, AcquireRelease) {
    hypercache::cache::RedisConnectionPool::Config config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.min_connections = 1;
    config.max_connections = 3;

    hypercache::cache::RedisConnectionPool pool(config);
    pool.start();

    {
        auto conn = pool.acquire();
        EXPECT_TRUE(conn);
        EXPECT_EQ(pool.in_use(), 1);
        EXPECT_EQ(pool.available(), pool.size() - 1);

        redisReply* reply = static_cast<redisReply*>(redisCommand(conn.get(), "PING"));
        EXPECT_TRUE(reply != nullptr);
        if (reply) freeReplyObject(reply);
    }

    EXPECT_EQ(pool.in_use(), 0);
    EXPECT_EQ(pool.available(), pool.size());

    pool.stop();
}

TEST(RedisConnectionPool, ConcurrentAccess) {
    hypercache::cache::RedisConnectionPool::Config config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.min_connections = 2;
    config.max_connections = 5;

    hypercache::cache::RedisConnectionPool pool(config);
    pool.start();

    const int num_threads = 10;
    const int ops_per_thread = 20;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&pool]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                auto conn = pool.acquire();
                EXPECT_TRUE(conn);
                redisReply* reply = static_cast<redisReply*>(redisCommand(conn.get(), "PING"));
                if (reply) freeReplyObject(reply);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(pool.in_use(), 0);
    pool.stop();
}

TEST(RedisConnectionPool, PoolExhaustion) {
    hypercache::cache::RedisConnectionPool::Config config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.min_connections = 1;
    config.max_connections = 2;
    config.acquire_timeout = std::chrono::milliseconds(100);

    hypercache::cache::RedisConnectionPool pool(config);
    pool.start();

    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();
    EXPECT_TRUE(conn1);
    EXPECT_TRUE(conn2);

    EXPECT_THROW(pool.acquire(), std::runtime_error);

    pool.stop();
}