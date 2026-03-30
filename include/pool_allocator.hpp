#pragma once

#include <algorithm>
#include <cstddef>
#include "utils.hpp"

namespace alloc {
    /**
     * @brief A fixed-capacity pool allocator for uniform-sized objects of type @p T.
     *
     * Manages a contiguous buffer divided into equally-sized, aligned blocks. Free blocks
     * are tracked via an intrusive singly-linked list, enabling O(1) allocation and
     * deallocation with no fragmentation.
     *
     * Best suited for workloads that repeatedly allocate and free many objects of the same
     * type (e.g. particle systems, node-based data structures, object pools in game engines).
     *
     * Not copyable. Move-only ownership semantics.
     *
     * @tparam T The type of objects managed by this pool. Each block is sized and aligned
     *           to hold exactly one @p T.
     */
    template <class T>
    class PoolAllocator {
    private:
        /**
         * @brief Intrusive free-list node overlaid on each unused block.
         *
         * When a block is free, its memory is reinterpreted as a FreeBlock so the pool
         * can chain all free blocks together without any external bookkeeping.
         */
        struct FreeBlock {
            FreeBlock* next;
        };

        std::byte* buffer_{nullptr};
        std::byte* buffer_end_{nullptr};
        std::size_t total_blocks_{0};
        FreeBlock* free_list_{nullptr};

        std::size_t allocated_blocks_{0};
        std::size_t peak_usage_{0};

    public:
        static constexpr std::size_t kBlockAlignment =
            std::max(alignof(T), alignof(FreeBlock));

        static constexpr std::size_t kBlockSize =
            align_up(std::max(sizeof(T), sizeof(FreeBlock)), kBlockAlignment);

        static_assert(kBlockSize >= sizeof(T), "block must be large enough to hold T");
        static_assert(kBlockSize >= sizeof(FreeBlock), "block must be large enough to hold free list pointer");
        static_assert((kBlockAlignment & (kBlockAlignment-1)) == 0, "block alignment must be power-of-2");

        /**
         * @brief Constructs a PoolAllocator with the given number of blocks.
         *
         * Allocates a contiguous, aligned buffer large enough to hold @p blocks objects
         * of type @p T, then initializes the free list so every block is available.
         *
         * @param blocks Number of blocks for provision. Must be greater than zero.
         * @throws std::bad_alloc if the underlying allocation fails.
         */
        explicit PoolAllocator(std::size_t blocks);

        /// @brief Copy construction is disabled; use move semantics instead.
        PoolAllocator(const PoolAllocator&) = delete;
        /// @brief Copy assignment is disabled; use move semantics instead.
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        /**
         * @brief Move constructor. Transfers ownership of the backing buffer.
         * @param other The allocator to move from. Left in a valid but empty state.
         */
        PoolAllocator(PoolAllocator&& other) noexcept;

        /**
         * @brief Move assignment operator. Frees the current buffer, then takes ownership.
         * @param other The allocator to move from. Left in a valid but empty state.
         * @return Reference to this allocator.
         */
        PoolAllocator& operator=(PoolAllocator&& other) noexcept;

        /// @brief Destructor. Frees the backing buffer (safe to call on moved-from instances).
        ~PoolAllocator();

        /**
         * @brief Allocates one block from the pool.
         *
         * Pops the head of the free list and returns it as a pointer to @p T.
         * Returns nullptr rather than throwing if the pool is exhausted.
         *
         * @pre The allocator must not have been moved-from.
         *
         * @note The returned memory is uninitialized. Construct an object in-place with
         *       placement new before use.
         * @return Pointer to an uninitialized block sized and aligned for @p T,
         *         or nullptr if no free blocks remain.
         */
        [[nodiscard]] T* allocate() noexcept;

        /**
         * @brief Returns a previously allocated block to the pool.
         *
         * Pushes @p ptr back onto the head of the free list in O(1).
         *
         * @note Does not invoke the destructor of @p T. Call it explicitly before
         *       deallocating if the object has been constructed.
         * @param ptr Pointer previously returned by allocate(). Must not be nullptr,
         *            must belong to this pool, and must be block-aligned within the buffer.
         */
        void deallocate(T* ptr) noexcept;

        /**
         * @brief Resets the pool, making all blocks available again.
         *
         * Rebuilds the free list from scratch so every block is considered free.
         * Resets the allocated block counter but preserves peak_usage().
         *
         * @note Does not invoke destructors for any live objects. Ensure all
         *       constructed objects are destroyed before calling reset().
         */
        void reset() noexcept;

