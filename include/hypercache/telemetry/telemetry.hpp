#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace hypercache::telemetry {

struct TokenStats {
    std::atomic<std::size_t> request_tokens{0};
    std::atomic<std::size_t> streaming_tokens{0};
    std::atomic<std::size_t> total_requests{0};
};

class TokenTelemetry {
public:
    static TokenTelemetry& instance();

    void record_request(std::size_t tokens);
    void record_streaming(std::size_t tokens);

    std::size_t get_request_tokens() const;
    std::size_t get_streaming_tokens() const;
    std::size_t get_total_requests() const;
    std::unordered_map<std::string, std::size_t> get_endpoint_stats() const;

    void reset();

private:
    TokenTelemetry() = default;
    TokenTelemetry(const TokenTelemetry&) = delete;
    TokenTelemetry& operator=(const TokenTelemetry&) = delete;

    mutable std::mutex mutex_;
    TokenStats stats_;
    std::unordered_map<std::string, std::size_t> endpoint_counts_;
};

} // namespace hypercache::telemetry
