#include <gtest/gtest.h>
#include "pool_allocator.hpp"
#include <set>
#include <vector>

struct MultiMember {
    int    x;
    double y;
    char   z;
};

struct alignas(64) Aligned64 {
    int value;
};

/* PoolAllocator - construction */
TEST(TestPoolAllocator, TotalBlocksMatchesRequestedCount) {
    alloc::PoolAllocator<int> pool{16};
    EXPECT_EQ(pool.total_blocks(), 16u);
}

TEST(TestPoolAllocator, AllocatedBlocksIsZero) {
    alloc::PoolAllocator<int> pool{16};
    EXPECT_EQ(pool.allocated_blocks(), 0u);
}

TEST(TestPoolAllocator, FreeBlocksEqualsTotalBlocks) {
    alloc::PoolAllocator<int> pool{16};
    EXPECT_EQ(pool.free_blocks(), pool.total_blocks());
}

TEST(TestPoolAllocator, PeakUsageIsZero) {
    alloc::PoolAllocator<int> pool{16};
    EXPECT_EQ(pool.peak_usage(), 0u);
}

TEST(TestPoolAllocator, BlockSizeAtLeastSizeofT) {
    EXPECT_GE(alloc::PoolAllocator<int>::kBlockSize,    sizeof(int));
    EXPECT_GE(alloc::PoolAllocator<double>::kBlockSize, sizeof(double));
}

TEST(TestPoolAllocator, CharBlockSizeAtLeastSizeofVoidPtr) {
    // char is 1 byte, but each block must be large enough to hold a free-list pointer
    EXPECT_GE(alloc::PoolAllocator<char>::kBlockSize, sizeof(void*));
}

TEST(TestPoolAllocator, BlockAlignmentAtLeastAlignofT) {
    EXPECT_GE(alloc::PoolAllocator<int>::kBlockAlignment,    alignof(int));
    EXPECT_GE(alloc::PoolAllocator<double>::kBlockAlignment, alignof(double));
    EXPECT_GE(alloc::PoolAllocator<char>::kBlockAlignment,   alignof(char));
}

/* PoolAllocator - basic allocation */
TEST(TestPoolAllocator, SingleAllocationReturnsNonNull) {
    alloc::PoolAllocator<int> pool{8};
    EXPECT_NE(pool.allocate(), nullptr);
}

TEST(TestPoolAllocator, ReturnedPointerIsAlignedToAlignofT) {
    alloc::PoolAllocator<int> pool{8};
    int* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(alloc::is_aligned(reinterpret_cast<std::uintptr_t>(p), alignof(int)));
}

TEST(TestPoolAllocator, AllocatedBlocksIncrementsOnAllocation) {
    alloc::PoolAllocator<int> pool{8};
    EXPECT_EQ(pool.allocated_blocks(), 0u);
    (void)pool.allocate();
    EXPECT_EQ(pool.allocated_blocks(), 1u);
    (void)pool.allocate();
    EXPECT_EQ(pool.allocated_blocks(), 2u);
}

TEST(TestPoolAllocator, FreeBlocksDecrementsOnAllocation) {
    alloc::PoolAllocator<int> pool{8};
    EXPECT_EQ(pool.free_blocks(), 8u);
    (void)pool.allocate();
    EXPECT_EQ(pool.free_blocks(), 7u);
    (void)pool.allocate();
    EXPECT_EQ(pool.free_blocks(), 6u);
}

TEST(TestPoolAllocator, MultipleAllocationsReturnDistinctPointers) {
    alloc::PoolAllocator<int> pool{8};
    std::set<int*> seen;
    for (int i = 0; i < 8; ++i) {
        int* p = pool.allocate();
        ASSERT_NE(p, nullptr) << "allocation " << i << " returned null";
        EXPECT_TRUE(seen.insert(p).second) << "duplicate pointer at allocation " << i;
    }
}

/* PoolAllocator - allocation alignment */
TEST(TestPoolAllocator, CharPoolPointersAreAligned) {
    alloc::PoolAllocator<char> pool{8};
    for (int i = 0; i < 8; ++i) {
        char* p = pool.allocate();
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(alloc::is_aligned(reinterpret_cast<std::uintptr_t>(p),
                                      alloc::PoolAllocator<char>::kBlockAlignment))
            << "misaligned at i=" << i;
    }
}

TEST(TestPoolAllocator, DoublePoolPointersAreAligned) {
    alloc::PoolAllocator<double> pool{8};
    for (int i = 0; i < 8; ++i) {
        double* p = pool.allocate();
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(alloc::is_aligned(reinterpret_cast<std::uintptr_t>(p), alignof(double)))
            << "misaligned at i=" << i;
    }
}

