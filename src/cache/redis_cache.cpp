#include "hypercache/cache/redis_cache.hpp"

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace hypercache::cache {

struct RedisCache::Impl {
    std::string host;
    int port;
    std::chrono::milliseconds connect_timeout;
    redisContext* context = nullptr;

    Impl(std::string_view h, int p, std::chrono::milliseconds timeout)
        : host(h), port(p), connect_timeout(timeout) {}

    ~Impl() {
        if (context) {
            redisFree(context);
        }
    }

    bool connect() {
        struct timeval tv;
        tv.tv_sec = static_cast<long>(connect_timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((connect_timeout.count() % 1000) * 1000);

        context = redisConnectWithTimeout(host.c_str(), port, tv);
        if (!context || context->err) {
            if (context) {
                redisFree(context);
                context = nullptr;
            }
            return false;
        }
        return true;
    }

    void disconnect() {
        if (context) {
            redisFree(context);
            context = nullptr;
        }
    }

    bool is_connected() const noexcept {
        return context != nullptr && context->err == 0;
    }

    redisReply* command(const char* format, ...) {
        if (!context) return nullptr;
        va_list args;
        va_start(args, format);
        redisReply* reply = static_cast<redisReply*>(redisvCommand(context, format, args));
        va_end(args);
        return reply;
    }
};

RedisCache::RedisCache(std::string_view host, int port,
                        std::chrono::milliseconds connect_timeout)
    : impl_(std::make_unique<Impl>(host, port, connect_timeout)) {}

RedisCache::~RedisCache() = default;

RedisCache::RedisCache(RedisCache&& other) noexcept = default;
RedisCache& RedisCache::operator=(RedisCache&& other) noexcept = default;

bool RedisCache::connect() {
    return impl_->connect();
}

void RedisCache::disconnect() {
    impl_->disconnect();
}

bool RedisCache::is_connected() const noexcept {
    return impl_->is_connected();
}

std::optional<std::string> RedisCache::get(std::string_view key) {
    redisReply* reply = impl_->command("GET %b", key.data(), key.size());
    if (!reply) return std::nullopt;

    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result.emplace(reply->str, reply->len);
    }
    freeReplyObject(reply);
    return result;
}

void RedisCache::put(std::string_view key, std::string_view value) {
    redisReply* reply = impl_->command("SET %b %b", key.data(), key.size(), value.data(), value.size());
    if (reply) freeReplyObject(reply);
}

void RedisCache::remove(std::string_view key) {
    redisReply* reply = impl_->command("DEL %b", key.data(), key.size());
    if (reply) freeReplyObject(reply);
}

bool RedisCache::exists(std::string_view key) {
    redisReply* reply = impl_->command("EXISTS %b", key.data(), key.size());
    if (!reply) return false;
    bool result = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return result;
}

void RedisCache::set_ttl(std::string_view key, std::chrono::seconds ttl) {
    redisReply* reply = impl_->command("EXPIRE %b %lld", key.data(), key.size(), static_cast<long long>(ttl.count()));
    if (reply) freeReplyObject(reply);
}

std::optional<std::chrono::seconds> RedisCache::get_ttl(std::string_view key) {
    redisReply* reply = impl_->command("TTL %b", key.data(), key.size());
    if (!reply) return std::nullopt;

    std::optional<std::chrono::seconds> result;
    if (reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0) {
        result = std::chrono::seconds(reply->integer);
    }
    freeReplyObject(reply);
    return result;
}

std::size_t RedisCache::size() {
    redisReply* reply = impl_->command("DBSIZE");
    if (!reply) return 0;
    std::size_t result = 0;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = static_cast<std::size_t>(reply->integer);
    }
    freeReplyObject(reply);
    return result;
}

} // namespace hypercache::cache