#include <gtest/gtest.h>
#include "linear_allocator.hpp"

/* LinearAllocator - construction */
TEST(TestLinearAllocator, CapacityIsAtLeastRequestedSize) {
    alloc::LinearAllocator a{100};
    EXPECT_GE(a.capacity(), 100u);
}

TEST(TestLinearAllocator, CapacityAlignedToMaxAlign) {
    alloc::LinearAllocator a{100};
    EXPECT_EQ(a.capacity() % alignof(std::max_align_t), 0u);
}

TEST(TestLinearAllocator, StatsZeroAfterConstruction) {
    alloc::LinearAllocator a{256};
    EXPECT_EQ(a.used(),             0u);
    EXPECT_EQ(a.allocation_count(), 0u);
    EXPECT_EQ(a.peak_usage(),       0u);
}

TEST(TestLinearAllocator, RemainingEqualsCapacityAfterConstruction) {
    alloc::LinearAllocator a{256};
    EXPECT_EQ(a.remaining(), a.capacity());
}

/* LinearAllocator - basic allocation */
TEST(TestLinearAllocator, SingleAllocationReturnsNonNull) {
    alloc::LinearAllocator a{256};
    EXPECT_NE(a.allocate(16), nullptr);
}

TEST(TestLinearAllocator, ReturnedPointerSatisfiesDefaultAlignment) {
    alloc::LinearAllocator a{256};
    std::byte* p = a.allocate(16);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(alloc::is_aligned(reinterpret_cast<std::uintptr_t>(p), alignof(std::max_align_t)));
}

TEST(TestLinearAllocator, UsedAndCountUpdateAfterAllocation) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(32);
    EXPECT_EQ(a.used(),             32u);
    EXPECT_EQ(a.allocation_count(), 1u);
}

TEST(TestLinearAllocator, UsedPlusRemainingEqualsCapacityInvariant) {
    alloc::LinearAllocator a{256};
    EXPECT_EQ(a.used() + a.remaining(), a.capacity());
    (void)a.allocate(32);
    EXPECT_EQ(a.used() + a.remaining(), a.capacity());
    (void)a.allocate(16);
    EXPECT_EQ(a.used() + a.remaining(), a.capacity());
}

/* LinearAllocator - alignment correctness */
TEST(TestLinearAllocator, ReturnedPointerSatisfiesRequestedAlignment) {
    // Use a large allocator so alignment padding never causes OOM
    alloc::LinearAllocator a{512};
    for (std::size_t align : {1u, 2u, 4u, 8u, 16u, 64u}) {
        std::byte* p = a.allocate(1, align);
        ASSERT_NE(p, nullptr) << "align=" << align;
        EXPECT_TRUE(alloc::is_aligned(reinterpret_cast<std::uintptr_t>(p), align))
            << "align=" << align;
    }
}

TEST(TestLinearAllocator, UsedAccountsForAlignmentPadding) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(1);          // offset_ = 1
    (void)a.allocate(8, 8);       // align_up(1, 8) = 8; offset_ = 8 + 8 = 16
    EXPECT_EQ(a.used(), 16u);
}

/* LinearAllocator - sequential allocations */
TEST(TestLinearAllocator, SequentialAllocationsDoNotOverlap) {
    alloc::LinearAllocator a{256};
    constexpr std::size_t sz = 16;
    std::byte* p1 = a.allocate(sz);
    std::byte* p2 = a.allocate(sz);
    std::byte* p3 = a.allocate(sz);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    EXPECT_GE(p2, p1 + sz);
    EXPECT_GE(p3, p2 + sz);
}

TEST(TestLinearAllocator, AllocationCountIncrements) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(8);
    EXPECT_EQ(a.allocation_count(), 1u);
    (void)a.allocate(8);
    EXPECT_EQ(a.allocation_count(), 2u);
    (void)a.allocate(8);
    EXPECT_EQ(a.allocation_count(), 3u);
}

/* LinearAllocator - exhaustion */
TEST(TestLinearAllocator, ExhaustionReturnsNull) {
    alloc::LinearAllocator a{64};
    std::size_t successful = 0;
    while (a.allocate(8) != nullptr) {
        ++successful;
        EXPECT_EQ(a.used() + a.remaining(), a.capacity());
    }
    EXPECT_LE(a.used(), a.capacity());
    EXPECT_EQ(a.allocation_count(), successful);
    EXPECT_EQ(a.allocate(1), nullptr);
}

TEST(TestLinearAllocator, FailedAllocationDoesNotChangeState) {
    alloc::LinearAllocator a{32};
    while (a.allocate(8) != nullptr) {}

    const std::size_t used_before  = a.used();
    const std::size_t count_before = a.allocation_count();

    EXPECT_EQ(a.allocate(1), nullptr);
    EXPECT_EQ(a.used(),             used_before);
    EXPECT_EQ(a.allocation_count(), count_before);
}

/* LinearAllocator - zero-size allocation */
TEST(TestLinearAllocator, ZeroSizeAllocation) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(16);

    const std::size_t used_before  = a.used();
    const std::size_t count_before = a.allocation_count();

    EXPECT_EQ(a.allocate(0), nullptr);
    EXPECT_EQ(a.used(),             used_before);
    EXPECT_EQ(a.allocation_count(), count_before);
}

