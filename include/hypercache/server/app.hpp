#pragma once

#include <memory>
#include <string>

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