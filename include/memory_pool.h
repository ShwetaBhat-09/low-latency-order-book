#pragma once

#include <vector>
#include <cstddef>
#include <memory>

namespace orderbook {

// ObjectPool pre-allocates objects to avoid malloc/new on the hot path.
// Uses chunk-based allocation for pointer stability (no reallocation invalidation).
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t chunk_size = 65536)
        : chunk_size_(chunk_size) {
        free_list_.reserve(chunk_size);
        grow();
    }

    // Acquire an object from the pool (O(1), no allocation in steady state)
    T* acquire() {
        if (free_list_.empty()) {
            grow();
        }
        T* obj = free_list_.back();
        free_list_.pop_back();
        return obj;
    }

    // Release an object back to the pool (O(1))
    void release(T* obj) {
        free_list_.push_back(obj);
    }

    size_t available() const { return free_list_.size(); }

private:
    void grow() {
        // Allocate a new chunk — existing pointers remain valid
        auto chunk = std::make_unique<T[]>(chunk_size_);
        T* base = chunk.get();
        for (size_t i = 0; i < chunk_size_; ++i) {
            free_list_.push_back(&base[i]);
        }
        chunks_.push_back(std::move(chunk));
    }

    size_t chunk_size_;
    std::vector<std::unique_ptr<T[]>> chunks_; // owns the memory, never moves
    std::vector<T*> free_list_;
};

} // namespace orderbook
