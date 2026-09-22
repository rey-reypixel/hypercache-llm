#include "hypercache/similarity/similarity_engine.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <cmath>

TEST(SimilarityEngine, ComputesCosineSimilarity) {
    const std::vector<float> lhs{1.0F, 0.0F};
    const std::vector<float> rhs{1.0F, 0.0F};
    EXPECT_FLOAT_EQ(hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs), 1.0F);
}

TEST(SimilarityEngine, OrthogonalVectors) {
    const std::vector<float> lhs{1.0F, 0.0F};
    const std::vector<float> rhs{0.0F, 1.0F};
    EXPECT_FLOAT_EQ(hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs), 0.0F);
}

TEST(SimilarityEngine, OppositeVectors) {
    const std::vector<float> lhs{1.0F, 0.0F};
    const std::vector<float> rhs{-1.0F, 0.0F};
    EXPECT_FLOAT_EQ(hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs), -1.0F);
}

TEST(SimilarityEngine, DimensionMismatchThrows) {
    const std::vector<float> lhs{1.0F, 0.0F};
    const std::vector<float> rhs{1.0F};
    EXPECT_THROW(hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs),
                 std::invalid_argument);
}

TEST(SimilarityEngine, ZeroVectorReturnsZero) {
    const std::vector<float> lhs{0.0F, 0.0F};
    const std::vector<float> rhs{1.0F, 2.0F};
    EXPECT_FLOAT_EQ(hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs), 0.0F);
}

TEST(SimilarityEngine, ScalarMatchesAVX2) {
    const std::vector<float> lhs(256, 0.5F);
    const std::vector<float> rhs(256, 0.3F);
    float scalar = hypercache::similarity::SimilarityEngine::cosine_similarity_scalar(lhs, rhs);
#if defined(__AVX2__) || defined(_M_AVX2)
    float avx2 = hypercache::similarity::SimilarityEngine::cosine_similarity_avx2(lhs, rhs);
    EXPECT_NEAR(scalar, avx2, 1e-5);
#endif
}

TEST(SimilarityEngine, ScalarMatchesSSE) {
    const std::vector<float> lhs(256, 0.7F);
    const std::vector<float> rhs(256, 0.2F);
    float scalar = hypercache::similarity::SimilarityEngine::cosine_similarity_scalar(lhs, rhs);
#if defined(__SSE4_1__) || defined(_M_SSE4_1)
    float sse = hypercache::similarity::SimilarityEngine::cosine_similarity_sse(lhs, rhs);
    EXPECT_NEAR(scalar, sse, 1e-5);
#endif
}

TEST(SimilarityEngine, LargeVectors) {
    const std::size_t dim = 10000;
    std::vector<float> lhs(dim), rhs(dim);
    for (std::size_t i = 0; i < dim; ++i) {
        lhs[i] = static_cast<float>(i) * 0.001F;
        rhs[i] = static_cast<float>(dim - i) * 0.001F;
    }
    float sim = hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs);
    EXPECT_GE(sim, -1.0F);
    EXPECT_LE(sim, 1.0F);
}

TEST(SimilarityEngine, NonMultipleOfSIMDWidth) {
    const std::vector<float> lhs{1.0F, 2.0F, 3.0F, 4.0F, 5.0F};
    const std::vector<float> rhs{5.0F, 4.0F, 3.0F, 2.0F, 1.0F};
    float sim = hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs);
    float expected = (1*5 + 2*4 + 3*3 + 4*2 + 5*1) /
                     std::sqrt((1+4+9+16+25) * (25+16+9+4+1));
    EXPECT_NEAR(sim, expected, 1e-5);
}