set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(common INTERFACE)

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

# @@ should do a test compile to see if asan is actually available
if(NOT MINGW)
  target_compile_options(common INTERFACE -fsanitize=address)
  target_link_options(common    INTERFACE -fsanitize=address)
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
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET SDL3)
    target_link_libraries(common INTERFACE PkgConfig::SDL3)
  endif()
endif()