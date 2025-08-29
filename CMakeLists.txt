cmake_minimum_required(VERSION 3.10)

project(cogniplexity VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Libraries
# tree-sitter
add_library(tree_sitter STATIC
  tree-sitter/lib/src/lib.c
)
target_include_directories(tree_sitter PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include"
)

# Helper to add a Tree-sitter grammar as a static library
# Usage: ts_add_grammar(<lang> <path-to-grammar-root>)
# Ex:    ts_add_grammar(javascript "${CMAKE_SOURCE_DIR}/tree-sitter-javascript")
function(ts_add_grammar LANG GRAMMAR_DIR)
  set(srcs
    "${GRAMMAR_DIR}/src/parser.c"
  )

  # Many grammars have external scanner, it can be C or C++
  if(EXISTS "${GRAMMAR_DIR}/src/scanner.c")
    list(APPEND srcs "${GRAMMAR_DIR}/src/scanner.c")
  elseif(EXISTS "${GRAMMAR_DIR}/src/scanner.cc")
    list(APPEND srcs "${GRAMMAR_DIR}/src/scanner.cc")
    # Ensure CMake compiles it as C++
    set_source_files_properties("${GRAMMAR_DIR}/src/scanner.cc" PROPERTIES LANGUAGE CXX)
  endif()

  add_library(ts_${LANG} STATIC ${srcs})
  target_include_directories(ts_${LANG} PUBLIC
    ${GRAMMAR_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include
  )
  target_link_libraries(ts_${LANG} PUBLIC tree_sitter)
endfunction()

# tree-sitter Python grammar
ts_add_grammar(python ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-python)


# Executable
set(SOURCES
  src/main.cpp
  src/cli_arguments.cpp
)

add_executable(cogniplexity ${SOURCES})
# target_include_directories(cogniplexity PRIVATE
#   "${CMAKE_CURRENT_SOURCE_DIR}/include"
# )
target_link_libraries(cogniplexity PRIVATE
  ts_python
  tree_sitter
)
