#pragma once

#include <cassert>
#include <cstddef>
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

        LinearAllocator(LinearAllocator&&) noexcept;
        LinearAllocator& operator=(LinearAllocator&&) noexcept;

        ~LinearAllocator();

        [[nodiscard]] std::byte* allocate(std::size_t size, std::size_t alignment=alignof(std::max_align_t)) noexcept;
        void reset() noexcept;

        [[nodiscard]] std::size_t capacity() const noexcept;
        [[nodiscard]] std::size_t allocation_count() const noexcept;
        [[nodiscard]] std::size_t peak_usage() const noexcept;
        [[nodiscard]] std::size_t used() const noexcept;
        [[nodiscard]] std::size_t remaining() const noexcept;
    };
}