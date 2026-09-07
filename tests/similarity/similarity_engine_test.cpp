#include "hypercache/similarity/similarity_engine.hpp"
#include <gtest/gtest.h>
#include <vector>

TEST(SimilarityEngine, ComputesCosineSimilarity) {
    const std::vector<float> lhs{1.0F, 0.0F};
    const std::vector<float> rhs{1.0F, 0.0F};
    EXPECT_FLOAT_EQ(hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs), 1.0F);
}
