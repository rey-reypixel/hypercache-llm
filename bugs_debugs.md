# Bugs & Debugs

Went through the whole project and wrote down everything that's broken or half-done, plus how I'm planning to fix each one. Going to knock these out one at a time and tick them off as I go.

CI has been red on `main` for a while now. I spent ~20 commits fiddling with CMake versions thinking that was the problem. It wasn't. So first priority is getting CI green, because until then none of the "verified" stuff in the README is actually verified.

---

## Part 1 — CI is broken

### 1. google-benchmark tries to download googletest and dies
- [x] **Status:** fixed. Configure goes through now (checked in an Ubuntu 24.04 container, same as CI)

**What's happening:** Every Linux job fails at configure with:

```
CMake step for googletest failed: 1
```

That's coming from inside `benchmark-src/cmake/GoogleTest.cmake`. google-benchmark builds its own tests by default, and to do that it tries to pull googletest. That's what's failing, not the CMake version.

**Plan:**
- Set `BENCHMARK_ENABLE_TESTING OFF` (and `BENCHMARK_ENABLE_GTEST_TESTS OFF`) before fetching benchmark.
- Only fetch benchmark at all when `HYPERCACHE_BUILD_BENCHMARKS` is ON. No reason to download it otherwise.

### 2. Unit tests never actually run in CI
- [x] **Status:** fixed. GoogleTest v1.15.2 now comes through FetchContent. Tested in a clean container with no system gtest, and all 37 tests ran

**What's happening:** CI doesn't install GoogleTest, so `find_package(GTest QUIET)` quietly fails and CMake just prints "GTest not found; unit tests will not be built". CI then "passes" tests that don't exist.

**Plan:** Pull GoogleTest in through FetchContent like the other deps, so it works the same everywhere (Linux, Windows, Docker, my machine) without anyone needing to install anything.

### 3. Redis tests aren't registered with ctest
- [x] **Status:** fixed

**What's happening:** `hypercache_redis_tests` gets built but there's no `gtest_discover_tests` for it, so `ctest` never runs it.

**Plan:** Add `gtest_discover_tests(hypercache_redis_tests)`.

### 4. Valgrind job can't fail
- [x] **Status:** fixed. Added `set -o pipefail`. Also found the Valgrind *server* step was running `valgrind timeout ./server`, so it was checking `timeout`, not our server. Flipped it to `timeout 30 valgrind ./server`

**What's happening:** `valgrind ... ./hypercache_tests 2>&1 | tail -50`. The step's exit code comes from `tail`, not Valgrind, so leaks would never fail the build.

**Plan:** Add `set -o pipefail` to the step (or drop the pipe).

### 5. Typo in the curl checks
- [x] **Status:** fixed. While I was in there: the separate "Test similarity endpoint" step ran after the LRU server's 10s timeout had probably already killed it, so I moved that check into the LRU step. Also added DELETE (expects 404 after) and `/stream` checks

**What's happening:** Three places hit `/cache.test` instead of `/cache/test`. They'd 404 and fail the job once we actually get that far.

**Plan:** Fix the paths.

### 6. Leftover `cmake/policy.cmake`
- [x] **Status:** fixed (deleted)

**What's happening:** Leftover from the CMake-version rabbit hole. Nothing uses it anymore.

**Plan:** Delete it.

### 7. Windows job is probably going to break
- [ ] **Status:** confirmed broken on PR #1. Fix drafted but not committed

**What actually happened:** It died at "Start Redis" with `Cannot find any service with service name 'redis'`. The Chocolatey `redis-64` package doesn't set up a Windows service.

**Drafted fix (uncommitted):** Dropped the Redis install/start steps from the Windows job and build with `HYPERCACHE_BUILD_REDIS=OFF`. Windows then only covers LRU, similarity and telemetry. Haven't seen whether MSVC compiles the rest of the code yet.

**What's happening:** hiredis on MSVC plus the Chocolatey Redis package is untested. The Redis tests also need a live server.

