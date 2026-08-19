# HyperCache-LLM

High-performance LLM telemetry and cache gateway built with C++20.

HyperCache-LLM is designed to sit in front of LLM APIs and provide:

- Token telemetry for request and streaming-response workflows
- A thread-safe LRU cache for prompt and embedding lookups
- SIMD-friendly vector similarity calculations
- REST and Server-Sent Events (SSE) endpoints using Crow
- Redis integration for shared cache storage
- GoogleTest coverage for cache behavior and concurrent access

## Status

Early development. APIs and implementation details may change.

## License

The community edition is licensed under the GNU Affero General Public License v3.0. See [LICENSE](LICENSE).

Commercial users who need closed-source integration, proprietary modifications, or commercial redistribution must obtain a separate commercial license from the copyright holder.

Contact: add your licensing email here before publishing the project.
