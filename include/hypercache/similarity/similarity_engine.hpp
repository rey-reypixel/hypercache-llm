#pragma once

#include <span>

namespace hypercache::similarity {

class SimilarityEngine {
public:
    static float cosine_similarity(std::span<const float> lhs,
                                    std::span<const float> rhs);
    static float cosine_similarity_avx2(std::span<const float> lhs,
                                         std::span<const float> rhs);
    static float cosine_similarity_sse(std::span<const float> lhs,
                                        std::span<const float> rhs);
    static float cosine_similarity_scalar(std::span<const float> lhs,
                                           std::span<const float> rhs);
};

} // namespace hypercache::similarity