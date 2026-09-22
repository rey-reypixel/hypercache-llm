#include "hypercache/similarity/similarity_engine.hpp"
#include <benchmark/benchmark.h>
#include <random>
#include <vector>

static void BM_CosineSimilarityScalar(benchmark::State& state) {
    const std::size_t dim = state.range(0);
    std::vector<float> lhs(dim), rhs(dim);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0F, 1.0F);

    for (std::size_t i = 0; i < dim; ++i) {
        lhs[i] = dist(rng);
        rhs[i] = dist(rng);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(
            hypercache::similarity::SimilarityEngine::cosine_similarity_scalar(lhs, rhs)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CosineSimilarityScalar)->Arg(128)->Arg(256)->Arg(512)->Arg(1024)->Arg(2048)->Arg(4096)->Arg(8192)->Arg(16384);

static void BM_CosineSimilaritySSE(benchmark::State& state) {
    const std::size_t dim = state.range(0);
    std::vector<float> lhs(dim), rhs(dim);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0F, 1.0F);

    for (std::size_t i = 0; i < dim; ++i) {
        lhs[i] = dist(rng);
        rhs[i] = dist(rng);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(
            hypercache::similarity::SimilarityEngine::cosine_similarity_sse(lhs, rhs)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CosineSimilaritySSE)->Arg(128)->Arg(256)->Arg(512)->Arg(1024)->Arg(2048)->Arg(4096)->Arg(8192)->Arg(16384);

static void BM_CosineSimilarityAVX2(benchmark::State& state) {
    const std::size_t dim = state.range(0);
    std::vector<float> lhs(dim), rhs(dim);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0F, 1.0F);

    for (std::size_t i = 0; i < dim; ++i) {
        lhs[i] = dist(rng);
        rhs[i] = dist(rng);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(
            hypercache::similarity::SimilarityEngine::cosine_similarity_avx2(lhs, rhs)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CosineSimilarityAVX2)->Arg(128)->Arg(256)->Arg(512)->Arg(1024)->Arg(2048)->Arg(4096)->Arg(8192)->Arg(16384);

static void BM_CosineSimilarityAuto(benchmark::State& state) {
    const std::size_t dim = state.range(0);
    std::vector<float> lhs(dim), rhs(dim);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0F, 1.0F);

    for (std::size_t i = 0; i < dim; ++i) {
        lhs[i] = dist(rng);
        rhs[i] = dist(rng);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(
            hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CosineSimilarityAuto)->Arg(128)->Arg(256)->Arg(512)->Arg(1024)->Arg(2048)->Arg(4096)->Arg(8192)->Arg(16384);