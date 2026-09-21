#include "hypercache/telemetry/telemetry.hpp"

#include <sstream>

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

void TokenTelemetry::record_cache_hit() {
    stats_.cache_hits.fetch_add(1);
}

void TokenTelemetry::record_cache_miss() {
    stats_.cache_misses.fetch_add(1);
}

void TokenTelemetry::record_error() {
    stats_.errors.fetch_add(1);
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

std::size_t TokenTelemetry::get_cache_hits() const {
    return stats_.cache_hits.load();
}

std::size_t TokenTelemetry::get_cache_misses() const {
    return stats_.cache_misses.load();
}

std::size_t TokenTelemetry::get_errors() const {
    return stats_.errors.load();
}

std::unordered_map<std::string, std::size_t> TokenTelemetry::get_endpoint_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endpoint_counts_;
}

std::string TokenTelemetry::to_prometheus() const {
    std::ostringstream oss;
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time_).count();

    oss << "# HELP hypercache_uptime_seconds Server uptime in seconds\n";
    oss << "# TYPE hypercache_uptime_seconds gauge\n";
    oss << "hypercache_uptime_seconds " << uptime << "\n";

    oss << "# HELP hypercache_request_tokens_total Total request tokens processed\n";
    oss << "# TYPE hypercache_request_tokens_total counter\n";
    oss << "hypercache_request_tokens_total " << stats_.request_tokens.load() << "\n";

    oss << "# HELP hypercache_streaming_tokens_total Total streaming tokens processed\n";
    oss << "# TYPE hypercache_streaming_tokens_total counter\n";
    oss << "hypercache_streaming_tokens_total " << stats_.streaming_tokens.load() << "\n";

    oss << "# HELP hypercache_total_requests_total Total HTTP requests\n";
    oss << "# TYPE hypercache_total_requests_total counter\n";
    oss << "hypercache_total_requests_total " << stats_.total_requests.load() << "\n";

    oss << "# HELP hypercache_cache_hits_total Total cache hits\n";
    oss << "# TYPE hypercache_cache_hits_total counter\n";
    oss << "hypercache_cache_hits_total " << stats_.cache_hits.load() << "\n";

    oss << "# HELP hypercache_cache_misses_total Total cache misses\n";
    oss << "# TYPE hypercache_cache_misses_total counter\n";
    oss << "hypercache_cache_misses_total " << stats_.cache_misses.load() << "\n";

    oss << "# HELP hypercache_errors_total Total errors\n";
    oss << "# TYPE hypercache_errors_total counter\n";
    oss << "hypercache_errors_total " << stats_.errors.load() << "\n";

    return oss.str();
}

std::string TokenTelemetry::to_json() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"uptime_seconds\":" << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time_).count() << ",";
    oss << "\"request_tokens\":" << stats_.request_tokens.load() << ",";
    oss << "\"streaming_tokens\":" << stats_.streaming_tokens.load() << ",";
    oss << "\"total_requests\":" << stats_.total_requests.load() << ",";
    oss << "\"cache_hits\":" << stats_.cache_hits.load() << ",";
    oss << "\"cache_misses\":" << stats_.cache_misses.load() << ",";
    oss << "\"errors\":" << stats_.errors.load();
    oss << "}";
    return oss.str();
}

void TokenTelemetry::reset() {
    stats_.request_tokens.store(0);
    stats_.streaming_tokens.store(0);
    stats_.total_requests.store(0);
    stats_.cache_hits.store(0);
    stats_.cache_misses.store(0);
    stats_.errors.store(0);
}

} // namespace hypercache::telemetry