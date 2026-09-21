#pragma once

#include <memory>
#include <string>
#include <chrono>

namespace hypercache::cache {
class CacheBackend;
}

namespace hypercache::server {

enum class CacheBackendType {
    LRU,
    Redis
};

struct CacheConfig {
    CacheBackendType type = CacheBackendType::LRU;
    std::size_t lru_capacity = 128;
    std::string redis_host = "127.0.0.1";
    int redis_port = 6379;
    std::size_t redis_pool_min = 2;
    std::size_t redis_pool_max = 10;
    std::chrono::milliseconds redis_connect_timeout = std::chrono::seconds(5);
    std::chrono::milliseconds redis_acquire_timeout = std::chrono::seconds(2);
};

class App {
public:
    App(unsigned short port, CacheConfig cache_config = {});
    ~App();

    void run();
    void stop();

private:
    unsigned short port_;
    CacheConfig cache_config_;
    std::unique_ptr<hypercache::cache::CacheBackend> cache_;
    bool running_;
};

} // namespace hypercache::server