TEST(TestPoolAllocator, Aligned64PoolPointersAreAligned) {
    alloc::PoolAllocator<Aligned64> pool{4};
    for (int i = 0; i < 4; ++i) {
        Aligned64* p = pool.allocate();
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(alloc::is_aligned(reinterpret_cast<std::uintptr_t>(p), 64u))
            << "misaligned at i=" << i;
    }
}

/* PoolAllocator - exhaustion */
TEST(TestPoolAllocator, ExhaustionSuccessfulAllocations) {
    constexpr std::size_t N = 8;
    alloc::PoolAllocator<int> pool{N};
    std::size_t successful = 0;
    while (pool.allocate() != nullptr)
        ++successful;
    EXPECT_EQ(successful, N);
}

TEST(TestPoolAllocator, AllocatedBlocksEqualsTotalOnExhaustion) {
    constexpr std::size_t N = 8;
    alloc::PoolAllocator<int> pool{N};
    while (pool.allocate() != nullptr) {}
    EXPECT_EQ(pool.allocated_blocks(), pool.total_blocks());
}

TEST(TestPoolAllocator, FreeBlocksIsZeroOnExhaustion) {
    constexpr std::size_t N = 8;
    alloc::PoolAllocator<int> pool{N};
    while (pool.allocate() != nullptr) {}
    EXPECT_EQ(pool.free_blocks(), 0u);
}

TEST(TestPoolAllocator, AllocationAfterExhaustionDoesNotChangeStats) {
    constexpr std::size_t N = 8;
    alloc::PoolAllocator<int> pool{N};
    while (pool.allocate() != nullptr) {}

    const std::size_t alloc_before = pool.allocated_blocks();
    const std::size_t free_before  = pool.free_blocks();

    EXPECT_EQ(pool.allocate(), nullptr);
    EXPECT_EQ(pool.allocated_blocks(), alloc_before);
    EXPECT_EQ(pool.free_blocks(),      free_before);
}

/* PoolAllocator - deallocation */
TEST(TestPoolAllocator, DeallocateDecrementsAllocatedBlocks) {
    alloc::PoolAllocator<int> pool{8};
    int* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(pool.allocated_blocks(), 1u);
    pool.deallocate(p);
    EXPECT_EQ(pool.allocated_blocks(), 0u);
}

TEST(TestPoolAllocator, DeallocateIncrementsFreeBlocks) {
    alloc::PoolAllocator<int> pool{8};
    int* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(pool.free_blocks(), 7u);
    pool.deallocate(p);
    EXPECT_EQ(pool.free_blocks(), 8u);
}

TEST(TestPoolAllocator, AllocationAfterDeallocateReturnsSameBlock) {
    alloc::PoolAllocator<int> pool{8};
    int* p1 = pool.allocate();
    ASSERT_NE(p1, nullptr);
    pool.deallocate(p1);
    int* p2 = pool.allocate();
    EXPECT_EQ(p1, p2);
}

/* PoolAllocator - random-order deallocation */
TEST(TestPoolAllocator, RandomOrderDeallocationUpdatesStatsCorrectly) {
    constexpr std::size_t N = 6;
    alloc::PoolAllocator<int> pool{N};
    std::vector<int*> ptrs;
    for (std::size_t i = 0; i < N; ++i) {
        ptrs.push_back(pool.allocate());
        ASSERT_NE(ptrs.back(), nullptr);
    }

    // Deallocate in non-LIFO order: 2, 0, 4, 1, 5, 3
    const std::size_t order[] = {2, 0, 4, 1, 5, 3};
    for (std::size_t i = 0; i < N; ++i) {
        pool.deallocate(ptrs[order[i]]);
        EXPECT_EQ(pool.allocated_blocks(), N - (i + 1));
        EXPECT_EQ(pool.free_blocks(),      i + 1);
    }
}

TEST(TestPoolAllocator, RandomOrderDeallocationAllowsFullReallocation) {
    // This distinguishes the pool from a stack allocator: all blocks are
    // recoverable regardless of deallocation order.
    constexpr std::size_t N = 6;
    alloc::PoolAllocator<int> pool{N};
    std::vector<int*> ptrs;
    for (std::size_t i = 0; i < N; ++i)
        ptrs.push_back(pool.allocate());

    for (std::size_t idx : {2u, 0u, 4u, 1u, 5u, 3u})
        pool.deallocate(ptrs[idx]);

    std::size_t realloced = 0;
    while (pool.allocate() != nullptr)
        ++realloced;
    EXPECT_EQ(realloced, N);
}

/* PoolAllocator - writing to allocated memory */
TEST(TestPoolAllocator, AllocatedMemoryIsWritable) {
    alloc::PoolAllocator<int> pool{8};
    int* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    new (p) int{42};
    EXPECT_EQ(*p, 42);
}

