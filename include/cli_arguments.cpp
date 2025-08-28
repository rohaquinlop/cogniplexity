#include <stdexcept>

#include "cli_arguments.h"

bool is_max_complexity(std::string &s) {
  return s == "--max-complexity" or s == "-mx";
}

bool is_quiet(std::string &s) { return s == "--quiet" or s == "-q"; }

bool is_ignore_complexity(std::string &s) {
  return s == "--ignore-complexity" or s == "-i";
}

bool is_detail(std::string &s) { return s == "--detail" or s == "-d"; }

bool is_sort(std::string &s) { return s == "--sort" or s == "-s"; }

bool is_output_csv(std::string &s) {
  return s == "--output-csv" or s == "-csv";
}

bool is_output_json(std::string &s) {
  return s == "--output-json" or s == "-json";
}

bool is_argument(std::string &s) {
  return is_max_complexity(s) or is_quiet(s) or is_ignore_complexity(s) or
         is_detail(s) or is_sort(s) or is_output_csv(s) or is_output_json(s);
}

CLI_ARGUMENTS load_from_vs_arguments(std::vector<std::string> &arguments) {
  int i;
  bool reading_paths = true;
  std::vector<std::string> paths;
  int max_complexity_allowed = 15;
  bool quiet = false;
  bool ignore_complexity = false;
  DetailType detail = NORMAL;
  SortType sort = NAME;
  bool output_csv = false;
  bool output_json = false;

  for (i = 0; i < arguments.size() && reading_paths; i++) {
    if (!is_argument(arguments[i]))
      paths.push_back(arguments[i]);
    else
      reading_paths = false;
  }

  if (!reading_paths and paths.empty())
    throw std::invalid_argument("Expected at least one path");

  return CLI_ARGUMENTS{paths,      max_complexity_allowed,
                       quiet,      ignore_complexity,
                       detail,     sort,
                       output_csv, output_json};
}
