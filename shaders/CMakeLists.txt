cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(shaders)

set(DEMO_NEEDS_DEAR_IMGUI TRUE)
set(DEMO_NEEDS_SDL3       TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../common/c_cpp/CMakeLists.txt")

add_executable(shaders main.cc)
target_link_libraries(shaders PRIVATE common)
