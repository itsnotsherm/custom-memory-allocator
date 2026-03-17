#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

#include "utils.hpp"

namespace alloc {
    /**
     * @brief A fast, non-freeing linear (bump pointer) allocator.
     *
     * Allocates memory by advancing a pointer through a contiguous buffer acquired during construction.
     * Individual allocations cannot be freed; the entire arena is reclaimed at once via reset().
     *
     * Best suited for batch-lifetime workloads where
     * many short-lived objects share a common deallocation point
     * (e.g. per-frame game data, request-scoped server processing).
     *
     * Not copyable. Move-only ownership semantics.
     */
    class LinearAllocator {
    private:
        std::byte* buffer_{nullptr};
        std::size_t capacity_{0};
        std::size_t offset_{0};

        std::size_t allocation_count_{0};
        std::size_t peak_usage_{0};

    public:
        /**
         * @brief Constructs a LinearAllocator with the given capacity.
         *
         * The actual allocated size is rounded up to the platform's maximum alignment boundary.
         *
         * @param size Minimum capacity in bytes. Must be greater than zero.
         * @throws std::bad_alloc if the underlying allocation fails.
         */
        explicit LinearAllocator(std::size_t size);

        /// @brief Copy construction is disabled; use move semantics instead.
        LinearAllocator(const LinearAllocator&) = delete;
        /// @brief Copy assignment is disabled; use move semantics instead.
        LinearAllocator& operator=(const LinearAllocator&) = delete;

        /**
         * @brief Move constructor. Transfers ownership of the buffer.
         * @param other The allocator to move from. Left in a valid but empty state.
         */
        LinearAllocator(LinearAllocator&& other) noexcept;

        /**
         * @brief Move assignment operator. Frees the current buffer, then takes ownership.
         * @param other The allocator to move from. Left in a valid but empty state.
         * @return Reference to this allocator.
         */
        LinearAllocator& operator=(LinearAllocator&& other) noexcept;

        /// @brief Destructor. Frees the backing buffer (safe to call on moved-from instances).
        ~LinearAllocator();

        /**
         * @brief Allocates a block of memory from the arena.
         *
         * Advances the internal bump pointer by the requested size (including any alignment padding).
         * Returns nullptr on failure rather than throwing.
         *
         * @param size      Number of bytes to allocate. Returns nullptr if zero.
         * @param alignment Required alignment in bytes. Must be a power of two.
         *                  Defaults to alignof(std::max_align_t).
         * @return Pointer to the allocated block, or nullptr if the arena is exhausted or @p size is zero.
         */
        [[nodiscard]] std::byte* allocate(std::size_t size, std::size_t alignment=alignof(std::max_align_t)) noexcept;

        /**
         * @brief Resets the allocator, reclaiming all memory at once.
         *
         * Sets the bump pointer and allocation count back to zero.
         *
         * @note Peak usage is preserved across resets.
         */
        void reset() noexcept;

        /// @return Total capacity of the backing buffer in bytes.
        [[nodiscard]] std::size_t capacity() const noexcept;

        /// @return Number of allocations in the current cycle (resets to zero on reset()).
        [[nodiscard]] std::size_t allocation_count() const noexcept;

        /// @return Highest number of bytes ever used (persists across reset() calls).
        [[nodiscard]] std::size_t peak_usage() const noexcept;

        /// @return Number of bytes currently in use.
        [[nodiscard]] std::size_t used() const noexcept;

        /// @return Number of bytes still available for allocation.
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
