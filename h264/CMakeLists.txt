cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(h264 C)

include("${CMAKE_CURRENT_LIST_DIR}/../common/c_cpp/CMakeLists.txt")

add_executable(h264 h264.c)
target_link_libraries(h264 PRIVATE common)
