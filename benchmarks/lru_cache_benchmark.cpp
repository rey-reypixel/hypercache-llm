#include "hypercache/cache/lru_cache_backend.hpp"
#include <benchmark/benchmark.h>
#include <random>
#include <string>
#include <vector>

static void BM_LruCachePut(benchmark::State& state) {
    hypercache::cache::LruCacheBackend cache(10000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 9999);

    for (auto _ : state) {
        int key = dist(rng);
        cache.put("key_" + std::to_string(key), "value_" + std::to_string(key));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LruCachePut);

static void BM_LruCacheGet(benchmark::State& state) {
    hypercache::cache::LruCacheBackend cache(10000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 9999);

    for (int i = 0; i < 5000; ++i) {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    for (auto _ : state) {
        int key = dist(rng);
        cache.get("key_" + std::to_string(key));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LruCacheGet);

static void BM_LruCacheMixed(benchmark::State& state) {
    hypercache::cache::LruCacheBackend cache(10000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> key_dist(0, 9999);
    std::uniform_int_distribution<int> op_dist(0, 2);

    for (int i = 0; i < 3000; ++i) {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    for (auto _ : state) {
        int key = key_dist(rng);
        std::string key_str = "key_" + std::to_string(key);
        int op = op_dist(rng);
        if (op == 0) {
            cache.get(key_str);
        } else if (op == 1) {
            cache.put(key_str, "new_value");
        } else {
            cache.remove(key_str);
        }
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LruCacheMixed);

static void BM_LruCacheConcurrent(benchmark::State& state) {
    hypercache::cache::LruCacheBackend cache(10000);
    const int num_threads = state.range(0);

    for (int i = 0; i < 5000; ++i) {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    std::vector<std::thread> threads;
    std::atomic<bool> stop{false};
    std::atomic<int64_t> total_ops{0};

    auto worker = [&](int tid) {
        std::mt19937 rng(tid + 42);
        std::uniform_int_distribution<int> key_dist(0, 9999);
        std::uniform_int_distribution<int> op_dist(0, 2);

        while (!stop.load(std::memory_order_relaxed)) {
            int key = key_dist(rng);
            std::string key_str = "key_" + std::to_string(key);
            int op = op_dist(rng);
            if (op == 0) {
                cache.get(key_str);
            } else if (op == 1) {
                cache.put(key_str, "new_value");
            } else {
                cache.remove(key_str);
            }
            total_ops.fetch_add(1, std::memory_order_relaxed);
        }
    };

    for (auto _ : state) {
        stop.store(false, std::memory_order_relaxed);
        total_ops.store(0, std::memory_order_relaxed);
        threads.clear();
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back(worker, t);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stop.store(true, std::memory_order_relaxed);
        for (auto& thread : threads) {
            thread.join();
        }
        state.SetItemsProcessed(total_ops.load());
    }
}
BENCHMARK(BM_LruCacheConcurrent)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16)->Arg(32);