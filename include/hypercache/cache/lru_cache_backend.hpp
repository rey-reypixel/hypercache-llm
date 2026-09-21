#pragma once

#include "hypercache/cache/cache_backend.hpp"
#include "hypercache/cache/lru_cache.hpp"

#include <string>
#include <string_view>

namespace hypercache::cache {

class LruCacheBackend : public CacheBackend {
public:
    explicit LruCacheBackend(std::size_t capacity = 128);

    std::optional<std::string> get(std::string_view key) override;
    void put(std::string_view key, std::string_view value) override;
    void remove(std::string_view key) override;
    bool exists(std::string_view key) override;
    std::size_t size() override;

private:
    LruCache<std::string, std::string> cache_;
};

} // namespace hypercache::cache