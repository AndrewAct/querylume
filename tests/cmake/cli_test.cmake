if(NOT DEFINED QUERYLUME_EXE OR NOT DEFINED QUERYLUME_SOURCE_DIR)
  message(FATAL_ERROR "QUERYLUME_EXE and QUERYLUME_SOURCE_DIR are required")
endif()

set(data_file "${QUERYLUME_SOURCE_DIR}/examples/stocks.json")
set(pipeline_file "${QUERYLUME_SOURCE_DIR}/examples/top_stocks.pipeline.json")

execute_process(
  COMMAND "${QUERYLUME_EXE}" run --data "${data_file}" --pipeline "${pipeline_file}"
  RESULT_VARIABLE run_status
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error
)
if(NOT run_status EQUAL 0)
  message(FATAL_ERROR "querylume run failed (${run_status}): ${run_error}")
endif()
string(JSON run_type TYPE "${run_output}")
string(JSON run_length LENGTH "${run_output}")
if(NOT run_type STREQUAL "ARRAY" OR NOT run_length EQUAL 2)
  message(FATAL_ERROR "querylume run did not emit the expected JSON array: ${run_output}")
endif()

execute_process(
  COMMAND "${QUERYLUME_EXE}" explain --data "${data_file}" --pipeline "${pipeline_file}"
  RESULT_VARIABLE explain_status
  OUTPUT_VARIABLE explain_output
  ERROR_VARIABLE explain_error
)
if(NOT explain_status EQUAL 0)
  message(FATAL_ERROR "querylume explain failed (${explain_status}): ${explain_error}")
endif()
string(JSON explain_type TYPE "${explain_output}")
string(JSON optimized_node GET "${explain_output}" optimizedLogicalPlan node)
if(NOT explain_type STREQUAL "OBJECT" OR NOT optimized_node STREQUAL "TopK")
  message(FATAL_ERROR "querylume explain did not emit an optimized TopK plan")
endif()

execute_process(
  COMMAND "${QUERYLUME_EXE}" run
          --data "${QUERYLUME_SOURCE_DIR}/examples/does-not-exist.json"
          --pipeline "${pipeline_file}"
  RESULT_VARIABLE invalid_status
  OUTPUT_VARIABLE invalid_output
  ERROR_VARIABLE invalid_error
)
if(invalid_status EQUAL 0)
  message(FATAL_ERROR "invalid CLI input unexpectedly returned exit code 0")
endif()
if(NOT invalid_output STREQUAL "")
  message(FATAL_ERROR "invalid CLI input unexpectedly wrote to stdout: ${invalid_output}")
endif()
if(NOT invalid_error MATCHES "error \\[JsonParseError\\]")
  message(FATAL_ERROR "invalid CLI stderr did not identify JsonParseError: ${invalid_error}")
endif()
