#include "hypercache/telemetry/telemetry.hpp"
#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>

static void BM_TelemetryRecordRequest(benchmark::State& state) {
    auto& telemetry = hypercache::telemetry::TokenTelemetry::instance();
    telemetry.reset();

    for (auto _ : state) {
        telemetry.record_request(10);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TelemetryRecordRequest);

static void BM_TelemetryRecordStreaming(benchmark::State& state) {
    auto& telemetry = hypercache::telemetry::TokenTelemetry::instance();
    telemetry.reset();

    for (auto _ : state) {
        telemetry.record_streaming(5);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TelemetryRecordStreaming);

static void BM_TelemetryConcurrent(benchmark::State& state) {
    auto& telemetry = hypercache::telemetry::TokenTelemetry::instance();
    telemetry.reset();
    const int num_threads = state.range(0);
    const int ops_per_thread = 10000;

    std::vector<std::thread> threads;
    std::atomic<bool> stop{false};

    auto worker = [&](int tid) {
        while (!stop.load(std::memory_order_relaxed)) {
            telemetry.record_request(tid + 1);
            telemetry.record_streaming(tid + 2);
            telemetry.record_cache_hit();
            telemetry.record_cache_miss();
        }
    };

    for (auto _ : state) {
        telemetry.reset();
        stop.store(false, std::memory_order_relaxed);
        threads.clear();
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back(worker, t);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        stop.store(true, std::memory_order_relaxed);
        for (auto& thread : threads) {
            thread.join();
        }
        state.SetItemsProcessed(telemetry.get_total_requests());
    }
}
BENCHMARK(BM_TelemetryConcurrent)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16)->Arg(32);

static void BM_TelemetryPrometheusFormat(benchmark::State& state) {
    auto& telemetry = hypercache::telemetry::TokenTelemetry::instance();
    telemetry.reset();

    for (int i = 0; i < 10000; ++i) {
        telemetry.record_request(10);
        telemetry.record_streaming(5);
        telemetry.record_cache_hit();
        telemetry.record_cache_miss();
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(telemetry.to_prometheus());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TelemetryPrometheusFormat);