**Plan:** Get Linux green first, then look at the Windows log. Probably build Windows with `HYPERCACHE_BUILD_REDIS=OFF` and just run the LRU/similarity/telemetry tests there.

---

## Part 1.5 — The code doesn't compile (found after fixing #1)

Once configure worked, I did a full build in the Ubuntu container, and turns out a lot of this code has never compiled at all. CI was dying at configure, so nobody ever saw these. This is the real blocker.

### 17. `lru_cache.cpp` missing `#include <string>`
- [x] **Status:** fixed

### 18. `<hiredis/hiredis.h>` not found
- [x] **Status:** fixed

**What was happening:** hiredis puts `hiredis.h` at the root of its checkout (`_deps/hiredis-src/hiredis.h`), so `#include <hiredis/hiredis.h>` couldn't find it.

**Fix:** Told FetchContent to check hiredis out into `_deps/hiredis`, and added `_deps` to the include path.

### 19. Redis code doesn't compile at all
- [x] **Status:** fixed. Everything builds with Redis ON, and all 12 Redis tests pass against a real redis-server (30 runs, no flakes)

**What I changed:**
- Forward-declared `redisContext` in the pool header.
- Deleted the pool's move constructor/assignment. It was broken anyway: the reaper thread holds `this`, so moving the pool would leave the thread pointing at the old object.
- Added `to_pool_config()` to map `RedisCacheBackend::Config` to `RedisConnectionPool::Config`.
- Replaced the `Config config = {}` default arg with a separate no-arg constructor. Also added the `(host, port)` constructor the tests were already using.
- The reaper used to `sleep_for(10s)`, so `stop()` could hang for 10 seconds. Now it waits on a condition variable and `stop()` wakes it up immediately.
- If `start()` fails partway (Redis down), it now cleans up the connections it already made instead of leaking them.
- `is_connected()` on a moved-from object used to dereference a null `impl_`. Now it's null-safe.
- Linked hiredis PUBLIC so tests that call `redisCommand` directly can link. Added the missing `<memory>` and `<cstdarg>` includes, and dropped an unused `<hiredis/async.h>`.
- `RedisCacheBackend.Size` failed on any Redis that already had `size_key` from an earlier run. The test now deletes the key first.

**What's happening:** Pretty much all of `redis_connection_pool` and `redis_cache_backend`:
- `redis_connection_pool.hpp` uses `redisContext` without including or forward-declaring it.
- `RedisConnectionPool` gets built from a `RedisCacheBackend::Config`, but there's no constructor for that. They're two different Config structs.
- `created_` is a plain `size_t` but the .cpp calls `.load()` on it like it's atomic.
- `RedisCacheBackend::Config config = {}` as a default argument doesn't compile (nested struct with default member initializers, a known GCC gotcha).

**Plan:** Go through the Redis files properly and make the header and .cpp agree. Map the backend Config to the pool Config. Once it builds, run the Redis tests against a real redis-server in the container.

### 20. Server doesn't compile: `req.matches[1]` isn't a string_view
- [x] **Status:** fixed. Switched to `req.path_params.at("key")`, and added the missing `<iostream>` too

**What's happening:** Routes use `/cache/:key` (httplib's path-param style), but the handlers read `req.matches[1]`, which is for regex routes. It's a `sub_match` and doesn't convert to `string_view`. Even if it did compile, `matches` would be empty for `:key` routes.

**Plan:** Use `req.path_params.at("key")`.

### 21. Benchmarks don't compile
- [x] **Status:** fixed

**What was happening:** `lru_cache_benchmark.cpp` used `std::thread` without `#include <thread>`. After fixing that, it failed to link with `undefined reference to 'main'`, because none of the benchmark files have `BENCHMARK_MAIN()`.

**Fix:** Added the include and linked `benchmark::benchmark_main`.

### 23. `/stream` never ends
- [x] **Status:** fixed. Added `sink.done()`, and curl now gets 3 events and exits cleanly

**What's happening:** The chunked content provider writes its 3 lines and returns `true` but never calls `sink.done()`. httplib keeps calling it, so the stream repeats forever and curl just hangs there.