/* LinearAllocator - peak usage */
TEST(TestLinearAllocator, CorrectPeakUsage) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(64);
    EXPECT_EQ(a.peak_usage(), 64u);
    (void)a.allocate(32);
    EXPECT_EQ(a.peak_usage(), 96u);
}

/* LinearAllocator - reset */
TEST(TestLinearAllocator, ResetStates) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(64);
    (void)a.allocate(32);
    a.reset();
    EXPECT_EQ(a.used(),             0u);
    EXPECT_EQ(a.allocation_count(), 0u);
}

TEST(TestLinearAllocator, ResetPreservesPeakUsage) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(64);
    const std::size_t peak_before = a.peak_usage();
    a.reset();
    EXPECT_EQ(a.peak_usage(), peak_before);
}

TEST(TestLinearAllocator, ResetPreservesCapacity) {
    alloc::LinearAllocator a{256};
    const std::size_t cap = a.capacity();
    (void)a.allocate(64);
    a.reset();
    EXPECT_EQ(a.capacity(), cap);
}

TEST(TestLinearAllocator, FirstAllocationAfterReset) {
    alloc::LinearAllocator a{256};
    std::byte* first = a.allocate(16);
    a.reset();
    std::byte* second = a.allocate(16);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first, second);
}

TEST(TestLinearAllocator, InvariantHoldsAfterReset) {
    alloc::LinearAllocator a{256};
    (void)a.allocate(64);
    a.reset();
    EXPECT_EQ(a.used() + a.remaining(), a.capacity());
}

/* LinearAllocator - move construction */
TEST(TestLinearAllocator, MoveConstructorTransfersState) {
    alloc::LinearAllocator src{256};
    (void)src.allocate(64);
    const std::size_t expected_capacity = src.capacity();
    const std::size_t expected_used     = src.used();
    const std::size_t expected_count    = src.allocation_count();
    const std::size_t expected_peak     = src.peak_usage();

    alloc::LinearAllocator dst{std::move(src)};

    EXPECT_EQ(dst.capacity(),         expected_capacity);
    EXPECT_EQ(dst.used(),             expected_used);
    EXPECT_EQ(dst.allocation_count(), expected_count);
    EXPECT_EQ(dst.peak_usage(),       expected_peak);
}

TEST(TestLinearAllocator, MoveConstructorLeavesSourceEmpty) {
    alloc::LinearAllocator src{256};
    (void)src.allocate(64);
    alloc::LinearAllocator dst{std::move(src)};

    EXPECT_EQ(src.capacity(),         0u);
    EXPECT_EQ(src.used(),             0u);
    EXPECT_EQ(src.allocation_count(), 0u);
    EXPECT_EQ(src.allocate(1),        nullptr);
}

TEST(TestLinearAllocator, MoveConstructedAllocatorIsUsable) {
    alloc::LinearAllocator src{256};
    alloc::LinearAllocator dst{std::move(src)};
    std::byte* p = dst.allocate(16);
    EXPECT_NE(p, nullptr);
}

/* LinearAllocator - move assignment */
TEST(TestLinearAllocator, MoveAssignmentTransfersState) {
    alloc::LinearAllocator src{256};
    (void)src.allocate(64);
    const std::size_t expected_capacity = src.capacity();
    const std::size_t expected_used     = src.used();
    const std::size_t expected_count    = src.allocation_count();

    alloc::LinearAllocator dst{128};
    dst = std::move(src);

    EXPECT_EQ(dst.capacity(),         expected_capacity);
    EXPECT_EQ(dst.used(),             expected_used);
    EXPECT_EQ(dst.allocation_count(), expected_count);
}

TEST(TestLinearAllocator, MoveAssignmentLeavesSourceEmpty) {
    alloc::LinearAllocator src{256};
    alloc::LinearAllocator dst{128};
    dst = std::move(src);

    EXPECT_EQ(src.capacity(),         0u);
    EXPECT_EQ(src.used(),             0u);
    EXPECT_EQ(src.allocation_count(), 0u);
    EXPECT_EQ(src.allocate(1),        nullptr);
}

TEST(TestLinearAllocator, MoveAssignedAllocatorIsUsable) {
    alloc::LinearAllocator src{256};
    alloc::LinearAllocator dst{128};
    dst = std::move(src);
    EXPECT_NE(dst.allocate(16), nullptr);
}

TEST(TestLinearAllocator, MoveAssignmentFreesDestinationOldBuffer) {
    // If the destination's old buffer is not freed during move assignment,
    // LeakSanitizer (part of ASAN) will report the leak at program exit.
    alloc::LinearAllocator src{256};
    alloc::LinearAllocator dst{1024};   // large buffer — must be freed on assignment
    dst = std::move(src);
    EXPECT_GE(dst.capacity(), 256u);
}

/* contract violations - only active in debug builds */
#ifndef NDEBUG
TEST(TestLinearAllocator, ZeroSizeConstruction) {
    EXPECT_DEATH(alloc::LinearAllocator{0}, "");
}

TEST(TestLinearAllocator, ZeroAlignmentAllocate) {
    alloc::LinearAllocator a{256};
    EXPECT_DEATH((void) a.allocate(8, 0), "");
}

TEST(TestLinearAllocator, NonPowerOfTwoAlignmentAllocate) {
    alloc::LinearAllocator a{256};
    EXPECT_DEATH((void) a.allocate(8, 3), "");
    EXPECT_DEATH((void) a.allocate(8, 5), "");
}
#endif
