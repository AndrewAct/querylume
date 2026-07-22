include(GoogleTest)

add_executable(querylume_tests
  unit/value_test.cpp
  unit/schema_test.cpp
  unit/json_loader_test.cpp
  unit/pipeline_parser_test.cpp
  unit/binder_test.cpp
  execution/plan_stage_test.cpp
  optimizer/sort_limit_to_topk_rule_test.cpp
  integration/optimized_vs_unoptimized_test.cpp
  integration/explain_golden_test.cpp
)

target_link_libraries(querylume_tests PRIVATE
  querylume_core
  GTest::gtest_main
  querylume_warnings
)

gtest_discover_tests(querylume_tests)
