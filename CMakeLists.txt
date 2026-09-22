cmake_minimum_required(VERSION 3.20)

# CMake 4.x compatibility for fetched dependencies with old CMake versions
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(HyperCacheLLM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HYPERCACHE_BUILD_TESTS "Build unit tests" ON)
option(HYPERCACHE_BUILD_SERVER "Build HTTP server" ON)
option(HYPERCACHE_BUILD_REDIS "Build Redis integration" ON)
option(HYPERCACHE_BUILD_BENCHMARKS "Build benchmarks" ON)

include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG master
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(benchmark)

if(HYPERCACHE_BUILD_REDIS)
    FetchContent_Declare(
        hiredis
        GIT_REPOSITORY https://github.com/redis/hiredis.git
        GIT_TAG v1.2.0
    )
    FetchContent_MakeAvailable(hiredis)
endif()

find_package(Threads QUIET)

add_library(hypercache_core
    src/cache/lru_cache.cpp
    src/cache/lru_cache_backend.cpp
    src/similarity/similarity_engine.cpp
    src/telemetry/telemetry.cpp
)
target_include_directories(hypercache_core PUBLIC include ${httplib_SOURCE_DIR}/include)
target_compile_features(hypercache_core PUBLIC cxx_std_20)
if(Threads_FOUND)
    target_link_libraries(hypercache_core PUBLIC Threads::Threads)
endif()

if(MSVC)
    target_compile_options(hypercache_core PRIVATE /W4 /permissive- /arch:AVX2)
else()
    target_compile_options(hypercache_core PRIVATE -Wall -Wextra -Wpedantic -mavx2 -msse4.1 -mfma)
endif()

# Compile similarity engine with SIMD flags
set_target_properties(hypercache_core PROPERTIES
    COMPILE_FLAGS "${CMAKE_CXX_FLAGS}"
)

if(HYPERCACHE_BUILD_REDIS)
    add_library(hypercache_redis
        src/cache/redis_cache.cpp
        src/cache/redis_cache_backend.cpp
        src/cache/redis_connection_pool.cpp
    )
    target_include_directories(hypercache_redis PUBLIC include ${hiredis_SOURCE_DIR})
    target_link_libraries(hypercache_redis PRIVATE hypercache_core hiredis::hiredis)
    target_compile_features(hypercache_redis PRIVATE cxx_std_20)
endif()

if(HYPERCACHE_BUILD_SERVER)
    add_executable(hypercache_server
        src/server/app.cpp
    )
    target_link_libraries(hypercache_server PRIVATE hypercache_core httplib::httplib nlohmann_json::nlohmann_json)
    if(HYPERCACHE_BUILD_REDIS)
        target_link_libraries(hypercache_server PRIVATE hypercache_redis)
    endif()
    target_compile_features(hypercache_server PRIVATE cxx_std_20)
    target_include_directories(hypercache_server PRIVATE include ${httplib_SOURCE_DIR}/include)
endif()

if(HYPERCACHE_BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(hypercache_tests
            tests/cache/lru_cache_test.cpp
            tests/cache/lru_cache_backend_test.cpp
            tests/similarity/similarity_engine_test.cpp
            tests/telemetry/telemetry_test.cpp
        )
        target_link_libraries(hypercache_tests PRIVATE hypercache_core GTest::gtest_main)
        if(HYPERCACHE_BUILD_REDIS)
            target_link_libraries(hypercache_tests PRIVATE hypercache_redis)
            add_executable(hypercache_redis_tests
                tests/cache/redis_cache_test.cpp
                tests/cache/redis_cache_backend_test.cpp
                tests/cache/redis_connection_pool_test.cpp
            )
            target_link_libraries(hypercache_redis_tests PRIVATE hypercache_redis GTest::gtest_main)
        endif()
        include(GoogleTest)
        gtest_discover_tests(hypercache_tests)
    else()
        message(STATUS "GTest not found; unit tests will not be built")
    endif()
endif()

if(HYPERCACHE_BUILD_BENCHMARKS)
    add_executable(hypercache_benchmarks
        benchmarks/lru_cache_benchmark.cpp
        benchmarks/similarity_benchmark.cpp
        benchmarks/telemetry_benchmark.cpp
    )
    target_link_libraries(hypercache_benchmarks PRIVATE hypercache_core benchmark::benchmark)
    if(HYPERCACHE_BUILD_REDIS)
        target_link_libraries(hypercache_benchmarks PRIVATE hypercache_redis)
    endif()
    target_compile_features(hypercache_benchmarks PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(hypercache_benchmarks PRIVATE /W4 /permissive- /arch:AVX2)
    else()
        target_compile_options(hypercache_benchmarks PRIVATE -Wall -Wextra -Wpedantic -mavx2 -msse4.1 -mfma)
    endif()
endif()