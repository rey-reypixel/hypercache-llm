#pragma once

#include "hypercache/cache/cache_backend.hpp"
#include "hypercache/cache/redis_connection_pool.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace hypercache::cache {

class RedisCacheBackend : public CacheBackend {
public:
    struct Config {
        std::string host = "127.0.0.1";
        int port = 6379;
        std::size_t pool_min = 2;
        std::size_t pool_max = 10;
        std::chrono::milliseconds connect_timeout = std::chrono::seconds(5);
        std::chrono::milliseconds acquire_timeout = std::chrono::seconds(2);
    };

    RedisCacheBackend();
    explicit RedisCacheBackend(Config config);
    RedisCacheBackend(std::string host, int port);
    ~RedisCacheBackend();

    RedisCacheBackend(const RedisCacheBackend&) = delete;
    RedisCacheBackend& operator=(const RedisCacheBackend&) = delete;
    RedisCacheBackend(RedisCacheBackend&&) noexcept;
    RedisCacheBackend& operator=(RedisCacheBackend&&) noexcept;

    std::optional<std::string> get(std::string_view key) override;
    void put(std::string_view key, std::string_view value) override;
    void remove(std::string_view key) override;
    bool exists(std::string_view key) override;
    std::size_t size() override;

    bool connect();
    void disconnect();
    bool is_connected() const noexcept;

    std::size_t pool_size() const noexcept;
    std::size_t pool_available() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hypercache::cache