#pragma once

#include <string>
#include <vector>

#include "../tree-sitter/lib/include/tree_sitter/api.h"
#include "./gsg.h"

// Detect language from file path by extension
Language detect_language_from_path(const std::string &path);

// Collect source files from input paths (files or directories),
// respecting language filter and explicit excludes. Uses .gitignore rules.
void collect_source_files(const std::vector<std::string> &inputs,
                          const std::vector<Language> &filter,
                          const std::vector<std::string> &excludes,
                          std::vector<std::string> &out);

// Configure Tree-sitter parser for a given language and path (handles TSX).
void set_ts_language_for_file(TSParser *parser, Language lang,
                              const std::string &path);
