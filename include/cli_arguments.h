#ifndef CLI_ARGUMENTS_H
#define CLI_ARGUMENTS_H

#include <string>
#include <vector>

#include "./gsg.h"

enum DetailType { LOW, NORMAL };

enum SortType { ASC, DESC, NAME };

struct CLI_ARGUMENTS {
  std::vector<std::string> paths;
  // Optional: files or directories to exclude from scanning
  std::vector<std::string> excludes;
  int max_complexity_allowed = 15;  // --max-complexity-allowed -mx
  bool quiet = false;               // --quiet -q
  bool ignore_complexity = false;   // --ignore-complexity -i
  DetailType detail = NORMAL;       // --detail -d
  SortType sort = NAME;             // --sort -s
  bool output_csv = false;          // --output-csv -csv
  bool output_json = false;         // --output-json -json
  // If > 0, truncate function names to this width when printing
  int max_function_width = 0;  // --max-fn-width -fw
  // Show help/usage and exit
  bool show_help = false;  // --help -h
  // Optional filter: if non-empty, only these languages are considered
  std::vector<Language> languages;
};

std::vector<std::string> args_to_string(char**, int);
CLI_ARGUMENTS load_from_vs_arguments(std::vector<std::string>&);

struct CLI_PARSE_RESULT {
  CLI_ARGUMENTS args;
  bool has_paths = false;
  bool has_excludes = false;
  bool has_max_complexity = false;
  bool has_quiet = false;
  bool has_ignore_complexity = false;
  bool has_detail = false;
  bool has_sort = false;
  bool has_output_csv = false;
  bool has_output_json = false;
  bool has_max_fn_width = false;
  bool has_lang = false;
  bool has_help = false;
};

CLI_PARSE_RESULT parse_arguments_relaxed(std::vector<std::string>&);

#endif
