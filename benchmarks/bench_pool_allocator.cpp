#include <benchmark/benchmark.h>
#include <algorithm>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

#include "pool_allocator.hpp"

// ── Benchmark 1: Pool allocate N blocks (reset to free) vs new/malloc ─────────
// Isolates allocation cost; reset is paused so only alloc path is measured.

static void BM_Pool_Allocate(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<int> pool{N};

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            auto* p = pool.allocate();
            benchmark::DoNotOptimize(p);
        }
        state.PauseTiming();
        pool.reset();
        state.ResumeTiming();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

static void BM_NewDelete_Allocate_Pool(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    std::vector<int*> ptrs(N);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            ptrs[i] = new int;
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < N; ++i)
            delete ptrs[i];
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

static void BM_MallocFree_Allocate_Pool(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    std::vector<void*> ptrs(N);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            ptrs[i] = std::malloc(sizeof(int));
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < N; ++i)
            std::free(ptrs[i]);
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

// ── Benchmark 2: Alloc + deallocate LIFO ─────────────────────────────────────
// Pool free-list is a stack, so LIFO deallocation reuses the most-recently-freed
// (hot) block. This is the best-case deallocation pattern for cache behaviour.

static void BM_Pool_AllocDealloc_LIFO(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<int> pool{N};
    std::vector<int*> ptrs(N);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            ptrs[i] = pool.allocate();
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = N; i-- > 0;)
            pool.deallocate(ptrs[i]);
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

// ── Benchmark 3: Alloc + deallocate FIFO ─────────────────────────────────────
// Free in insertion order — tests free-list pointer-chasing across the buffer.

static void BM_Pool_AllocDealloc_FIFO(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<int> pool{N};
    std::vector<int*> ptrs(N);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            ptrs[i] = pool.allocate();
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < N; ++i)
            pool.deallocate(ptrs[i]);
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

// ── Benchmark 4: Alloc + deallocate random order ──────────────────────────────
// Measures free-list cost when deallocation order is unpredictable — the key
// pool advantage over a linear/stack allocator.

static void BM_Pool_AllocDealloc_Random(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<int> pool{N};
    std::vector<int*> ptrs(N);

    std::vector<std::size_t> order(N);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng{42};
    std::shuffle(order.begin(), order.end(), rng);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            ptrs[i] = pool.allocate();
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (const std::size_t idx : order)
            pool.deallocate(ptrs[idx]);
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

// ── Benchmark 5: Single-block reuse ──────────────────────────────────────────
// Allocate one block, use it, free it, repeat. The pool should always return
// the same hot block — compare against malloc/free for a single pointer.

static void BM_Pool_SingleBlockReuse(benchmark::State& state) {
    alloc::PoolAllocator<int> pool{16};

    for (auto _ : state) {
        auto* p = pool.allocate();
        benchmark::DoNotOptimize(p);
        pool.deallocate(p);
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate);
}

static void BM_MallocFree_SingleBlock(benchmark::State& state) {
    for (auto _ : state) {
        auto* p = static_cast<int*>(std::malloc(sizeof(int)));
        benchmark::DoNotOptimize(p);
        std::free(p);
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate);
}

// ── Benchmark 6: Alloc + reset cycle vs new/delete ───────────────────────────
// Models a per-frame pool pattern: fill the pool completely, then bulk-reset.
// Reset is O(N) but avoids N individual frees.

static void BM_Pool_AllocResetCycle(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<int> pool{N};

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            auto* p = pool.allocate();
            benchmark::DoNotOptimize(p);
        }
        pool.reset();
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

static void BM_NewDelete_AllocFreeCycle_Pool(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    std::vector<int*> ptrs(N);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            ptrs[i] = new int;
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < N; ++i)
            delete ptrs[i];
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
}

// ── Benchmark 7: Fragmented free-list reallocation ───────────────────────────
// Allocate all N blocks, then free every other one (producing a fragmented
// free-list). Measure allocation speed from this fragmented state.
// The pool should show flat O(1) cost regardless of fragmentation pattern.

static void BM_Pool_FragmentedReallocation(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<int> pool{N};
    std::vector<int*> ptrs(N);

    // Fragment the free list once before the timed loop.
    for (std::size_t i = 0; i < N; ++i)
        ptrs[i] = pool.allocate();
    for (std::size_t i = 0; i < N; i += 2)
        pool.deallocate(ptrs[i]);   // N/2 free slots, non-contiguous

    for (auto _ : state) {
        // Fill the N/2 free slots then immediately return them — stay fragmented.
        for (std::size_t i = 0; i < N / 2; ++i) {
            auto* p = pool.allocate();
            benchmark::DoNotOptimize(p);
            pool.deallocate(p);
        }
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N / 2),
        benchmark::Counter::kIsRate);
}

// ── Benchmark 8: Type-size sensitivity ───────────────────────────────────────
// Pool block size is baked in per instantiation. Compare allocation throughput
// for a minimal type (char) vs a large struct to confirm O(1) cost per alloc.

static void BM_Pool_SmallType(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<char> pool{N};

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            auto* p = pool.allocate();
            benchmark::DoNotOptimize(p);
        }
        state.PauseTiming();
        pool.reset();
        state.ResumeTiming();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
    state.counters["block_size_bytes"] = benchmark::Counter(
        static_cast<double>(alloc::PoolAllocator<char>::kBlockSize));
}

struct LargeBlock { char data[256]; };

static void BM_Pool_LargeType(benchmark::State& state) {
    const auto N = static_cast<std::size_t>(state.range(0));
    alloc::PoolAllocator<LargeBlock> pool{N};

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) {
            auto* p = pool.allocate();
            benchmark::DoNotOptimize(p);
        }
        state.PauseTiming();
        pool.reset();
        state.ResumeTiming();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(N),
        benchmark::Counter::kIsRate);
    state.counters["block_size_bytes"] = benchmark::Counter(
        static_cast<double>(alloc::PoolAllocator<LargeBlock>::kBlockSize));
}

// ── Registration ──────────────────────────────────────────────────────────────

BENCHMARK(BM_Pool_Allocate)->Range(128, 65536);
BENCHMARK(BM_NewDelete_Allocate_Pool)->Range(128, 65536);
BENCHMARK(BM_MallocFree_Allocate_Pool)->Range(128, 65536);

BENCHMARK(BM_Pool_AllocDealloc_LIFO)->Range(128, 65536);
BENCHMARK(BM_Pool_AllocDealloc_FIFO)->Range(128, 65536);
BENCHMARK(BM_Pool_AllocDealloc_Random)->Range(128, 65536);

BENCHMARK(BM_Pool_SingleBlockReuse);
BENCHMARK(BM_MallocFree_SingleBlock);

BENCHMARK(BM_Pool_AllocResetCycle)->Range(128, 65536);
BENCHMARK(BM_NewDelete_AllocFreeCycle_Pool)->Range(128, 65536);

BENCHMARK(BM_Pool_FragmentedReallocation)->Arg(128)->Arg(1024)->Arg(8192);

BENCHMARK(BM_Pool_SmallType)->Arg(1024);
BENCHMARK(BM_Pool_LargeType)->Arg(1024);
