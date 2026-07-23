cmake_minimum_required(VERSION 3.20)
project(querylume_install_smoke LANGUAGES CXX)

find_package(QueryLume 0.1 REQUIRED)

add_executable(querylume_install_smoke ../embedding/main.cpp)
target_link_libraries(querylume_install_smoke PRIVATE QueryLume::Core)