**Plan:** Call `sink.done()` after the last line.

### 22. LRU tests are failing
- [x] **Status:** fixed. `PutGetRemove` was #10. The two concurrent tests had wrong expectations, not a cache bug:
  - `ConcurrentReadWrite` expected puts to be exactly half the ops, but it picks get or put with a random coin flip. Now it checks gets + puts == total ops (every get should hit, since nothing gets evicted there).
  - `ConcurrentEvictionUnderLoad` expected every put-then-get to hit. With 16 threads fighting over 100 slots, another thread can evict your key in between. Now it checks some hits and that size stays at 100.

  Ran all 24 tests 20x in a row, no flakes.

**What's happening:** With Redis off, core + tests build, and 3 tests fail:
- `LruCacheBackend.PutGetRemove`: that's bug #10 (remove doesn't remove).
- `LruCacheBackend.ConcurrentReadWrite`
- `LruCacheBackend.ConcurrentEvictionUnderLoad`

**Plan:** Fix #10 first, then look at the concurrent ones with `--output-on-failure`. They might be expecting something about remove too.

### 24. Docker image never built
- [ ] **Status:** fixed locally, not committed

**What's happening:** The runtime stage does `useradd -r -u 1000 ...`, but Ubuntu 24.04 already ships a user called `ubuntu` with UID 1000. So `useradd` exits with code 4 and the whole image build fails. Nobody noticed because CI only builds Docker on `main` after everything else passes, and it never did.

**Fix:** Dropped the hard-coded `-u 1000`. `docker build` finishes now.

### 25. AddressSanitizer job fails on hiredis's own tests
- [ ] **Status:** fix drafted, not committed

**What's happening:** Our 37 tests all passed under ASan. What failed was `hiredis-test`, hiredis's own test suite, which FetchContent pulled into our ctest run. ASan flagged a `global-buffer-overflow` in *their* test code, not ours.

**Drafted fix:** `set(DISABLE_TESTS ON ...)` before fetching hiredis, so their tests don't get registered with our ctest.

### 26. `RedisCacheBackend.Size` is flaky in CI
- [ ] **Status:** fix drafted, not committed

**What's happening:** CI runs `ctest -j$(nproc)`, so several Redis tests run at the same time against the same Redis server. One test's DELETE can land between `Size`'s "before" and "after" reads, so the count doesn't go up. It passed locally because I ran the test binary directly (sequential), not through `ctest -j`.

**Drafted fix:** `gtest_discover_tests(hypercache_redis_tests PROPERTIES RESOURCE_LOCK redis)`, so ctest never runs two Redis tests at once.

---

## Part 2 — Actual bugs in the code

### 8. `--cache redis` never works in the server
- [x] **Status:** fixed. Added the compile define. That exposed the Redis branch in `app.cpp` compiling for the first time and failing: it called `connect()` on the base `CacheBackend` pointer, which has no `connect()`. Now it connects on the concrete `RedisCacheBackend` first, then moves it into `cache_`. Checked end to end: PUT through the server shows up in `redis-cli`, DELETE removes it, and a bad Redis port exits with a clean error

**What's happening:** `app.cpp` wraps all the Redis code in `#ifdef HYPERCACHE_BUILD_REDIS`, but that's only a CMake option. It never gets passed to the compiler as a define. So the Redis branch is always compiled out and the server always throws "Redis backend not available".

**Plan:** `target_compile_definitions(hypercache_server PRIVATE HYPERCACHE_BUILD_REDIS)` when Redis is ON.

### 9. Docker ignores all the env vars
- [ ] **Status:** in progress, not committed

