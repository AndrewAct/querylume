cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

project(querylume LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(QUERYLUME_BUILD_TESTS "Build QueryLume unit/integration tests" ON)
option(QUERYLUME_BUILD_BENCHMARKS "Build QueryLume benchmarks" ON)
option(QUERYLUME_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(QUERYLUME_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

add_library(querylume_warnings INTERFACE)
target_compile_options(querylume_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
)

if(QUERYLUME_ENABLE_ASAN)
  # Applied globally (not via a linkable interface target) so every
  # translation unit -- including FetchContent'd dependencies -- is built
  # and linked consistently; mixing sanitized and non-sanitized objects in
  # one binary is unsupported and can crash at startup.
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

if(QUERYLUME_ENABLE_UBSAN)
  add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=undefined)
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(src)
add_subdirectory(apps/querylume_cli)

if(QUERYLUME_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(QUERYLUME_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
