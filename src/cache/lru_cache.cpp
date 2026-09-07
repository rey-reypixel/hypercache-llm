#include "hypercache/cache/lru_cache.hpp"

// The cache is header-only because its behavior is generic; this translation unit
// keeps the library target stable as additional cache adapters are added.
