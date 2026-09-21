#pragma once

#include "hypercache/cache/redis_cache.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace hypercache::cache {

class RedisConnectionPool {
public:
    struct Config {
        std::string host = "127.0.0.1";
        int port = 6379;
        std::size_t min_connections = 2;
        std::size_t max_connections = 10;
        std::chrono::milliseconds connect_timeout = std::chrono::seconds(5);
        std::chrono::milliseconds acquire_timeout = std::chrono::seconds(2);
        std::chrono::seconds idle_timeout = std::chrono::seconds(60);
    };

    explicit RedisConnectionPool(Config config);
    ~RedisConnectionPool();

    RedisConnectionPool(const RedisConnectionPool&) = delete;
    RedisConnectionPool& operator=(const RedisConnectionPool&) = delete;
    RedisConnectionPool(RedisConnectionPool&&) noexcept;
    RedisConnectionPool& operator=(RedisConnectionPool&&) noexcept;

    void start();
    void stop();

    class ConnectionGuard {
    public:
        ConnectionGuard() = default;
        ConnectionGuard(ConnectionGuard&&) noexcept;
        ConnectionGuard& operator=(ConnectionGuard&&) noexcept;
        ~ConnectionGuard();

        redisContext* operator->() const noexcept { return context_; }
        redisContext* get() const noexcept { return context_; }
        explicit operator bool() const noexcept { return context_ != nullptr; }

    private:
        friend class RedisConnectionPool;
        ConnectionGuard(redisContext* ctx, RedisConnectionPool* pool) noexcept;
        redisContext* context_ = nullptr;
        RedisConnectionPool* pool_ = nullptr;
    };

    ConnectionGuard acquire();

    std::size_t size() const noexcept;
    std::size_t available() const noexcept;
    std::size_t in_use() const noexcept;

private:
    struct PooledConnection {
        redisContext* context = nullptr;
        std::chrono::steady_clock::time_point last_used;
        bool in_use = false;
    };

    Config config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::unique_ptr<PooledConnection>> connections_;
    std::size_t created_ = 0;
    std::atomic<bool> running_{false};
    std::thread reaper_;

    redisContext* create_connection();
    void return_connection(redisContext* ctx);
    void reaper_loop();
    void ensure_min_connections();
};

} // namespace hypercache::cache