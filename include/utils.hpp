#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace alloc {
    /**
     * @brief Rounds @p value up to the next multiple of @p alignment.
     *
     * Uses the identity `(value + alignment - 1) & ~(alignment - 1)`:
     * adding `alignment - 1` ensures we overshoot the next boundary, then
     * masking off the least significant bits snaps back down to it.
     *
     * @param value     The value to align. Must satisfy `value <= SIZE_MAX - (alignment - 1)` to prevent wraparound.
     * @param alignment Alignment boundary in bytes. Must be a non-zero power of two.
     * @return The smallest multiple of @p alignment that is >= @p value.
     */
    [[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
        /* alignment must be non-zero and a power of two */
        assert(alignment != 0);
        assert((alignment & (alignment - 1)) == 0);
        /* prevent value from silent overflow */
        assert(value <= SIZE_MAX - (alignment - 1));

        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    /**
     * @brief Checks whether a pointer address is aligned to the given boundary.
     *
     * A pointer is aligned when its address has no set bits below the alignment boundary,
     * i.e. `(ptr & (alignment - 1)) == 0`.
     *
     * @param ptr       The pointer address to check.
     * @param alignment Alignment boundary in bytes. Must be a non-zero power of two.
     * @return `true` if @p ptr is a multiple of @p alignment, `false` otherwise.
     */
    [[nodiscard]] constexpr bool is_aligned(std::uintptr_t ptr, std::size_t alignment) noexcept {
        /* alignment must be non-zero and a power of two */
        assert(alignment != 0);
        assert((alignment & (alignment - 1)) == 0);

        return (ptr & (alignment - 1)) == 0;
    }
}