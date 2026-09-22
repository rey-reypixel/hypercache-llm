#include "hypercache/similarity/similarity_engine.hpp"

#include <cmath>
#include <immintrin.h>
#include <stdexcept>

namespace hypercache::similarity {

float SimilarityEngine::cosine_similarity_scalar(std::span<const float> lhs,
                                                  std::span<const float> rhs) {
    if (lhs.size() != rhs.size()) throw std::invalid_argument("vector dimensions differ");
    float dot = 0.0F, lhs_norm = 0.0F, rhs_norm = 0.0F;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }
    const float denominator = std::sqrt(lhs_norm * rhs_norm);
    return denominator == 0.0F ? 0.0F : dot / denominator;
}

float SimilarityEngine::cosine_similarity_sse(std::span<const float> lhs,
                                               std::span<const float> rhs) {
    if (lhs.size() != rhs.size()) throw std::invalid_argument("vector dimensions differ");
    const std::size_t n = lhs.size();
    const std::size_t vec_size = 4;
    const std::size_t vec_count = n / vec_size;

    __m128 dot_vec = _mm_setzero_ps();
    __m128 lhs_norm_vec = _mm_setzero_ps();
    __m128 rhs_norm_vec = _mm_setzero_ps();

    for (std::size_t i = 0; i < vec_count; ++i) {
        const std::size_t idx = i * vec_size;
        __m128 a = _mm_loadu_ps(&lhs[idx]);
        __m128 b = _mm_loadu_ps(&rhs[idx]);
        dot_vec = _mm_fmadd_ps(a, b, dot_vec);
        lhs_norm_vec = _mm_fmadd_ps(a, a, lhs_norm_vec);
        rhs_norm_vec = _mm_fmadd_ps(b, b, rhs_norm_vec);
    }

    // Horizontal sum using hadd (same pattern as AVX2)
    __m128 dot_sum = _mm_hadd_ps(dot_vec, dot_vec);
    dot_sum = _mm_hadd_ps(dot_sum, dot_sum);
    float dot = _mm_cvtss_f32(dot_sum);

    __m128 lhs_sum = _mm_hadd_ps(lhs_norm_vec, lhs_norm_vec);
    lhs_sum = _mm_hadd_ps(lhs_sum, lhs_sum);
    float lhs_norm = _mm_cvtss_f32(lhs_sum);

    __m128 rhs_sum = _mm_hadd_ps(rhs_norm_vec, rhs_norm_vec);
    rhs_sum = _mm_hadd_ps(rhs_sum, rhs_sum);
    float rhs_norm = _mm_cvtss_f32(rhs_sum);

    for (std::size_t i = vec_count * vec_size; i < n; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    const float denominator = std::sqrt(lhs_norm * rhs_norm);
    return denominator == 0.0F ? 0.0F : dot / denominator;
}

float SimilarityEngine::cosine_similarity_avx2(std::span<const float> lhs,
                                                std::span<const float> rhs) {
    if (lhs.size() != rhs.size()) throw std::invalid_argument("vector dimensions differ");
    const std::size_t n = lhs.size();
    const std::size_t vec_size = 8;
    const std::size_t vec_count = n / vec_size;

    __m256 dot_vec = _mm256_setzero_ps();
    __m256 lhs_norm_vec = _mm256_setzero_ps();
    __m256 rhs_norm_vec = _mm256_setzero_ps();

    for (std::size_t i = 0; i < vec_count; ++i) {
        const std::size_t idx = i * vec_size;
        __m256 a = _mm256_loadu_ps(&lhs[idx]);
        __m256 b = _mm256_loadu_ps(&rhs[idx]);
        dot_vec = _mm256_fmadd_ps(a, b, dot_vec);
        lhs_norm_vec = _mm256_fmadd_ps(a, a, lhs_norm_vec);
        rhs_norm_vec = _mm256_fmadd_ps(b, b, rhs_norm_vec);
    }

    __m128 dot_hi = _mm256_extractf128_ps(dot_vec, 1);
    __m128 dot_lo = _mm256_castps256_ps128(dot_vec);
    __m128 dot_sum = _mm_add_ps(dot_lo, dot_hi);
    dot_sum = _mm_hadd_ps(dot_sum, dot_sum);
    dot_sum = _mm_hadd_ps(dot_sum, dot_sum);
    float dot = _mm_cvtss_f32(dot_sum);

    __m128 lhs_hi = _mm256_extractf128_ps(lhs_norm_vec, 1);
    __m128 lhs_lo = _mm256_castps256_ps128(lhs_norm_vec);
    __m128 lhs_sum = _mm_add_ps(lhs_lo, lhs_hi);
    lhs_sum = _mm_hadd_ps(lhs_sum, lhs_sum);
    lhs_sum = _mm_hadd_ps(lhs_sum, lhs_sum);
    float lhs_norm = _mm_cvtss_f32(lhs_sum);

    __m128 rhs_hi = _mm256_extractf128_ps(rhs_norm_vec, 1);
    __m128 rhs_lo = _mm256_castps256_ps128(rhs_norm_vec);
    __m128 rhs_sum = _mm_add_ps(rhs_lo, rhs_hi);
    rhs_sum = _mm_hadd_ps(rhs_sum, rhs_sum);
    rhs_sum = _mm_hadd_ps(rhs_sum, rhs_sum);
    float rhs_norm = _mm_cvtss_f32(rhs_sum);

    for (std::size_t i = vec_count * vec_size; i < n; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    const float denominator = std::sqrt(lhs_norm * rhs_norm);
    return denominator == 0.0F ? 0.0F : dot / denominator;
}

float SimilarityEngine::cosine_similarity(std::span<const float> lhs,
                                           std::span<const float> rhs) {
#if defined(__AVX2__) || defined(_M_AVX2)
    return cosine_similarity_avx2(lhs, rhs);
#elif defined(__SSE4_1__) || defined(_M_SSE4_1)
    return cosine_similarity_sse(lhs, rhs);
#else
    return cosine_similarity_scalar(lhs, rhs);
#endif
}

} // namespace hypercache::similarity