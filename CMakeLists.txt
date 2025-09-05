cmake_minimum_required(VERSION 3.16)

project(cognity VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(COGNITY_FETCH_GRAMMARS "Fetch Tree-sitter grammars from GitHub" ON)

# Libraries
# tree-sitter core (vendored)
add_library(tree_sitter STATIC
  tree-sitter/lib/src/lib.c
)
target_include_directories(tree_sitter PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include"
)

include(FetchContent)

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

function(ts_fetch_grammar LANG REPO)
  # REPO should be full git URL, e.g. https://github.com/tree-sitter/tree-sitter-python
  string(TOLOWER "${LANG}" LANG_LOWER)
  set(FETCH_NAME ts_${LANG_LOWER}_repo)
  FetchContent_Declare(
    ${FETCH_NAME}
    GIT_REPOSITORY ${REPO}
  )
  # Populate only; do not add as a subdirectory to avoid target name clashes
  FetchContent_Populate(${FETCH_NAME})
  ts_add_grammar(${LANG_LOWER} ${${FETCH_NAME}_SOURCE_DIR})
endfunction()

# Fetch a grammar from a subdirectory in the repo (e.g., typescript/tsx)
function(ts_fetch_grammar_subdir LANG REPO SUBDIR)
  string(TOLOWER "${LANG}" LANG_LOWER)
  set(FETCH_NAME ts_${LANG_LOWER}_repo)
  FetchContent_Declare(
    ${FETCH_NAME}
    GIT_REPOSITORY ${REPO}
  )
  FetchContent_Populate(${FETCH_NAME})
  ts_add_grammar(${LANG_LOWER} ${${FETCH_NAME}_SOURCE_DIR}/${SUBDIR})
endfunction()

if(COGNITY_FETCH_GRAMMARS)
  ts_fetch_grammar(PYTHON https://github.com/tree-sitter/tree-sitter-python)
  ts_fetch_grammar(JAVASCRIPT https://github.com/tree-sitter/tree-sitter-javascript)
  ts_fetch_grammar_subdir(TYPESCRIPT https://github.com/tree-sitter/tree-sitter-typescript typescript)
  ts_fetch_grammar_subdir(TSX https://github.com/tree-sitter/tree-sitter-typescript tsx)
  ts_fetch_grammar(C https://github.com/tree-sitter/tree-sitter-c)
  ts_fetch_grammar(CPP https://github.com/tree-sitter/tree-sitter-cpp)
else()
  # Fallback to vendored directories
  # ts_add_grammar(python ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-python)
  # Uncomment if vendored JS grammar exists
  # ts_add_grammar(javascript ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-javascript)
  # ts_add_grammar(typescript ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-typescript/typescript)
  # ts_add_grammar(tsx ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-typescript/tsx)
  # ts_add_grammar(c ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-c)
  # ts_add_grammar(cpp ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cpp)
endif()


# Executable
set(SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/cli_arguments.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/file_operations.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/cognitive_complexity.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/builders/python_gsg_builder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/builders/javascript_gsg_builder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/builders/c_gsg_builder.cpp"
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
