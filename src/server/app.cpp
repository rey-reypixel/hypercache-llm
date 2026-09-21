#include "hypercache/server/app.hpp"
#include "hypercache/cache/lru_cache_backend.hpp"
#include "hypercache/telemetry/telemetry.hpp"
#include "hypercache/similarity/similarity_engine.hpp"

#include "httplib.h"

#include <string>
#include <sstream>
#include <functional>
#include <memory>

#ifdef HYPERCACHE_BUILD_REDIS
#include "hypercache/cache/redis_cache_backend.hpp"
#endif

namespace hypercache::server {

namespace {

auto& get_telemetry() {
    return hypercache::telemetry::TokenTelemetry::instance();
}

std::string json_telemetry() {
    std::ostringstream oss;
    oss << "{\"request_tokens\":" << get_telemetry().get_request_tokens()
        << ",\"streaming_tokens\":" << get_telemetry().get_streaming_tokens()
        << ",\"total_requests\":" << get_telemetry().get_total_requests()
        << "}";
    return oss.str();
}

std::string json_similarity(float sim) {
    std::ostringstream oss;
    oss << "{\"similarity\":" << sim << "}";
    return oss.str();
}

std::vector<float> parse_float_array(const std::string& json_str, const std::string& key) {
    std::vector<float> result;
    size_t pos = json_str.find("\"" + key + "\"");
    if (pos == std::string::npos) return result;
    pos = json_str.find('[', pos);
    if (pos == std::string::npos) return result;
    size_t end = json_str.find(']', pos);
    std::string arr = json_str.substr(pos + 1, end - pos - 1);
    std::istringstream iss(arr);
    std::string token;
    while (std::getline(iss, token, ',')) {
        token.erase(0, token.find_first_of("0123456789-."));
        token.erase(token.find_last_of("0123456789-.\"") + 1);
        if (!token.empty()) {
            result.push_back(std::stof(token));
        }
    }
    return result;
}

} // namespace

App::App(unsigned short port, CacheConfig cache_config)
    : port_(port), cache_config_(std::move(cache_config)), running_(false) {

    switch (cache_config_.type) {
        case CacheBackendType::LRU:
            cache_ = std::make_unique<hypercache::cache::LruCacheBackend>(cache_config_.lru_capacity);
            break;
        case CacheBackendType::Redis:
#ifdef HYPERCACHE_BUILD_REDIS
            cache_ = std::make_unique<hypercache::cache::RedisCacheBackend>(
                cache_config_.redis_host, cache_config_.redis_port);
            if (!cache_->connect()) {
                throw std::runtime_error("Failed to connect to Redis at " +
                    cache_config_.redis_host + ":" + std::to_string(cache_config_.redis_port));
            }
#else
            throw std::runtime_error("Redis backend not available: rebuild with -DHYPERCACHE_BUILD_REDIS=ON");
#endif
            break;
    }
}

App::~App() { stop(); }

void App::run() {
    running_ = true;
    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok","uptime":"running"})", "application/json");
    });

    svr.Get("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        auto result = cache_->get(req.matches[1]);
        get_telemetry().record_request(1);
        if (result.has_value()) {
            res.set_content(result.value(), "text/plain");
        } else {
            res.status = 404;
            res.set_content("Key not found", "text/plain");
        }
    });

    svr.Put("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        cache_->put(req.matches[1], req.body);
        get_telemetry().record_request(1);
        res.set_content("Cached", "text/plain");
    });

    svr.Delete("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        cache_->remove(req.matches[1]);
        get_telemetry().record_request(1);
        res.set_content("Removed", "text/plain");
    });

    svr.Get("/cache", [&](const httplib::Request&, httplib::Response& res) {
        get_telemetry().record_request(1);
        std::ostringstream oss;
        oss << "{\"size\":" << cache_->size() << "}";
        res.set_content(oss.str(), "application/json");
    });

    svr.Get("/telemetry", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(json_telemetry(), "application/json");
    });

    svr.Post("/similarity", [&](const httplib::Request& req, httplib::Response& res) {
        get_telemetry().record_request(1);
        try {
            auto lhs = parse_float_array(req.body, "lhs");
            auto rhs = parse_float_array(req.body, "rhs");
            if (lhs.empty() || rhs.empty() || lhs.size() != rhs.size()) {
                res.status = 400;
                res.set_content("Invalid vectors", "text/plain");
                return;
            }
            float sim = hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs);
            res.set_content(json_similarity(sim), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr.Get("/stream", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_chunked_content_provider(
            "text/event-stream",
            [&](size_t, httplib::DataSink& sink) -> bool {
                std::string d1 = "data: streaming started\n\n";
                std::string d2 = "data: token delta\n\n";
                std::string d3 = "data: streaming complete\n\n";
                sink.write(d1.data(), d1.size());
                get_telemetry().record_streaming(1);
                sink.write(d2.data(), d2.size());
                sink.write(d3.data(), d3.size());
                return true;
            });
    });

    svr.listen("0.0.0.0", port_);
}

void App::stop() {
    running_ = false;
}

} // namespace hypercache::server

int main(int argc, char* argv[]) {
    unsigned short port = 18080;
    hypercache::server::CacheConfig cache_config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<unsigned short>(std::stoi(argv[++i]));
        } else if (arg == "--cache" && i + 1 < argc) {
            std::string cache_type = argv[++i];
            if (cache_type == "redis") {
                cache_config.type = hypercache::server::CacheBackendType::Redis;
            } else if (cache_type == "lru") {
                cache_config.type = hypercache::server::CacheBackendType::LRU;
            }
        } else if (arg == "--redis-host" && i + 1 < argc) {
            cache_config.redis_host = argv[++i];
        } else if (arg == "--redis-port" && i + 1 < argc) {
            cache_config.redis_port = std::stoi(argv[++i]);
        } else if (arg == "--lru-capacity" && i + 1 < argc) {
            cache_config.lru_capacity = std::stoull(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: hypercache_server [options]\n"
                      << "Options:\n"
                      << "  --port PORT           Server port (default: 18080)\n"
                      << "  --cache TYPE          Cache backend: lru|redis (default: lru)\n"
                      << "  --redis-host HOST     Redis host (default: 127.0.0.1)\n"
                      << "  --redis-port PORT     Redis port (default: 6379)\n"
                      << "  --lru-capacity SIZE   LRU cache capacity (default: 128)\n"
                      << "  --help                Show this help\n";
            return 0;
        }
    }

    try {
        hypercache::server::App app(port, cache_config);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}