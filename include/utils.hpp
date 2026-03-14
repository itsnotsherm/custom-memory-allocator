#pragma once

#include <cstddef>
#include <cstdint>

namespace alloc {
    [[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t alignment) {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    [[nodiscard]] constexpr bool is_aligned(std::uintptr_t ptr, std::size_t alignment) {
        return (ptr & (alignment - 1)) == 0;
    }
}