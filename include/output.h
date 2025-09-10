#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "./cli_arguments.h"
#include "./cognitive_complexity.h"

namespace term {

enum class Style { reset, bold, dim, red, green, yellow, blue, magenta, cyan };

struct ColorConfig {
  bool ansi_enabled_stdout = false;
  bool ansi_enabled_stderr = false;
};

// Return ANSI escape sequence for a given style (or empty string if disabled)
const char *code(Style s);

struct Painter {
  ColorConfig cfg;
  bool out_enabled = false;
  bool err_enabled = false;

  // Initialize color support based on TTY and output mode (json/csv disable)
  void init(bool json_mode, bool csv_mode);

  template <typename T>
  void print(std::ostream &os, Style style, const T &text,
             bool is_err = false) {
    bool enabled = is_err ? err_enabled : out_enabled;
    if (enabled) {
      os << code(style) << text << code(Style::reset);
    } else {
      os << text;
    }
  }
};

}  // namespace term

namespace report {

struct Row {
  std::string file;
  FunctionComplexity fn;
};

// Sort a list of functions according to CLI sort option.
void sort_functions(std::vector<FunctionComplexity> &functions, SortType sort);

// Render rows as JSON array to stdout; applies global sort.
void print_json(std::vector<Row> rows, SortType sort);

// Render rows as CSV to stdout with header; applies global sort.
void print_csv(std::vector<Row> rows, SortType sort);

// Compute if any row exceeds the threshold, honoring ignore flag.
bool any_exceeds(const std::vector<Row> &rows, int max_complexity_allowed,
                 bool ignore_complexity);

// Render a colorized table to stdout; applies global sort.
// max_fn_width: if > 0, truncate function column to this width.
// max_complexity_allowed: used to mark and annotate exceeding functions.
// ignore_complexity: if false, prints an "exceeds N" note for offenders.
// quiet: if true and not ignoring complexity, only prints offenders.
void print_table(std::vector<Row> rows, SortType sort, int max_fn_width,
                 int max_complexity_allowed, bool ignore_complexity,
                 bool quiet);

}  // namespace report