TEST(TestPoolAllocator, SubsequentAllocationsDoNotCorruptEarlierBlocks) {
    alloc::PoolAllocator<int> pool{8};
    int* p1 = pool.allocate();
    ASSERT_NE(p1, nullptr);
    new (p1) int{123};

    // Allocate two more blocks — if they overlap with p1 the write above gets clobbered.
    int* p2 = pool.allocate();
    int* p3 = pool.allocate();
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(*p1, 123);
}

/* PoolAllocator - peak usage */
TEST(TestPoolAllocator, PeakUsageTracking) {
    alloc::PoolAllocator<int> pool{16};
    std::vector<int*> ptrs;

    for (int i = 0; i < 5; ++i)
        ptrs.push_back(pool.allocate());
    EXPECT_EQ(pool.peak_usage(), 5u);

    // Deallocate 3 — peak must not decrease
    pool.deallocate(ptrs[0]);
    pool.deallocate(ptrs[1]);
    pool.deallocate(ptrs[2]);
    EXPECT_EQ(pool.peak_usage(), 5u);

    // Reallocate 2 (4 active now) — still below previous peak
    (void) pool.allocate();
    (void) pool.allocate();
    EXPECT_EQ(pool.peak_usage(), 5u);

    // Allocate 2 more (6 active) — exceeds old peak
    (void) pool.allocate();
    (void) pool.allocate();
    EXPECT_EQ(pool.peak_usage(), 6u);
}

/* PoolAllocator - reset */
TEST(TestPoolAllocator, ResetZeroesAllocatedBlocks) {
    alloc::PoolAllocator<int> pool{8};
    (void)pool.allocate();
    (void)pool.allocate();
    pool.reset();
    EXPECT_EQ(pool.allocated_blocks(), 0u);
}

TEST(TestPoolAllocator, ResetRestoresFreeBlocks) {
    alloc::PoolAllocator<int> pool{8};
    (void)pool.allocate();
    (void)pool.allocate();
    pool.reset();
    EXPECT_EQ(pool.free_blocks(), pool.total_blocks());
}

TEST(TestPoolAllocator, ResetPreservesPeakUsage) {
    alloc::PoolAllocator<int> pool{8};
    (void)pool.allocate();
    (void)pool.allocate();
    (void)pool.allocate();
    const std::size_t peak_before = pool.peak_usage();
    pool.reset();
    EXPECT_EQ(pool.peak_usage(), peak_before);
}

TEST(TestPoolAllocator, AllocationAfterReset) {
    alloc::PoolAllocator<int> pool{8};
    for (int i = 0; i < 8; ++i)
        (void)pool.allocate();
    pool.reset();
    EXPECT_NE(pool.allocate(), nullptr);
}

/* PoolAllocator - move construction */
TEST(TestPoolAllocator, MoveConstructorTransfersStats) {
    alloc::PoolAllocator<int> src{16};
    (void)src.allocate();
    (void)src.allocate();

    const std::size_t expected_total     = src.total_blocks();
    const std::size_t expected_allocated = src.allocated_blocks();
    const std::size_t expected_free      = src.free_blocks();

    const alloc::PoolAllocator<int> dst{std::move(src)};

    EXPECT_EQ(dst.total_blocks(),     expected_total);
    EXPECT_EQ(dst.allocated_blocks(), expected_allocated);
    EXPECT_EQ(dst.free_blocks(),      expected_free);
}

TEST(TestPoolAllocator, MoveConstructorLeavesSourceEmpty) {
    alloc::PoolAllocator<int> src{16};
    alloc::PoolAllocator<int> dst{std::move(src)};

    EXPECT_EQ(src.total_blocks(), 0u);
    EXPECT_EQ(src.allocate(),     nullptr);
}

TEST(TestPoolAllocator, MoveConstructedAllocatorIsUsable) {
    alloc::PoolAllocator<int> src{16};
    alloc::PoolAllocator<int> dst{std::move(src)};

    int* p = dst.allocate();
    ASSERT_NE(p, nullptr);
    dst.deallocate(p);
    EXPECT_EQ(dst.allocated_blocks(), 0u);
}

/* PoolAllocator - move assignment */
TEST(TestPoolAllocator, MoveAssignmentTransfersStats) {
    alloc::PoolAllocator<int> src{16};
    (void)src.allocate();
    (void)src.allocate();

    const std::size_t expected_total     = src.total_blocks();
    const std::size_t expected_allocated = src.allocated_blocks();
    const std::size_t expected_free      = src.free_blocks();

    alloc::PoolAllocator<int> dst{4};   // deliberately different block count
    dst = std::move(src);

    EXPECT_EQ(dst.total_blocks(),     expected_total);
    EXPECT_EQ(dst.allocated_blocks(), expected_allocated);
    EXPECT_EQ(dst.free_blocks(),      expected_free);
}

