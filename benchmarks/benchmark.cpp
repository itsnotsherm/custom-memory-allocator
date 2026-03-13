#include <benchmark/benchmark.h>
#include <vector>

// Smoke test for benchmark framework
static void bm(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i) {
            v.push_back(i);
        }

        // Prevent compiler from optimizing away the vector
        benchmark::DoNotOptimize(v.data());
    }
}

BENCHMARK(bm)->Range(100, 10000);