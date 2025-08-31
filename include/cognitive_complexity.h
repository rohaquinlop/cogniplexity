#ifndef COGNITIVE_COMPLEXITY_H
#define COGNITIVE_COMPLEXITY_H

#include <iostream>
#include <vector>

#include "../tree-sitter/lib/include/tree_sitter/api.h"

extern "C" {
const TSLanguage* tree_sitter_python();
}

struct LineComplexity {
  unsigned int row;
  unsigned int start_col;
  unsigned int end_col;
  int complexity;
};

struct FunctionComplexity {
  std::string_view name;
  unsigned int complexity;
  unsigned int row;
  unsigned int start_col;
  unsigned int end_col;
  std::vector<LineComplexity> lines;
};

struct FileComplexity {
  std::string path;
  std::string file_name;
  std::vector<FunctionComplexity> functions;
};

struct CodeComplexity {
  std::vector<FunctionComplexity> functions;
};

std::vector<FunctionComplexity> functions_complexity_file(const std::string&,
                                                          TSParser*);
std::pair<unsigned int, std::vector<LineComplexity>>
compute_cognitive_complexity(TSNode, int);
static bool is_decorator(TSNode&);
static std::string_view slice_source(const std::string&, TSNode);
static std::string_view get_function_name(TSNode, const std::string&);

#endif
