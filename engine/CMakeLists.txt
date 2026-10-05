cmake_minimum_required(VERSION 3.24)
project(return_line_engine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)

add_executable(return_line_engine
  src/main.cpp
  src/PixelBuffer.cpp
)
target_include_directories(return_line_engine PRIVATE include)
target_link_libraries(return_line_engine PRIVATE SDL3::SDL3)

if(EMSCRIPTEN)
  set_target_properties(return_line_engine PROPERTIES SUFFIX ".html")
  target_link_options(return_line_engine PRIVATE
    "-sALLOW_MEMORY_GROWTH=1"
    "-sMIN_WEBGL_VERSION=2"
    "-sMAX_WEBGL_VERSION=2"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
  )
endif()

if(MSVC)
  target_compile_options(return_line_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(return_line_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
