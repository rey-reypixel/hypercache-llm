cmake_minimum_required(VERSION 3.20)
project(HyperCacheLLM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HYPERCACHE_BUILD_TESTS "Build unit tests" ON)
option(HYPERCACHE_BUILD_SERVER "Build HTTP server" ON)
option(HYPERCACHE_BUILD_REDIS "Build Redis integration" ON)

include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG master
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    hiredis
    GIT_REPOSITORY https://github.com/redis/hiredis.git
    GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(hiredis)

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
    target_compile_options(hypercache_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(hypercache_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

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
    target_link_libraries(hypercache_server PRIVATE hypercache_core httplib::httplib)
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