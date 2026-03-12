cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(vk-asylum C)

set(DEMO_NEEDS_SDL3 TRUE)
set(DEMO_NEEDS_VULKAN TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../common/c_cpp/CMakeLists.txt")

add_executable(vk-asylum main.c)
target_link_libraries(vk-asylum PRIVATE common)
