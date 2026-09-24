#include "hypercache/server/app.hpp"
#include "hypercache/cache/lru_cache_backend.hpp"
#include "hypercache/telemetry/telemetry.hpp"
#include "hypercache/similarity/similarity_engine.hpp"

#include "httplib.h"
#include "nlohmann/json.hpp"

#include <string>
#include <sstream>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#ifdef HYPERCACHE_BUILD_REDIS
#include "hypercache/cache/redis_cache_backend.hpp"
#endif

namespace hypercache::server {

namespace {

using json = nlohmann::json;

auto& get_telemetry() {
    return hypercache::telemetry::TokenTelemetry::instance();
}

std::string json_telemetry() {
    json j;
    j["request_tokens"] = get_telemetry().get_request_tokens();
    j["streaming_tokens"] = get_telemetry().get_streaming_tokens();
    j["total_requests"] = get_telemetry().get_total_requests();
    j["cache_hits"] = get_telemetry().get_cache_hits();
    j["cache_misses"] = get_telemetry().get_cache_misses();
    j["errors"] = get_telemetry().get_errors();
    return j.dump();
}

std::string json_similarity(float sim) {
    json j;
    j["similarity"] = sim;
    return j.dump();
}

std::vector<float> parse_float_array(const json& j, const std::string& key) {
    std::vector<float> result;
    if (j.contains(key) && j[key].is_array()) {
        for (const auto& val : j[key]) {
            if (val.is_number()) {
                result.push_back(val.get<float>());
            }
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
            {
                hypercache::cache::RedisCacheBackend::Config redis_cfg;
                redis_cfg.host = cache_config_.redis_host;
                redis_cfg.port = cache_config_.redis_port;
                redis_cfg.pool_min = cache_config_.redis_pool_min;
                redis_cfg.pool_max = cache_config_.redis_pool_max;
                redis_cfg.connect_timeout = cache_config_.redis_connect_timeout;
                redis_cfg.acquire_timeout = cache_config_.redis_acquire_timeout;
                cache_ = std::make_unique<hypercache::cache::RedisCacheBackend>(std::move(redis_cfg));
            }
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
        json j;
        j["status"] = "ok";
        j["uptime"] = "running";
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        auto result = cache_->get(req.path_params.at("key"));
        get_telemetry().record_request(1);
        if (result.has_value()) {
            get_telemetry().record_cache_hit();
            res.set_content(result.value(), "text/plain");
        } else {
            get_telemetry().record_cache_miss();
            res.status = 404;
            json j;
            j["error"] = "Key not found";
            res.set_content(j.dump(), "application/json");
        }
    });

    svr.Put("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        cache_->put(req.path_params.at("key"), req.body);
        get_telemetry().record_request(1);
        json j;
        j["status"] = "cached";
        res.set_content(j.dump(), "application/json");
    });

    svr.Delete("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        cache_->remove(req.path_params.at("key"));
        get_telemetry().record_request(1);
        json j;
        j["status"] = "removed";
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/cache", [&](const httplib::Request&, httplib::Response& res) {
        get_telemetry().record_request(1);
        json j;
        j["size"] = cache_->size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/telemetry", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(json_telemetry(), "application/json");
    });

    svr.Get("/metrics", [&](const httplib::Request&, httplib::Response& res) {
        get_telemetry().record_request(1);
        res.set_content(get_telemetry().to_prometheus(), "text/plain; version=0.0.4");
    });

    svr.Get("/metrics/json", [&](const httplib::Request&, httplib::Response& res) {
        get_telemetry().record_request(1);
        res.set_content(get_telemetry().to_json(), "application/json");
    });

    svr.Post("/similarity", [&](const httplib::Request& req, httplib::Response& res) {
        get_telemetry().record_request(1);
        try {
            auto j = json::parse(req.body);
            auto lhs = parse_float_array(j, "lhs");
            auto rhs = parse_float_array(j, "rhs");
            if (lhs.empty() || rhs.empty() || lhs.size() != rhs.size()) {
                res.status = 400;
                json err;
                err["error"] = "Invalid vectors: lhs and rhs must be non-empty arrays of equal length";
                res.set_content(err.dump(), "application/json");
                return;
            }
            float sim = hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs);
            res.set_content(json_similarity(sim), "application/json");
        } catch (const json::parse_error& e) {
            get_telemetry().record_error();
            res.status = 400;
            json err;
            err["error"] = "Invalid JSON: " + std::string(e.what());
            res.set_content(err.dump(), "application/json");
        } catch (const std::exception& e) {
            get_telemetry().record_error();
            res.status = 400;
            json err;
            err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
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
                sink.done();
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
        } else if (arg == "--redis-pool-min" && i + 1 < argc) {
            cache_config.redis_pool_min = std::stoull(argv[++i]);
        } else if (arg == "--redis-pool-max" && i + 1 < argc) {
            cache_config.redis_pool_max = std::stoull(argv[++i]);
        } else if (arg == "--redis-connect-timeout" && i + 1 < argc) {
            cache_config.redis_connect_timeout = std::chrono::milliseconds(std::stoll(argv[++i]));
        } else if (arg == "--redis-acquire-timeout" && i + 1 <argc) {
            cache_config.redis_acquire_timeout = std::chrono::milliseconds(std::stoll(argv[++i]));
        } else if (arg == "--lru-capacity" && i + 1 < argc) {
            cache_config.lru_capacity = std::stoull(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: hypercache_server [options]\n"
                      << "Options:\n"
                      << "  --port PORT                    Server port (default: 18080)\n"
                      << "  --cache TYPE                   Cache backend: lru|redis (default: lru)\n"
                      << "  --redis-host HOST              Redis host (default: 127.0.0.1)\n"
                      << "  --redis-port PORT              Redis port (default: 6379)\n"
                      << "  --redis-pool-min SIZE          Redis pool min connections (default: 2)\n"
                      << "  --redis-pool-max SIZE          Redis pool max connections (default: 10)\n"
                      << "  --redis-connect-timeout MS     Redis connect timeout ms (default: 5000)\n"
                      << "  --redis-acquire-timeout MS     Redis acquire timeout ms (default: 2000)\n"
                      << "  --lru-capacity SIZE            LRU cache capacity (default: 128)\n"
                      << "  --help                         Show this help\n";
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