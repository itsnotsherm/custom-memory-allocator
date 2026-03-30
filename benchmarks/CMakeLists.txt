# ─── Benchmark Executable ────────────────────────────────────────────────────
add_executable(allocator_benchmarks
        benchmark.cpp
        bench_linear_allocator.cpp
        bench_pool_allocator.cpp
)

target_link_libraries(allocator_benchmarks
        PRIVATE
        allocators
        project_warnings
        benchmark::benchmark_main
)
