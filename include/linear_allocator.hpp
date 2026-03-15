#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

#include "utils.hpp"

namespace alloc {
    // TODO: add Doxygen comments
    class LinearAllocator {
    private:
        std::byte* buffer_{nullptr};
        std::size_t capacity_{0};
        std::size_t offset_{0};

        std::size_t allocation_count_{0};
        std::size_t peak_usage_{0};

    public:
        explicit LinearAllocator(std::size_t size);

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;

        LinearAllocator(LinearAllocator&& other) noexcept;
        LinearAllocator& operator=(LinearAllocator&& other) noexcept;

        ~LinearAllocator();

        [[nodiscard]] std::byte* allocate(std::size_t size, std::size_t alignment=alignof(std::max_align_t)) noexcept;
        void reset() noexcept;

        [[nodiscard]] std::size_t capacity() const noexcept;
        [[nodiscard]] std::size_t allocation_count() const noexcept;
        [[nodiscard]] std::size_t peak_usage() const noexcept;
        [[nodiscard]] std::size_t used() const noexcept;
        [[nodiscard]] std::size_t remaining() const noexcept;
    };

    inline LinearAllocator::LinearAllocator(const std::size_t size) {
        assert(size > 0 && "allocator size must be non-zero and positive");

        constexpr auto alignment{alignof(std::max_align_t)};
        capacity_ = align_up(size, alignment);
        buffer_ = static_cast<std::byte*>(std::aligned_alloc(alignment, capacity_));
        if (buffer_ == nullptr) {
            throw std::bad_alloc{};
        }
    }

    inline LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
        : buffer_{std::exchange(other.buffer_, nullptr)}
        , capacity_{std::exchange(other.capacity_, 0)}
        , offset_{std::exchange(other.offset_, 0)}
        , allocation_count_{std::exchange(other.allocation_count_, 0)}
        , peak_usage_{std::exchange(other.peak_usage_, 0)}
    {}

    inline LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept {
        if (this != &other) {
            std::free(buffer_);

            buffer_ = std::exchange(other.buffer_, nullptr);
            capacity_ = std::exchange(other.capacity_, 0);
            offset_ = std::exchange(other.offset_, 0);

            allocation_count_ = std::exchange(other.allocation_count_, 0);
            peak_usage_ = std::exchange(other.peak_usage_, 0);
        }

        return *this;
    }

    inline LinearAllocator::~LinearAllocator() {
        std::free(buffer_);
    }

    inline std::byte* LinearAllocator::allocate(const std::size_t size, const std::size_t alignment) noexcept {
        assert((alignment > 0) && (alignment & (alignment - 1)) == 0 && "alignment must be positive and a power of 2");
        if (size == 0)
            return nullptr;

        const auto aligned_offset = align_up(offset_, alignment);
        if (aligned_offset > capacity_ || size > capacity_ - aligned_offset)
            return nullptr;

        offset_ = aligned_offset + size;
        allocation_count_++;
        if (offset_ > peak_usage_)
            peak_usage_ = offset_;

        return buffer_ + aligned_offset;
    }

    inline void LinearAllocator::reset() noexcept {
        offset_ = 0;
        allocation_count_ = 0;
    }

    inline std::size_t LinearAllocator::capacity() const noexcept {
        return capacity_;
    }

    inline std::size_t LinearAllocator::allocation_count() const noexcept {
        return allocation_count_;
    }

    inline std::size_t LinearAllocator::peak_usage() const noexcept {
        return peak_usage_;
    }

    inline std::size_t LinearAllocator::used() const noexcept {
        return offset_;
    }

    inline std::size_t LinearAllocator::remaining() const noexcept {
        return capacity_ - used();
    }
}
