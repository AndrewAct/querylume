cmake_minimum_required(VERSION 3.20)
project(querylume_embedding_smoke LANGUAGES CXX)

# QueryLume is deliberately a subproject here. Its CLI, tests, and benchmarks
# must default to OFF so an embedding application only pays for the core.
add_subdirectory(../.. querylume)

if(TARGET querylume OR TARGET querylume_tests OR TARGET sort_vs_topk_benchmark)
  message(FATAL_ERROR "QueryLume developer targets should default to OFF as a subproject")
endif()

add_executable(querylume_embedding_smoke main.cpp)
target_link_libraries(querylume_embedding_smoke PRIVATE QueryLume::Core)
