#include "hypercache/telemetry/telemetry.hpp"

namespace hypercache::telemetry {

TokenTelemetry& TokenTelemetry::instance() {
    static TokenTelemetry inst;
    return inst;
}

void TokenTelemetry::record_request(std::size_t tokens) {
    stats_.request_tokens.fetch_add(tokens);
    stats_.total_requests.fetch_add(1);
}

void TokenTelemetry::record_streaming(std::size_t tokens) {
    stats_.streaming_tokens.fetch_add(tokens);
}

std::size_t TokenTelemetry::get_request_tokens() const {
    return stats_.request_tokens.load();
}

std::size_t TokenTelemetry::get_streaming_tokens() const {
    return stats_.streaming_tokens.load();
}

std::size_t TokenTelemetry::get_total_requests() const {
    return stats_.total_requests.load();
}

std::unordered_map<std::string, std::size_t> TokenTelemetry::get_endpoint_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endpoint_counts_;
}

void TokenTelemetry::reset() {
    stats_.request_tokens.store(0);
    stats_.streaming_tokens.store(0);
    stats_.total_requests.store(0);
}

} // namespace hypercache::telemetry
