#include <gtest/gtest.h>
#include "utils.hpp"

/* align_up */
TEST(TestAlignUp, ValueAlreadyAligned) {
    EXPECT_EQ(alloc::align_up(0,  4),  0u);
    EXPECT_EQ(alloc::align_up(8,  8),  8u);
    EXPECT_EQ(alloc::align_up(16, 16), 16u);
    EXPECT_EQ(alloc::align_up(64, 32), 64u);
}

TEST(TestAlignUp, UnalignedValueRoundedUp) {
    EXPECT_EQ(alloc::align_up(1,  4),  4u);
    EXPECT_EQ(alloc::align_up(3,  4),  4u);
    EXPECT_EQ(alloc::align_up(5,  8),  8u);
    EXPECT_EQ(alloc::align_up(9,  8),  16u);
    EXPECT_EQ(alloc::align_up(17, 16), 32u);
}

TEST(TestAlignUp, AlignmentOfOne) {
    EXPECT_EQ(alloc::align_up(0,  1), 0u);
    EXPECT_EQ(alloc::align_up(1,  1), 1u);
    EXPECT_EQ(alloc::align_up(7,  1), 7u);
    EXPECT_EQ(alloc::align_up(99, 1), 99u);
}

TEST(TestAlignUp, ResultIsMultipleOfAlignment) {
    for (std::size_t align : {1u, 2u, 4u, 8u, 16u, 32u, 64u}) {
        for (std::size_t val = 0; val < 130; ++val) {
            std::size_t result = alloc::align_up(val, align);
            EXPECT_EQ(result % align, 0u) << "val=" << val << " align=" << align;
            EXPECT_GE(result, val)        << "val=" << val << " align=" << align;
            EXPECT_LT(result, val + align) << "val=" << val << " align=" << align;
        }
    }
}

TEST(TestAlignUp, IsConstexpr) {
    // If this compiles, align_up is usable at compile time.
    constexpr std::size_t result = alloc::align_up(13, 8);
    static_assert(result == 16, "align_up must be constexpr-correct");
    EXPECT_EQ(result, 16u);
}

TEST(TestAlignUp, LargeAlignments) {
    EXPECT_EQ(alloc::align_up(1,    64),   64u);   // cache line
    EXPECT_EQ(alloc::align_up(64,   64),   64u);   // already aligned
    EXPECT_EQ(alloc::align_up(65,   64),   128u);
    EXPECT_EQ(alloc::align_up(1,    4096), 4096u); // page
    EXPECT_EQ(alloc::align_up(4096, 4096), 4096u); // already aligned
    EXPECT_EQ(alloc::align_up(4097, 4096), 8192u);
}

TEST(TestAlignUp, NearOverflowBoundary) {
    // MAX_SIZE - 7 is the largest value safely alignable to 8; it is itself aligned to 8
    constexpr std::size_t val = SIZE_MAX - 7;
    EXPECT_EQ(alloc::align_up(val, 8), val);
    // One below: MAX_SIZE - 8, not aligned to 8, rounds up to MAX_SIZE - 7
    EXPECT_EQ(alloc::align_up(SIZE_MAX - 8, 8), val);
}

/* is_aligned */
TEST(TestIsAligned, AlignedAddresses) {
    EXPECT_TRUE(alloc::is_aligned(0,   4));
    EXPECT_TRUE(alloc::is_aligned(8,   8));
    EXPECT_TRUE(alloc::is_aligned(16,  16));
    EXPECT_TRUE(alloc::is_aligned(32,  32));
    EXPECT_TRUE(alloc::is_aligned(128, 64));
}

TEST(TestIsAligned, UnalignedAddresses) {
    EXPECT_FALSE(alloc::is_aligned(1,  4));
    EXPECT_FALSE(alloc::is_aligned(3,  4));
    EXPECT_FALSE(alloc::is_aligned(5,  8));
    EXPECT_FALSE(alloc::is_aligned(15, 16));
    EXPECT_FALSE(alloc::is_aligned(33, 32));
}

TEST(TestIsAligned, PtrIsZero) {
    for (std::size_t align : {1u, 2u, 4u, 8u, 16u, 32u, 64u}) {
        EXPECT_TRUE(alloc::is_aligned(0, align)) << "align=" << align;
    }
}

TEST(TestIsAligned, AlignmentOfOne) {
    for (std::uintptr_t addr = 0; addr < 20; ++addr) {
        EXPECT_TRUE(alloc::is_aligned(addr, 1)) << "addr=" << addr;
    }
}

TEST(TestIsAligned, IsConstexpr) {
    constexpr bool aligned     = alloc::is_aligned(16, 8);
    constexpr bool not_aligned = alloc::is_aligned(13, 8);
    static_assert(aligned,     "16 should be aligned to 8");
    static_assert(!not_aligned, "13 should not be aligned to 8");
    EXPECT_TRUE(aligned);
    EXPECT_FALSE(not_aligned);
}

/* align_up + is_aligned */
TEST(Utils, AlignUpResultSatisfiesIsAligned) {
    for (std::size_t align : {1u, 2u, 4u, 8u, 16u, 32u}) {
        for (std::size_t val = 0; val < 100; ++val) {
            std::size_t result = alloc::align_up(val, align);
            EXPECT_TRUE(alloc::is_aligned(static_cast<std::uintptr_t>(result), align))
                << "val=" << val << " align=" << align;
        }
    }
}

#ifndef NDEBUG
TEST(TestAlignUp, ZeroAlignment) {
    EXPECT_DEATH((void) alloc::align_up(8, 0), "");
}

TEST(TestAlignUp, NonPowerOfTwoAlignment) {
    EXPECT_DEATH((void) alloc::align_up(8, 3), "");
    EXPECT_DEATH((void) alloc::align_up(8, 5), "");
    EXPECT_DEATH((void) alloc::align_up(8, 6), "");
}

TEST(TestAlignUp, OverflowAborts) {
    // MAX_SIZE - 6 cannot be aligned to 8 without wrapping
    EXPECT_DEATH((void) alloc::align_up(SIZE_MAX - 6, 8), "");
    EXPECT_DEATH((void) alloc::align_up(SIZE_MAX, 8), "");
}

TEST(TestIsAligned, ZeroAlignment) {
    EXPECT_DEATH((void) alloc::is_aligned(8, 0), "");
}

TEST(TestIsAligned, NonPowerOfTwoAlignment) {
    EXPECT_DEATH((void) alloc::is_aligned(8, 3), "");
    EXPECT_DEATH((void) alloc::is_aligned(8, 5), "");
    EXPECT_DEATH((void) alloc::is_aligned(8, 6), "");
}
#endif
