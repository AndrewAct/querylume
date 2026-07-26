add_executable(querylume main.cpp)

target_link_libraries(querylume PRIVATE
  querylume_core
  CLI11::CLI11
  nlohmann_json::nlohmann_json
  querylume_warnings
)

install(TARGETS querylume RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
