# ─── Benchmark Executable ────────────────────────────────────────────────────
add_executable(allocator_benchmarks
        benchmark.cpp
)

target_link_libraries(allocator_benchmarks
        PRIVATE
        allocators
        project_warnings
        benchmark::benchmark_main
)
