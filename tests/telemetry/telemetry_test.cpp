#include "hypercache/telemetry/telemetry.hpp"
#include <gtest/gtest.h>
#include <string>

TEST(Telemetry, PrometheusFormat) {
    auto& telemetry = hypercache::telemetry::TokenTelemetry::instance();
    telemetry.reset();

    telemetry.record_request(10);
    telemetry.record_streaming(5);
    telemetry.record_cache_hit();
    telemetry.record_cache_miss();
    telemetry.record_error();

    std::string prom = telemetry.to_prometheus();

    EXPECT_NE(prom.find("hypercache_uptime_seconds"), std::string::npos);
    EXPECT_NE(prom.find("hypercache_request_tokens_total 10"), std::string::npos);
    EXPECT_NE(prom.find("hypercache_streaming_tokens_total 5"), std::string::npos);
    EXPECT_NE(prom.find("hypercache_total_requests_total 1"), std::string::npos);
    EXPECT_NE(prom.find("hypercache_cache_hits_total 1"), std::string::npos);
    EXPECT_NE(prom.find("hypercache_cache_misses_total 1"), std::string::npos);
    EXPECT_NE(prom.find("hypercache_errors_total 1"), std::string::npos);
}

TEST(Telemetry, JsonFormat) {
    auto& telemetry = hypercache::telemetry::TokenTelemetry::instance();
    telemetry.reset();

    telemetry.record_request(10);
    telemetry.record_streaming(5);
    telemetry.record_cache_hit();
    telemetry.record_cache_miss();
    telemetry.record_error();

    std::string json = telemetry.to_json();

    EXPECT_NE(json.find("\"request_tokens\":10"), std::string::npos);
    EXPECT_NE(json.find("\"streaming_tokens\":5"), std::string::npos);
    EXPECT_NE(json.find("\"total_requests\":1"), std::string::npos);
    EXPECT_NE(json.find("\"cache_hits\":1"), std::string::npos);
    EXPECT_NE(json.find("\"cache_misses\":1"), std::string::npos);
    EXPECT_NE(json.find("\"errors\":1"), std::string::npos);
    EXPECT_NE(json.find("\"uptime_seconds\""), std::string::npos);
}

TEST(Telemetry, Reset) {
    auto& telemetry = hypercache::telemetry::TokenTelemetry::instance();
    telemetry.reset();

    telemetry.record_request(100);
    telemetry.record_streaming(50);
    telemetry.record_cache_hit();
    telemetry.record_cache_miss();
    telemetry.record_error();

    telemetry.reset();

    EXPECT_EQ(telemetry.get_request_tokens(), 0);
    EXPECT_EQ(telemetry.get_streaming_tokens(), 0);
    EXPECT_EQ(telemetry.get_total_requests(), 0);
    EXPECT_EQ(telemetry.get_cache_hits(), 0);
    EXPECT_EQ(telemetry.get_cache_misses(), 0);
    EXPECT_EQ(telemetry.get_errors(), 0);
}