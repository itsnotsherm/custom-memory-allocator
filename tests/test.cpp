#include <gtest/gtest.h>
#include "utils.hpp"

/* align_up */
TEST(AlignUp, AlreadyAlignedValueIsUnchanged) {
    EXPECT_EQ(alloc::align_up(0,  4),  0u);
    EXPECT_EQ(alloc::align_up(8,  8),  8u);
    EXPECT_EQ(alloc::align_up(16, 16), 16u);
    EXPECT_EQ(alloc::align_up(64, 32), 64u);
}

TEST(AlignUp, UnalignedValueIsRoundedUp) {
    EXPECT_EQ(alloc::align_up(1,  4),  4u);
    EXPECT_EQ(alloc::align_up(3,  4),  4u);
    EXPECT_EQ(alloc::align_up(5,  8),  8u);
    EXPECT_EQ(alloc::align_up(9,  8),  16u);
    EXPECT_EQ(alloc::align_up(17, 16), 32u);
}

TEST(AlignUp, AlignmentOfOneNeverChangesValue) {
    EXPECT_EQ(alloc::align_up(0,  1), 0u);
    EXPECT_EQ(alloc::align_up(1,  1), 1u);
    EXPECT_EQ(alloc::align_up(7,  1), 7u);
    EXPECT_EQ(alloc::align_up(99, 1), 99u);
}

TEST(AlignUp, ResultIsAlwaysAMultipleOfAlignment) {
    for (std::size_t align : {1u, 2u, 4u, 8u, 16u, 32u, 64u}) {
        for (std::size_t val = 0; val < 130; ++val) {
            std::size_t result = alloc::align_up(val, align);
            EXPECT_EQ(result % align, 0u) << "val=" << val << " align=" << align;
            EXPECT_GE(result, val)        << "val=" << val << " align=" << align;
            EXPECT_LT(result, val + align) << "val=" << val << " align=" << align;
        }
    }
}

TEST(AlignUp, IsConstexpr) {
    // If this compiles, align_up is usable at compile time.
    constexpr std::size_t result = alloc::align_up(13, 8);
    static_assert(result == 16, "align_up must be constexpr-correct");
    EXPECT_EQ(result, 16u);
}

/* is_aligned */
TEST(IsAligned, AlignedAddressesReturnTrue) {
    EXPECT_TRUE(alloc::is_aligned(0,   4));
    EXPECT_TRUE(alloc::is_aligned(8,   8));
    EXPECT_TRUE(alloc::is_aligned(16,  16));
    EXPECT_TRUE(alloc::is_aligned(32,  32));
    EXPECT_TRUE(alloc::is_aligned(128, 64));
}

TEST(IsAligned, UnalignedAddressesReturnFalse) {
    EXPECT_FALSE(alloc::is_aligned(1,  4));
    EXPECT_FALSE(alloc::is_aligned(3,  4));
    EXPECT_FALSE(alloc::is_aligned(5,  8));
    EXPECT_FALSE(alloc::is_aligned(15, 16));
    EXPECT_FALSE(alloc::is_aligned(33, 32));
}

TEST(IsAligned, ZeroIsAlignedToAnything) {
    for (std::size_t align : {1u, 2u, 4u, 8u, 16u, 32u, 64u}) {
        EXPECT_TRUE(alloc::is_aligned(0, align)) << "align=" << align;
    }
}

TEST(IsAligned, AlignmentOfOneMatchesEverything) {
    for (std::uintptr_t addr = 0; addr < 20; ++addr) {
        EXPECT_TRUE(alloc::is_aligned(addr, 1)) << "addr=" << addr;
    }
}

TEST(IsAligned, IsConstexpr) {
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