**Done so far:** The server now reads `CACHE_TYPE`, `REDIS_HOST`, `REDIS_PORT`, `REDIS_POOL_MIN` and `REDIS_POOL_MAX` as defaults, and CLI flags still override them. Removed the broken `CMD` from the Dockerfile. The image builds now (see #24). **Still to do:** actually run the container with and without `CACHE_TYPE=redis` and confirm it picks the right backend.

**What's happening:** The Dockerfile's `CMD` is in exec form (`["--cache", "${CACHE_TYPE}", ...]`). Exec form doesn't expand variables, so the server literally gets the string `${CACHE_TYPE}`. `CACHE_TYPE=redis` does nothing.

**Plan:** Small `entrypoint.sh` that builds the args from env vars and `exec`s the server. Or have the server read env vars itself as fallbacks. I'm leaning toward the second, since it's cleaner and works outside Docker too.

### 10. DELETE doesn't delete
- [x] **Status:** fixed. Added `LruCache::erase()`, and `remove()` now calls it. Added tests: remove clears `exists()`/`size()`, remove frees the slot (no extra eviction), and removing a missing key is a no-op

**What's happening:** `LruCacheBackend::remove` just does `put(key, "")`. The key sticks around with an empty value, `exists()` still says true, and it still takes a slot.

**Plan:** Add a real `erase()` to `LruCache` and call that. Add a test so this can't sneak back.

### 11. Redis calls can crash when the pool times out
- [ ] **Status:** open

**What's happening:** `acquire()` can hand back an empty guard on timeout, and then we call `redisCommand(nullptr, ...)`. Dead connections are also never detected, so one Redis restart and the pool is full of broken contexts.

**Plan:**
- Check the guard, throw or return a clean error if it's empty.
- Check `ctx->err` after commands and drop and reconnect bad connections instead of putting them back in the pool.

### 12. SIMD "runtime dispatch" isn't runtime, and SSE uses FMA
- [ ] **Status:** open

**What's happening:**
- Dispatch is a compile-time `#if`, and we compile everything with `-mavx2 -mfma` (`/arch:AVX2` on MSVC). So the binary just crashes on a CPU without AVX2.
- The "SSE4.1" path uses `_mm_fmadd_ps`, which is FMA, not SSE4.1.

**Plan:**
- Use SSE-only ops in the SSE path (`_mm_mul_ps` + `_mm_add_ps`).
- Put the AVX2 code behind `__attribute__((target("avx2,fma")))` on GCC/Clang instead of global flags.
- Pick the path at startup with a CPUID check (`__builtin_cpu_supports` / `__cpuid` on MSVC).

### 13. Server can't be stopped
- [ ] **Status:** open

**What's happening:** `App::stop()` only flips a bool. The `httplib::Server` is a local inside `run()`, so nothing can actually call `svr.stop()`. No Ctrl+C / SIGTERM handling either, which matters in Docker.

**Plan:** Make the server a member, have `stop()` call `svr_.stop()`, and hook SIGINT/SIGTERM to it.

### 14. Bad CLI args crash the server
- [ ] **Status:** open

**What's happening:** `std::stoi` throws on junk like `--port abc`, and that's outside the try block.

**Plan:** Move arg parsing inside the try, and print a proper error and the usage text.

### 15. Telemetry numbers are kind of fake
- [ ] **Status:** open

**What's happening:**
- Every endpoint calls `record_request(1)`, so "request tokens" is really just a request count.
- `endpoint_counts_` exists but nothing ever writes to it.

**Plan:**
- Count per-endpoint hits properly.
- Keep token counting honest. For now either an approximate count (whitespace/char based, clearly labeled as approximate) or remove the field until we have a real tokenizer.

### 16. Small CMake stuff
- [ ] **Status:** open

**What's happening:**
- `set_target_properties(... COMPILE_FLAGS "${CMAKE_CXX_FLAGS}")` does nothing useful.
- cpp-httplib is pinned to `master`, so builds can break any day.

**Plan:** Remove the no-op line and pin httplib to a release tag.

---

## Part 3 — The big missing pieces (after bugs are fixed)

These aren't bugs exactly. They're the stuff the README promises that doesn't exist yet:

- **No actual LLM gateway.** Nothing forwards requests to OpenAI / Anthropic / whatever. Right now it's a key-value cache with an HTTP API.
- **No semantic cache.** The similarity engine is just a standalone `/similarity` endpoint. Nothing embeds prompts or checks "is this close enough to something we already answered".
- **`/stream` is a hardcoded demo.** It's 3 fixed lines, not real streaming.
- **No TTL on the LRU cache.** Redis has `set_ttl` but the backend doesn't expose it.
- **No auth / rate limiting / config file.**
- **No server integration tests.** Nothing spins up the server and hits the endpoints from tests.
- **README oversells.** It says ASan/TSan/Valgrind are clean and Windows CI works, but none of that has run successfully yet. I'll update the README once things actually pass.

---

## Order I'm going in

1. CI stuff (1–7) plus making it compile (17–22). Get it green so every fix after this gets checked automatically.
2. Redis define (8), Docker CMD (9), DELETE (10). Quick wins, and they're what people hit first.
3. Redis pool safety (11), SIMD dispatch (12).
4. Shutdown (13), CLI parsing (14), telemetry (15), CMake cleanup (16).
5. Then start on Part 3: gateway + semantic cache.

## Log

Notes as I fix things. What worked, what didn't, anything weird.

**2026-09-23**
- Fixed #1. The whole CMake-version saga was a red herring. The actual failure was google-benchmark trying to fetch googletest for its own test suite. Turning off `BENCHMARK_ENABLE_TESTING` fixed configure.
- I can't build locally (no compiler on my Windows box), so I'm testing in an `ubuntu:24.04` Docker container, which matches CI.
- With configure fixed, the build shows the real state: Redis, server and benchmarks never compiled. Added those as #17–#22. Fixed #17 and #18 on the way.
- Core library + unit tests (LRU, similarity, telemetry) do build with Redis off. 3 LRU tests fail.
- Fixed #20 and #21. With Redis off, everything builds. Ran the server in the container and hit every endpoint: PUT/GET/404/size/similarity/telemetry all respond correctly. Benchmarks run too.
- Found #23 while testing: `/stream` loops forever. Fixed with one line, `sink.done()`.
- Fixed #10 + #22. With Redis off, everything builds and all tests pass. Checked PUT, then DELETE, then GET through the real server: returns 404 and size 0.
- Next up: the big one, #19 (Redis doesn't compile).
- Fixed #19. Bigger than the others, but it wasn't a rewrite, mostly the header and .cpp not agreeing with each other. Now the full build (server + Redis + tests + benchmarks) compiles clean on Ubuntu 24.04, and all 36 tests pass.
- Heads-up: the Redis pool tests don't `GTEST_SKIP` when Redis isn't running, they just fail. Fine for CI since it starts Redis, but worth knowing locally.
- Fixed CI items #2 to #6, plus #8. Ran the CI build-and-test job in a clean Ubuntu container: 37/37 tests pass, and both the LRU and Redis server checks pass. Haven't pushed yet, so real CI hasn't seen any of this. The ASan/TSan/Valgrind jobs will probably turn up new stuff the first time they actually run.

**2026-09-26**
- Pushed everything to branch `fix/build-and-ci` and opened PR #1. First CI run that actually got past configure.
- Results: **TSan, Valgrind and Benchmarks passed.** Three jobs failed: build-and-test (#26), AddressSanitizer (#25) and Windows (#7). None of them were bugs in the main code. They were a flaky test setup, hiredis's own tests, and Redis not existing on the Windows runner.
- Started #9 and hit #24 on the way (the Docker image had literally never built).
- Paused here. Fixes for #7, #9, #24, #25 and #26 are sitting in the working tree, **uncommitted and not verified in CI**. PR #1 still has the 3 red checks.

## Where I left off

- Branch: `fix/build-and-ci`, PR #1 open, 7 commits pushed.
- Uncommitted: `CMakeLists.txt` + `ci.yml` (#7, #25, #26), `app.cpp` + `Dockerfile` (#9, #24).
- To pick back up:
  1. Run `ctest -j` locally with Redis and confirm #25/#26 are fixed.
  2. Run the Docker image with and without `CACHE_TYPE=redis` to finish #9.
  3. Commit, push, and see if CI goes fully green.
- Also: Auto-fix is still switched on for PR #1 in the app. Turn it off if I don't want fixes auto-pushed.
