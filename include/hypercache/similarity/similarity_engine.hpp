#pragma once

#include <span>

namespace hypercache::similarity {

class SimilarityEngine {
public:
    static float cosine_similarity(std::span<const float> lhs,
                                    std::span<const float> rhs);
};

} // namespace hypercache::similarity
