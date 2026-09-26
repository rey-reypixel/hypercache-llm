#include "hypercache/cache/lru_cache.hpp"

#include <string>

namespace hypercache::cache {

template class LruCache<std::string, std::string>;
template class LruCache<int, int>;

} // namespace hypercache::cache