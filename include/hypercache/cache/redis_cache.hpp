#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace hypercache::cache {

class RedisCache {
public:
    explicit RedisCache(std::string_view host, int port = 6379,
                         std::chrono::milliseconds connect_timeout = std::chrono::seconds(5));
    ~RedisCache();

    RedisCache(const RedisCache&) = delete;
    RedisCache& operator=(const RedisCache&) = delete;
    RedisCache(RedisCache&&) noexcept;
    RedisCache& operator=(RedisCache&&) noexcept;

    bool connect();
    void disconnect();
    bool is_connected() const noexcept;

    std::optional<std::string> get(std::string_view key);
    void put(std::string_view key, std::string_view value);
    void remove(std::string_view key);
    bool exists(std::string_view key);

    void set_ttl(std::string_view key, std::chrono::seconds ttl);
    std::optional<std::chrono::seconds> get_ttl(std::string_view key);

    std::size_t size();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hypercache::cache