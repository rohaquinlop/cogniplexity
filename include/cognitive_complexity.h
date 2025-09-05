#ifndef COGNITIVE_COMPLEXITY_H
#define COGNITIVE_COMPLEXITY_H

#include <cstring>
#include <iostream>
#include <vector>

#include "../tree-sitter/lib/include/tree_sitter/api.h"
#include "./gsg.h"

extern "C" {
const TSLanguage* tree_sitter_python();
const TSLanguage* tree_sitter_javascript();
const TSLanguage* tree_sitter_typescript();
const TSLanguage* tree_sitter_tsx();
const TSLanguage* tree_sitter_c();
const TSLanguage* tree_sitter_cpp();
}

struct LineComplexity {
  unsigned int row;
  unsigned int start_col;
  unsigned int end_col;
  unsigned int complexity;
};

struct FunctionComplexity {
  std::string name;
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

enum BoolOp { And, Or, Not, Unknown };

// Entry-point to get complexity per function for a given language
std::vector<FunctionComplexity> functions_complexity_file(const std::string&,
                                                          TSParser*, Language);

// Compute complexity from a GSG node
std::pair<unsigned int, std::vector<LineComplexity>>
compute_cognitive_complexity_gsg(const GSGNode&, int);

#endif
