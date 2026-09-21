#include "hypercache/cache/redis_connection_pool.hpp"

#include <hiredis/hiredis.h>
#include <algorithm>
#include <stdexcept>

namespace hypercache::cache {

RedisConnectionPool::RedisConnectionPool(Config config)
    : config_(std::move(config)) {}

RedisConnectionPool::~RedisConnectionPool() {
    stop();
}

RedisConnectionPool::RedisConnectionPool(RedisConnectionPool&& other) noexcept
    : config_(std::move(other.config_)),
      connections_(std::move(other.connections_)),
      created_(other.created_.load()),
      running_(other.running_.load()),
      reaper_(std::move(other.reaper_)) {
    other.running_ = false;
}

RedisConnectionPool& RedisConnectionPool::operator=(RedisConnectionPool&& other) noexcept {
    if (this != &other) {
        stop();
        config_ = std::move(other.config_);
        connections_ = std::move(other.connections_);
        created_ = other.created_.load();
        running_ = other.running_.load();
        reaper_ = std::move(other.reaper_);
        other.running_ = false;
    }
    return *this;
}

void RedisConnectionPool::start() {
    if (running_.exchange(true)) return;
    ensure_min_connections();
    reaper_ = std::thread(&RedisConnectionPool::reaper_loop, this);
}

void RedisConnectionPool::stop() {
    if (!running_.exchange(false)) return;
    if (reaper_.joinable()) reaper_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& conn : connections_) {
        if (conn->context) {
            redisFree(conn->context);
            conn->context = nullptr;
        }
    }
    connections_.clear();
    created_ = 0;
}

redisContext* RedisConnectionPool::create_connection() {
    struct timeval tv;
    tv.tv_sec = static_cast<long>(config_.connect_timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((config_.connect_timeout.count() % 1000) * 1000);

    redisContext* ctx = redisConnectWithTimeout(config_.host.c_str(), config_.port, tv);
    if (!ctx || ctx->err) {
        if (ctx) redisFree(ctx);
        throw std::runtime_error("Failed to connect to Redis");
    }
    return ctx;
}

void RedisConnectionPool::ensure_min_connections() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (created_ < config_.min_connections) {
        auto pooled = std::make_unique<PooledConnection>();
        pooled->context = create_connection();
        pooled->last_used = std::chrono::steady_clock::now();
        connections_.push_back(std::move(pooled));
        ++created_;
    }
}

RedisConnectionPool::ConnectionGuard RedisConnectionPool::acquire() {
    if (!running_) throw std::runtime_error("Connection pool not running");

    const auto deadline = std::chrono::steady_clock::now() + config_.acquire_timeout;
    std::unique_lock<std::mutex> lock(mutex_);

    while (running_) {
        for (auto& pooled : connections_) {
            if (!pooled->in_use && pooled->context) {
                pooled->in_use = true;
                pooled->last_used = std::chrono::steady_clock::now();
                return ConnectionGuard(pooled->context, this);
            }
        }

        if (created_ < config_.max_connections) {
            auto pooled = std::make_unique<PooledConnection>();
            pooled->context = create_connection();
            pooled->last_used = std::chrono::steady_clock::now();
            pooled->in_use = true;
            auto* ctx = pooled->context;
            connections_.push_back(std::move(pooled));
            ++created_;
            return ConnectionGuard(ctx, this);
        }

        if (!cv_.wait_until(lock, deadline, [this] {
            for (const auto& p : connections_) {
                if (!p->in_use) return true;
            }
            return created_ < config_.max_connections;
        })) {
            throw std::runtime_error("Timeout acquiring Redis connection");
        }
    }
    throw std::runtime_error("Connection pool stopped");
}

void RedisConnectionPool::return_connection(redisContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pooled : connections_) {
        if (pooled->context == ctx) {
            pooled->in_use = false;
            pooled->last_used = std::chrono::steady_clock::now();
            cv_.notify_one();
            return;
        }
    }
}

void RedisConnectionPool::reaper_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (!running_) break;

        std::lock_guard<std::mutex> lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        auto it = connections_.begin();
        while (it != connections_.end()) {
            if (!(*it)->in_use &&
                created_ > config_.min_connections &&
                now - (*it)->last_used > config_.idle_timeout) {
                if ((*it)->context) {
                    redisFree((*it)->context);
                }
                it = connections_.erase(it);
                --created_;
            } else {
                ++it;
            }
        }
    }
}

std::size_t RedisConnectionPool::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.size();
}

std::size_t RedisConnectionPool::available() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::count_if(connections_.begin(), connections_.end(),
                         [](const auto& p) { return !p->in_use; });
}

std::size_t RedisConnectionPool::in_use() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::count_if(connections_.begin(), connections_.end(),
                         [](const auto& p) { return p->in_use; });
}

RedisConnectionPool::ConnectionGuard::ConnectionGuard(redisContext* ctx, RedisConnectionPool* pool) noexcept
    : context_(ctx), pool_(pool) {}

RedisConnectionPool::ConnectionGuard::ConnectionGuard(ConnectionGuard&& other) noexcept
    : context_(other.context_), pool_(other.pool_) {
    other.context_ = nullptr;
    other.pool_ = nullptr;
}

RedisConnectionPool::ConnectionGuard& RedisConnectionPool::ConnectionGuard::operator=(ConnectionGuard&& other) noexcept {
    if (this != &other) {
        if (context_ && pool_) pool_->return_connection(context_);
        context_ = other.context_;
        pool_ = other.pool_;
        other.context_ = nullptr;
        other.pool_ = nullptr;
    }
    return *this;
}

RedisConnectionPool::ConnectionGuard::~ConnectionGuard() {
    if (context_ && pool_) pool_->return_connection(context_);
}

} // namespace hypercache::cache