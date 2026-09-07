# HyperCache-LLM

High-performance LLM telemetry and cache gateway built with C++20.

HyperCache-LLM is designed to sit in front of LLM APIs and provide:

- Token telemetry for request and streaming-response workflows
- A thread-safe LRU cache for prompt and embedding lookups
- SIMD-friendly vector similarity calculations
- REST and Server-Sent Events (SSE) endpoints using cpp-httplib
- Redis integration for shared cache storage
- GoogleTest coverage for cache behavior and concurrent access

## Building

```bash
cmake -B build -DHYPERCACHE_BUILD_SERVER=ON
cmake --build build --config Release
```

The server runs on port 18080 with these endpoints:

| Method | Path | Description |
|--------|------|-------------|
| GET | /health | Health check |
| GET | /cache/:key | Lookup cache entry |
| PUT | /cache/:key | Store cache entry |
| DELETE | /cache/:key | Remove cache entry |
| GET | /cache | Cache info |
| GET | /telemetry | Token telemetry |
| POST | /similarity | Compute cosine similarity |
| GET | /stream | SSE stream |

## Status

Early development. APIs and implementation details may change.

## License

The community edition is licensed under the GNU Affero General Public License v3.0. See [LICENSE](LICENSE).

Commercial users who need closed-source integration, proprietary modifications, or commercial redistribution must obtain a separate commercial license from the copyright holder.

Commercial licensing contact: mahashreyaa@gmail.com
