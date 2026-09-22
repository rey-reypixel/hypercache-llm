# HyperCache-LLM

High-performance LLM telemetry and cache gateway built with C++20.

HyperCache-LLM is designed to sit in front of LLM APIs and provide:

- Token telemetry for request and streaming-response workflows
- Configurable cache backends: thread-safe LRU (in-memory) or Redis (distributed)
- Redis connection pooling with configurable min/max connections and idle reaper
- **SIMD-optimized vector similarity (AVX2/SSE4.1 with scalar fallback)**
- REST and Server-Sent Events (SSE) endpoints using **cpp-httplib**
- Proper JSON request/response handling via nlohmann/json
- Containerized deployment with Docker and docker-compose
- GoogleTest coverage for cache behavior and **concurrent access**
- **google-benchmark harness for latency measurements**
- **AddressSanitizer + Valgrind verified (zero leaks)**

## Features

### Cache Backends

| Backend | Description | Use Case |
|---------|-------------|----------|
| **LRU** | In-memory, thread-safe, O(1) operations | Single instance, low latency |
| **Redis** | Distributed, persistent, connection pooled | Multi-instance, shared cache |

### Redis Connection Pool

- Configurable min/max connections
- Thread-safe acquire/release with timeout
- Background idle connection reaper
- Pool statistics endpoint

### SIMD-Accelerated Similarity

- **AVX2** (8 floats/cycle) - default on supported CPUs
- **SSE4.1** (4 floats/cycle) - fallback
- **Scalar** - portable fallback
- Automatic runtime dispatch via `#ifdef`

### JSON API

All endpoints return structured JSON. Errors follow `{"error": "message"}` format.

## Building

### From Source

```bash
# With Redis support (default)
cmake -B build -DHYPERCACHE_BUILD_SERVER=ON -DHYPERCACHE_BUILD_REDIS=ON
cmake --build build --config Release

# LRU only (no Redis dependency)
cmake -B build -DHYPERCACHE_BUILD_SERVER=ON -DHYPERCACHE_BUILD_REDIS=OFF
cmake --build build --config Release

# Run tests
cmake -B build -DHYPERCACHE_BUILD_TESTS=ON
cmake --build build --config Release
./build/hypercache_tests

# Run benchmarks
cmake -B build -DHYPERCACHE_BUILD_BENCHMARKS=ON
cmake --build build --config Release
./build/hypercache_benchmarks

# AddressSanitizer build
cmake -B build-asan \
  -DHYPERCACHE_BUILD_SERVER=ON -DHYPERCACHE_BUILD_REDIS=ON -DHYPERCACHE_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -O1 -g"
cmake --build build-asan --config Debug
ASAN_OPTIONS=detect_leaks=1 ./build-asan/hypercache_tests
```

### With Docker

```bash
# Build image
docker build -t hypercache-llm .

# Run with LRU cache (default)
docker run -p 18080:18080 hypercache-llm

# Run with Redis backend
docker run -p 18080:18080 \
  -e CACHE_TYPE=redis \
  -e REDIS_HOST=host.docker.internal \
  hypercache-llm

# Development stack (Redis + HyperCache)
docker-compose up --build
```

## Running the Server

### Command Line Options

```bash
./build/hypercache_server [options]

Options:
  --port PORT                    Server port (default: 18080)
  --cache TYPE                   Cache backend: lru|redis (default: lru)
  --redis-host HOST              Redis host (default: 127.0.0.1)
  --redis-port PORT              Redis port (default: 6379)
  --redis-pool-min SIZE          Redis pool min connections (default: 2)
  --redis-pool-max SIZE          Redis pool max connections (default: 10)
  --redis-connect-timeout MS     Redis connect timeout ms (default: 5000)
  --redis-acquire-timeout MS     Redis acquire timeout ms (default: 2000)
  --lru-capacity SIZE            LRU cache capacity (default: 128)
  --help                         Show this help
```

### Environment Variables (Docker)

| Variable | Default | Description |
|----------|---------|-------------|
| `CACHE_TYPE` | `lru` | Cache backend: `lru` or `redis` |
| `REDIS_HOST` | `redis` | Redis hostname |
| `REDIS_PORT` | `6379` | Redis port |
| `REDIS_POOL_MIN` | `2` | Min pool connections |
| `REDIS_POOL_MAX` | `10` | Max pool connections |

## API Endpoints

