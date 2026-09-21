#include "hypercache/cache/redis_cache_backend.hpp"

#include <hiredis/hiredis.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace hypercache::cache {

struct RedisCacheBackend::Impl {
    RedisConnectionPool pool;
    bool connected = false;

    explicit Impl(Config config) : pool(std::move(config)) {}
};

RedisCacheBackend::RedisCacheBackend(Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

RedisCacheBackend::~RedisCacheBackend() = default;

RedisCacheBackend::RedisCacheBackend(RedisCacheBackend&&) noexcept = default;
RedisCacheBackend& RedisCacheBackend::operator=(RedisCacheBackend&&) noexcept = default;

bool RedisCacheBackend::connect() {
    if (impl_->connected) return true;
    try {
        impl_->pool.start();
        impl_->connected = true;
        return true;
    } catch (...) {
        return false;
    }
}

void RedisCacheBackend::disconnect() {
    if (impl_->connected) {
        impl_->pool.stop();
        impl_->connected = false;
    }
}

bool RedisCacheBackend::is_connected() const noexcept {
    return impl_->connected;
}

std::size_t RedisCacheBackend::pool_size() const noexcept {
    return impl_->pool.size();
}

std::size_t RedisCacheBackend::pool_available() const noexcept {
    return impl_->pool.available();
}

static std::optional<std::string> reply_to_string(redisReply* reply) {
    if (!reply) return std::nullopt;
    if (reply->type == REDIS_REPLY_STRING) {
        return std::string(reply->str, reply->len);
    }
    return std::nullopt;
}

static bool reply_to_bool(redisReply* reply) {
    return reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
}

static std::size_t reply_to_size(redisReply* reply) {
    return (reply && reply->type == REDIS_REPLY_INTEGER) ? static_cast<std::size_t>(reply->integer) : 0;
}

std::optional<std::string> RedisCacheBackend::get(std::string_view key) {
    auto guard = impl_->pool.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommand(guard.get(), "GET %b", key.data(), key.size()));
    auto result = reply_to_string(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

void RedisCacheBackend::put(std::string_view key, std::string_view value) {
    auto guard = impl_->pool.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommand(guard.get(), "SET %b %b", key.data(), key.size(), value.data(), value.size()));
    if (reply) freeReplyObject(reply);
}

void RedisCacheBackend::remove(std::string_view key) {
    auto guard = impl_->pool.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommand(guard.get(), "DEL %b", key.data(), key.size()));
    if (reply) freeReplyObject(reply);
}

bool RedisCacheBackend::exists(std::string_view key) {
    auto guard = impl_->pool.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommand(guard.get(), "EXISTS %b", key.data(), key.size()));
    bool result = reply_to_bool(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

std::size_t RedisCacheBackend::size() {
    auto guard = impl_->pool.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommand(guard.get(), "DBSIZE"));
    std::size_t result = reply_to_size(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

} // namespace hypercache::cache