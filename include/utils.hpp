#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace alloc {
    [[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t alignment) {
        /* alignment must be non-zero and a power of two */
        assert(alignment != 0);
        assert((alignment & (alignment - 1)) == 0);
        /* prevent value from silent overflow */
        assert(value <= SIZE_MAX - (alignment - 1));

        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    [[nodiscard]] constexpr bool is_aligned(std::uintptr_t ptr, std::size_t alignment) {
        /* alignment must be non-zero and a power of two */
        assert(alignment != 0);
        assert ((alignment & (alignment - 1)) == 0);

        return (ptr & (alignment - 1)) == 0;
    }
}