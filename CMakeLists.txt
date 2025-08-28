cmake_minimum_required(VERSION 3.10)

project(cogniplexity VERSION 0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(SOURCES
  src/main.cpp
  include/cli_arguments.cpp
)

add_executable(cogniplexity ${SOURCES})
target_include_directories(cogniplexity PUBLIC include)