TEST(TestPoolAllocator, MoveAssignmentLeavesSourceEmpty) {
    alloc::PoolAllocator<int> src{16};
    alloc::PoolAllocator<int> dst{4};
    dst = std::move(src);

    EXPECT_EQ(src.total_blocks(), 0u);
    EXPECT_EQ(src.allocate(),     nullptr);
}

TEST(TestPoolAllocator, MoveAssignedAllocatorIsUsable) {
    alloc::PoolAllocator<int> src{16};
    alloc::PoolAllocator<int> dst{4};
    dst = std::move(src);

    int* p = dst.allocate();
    ASSERT_NE(p, nullptr);
    dst.deallocate(p);
    EXPECT_EQ(dst.allocated_blocks(), 0u);
}

TEST(TestPoolAllocator, MoveAssignmentFreesDestinationOldBuffer) {
    // If the destination's old buffer is not freed on move assignment,
    // LeakSanitizer (part of ASAN) will report the leak at program exit.
    alloc::PoolAllocator<int> src{16};
    alloc::PoolAllocator<int> dst{1024};    // large buffer — must be freed
    dst = std::move(src);
    EXPECT_EQ(dst.total_blocks(), 16u);
}

/* PoolAllocator - contract violations (debug builds only) */
#ifndef NDEBUG
TEST(TestPoolAllocator, ZeroBlockCountAsserts) {
    EXPECT_DEATH(alloc::PoolAllocator<int>{0}, "");
}

TEST(TestPoolAllocator, DeallocateNullptrAsserts) {
    alloc::PoolAllocator<int> pool{8};
    EXPECT_DEATH(pool.deallocate(nullptr), "");
}

TEST(TestPoolAllocator, DeallocateStackPointerAsserts) {
    alloc::PoolAllocator<int> pool{8};
    int stack_var = 0;
    EXPECT_DEATH(pool.deallocate(&stack_var), "");
}

TEST(TestPoolAllocator, DeallocatePointerFromOtherPoolAsserts) {
    alloc::PoolAllocator<int> pool_a{8};
    alloc::PoolAllocator<int> pool_b{8};
    int* p = pool_a.allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_DEATH(pool_b.deallocate(p), "");
}

TEST(TestPoolAllocator, DeallocateMisalignedPointerAsserts) {
    alloc::PoolAllocator<int> pool{8};
    int* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    // Advance 1 byte into the block — misaligned relative to kBlockSize
    auto* misaligned = reinterpret_cast<int*>(reinterpret_cast<std::byte*>(p) + 1);
    EXPECT_DEATH(pool.deallocate(misaligned), "");
}
#endif

/* PoolAllocator - type-specific tests */
TEST(TestPoolAllocator, PrimitiveTypeDouble) {
    alloc::PoolAllocator<double> pool{4};
    EXPECT_EQ(pool.total_blocks(), 4u);
    double* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(alloc::is_aligned(reinterpret_cast<std::uintptr_t>(p), alignof(double)));
    new (p) double{3.14};
    EXPECT_DOUBLE_EQ(*p, 3.14);
    pool.deallocate(p);
    EXPECT_EQ(pool.allocated_blocks(), 0u);
}

TEST(TestPoolAllocator, MultiMemberStructType) {
    alloc::PoolAllocator<MultiMember> pool{4};
    EXPECT_EQ(pool.total_blocks(), 4u);
    MultiMember* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    new (p) MultiMember{1, 2.5, 'a'};
    EXPECT_EQ(p->x, 1);
    EXPECT_DOUBLE_EQ(p->y, 2.5);
    EXPECT_EQ(p->z, 'a');
    pool.deallocate(p);
    EXPECT_EQ(pool.allocated_blocks(), 0u);
}

TEST(TestPoolAllocator, CharTypeForcesLargerBlockSize) {
    // char is 1 byte — kBlockSize must be at least sizeof(void*) so the block
    // can hold a free-list pointer when it is on the free list.
    EXPECT_GE(alloc::PoolAllocator<char>::kBlockSize, sizeof(void*));

    alloc::PoolAllocator<char> pool{8};
    EXPECT_EQ(pool.total_blocks(), 8u);
    char* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    *p = 'X';
    EXPECT_EQ(*p, 'X');
    pool.deallocate(p);
    EXPECT_EQ(pool.allocated_blocks(), 0u);
}

TEST(TestPoolAllocator, BlockIsUsableAfterRecycling) {
    alloc::PoolAllocator<int> pool{4};
    int* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    new (p) int{42};
    EXPECT_EQ(*p, 42);

    pool.deallocate(p);           // block now holds free list pointer

    int* p2 = pool.allocate();    // same block returned
    ASSERT_NE(p2, nullptr);
    new (p2) int{99};             // overwrite the free list pointer with user data
    EXPECT_EQ(*p2, 99);
}