        /// @return Total number of blocks in the pool (fixed at construction).
        [[nodiscard]] std::size_t total_blocks() const noexcept;

        /// @return Number of blocks currently available for allocation.
        [[nodiscard]] std::size_t free_blocks() const noexcept;

        /// @return Number of blocks currently allocated.
        [[nodiscard]] std::size_t allocated_blocks() const noexcept;

        /// @return Highest number of simultaneously allocated blocks (persists across reset() calls).
        [[nodiscard]] std::size_t peak_usage() const noexcept;

    private:
        /**
         * @note Does not modify peak_usage_ (lifetime metric).
         */
        void init_free_list() noexcept {
            free_list_ = nullptr;
            for (std::size_t i = 0; i < total_blocks_; ++i) {
                auto* free_block = reinterpret_cast<FreeBlock*>(buffer_ + i * kBlockSize);
                free_block->next = free_list_;
                free_list_ = free_block;
            }
            allocated_blocks_ = 0;
        }
    };

    template <class T>
    PoolAllocator<T>::PoolAllocator(const std::size_t blocks) {
        assert(blocks > 0 && "no. of blocks must be a positive number");
        buffer_ = static_cast<std::byte*>(std::aligned_alloc(kBlockAlignment, blocks * kBlockSize));
        if (buffer_ == nullptr) {
            throw std::bad_alloc{};
        }

        buffer_end_ = buffer_ + blocks * kBlockSize;
        total_blocks_ = blocks;
        init_free_list();
        assert(free_list_ != nullptr);
    }

    template <class T>
    inline PoolAllocator<T>::PoolAllocator(PoolAllocator&& other) noexcept
        : buffer_{std::exchange(other.buffer_, nullptr)}
        , buffer_end_{std::exchange(other.buffer_end_, nullptr)}
        , total_blocks_{std::exchange(other.total_blocks_, 0)}
        , free_list_{std::exchange(other.free_list_, nullptr)}
        , allocated_blocks_{std::exchange(other.allocated_blocks_, 0)}
        , peak_usage_{std::exchange(other.peak_usage_, 0)}
    {}

    template <class T>
    inline PoolAllocator<T>& PoolAllocator<T>::operator=(PoolAllocator&& other) noexcept {
        if (this != &other) {
            std::free(buffer_);

            buffer_ = std::exchange(other.buffer_, nullptr);
            buffer_end_ = std::exchange(other.buffer_end_, nullptr);
            total_blocks_ = std::exchange(other.total_blocks_, 0);
            free_list_ = std::exchange(other.free_list_, nullptr);
            allocated_blocks_ = std::exchange(other.allocated_blocks_, 0);
            peak_usage_ = std::exchange(other.peak_usage_, 0);
        }

        return *this;
    }

    template <class T>
    inline PoolAllocator<T>::~PoolAllocator() {
        std::free(buffer_);
    }

    template <class T>
    inline T* PoolAllocator<T>::allocate() noexcept {
        if (free_list_ == nullptr)
            return nullptr;

        auto free_block = free_list_;
        free_list_ = free_list_->next;

        allocated_blocks_++;
        if (allocated_blocks_ > peak_usage_)
            peak_usage_ = allocated_blocks_;

        return reinterpret_cast<T*>(free_block);
    }

    template<class T>
    inline void PoolAllocator<T>::deallocate(T* ptr) noexcept {
        assert(ptr != nullptr);


        auto block = reinterpret_cast<FreeBlock*>(ptr);
        assert(reinterpret_cast<std::byte*>(block) >= buffer_ &&
                reinterpret_cast<std::byte*>(block) < buffer_end_);
        assert(static_cast<std::size_t>(reinterpret_cast<std::byte*>(block) - buffer_) % kBlockSize == 0);
        block->next = free_list_;
        free_list_ = block;
        allocated_blocks_--;
    }

    template<class T>
    inline void PoolAllocator<T>::reset() noexcept {
        init_free_list();
    }

    template <class T>
    inline std::size_t PoolAllocator<T>::total_blocks() const noexcept {
        return total_blocks_;
    }

    template <class T>
    inline std::size_t PoolAllocator<T>::free_blocks() const noexcept {
        return total_blocks_ - allocated_blocks_;
    }

    template <class T>
    inline std::size_t PoolAllocator<T>::allocated_blocks() const noexcept {
        return allocated_blocks_;
    }

    template <class T>
    inline std::size_t PoolAllocator<T>::peak_usage() const noexcept {
        return peak_usage_;
    }
}
