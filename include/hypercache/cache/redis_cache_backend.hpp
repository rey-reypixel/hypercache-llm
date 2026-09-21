#pragma once

#include "hypercache/cache/cache_backend.hpp"
#include "hypercache/cache/redis_cache.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace hypercache::cache {

class RedisCacheBackend : public CacheBackend {
public:
    RedisCacheBackend(std::string_view host, int port = 6379);

    std::optional<std::string> get(std::string_view key) override;
    void put(std::string_view key, std::string_view value) override;
    void remove(std::string_view key) override;
    bool exists(std::string_view key) override;
    std::size_t size() override;

    bool connect();
    void disconnect();
    bool is_connected() const noexcept;

private:
    std::unique_ptr<RedisCache> redis_;
};

} // namespace hypercache::cache