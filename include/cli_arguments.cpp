#include <iostream>
#include <stdexcept>
#include <string>

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

  for (i = i - 1; i < arguments.size(); i++) {
    if (is_max_complexity(arguments[i])) {
      try {
        max_complexity_allowed = std::stoi(arguments[i + 1]);
      } catch (const std::invalid_argument &e) {
        throw std::invalid_argument(
            "Expected a number when defining max_complexity_allowed");
      }
      i++;
    } else if (is_quiet(arguments[i]))
      quiet = true;
    else if (is_ignore_complexity(arguments[i]))
      ignore_complexity = true;
    else if (is_detail(arguments[i])) {
      // Validate that there's still range, then check that the next value is
      // one of the two possibles
    }
  }

  return CLI_ARGUMENTS{paths,      max_complexity_allowed,
                       quiet,      ignore_complexity,
                       detail,     sort,
                       output_csv, output_json};
}
