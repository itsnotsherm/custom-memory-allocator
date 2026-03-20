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

        static constexpr std::size_t kBlockAlignment =
            std::max(alignof(T), alignof(FreeBlock));
        static constexpr std::size_t kBlockSize =
            align_up(std::max(sizeof(T), sizeof(FreeBlock)), kBlockAlignment);

        std::byte* buffer_{nullptr};
        std::size_t total_blocks_{0};
        FreeBlock* free_list_{nullptr};

        std::size_t allocated_blocks_{0};
        std::size_t peak_usage_{0};

    public:
        explicit PoolAllocator(std::size_t blocks);

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        PoolAllocator(PoolAllocator&&) noexcept;
        PoolAllocator& operator=(PoolAllocator&&) noexcept;

        ~PoolAllocator();

        [[nodiscard]] T* allocate() noexcept;
        void deallocate(T* ptr) noexcept;
        void reset() noexcept;

        [[nodiscard]] std::size_t block_size() const noexcept;
        [[nodiscard]] std::size_t total_blocks() const noexcept;
        [[nodiscard]] std::size_t free_blocks() const noexcept;
        [[nodiscard]] std::size_t allocated_blocks() const noexcept;
        [[nodiscard]] std::size_t peak_usage() const noexcept;
    };
}
