cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(ffmpeg-shadertoy C)

set(DEMO_NEEDS_FFMPEG TRUE)
set(DEMO_NEEDS_SDL3   TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../common/c_cpp/CMakeLists.txt")

add_executable(ffmpeg-shadertoy main.c)
target_link_libraries(ffmpeg-shadertoy PRIVATE common)