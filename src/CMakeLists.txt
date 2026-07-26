add_library(querylume_core
  common/error.cpp
  data/value.cpp
  data/schema.cpp
  data/json_loader.cpp
  expression/comparison_operator.cpp
  expression/literal_expression.cpp
  expression/comparison_expression.cpp
  expression/conjunction_expression.cpp
  parser/pipeline_parser.cpp
  binder/binder.cpp
  logical/logical_plan_utils.cpp
  optimizer/optimizer.cpp
  optimizer/sort_limit_to_topk_rule.cpp
  physical/plan_stage_base.cpp
  physical/collection_scan_stage.cpp
  physical/filter_stage.cpp
  physical/project_stage.cpp
  physical/limit_stage.cpp
  physical/row_comparator.cpp
  physical/memory_accounting.cpp
  physical/sort_stage.cpp
  physical/topk_stage.cpp
  physical/physical_planner.cpp
  explain/json_serialize.cpp
  explain/query_facade.cpp
)

add_library(QueryLume::Core ALIAS querylume_core)

target_include_directories(querylume_core PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(querylume_core
  PRIVATE
    $<BUILD_INTERFACE:querylume_warnings>
)

target_compile_features(querylume_core PUBLIC cxx_std_17)

set_target_properties(querylume_core PROPERTIES EXPORT_NAME Core)

install(
  TARGETS querylume_core
  EXPORT QueryLumeTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
