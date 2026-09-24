#include "hypercache/cache/lru_cache_backend.hpp"

namespace hypercache::cache {

LruCacheBackend::LruCacheBackend(std::size_t capacity) : cache_(capacity) {}

std::optional<std::string> LruCacheBackend::get(std::string_view key) {
    return cache_.get(std::string(key));
}

void LruCacheBackend::put(std::string_view key, std::string_view value) {
    cache_.put(std::string(key), std::string(value));
}

void LruCacheBackend::remove(std::string_view key) {
    cache_.erase(std::string(key));
}

bool LruCacheBackend::exists(std::string_view key) {
    return cache_.get(std::string(key)).has_value();
}

std::size_t LruCacheBackend::size() {
    return cache_.size();
}

} // namespace hypercache::cache