| Method | Path | Description | Request Body | Response |
|--------|------|-------------|--------------|----------|
| GET | `/health` | Health check | - | `{"status":"ok","uptime":"running"}` |
| GET | `/cache/:key` | Get cache entry | - | Value (text/plain) or 404 |
| PUT | `/cache/:key` | Store cache entry | Raw body | `{"status":"cached"}` |
| DELETE | `/cache/:key` | Remove cache entry | - | `{"status":"removed"}` |
| GET | `/cache` | Cache info | - | `{"size":N}` |
| GET | `/telemetry` | Token telemetry | - | `{"request_tokens":N,"streaming_tokens":N,"total_requests":N,"cache_hits":N,"cache_misses":N,"errors":N}` |
| GET | `/metrics` | Prometheus metrics | - | `text/plain` (Prometheus format) |
| GET | `/metrics/json` | JSON metrics | - | `application/json` |
| POST | `/similarity` | Cosine similarity | `{"lhs":[...],"rhs":[...]}` | `{"similarity":0.5}` |
| GET | `/stream` | SSE stream demo | - | `text/event-stream` |

### Example Requests

**Store a prompt:**
```bash
curl -X PUT http://localhost:18080/cache/my-prompt \
  -d "What is the capital of France?"
```

**Retrieve:**
```bash
curl http://localhost:18080/cache/my-prompt
```

**Compute similarity:**
```bash
curl -X POST http://localhost:18080/similarity \
  -H "Content-Type: application/json" \
  -d '{"lhs":[1.0,0.0,0.5],"rhs":[0.8,0.2,0.4]}'
```

**Get telemetry:**
```bash
curl http://localhost:18080/telemetry
```

**Get Prometheus metrics:**
```bash
curl http://localhost:18080/metrics
```

## Project Structure

```
hypercache-llm/
├── include/hypercache/
│   ├── cache/
│   │   ├── lru_cache.hpp           # Thread-safe LRU template
│   │   ├── lru_cache_backend.hpp   # LRU CacheBackend adapter
│   │   ├── redis_cache.hpp         # Low-level Redis client
│   │   ├── redis_cache_backend.hpp # Redis CacheBackend with pooling
│   │   ├── redis_connection_pool.hpp # Connection pool
│   │   └── cache_backend.hpp       # Abstract cache interface
│   ├── similarity/
│   │   └── similarity_engine.hpp   # Cosine similarity (AVX2/SSE/Scalar)
│   ├── telemetry/
│   │   └── telemetry.hpp           # Token counters + Prometheus/JSON export
│   └── server/
│       └── app.hpp                 # HTTP server
├── src/
│   ├── cache/                      # Cache implementations
│   ├── similarity/                 # Similarity engine (SIMD)
│   ├── telemetry/                  # Telemetry
│   └── server/                     # Server main
├── tests/
│   ├── cache/                      # Cache tests (incl. concurrent stress)
│   ├── similarity/                 # Similarity tests (SIMD correctness)
│   └── telemetry/                  # Telemetry tests
├── benchmarks/
│   ├── lru_cache_benchmark.cpp     # LRU latency/throughput
│   ├── similarity_benchmark.cpp    # SIMD vs scalar comparison
│   └── telemetry_benchmark.cpp     # Telemetry overhead
├── .github/workflows/ci.yml        # CI: build, test, ASan, Valgrind
├── .github/valgrind.supp           # Valgrind suppressions
├── Dockerfile                      # Multi-stage build
├── docker-compose.yml              # Dev stack
├── .dockerignore
├── CMakeLists.txt
└── LICENSE
```

## Dependencies

- **C++20** compiler (GCC 10+, Clang 12+, MSVC 19.28+)
- **CMake** 3.20+
- **cpp-httplib** (via FetchContent)
- **hiredis** (via FetchContent, optional)
- **nlohmann/json** (via FetchContent)
- **GoogleTest** (for tests, via find_package)
- **google-benchmark** (for benchmarks, via FetchContent)

## Verification

| Check | Status |
|-------|--------|
| Unit tests | ✅ GitHub Actions (Linux/Windows) |
| Concurrent stress tests | ✅ 32-thread LRU/Redis |
| SIMD correctness | ✅ Scalar vs AVX2/SSE parity tests |
| AddressSanitizer | ✅ Zero leaks, zero errors |
| Valgrind Memcheck | ✅ Clean |
| Benchmarks | ✅ google-benchmark harness |

## Status

Active development. APIs may change.

## License

Community edition: GNU AGPL v3.0. See [LICENSE](LICENSE).

Commercial licensing: contact mahashreyaa@gmail.com