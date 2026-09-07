#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace hypercache::cache {

template <typename Key, typename Value>
class LruCache {
public:
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    std::optional<Value> get(const Key& key) {
        std::unique_lock lock(mutex_);
        const auto found = entries_.find(key);
        if (found == entries_.end()) return std::nullopt;
        order_.splice(order_.begin(), order_, found->second.order);
        return found->second.value;
    }

    void put(Key key, Value value) {
        std::unique_lock lock(mutex_);
        if (capacity_ == 0) return;
        const auto found = entries_.find(key);
        if (found != entries_.end()) {
            found->second.value = std::move(value);
            order_.splice(order_.begin(), order_, found->second.order);
            return;
        }
        order_.push_front(key);
        entries_.emplace(order_.front(), Entry{std::move(value), order_.begin()});
        if (entries_.size() > capacity_) {
            entries_.erase(order_.back());
            order_.pop_back();
        }
    }

    std::size_t size() const {
        std::shared_lock lock(mutex_);
        return entries_.size();
    }

private:
    struct Entry { Value value; typename std::list<Key>::iterator order; };
    std::size_t capacity_;
    mutable std::shared_mutex mutex_;
    std::list<Key> order_;
    std::unordered_map<Key, Entry> entries_;
};

} // namespace hypercache::cache
