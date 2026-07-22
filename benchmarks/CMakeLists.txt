add_executable(sort_vs_topk_benchmark sort_vs_topk_benchmark.cpp)

target_link_libraries(sort_vs_topk_benchmark PRIVATE
  querylume_core
  benchmark::benchmark_main
)
