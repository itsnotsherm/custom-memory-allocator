#include <benchmark/benchmark.h>
#include <cstdlib>
#include <vector>

#include "linear_allocator.hpp"

// Benchmark 1: Linear allocator vs new/malloc for N small (32-byte) allocations.
static void BM_Linear_Allocate(benchmark::State& state) {
    const auto num_allocs = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t block_size = 32;

    alloc::LinearAllocator alloc{num_allocs * block_size * 2};

    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            auto* p = alloc.allocate(block_size);
            benchmark::DoNotOptimize(p);
        }
        alloc.reset();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
}

// Benchmark 2a: new/delete baseline.
static void BM_NewDelete_Allocate(benchmark::State& state) {
    const auto num_allocs = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t block_size = 32;

    std::vector<std::byte*> ptrs(num_allocs);
    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            ptrs[i] = new std::byte[block_size];
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < num_allocs; ++i) {
            delete[] ptrs[i];
        }
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
}

// Benchmark 2b: malloc/free baseline.
static void BM_MallocFree_Allocate(benchmark::State& state) {
    const auto num_allocs = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t block_size = 32;

    std::vector<void*> ptrs(num_allocs);
    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            ptrs[i] = std::malloc(block_size);
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < num_allocs; ++i) {
            std::free(ptrs[i]);
        }
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
}

// Benchmark 3: Varying allocation sizes with fixed N.
// For a bump allocator, cost should be flat regardless of size. This confirms O(1) per alloc.
static void BM_Linear_VaryingSize(benchmark::State& state) {
    constexpr std::size_t num_allocs = 1024;
    const auto block_size = static_cast<std::size_t>(state.range(0));

    alloc::LinearAllocator alloc{num_allocs * block_size * 2};

    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            auto* p = alloc.allocate(block_size);
            benchmark::DoNotOptimize(p);
        }
        alloc.reset();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
    state.counters["block_size_bytes"] = benchmark::Counter(
        static_cast<double>(block_size)
        );
}

// Benchmark 4: Default alignment vs cache-line (64-byte) alignment.
// Allocation speed should be similar — the difference shows up in memory efficiency.
// bytes_wasted_per_alloc = (used - num_allocs * block_size) / num_allocs after one pass.
static void BM_Linear_DefaultAlignment(benchmark::State& state) {
    constexpr std::size_t num_allocs = 1024;
    constexpr std::size_t block_size = 32;

    alloc::LinearAllocator alloc{num_allocs * 64 * 2}; // enough for worst-case padding

    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            auto* p = alloc.allocate(block_size); // default: alignof(std::max_align_t)
            benchmark::DoNotOptimize(p);
        }
        state.PauseTiming();
        const double wasted = static_cast<double>(alloc.used()) / static_cast<double>(num_allocs)
                              - static_cast<double>(block_size);
        state.counters["bytes_wasted_per_alloc"] = benchmark::Counter(wasted);
        alloc.reset();
        state.ResumeTiming();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
}

static void BM_Linear_CacheLineAlignment(benchmark::State& state) {
    constexpr std::size_t num_allocs = 1024;
    constexpr std::size_t block_size = 32;
    constexpr std::size_t cache_line = 64;

    alloc::LinearAllocator alloc{num_allocs * cache_line * 2};

    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            auto* p = alloc.allocate(block_size, cache_line);
            benchmark::DoNotOptimize(p);
        }
        state.PauseTiming();
        const double wasted = static_cast<double>(alloc.used()) / static_cast<double>(num_allocs)
                              - static_cast<double>(block_size);
        state.counters["bytes_wasted_per_alloc"] = benchmark::Counter(wasted);
        alloc.reset();
        state.ResumeTiming();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
}

// Benchmark 5: Full alloc+reset cycle vs N new/delete pairs.
// Models the game-engine frame loop pattern: allocate everything, then bulk-free.
static void BM_Linear_AllocResetCycle(benchmark::State& state) {
    const auto num_allocs = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t block_size = 32;

    alloc::LinearAllocator alloc{num_allocs * block_size * 2};

    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            auto* p = alloc.allocate(block_size);
            benchmark::DoNotOptimize(p);
        }
        alloc.reset();
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
}

static void BM_NewDelete_AllocFreeCycle(benchmark::State& state) {
    const auto num_allocs = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t block_size = 32;

    std::vector<std::byte*> ptrs(num_allocs);

    for (auto _ : state) {
        for (std::size_t i = 0; i < num_allocs; ++i) {
            ptrs[i] = new std::byte[block_size];
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < num_allocs; ++i) {
            delete[] ptrs[i];
        }
        benchmark::ClobberMemory();
    }

    state.counters["allocs_per_sec"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(num_allocs),
        benchmark::Counter::kIsRate
        );
}

BENCHMARK(BM_Linear_Allocate)->Range(128, 1048576);
BENCHMARK(BM_NewDelete_Allocate)->Range(128, 1048576);
BENCHMARK(BM_MallocFree_Allocate)->Range(128, 1048576);

BENCHMARK(BM_Linear_VaryingSize)->Arg(8)->Arg(64)->Arg(256)->Arg(1024);

BENCHMARK(BM_Linear_DefaultAlignment);
BENCHMARK(BM_Linear_CacheLineAlignment);

BENCHMARK(BM_Linear_AllocResetCycle)->Range(128, 1048576);
BENCHMARK(BM_NewDelete_AllocFreeCycle)->Range(128, 1048576);
