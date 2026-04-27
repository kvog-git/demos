cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(gl-asylum C)

set(DEMO_NEEDS_SDL3 TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../common/c_cpp/CMakeLists.txt")

add_executable(asylum main.c)
target_link_libraries(asylum PRIVATE common)
