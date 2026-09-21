#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace hypercache::telemetry {

struct TokenStats {
    std::atomic<std::size_t> request_tokens{0};
    std::atomic<std::size_t> streaming_tokens{0};
    std::atomic<std::size_t> total_requests{0};
    std::atomic<std::size_t> cache_hits{0};
    std::atomic<std::size_t> cache_misses{0};
    std::atomic<std::size_t> errors{0};
};

class TokenTelemetry {
public:
    static TokenTelemetry& instance();

    void record_request(std::size_t tokens);
    void record_streaming(std::size_t tokens);
    void record_cache_hit();
    void record_cache_miss();
    void record_error();

    std::size_t get_request_tokens() const;
    std::size_t get_streaming_tokens() const;
    std::size_t get_total_requests() const;
    std::size_t get_cache_hits() const;
    std::size_t get_cache_misses() const;
    std::size_t get_errors() const;
    std::unordered_map<std::string, std::size_t> get_endpoint_stats() const;

    std::string to_prometheus() const;
    std::string to_json() const;

    void reset();

private:
    TokenTelemetry() = default;
    TokenTelemetry(const TokenTelemetry&) = delete;
    TokenTelemetry& operator=(const TokenTelemetry&) = delete;

    mutable std::mutex mutex_;
    TokenStats stats_;
    std::unordered_map<std::string, std::size_t> endpoint_counts_;
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
};

} // namespace hypercache::telemetry