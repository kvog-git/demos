set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(common INTERFACE)

set(FETCHCONTENT_QUIET FALSE)
include(FetchContent)

# -----------------------------------------------------------------------------
# Language options
# -----------------------------------------------------------------------------
target_compile_features(common INTERFACE cxx_std_20)
target_compile_features(common INTERFACE c_std_17)

# -----------------------------------------------------------------------------
# Compiler flags for maximum debuggage
# -----------------------------------------------------------------------------
if(MSVC)
  target_compile_options(common INTERFACE /W3)
else()
  target_compile_options(common INTERFACE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Disable stupid warnings on Windows
# -----------------------------------------------------------------------------
if(WIN32)
  target_compile_definitions(common INTERFACE _CRT_SECURE_NO_WARNINGS)
endif()

# @@ should do a test compile to see if asan is actually available
if(NOT MINGW)
  target_compile_options(common INTERFACE -fsanitize=address)
  target_link_options(common    INTERFACE -fsanitize=address)
endif()

# -----------------------------------------------------------------------------
# Dependency: Dear ImGui
# -----------------------------------------------------------------------------
if(DEMO_NEEDS_DEAR_IMGUI)
  FetchContent_Declare(
    DEAR_IMGUI
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.4 
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(DEAR_IMGUI)
  message("dear_imgui_SOURCE_DIR: ${dear_imgui_SOURCE_DIR}")
  add_library(common_dear_imgui STATIC
    "${dear_imgui_SOURCE_DIR}/imgui.cpp"
    "${dear_imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${dear_imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${dear_imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${dear_imgui_SOURCE_DIR}/imgui_widgets.cpp"
  )
  target_include_directories(common_dear_imgui PUBLIC
    "${dear_imgui_SOURCE_DIR}"
    "${dear_imgui_SOURCE_DIR}/backends"
  )
  target_compile_features(common_dear_imgui PRIVATE cxx_std_17)
  target_link_libraries(common INTERFACE common_dear_imgui)
endif()

# -----------------------------------------------------------------------------
# Dependency: FFmpeg
# -----------------------------------------------------------------------------
if(DEMO_NEEDS_FFMPEG)
  if(WIN32 AND MSVC)
    message(FATAL_ERROR "FFmpeg is not available for msvc")
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVCODEC     REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(AVFORMAT    REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(AVUTIL      REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(SWRESAMPLE  REQUIRED IMPORTED_TARGET libswresample)
    pkg_check_modules(SWSCALE     REQUIRED IMPORTED_TARGET libswscale)
    target_link_libraries(common INTERFACE
      PkgConfig::AVCODEC
      PkgConfig::AVFORMAT
      PkgConfig::AVUTIL
      PkgConfig::SWRESAMPLE
      PkgConfig::SWSCALE
    )
  endif()
endif()

# -----------------------------------------------------------------------------
# Dependency: SDL3
# -----------------------------------------------------------------------------
if(DEMO_NEEDS_SDL3)
  if(WIN32 AND MSVC)
    FetchContent_Declare(
      SDL
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG        release-3.2.18
      GIT_PROGRESS   TRUE
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL)
    target_link_libraries(common INTERFACE SDL3::SDL3)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(sdl3 REQUIRED IMPORTED_TARGET sdl3)
    target_link_libraries(common INTERFACE PkgConfig::sdl3)
  endif()
endif()

# -----------------------------------------------------------------------------
# Dependency: Vulkan
# -----------------------------------------------------------------------------
if(DEMO_NEEDS_VULKAN)
  find_package(Vulkan REQUIRED)
  target_link_libraries(common INTERFACE Vulkan::Vulkan)  
endif()
