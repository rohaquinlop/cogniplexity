cmake_minimum_required(VERSION 3.16)

project(cognity VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Grammars are always fetched from upstream (no vendored copies kept)

# Libraries
# tree-sitter core (vendored)
add_library(tree_sitter STATIC
  tree-sitter/lib/src/lib.c
)
target_include_directories(tree_sitter PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include"
)

include(FetchContent)

# Pinned grammar versions (tags or commit SHAs).
# To update, edit these values directly.
set(TS_PYTHON_TAG      "v0.23.0")
set(TS_JAVASCRIPT_TAG  "v0.23.0")
set(TS_TYPESCRIPT_TAG  "v0.23.0")
set(TS_TSX_TAG         "v0.23.0")
set(TS_C_TAG           "v0.23.0")
set(TS_CPP_TAG         "v0.23.0")

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

function(ts_fetch_grammar LANG REPO TAG)
  # REPO should be full git URL, e.g. https://github.com/tree-sitter/tree-sitter-python
  string(TOLOWER "${LANG}" LANG_LOWER)
  set(FETCH_NAME ts_${LANG_LOWER}_repo)
  FetchContent_Declare(
    ${FETCH_NAME}
    GIT_REPOSITORY ${REPO}
    GIT_TAG        ${TAG}
  )
  # Populate only; do not add as a subdirectory to avoid target name clashes
  FetchContent_Populate(${FETCH_NAME})
  ts_add_grammar(${LANG_LOWER} ${${FETCH_NAME}_SOURCE_DIR})
endfunction()

# Fetch a grammar from a subdirectory in the repo (e.g., typescript/tsx)
function(ts_fetch_grammar_subdir LANG REPO SUBDIR TAG)
  string(TOLOWER "${LANG}" LANG_LOWER)
  set(FETCH_NAME ts_${LANG_LOWER}_repo)
  FetchContent_Declare(
    ${FETCH_NAME}
    GIT_REPOSITORY ${REPO}
    GIT_TAG        ${TAG}
  )
  FetchContent_Populate(${FETCH_NAME})
  ts_add_grammar(${LANG_LOWER} ${${FETCH_NAME}_SOURCE_DIR}/${SUBDIR})
endfunction()

# Always fetch grammars from upstream
ts_fetch_grammar(PYTHON https://github.com/tree-sitter/tree-sitter-python ${TS_PYTHON_TAG})
ts_fetch_grammar(JAVASCRIPT https://github.com/tree-sitter/tree-sitter-javascript ${TS_JAVASCRIPT_TAG})
ts_fetch_grammar_subdir(TYPESCRIPT https://github.com/tree-sitter/tree-sitter-typescript typescript ${TS_TYPESCRIPT_TAG})
ts_fetch_grammar_subdir(TSX https://github.com/tree-sitter/tree-sitter-typescript tsx ${TS_TSX_TAG})
ts_fetch_grammar(C https://github.com/tree-sitter/tree-sitter-c ${TS_C_TAG})
ts_fetch_grammar(CPP https://github.com/tree-sitter/tree-sitter-cpp ${TS_CPP_TAG})


# Executable
set(SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/cli_arguments.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/file_operations.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/cognitive_complexity.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/builders/python_gsg_builder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/builders/javascript_gsg_builder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/builders/c_gsg_builder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/gitignore.cpp"
)

add_executable(cognity ${SOURCES})
target_link_libraries(cognity PRIVATE
  ts_python
  ts_javascript
  ts_typescript
  ts_tsx
  ts_c
  ts_cpp
  tree_sitter
)

# Test runner mirroring complexipy's expectations for sample Python files
add_executable(cognity_tests
  tests/test_complexity.cpp
  src/cognitive_complexity.cpp
  src/builders/python_gsg_builder.cpp
  src/builders/javascript_gsg_builder.cpp
  src/builders/c_gsg_builder.cpp
  src/gitignore.cpp
  src/file_operations.cpp
  src/cli_arguments.cpp
  src/config.cpp
)
target_link_libraries(cognity_tests PRIVATE
  ts_python
  ts_javascript
  ts_typescript
  ts_c
  ts_cpp
  tree_sitter
)

enable_testing()
add_test(NAME cognity_complexity_tests COMMAND cognity_tests)
