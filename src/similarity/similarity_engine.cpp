#include "hypercache/similarity/similarity_engine.hpp"

#include <cmath>
#include <stdexcept>

namespace hypercache::similarity {

float SimilarityEngine::cosine_similarity(std::span<const float> lhs,
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

} // namespace hypercache::similarity