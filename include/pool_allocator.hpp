#pragma once

#include <algorithm>
#include <cstddef>
#include "utils.hpp"

namespace alloc {
    template <class T>
    class PoolAllocator {
    private:
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

        static_assert(kBlockSize >= sizeof(T), "kBlockSize can hold at least T");
        static_assert(kBlockSize >= sizeof(FreeBlock), "kBlockSize can hold free list pointer");
        static_assert((kBlockAlignment & (kBlockAlignment-1)) == 0);

        explicit PoolAllocator(std::size_t blocks);

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        PoolAllocator(PoolAllocator&&) noexcept;
        PoolAllocator& operator=(PoolAllocator&&) noexcept;

        ~PoolAllocator();

        [[nodiscard]] T* allocate() noexcept;
        void deallocate(T* ptr) noexcept;
        void reset() noexcept;

        [[nodiscard]] std::size_t total_blocks() const noexcept;
        [[nodiscard]] std::size_t free_blocks() const noexcept;
        [[nodiscard]] std::size_t allocated_blocks() const noexcept;
        [[nodiscard]] std::size_t peak_usage() const noexcept;

    private:
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
