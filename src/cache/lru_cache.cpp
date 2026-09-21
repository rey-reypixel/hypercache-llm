#include "hypercache/cache/lru_cache.hpp"

namespace hypercache::cache {

template class LruCache<std::string, std::string>;
template class LruCache<int, int>;

} // namespace hypercache::cache