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

const char *code(Style s);

struct Painter {
  ColorConfig cfg;
  bool out_enabled = false;
  bool err_enabled = false;

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

void sort_functions(std::vector<FunctionComplexity> &functions, SortType sort);

void print_json(std::vector<Row> rows, SortType sort,
                int max_complexity_allowed, bool ignore_complexity,
                DetailType detail);

void print_csv(std::vector<Row> rows, SortType sort, int max_complexity_allowed,
               bool ignore_complexity, DetailType detail);

bool any_exceeds(const std::vector<Row> &rows, int max_complexity_allowed,
                 bool ignore_complexity);
void print_table(std::vector<Row> rows, SortType sort, int max_fn_width,
                 int max_complexity_allowed, bool ignore_complexity, bool quiet,
                 DetailType detail);

}  // namespace report
