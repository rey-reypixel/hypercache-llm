#include "hypercache/cache/redis_cache_backend.hpp"

namespace hypercache::cache {

RedisCacheBackend::RedisCacheBackend(std::string_view host, int port)
    : redis_(std::make_unique<RedisCache>(host, port)) {}

std::optional<std::string> RedisCacheBackend::get(std::string_view key) {
    return redis_->get(key);
}

void RedisCacheBackend::put(std::string_view key, std::string_view value) {
    redis_->put(key, value);
}

void RedisCacheBackend::remove(std::string_view key) {
    redis_->remove(key);
}

bool RedisCacheBackend::exists(std::string_view key) {
    return redis_->exists(key);
}

std::size_t RedisCacheBackend::size() {
    return redis_->size();
}

bool RedisCacheBackend::connect() {
    return redis_->connect();
}

void RedisCacheBackend::disconnect() {
    redis_->disconnect();
}

bool RedisCacheBackend::is_connected() const noexcept {
    return redis_->is_connected();
}

} // namespace hypercache::cache