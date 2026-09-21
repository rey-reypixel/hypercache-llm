#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace hypercache::cache {

class CacheBackend {
public:
    virtual ~CacheBackend() = default;

    virtual std::optional<std::string> get(std::string_view key) = 0;
    virtual void put(std::string_view key, std::string_view value) = 0;
    virtual void remove(std::string_view key) = 0;
    virtual bool exists(std::string_view key) = 0;
    virtual std::size_t size() = 0;
};

} // namespace hypercache::cache