#pragma once

#include <tree_sitter/api.h>

#ifdef __linux__
#include <cstring>
#endif

#include <string>
#include <vector>

#include "./gsg.h"

Language detect_language_from_path(const std::string &path);

void collect_source_files(const std::vector<std::string> &inputs,
                          const std::vector<Language> &filter,
                          const std::vector<std::string> &excludes,
                          std::vector<std::string> &out);

void set_ts_language_for_file(TSParser *parser, Language lang,
                              const std::string &path);
