#include <cstdio>
#include <cstdlib>
#include <ostream>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

#include <algorithm>
#include <iomanip>

#include "../include/output.h"

namespace term {

static inline bool env_no_color() {
  const char *v = std::getenv("NO_COLOR");
  return v != nullptr;  // https://no-color.org/
}

static inline void enable_win_vt_if_possible(ColorConfig &cfg, bool for_stdout,
                                             bool for_stderr) {
#ifdef _WIN32
  auto try_enable = [](DWORD handle_id) -> bool {
    HANDLE h = GetStdHandle(handle_id);
    if (h == INVALID_HANDLE_VALUE || h == NULL) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return false;
    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) return true;
    DWORD new_mode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(h, new_mode) != 0;
  };
  if (for_stdout) cfg.ansi_enabled_stdout = try_enable(STD_OUTPUT_HANDLE);
  if (for_stderr) cfg.ansi_enabled_stderr = try_enable(STD_ERROR_HANDLE);
#else
  (void)cfg;
  (void)for_stdout;
  (void)for_stderr;
#endif
}

static inline bool stream_supports_color(std::ostream &os, bool is_stderr,
                                         const ColorConfig &cfg) {
  if (env_no_color()) return false;
  FILE *f = is_stderr ? stderr : stdout;
  if (!ISATTY(FILENO(f))) return false;
#ifdef _WIN32
  // Use VT only if we enabled it; otherwise avoid raw escape codes
  return is_stderr ? cfg.ansi_enabled_stderr : cfg.ansi_enabled_stdout;
#else
  (void)os;
  return true;  // TTY on POSIX supports ANSI by default
#endif
}

const char *code(Style s) {
  switch (s) {
    case Style::reset:
      return "\x1b[0m";
    case Style::bold:
      return "\x1b[1m";
    case Style::dim:
      return "\x1b[2m";
    case Style::red:
      return "\x1b[31m";
    case Style::green:
      return "\x1b[32m";
    case Style::yellow:
      return "\x1b[33m";
    case Style::blue:
      return "\x1b[34m";
    case Style::magenta:
      return "\x1b[35m";
    case Style::cyan:
      return "\x1b[36m";
  }
  return "";
}

void Painter::init(bool json_mode, bool csv_mode) {
  // If machine-readable outputs, disable colors regardless.
  if (json_mode || csv_mode) {
    out_enabled = false;
    err_enabled = false;
    return;
  }

  // Try enabling VT on Windows; on POSIX this is a no-op.
  enable_win_vt_if_possible(cfg, true, true);
  out_enabled = stream_supports_color(std::cout, false, cfg);
  err_enabled = stream_supports_color(std::cerr, true, cfg);
}

}  // namespace term

namespace report {

bool any_exceeds(const std::vector<Row> &rows, int max_complexity_allowed,
                 bool ignore_complexity) {
  if (ignore_complexity) return false;
  for (const auto &r : rows) {
    if (r.fn.complexity > (unsigned)max_complexity_allowed) return true;
  }
  return false;
}

static inline void sort_rows(std::vector<Row> &rows, SortType sort) {
  auto rows_cmp_name = [](const Row &a, const Row &b) {
    if (a.file != b.file) return a.file < b.file;
    if (a.fn.name != b.fn.name) return a.fn.name < b.fn.name;
    if (a.fn.row != b.fn.row) return a.fn.row < b.fn.row;
    return a.fn.complexity < b.fn.complexity;
  };
  auto rows_cmp_asc = [](const Row &a, const Row &b) {
    if (a.fn.complexity != b.fn.complexity)
      return a.fn.complexity < b.fn.complexity;
    if (a.file != b.file) return a.file < b.file;
    if (a.fn.name != b.fn.name) return a.fn.name < b.fn.name;
    return a.fn.row < b.fn.row;
  };
  auto rows_cmp_desc = [](const Row &a, const Row &b) {
    if (a.fn.complexity != b.fn.complexity)
      return a.fn.complexity > b.fn.complexity;
    if (a.file != b.file) return a.file < b.file;
    if (a.fn.name != b.fn.name) return a.fn.name < b.fn.name;
    return a.fn.row < b.fn.row;
  };

  switch (sort) {
    case NAME:
      std::sort(rows.begin(), rows.end(), rows_cmp_name);
      break;
    case ASC:
      std::sort(rows.begin(), rows.end(), rows_cmp_asc);
      break;
    case DESC:
      std::sort(rows.begin(), rows.end(), rows_cmp_desc);
      break;
  }
}

void sort_functions(std::vector<FunctionComplexity> &functions, SortType sort) {
  auto cmp_name = [](const FunctionComplexity &a, const FunctionComplexity &b) {
    if (a.name != b.name) return a.name < b.name;
    if (a.row != b.row) return a.row < b.row;
    return a.complexity < b.complexity;
  };
  auto cmp_asc = [](const FunctionComplexity &a, const FunctionComplexity &b) {
    if (a.complexity != b.complexity) return a.complexity < b.complexity;
    if (a.name != b.name) return a.name < b.name;
    return a.row < b.row;
  };
  auto cmp_desc = [](const FunctionComplexity &a, const FunctionComplexity &b) {
    if (a.complexity != b.complexity) return a.complexity > b.complexity;
    if (a.name != b.name) return a.name < b.name;
    return a.row < b.row;
  };
  switch (sort) {
    case NAME:
      std::sort(functions.begin(), functions.end(), cmp_name);
      break;
    case ASC:
      std::sort(functions.begin(), functions.end(), cmp_asc);
      break;
    case DESC:
      std::sort(functions.begin(), functions.end(), cmp_desc);
      break;
  }
}

void print_json(std::vector<Row> rows, SortType sort,
                int max_complexity_allowed, bool ignore_complexity,
                DetailType detail) {
  if (detail == LOW && !ignore_complexity) {
    rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const Row &r) {
                  return r.fn.complexity <= (unsigned)max_complexity_allowed;
                }),
               rows.end());
  }
  sort_rows(rows, sort);
  std::cout << "[";
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto &r = rows[i];
    std::cout << (i ? ",\n" : "\n");
    std::cout << "  {\"file\": \"" << r.file << "\", "
              << "\"function\": \"" << r.fn.name << "@" << r.fn.row + 1
              << "\", "
              << "\"complexity\": " << r.fn.complexity << ", "
              << "\"line\": " << r.fn.row + 1 << " }";
  }
  if (!rows.empty()) std::cout << "\n";
  std::cout << "]" << '\n';
}

void print_csv(std::vector<Row> rows, SortType sort,
               int max_complexity_allowed, bool ignore_complexity,
               DetailType detail) {
  if (detail == LOW && !ignore_complexity) {
    rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const Row &r) {
                  return r.fn.complexity <= (unsigned)max_complexity_allowed;
                }),
               rows.end());
  }
  sort_rows(rows, sort);
  std::cout << "file,function,complexity,line" << '\n';
  for (const auto &r : rows) {
    std::cout << r.file << "," << r.fn.name << "@" << r.fn.row + 1 << ","
              << r.fn.complexity << "," << r.fn.row + 1 << '\n';
  }
}

void print_table(std::vector<Row> rows, SortType sort, int max_fn_width,
                 int max_complexity_allowed, bool ignore_complexity,
                 bool quiet, DetailType detail) {
  sort_rows(rows, sort);

  term::Painter painter;
  painter.init(false, false);

  // In low detail or quiet mode (and not ignoring complexity),
  // only display offenders that exceed the threshold.
  if ((quiet || detail == LOW) && !ignore_complexity) {
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [&](const Row &r) {
                                return r.fn.complexity <=
                                       (unsigned)max_complexity_allowed;
                              }),
               rows.end());
  }

  const std::string file_header = "File";
  const std::string func_header = "Function";
  const std::string cc_header = "cognitive complexity";

  auto digits = [](unsigned int v) -> int {
    int d = 1;
    while (v >= 10) {
      v /= 10;
      ++d;
    }
    return d;
  };

  int file_w = static_cast<int>(file_header.size());
  int fn_w = static_cast<int>(func_header.size());
  int cc_w = static_cast<int>(cc_header.size());
  for (const auto &r : rows) {
    file_w = std::max(file_w, static_cast<int>(r.file.size()));
    std::string suffix = "@" + std::to_string(r.fn.row + 1);
    fn_w = std::max(fn_w, static_cast<int>(r.fn.name.size() + suffix.size()));
    cc_w = std::max(cc_w, digits(r.fn.complexity));
  }
  if (max_fn_width > 0) fn_w = std::max(8, std::min(fn_w, max_fn_width));

  if (painter.out_enabled) std::cout << term::code(term::Style::bold);
  std::cout << std::left << std::setw(file_w) << file_header << "  "
            << std::left << std::setw(fn_w) << func_header << "  " << std::left
            << std::setw(cc_w) << cc_header;
  if (painter.out_enabled) std::cout << term::code(term::Style::reset);
  std::cout << '\n';

  for (const auto &r : rows) {
    std::string suffix = " @ " + std::to_string(r.fn.row + 1);
    std::string base = r.fn.name;
    std::string fn_name = base + suffix;
    if (static_cast<int>(fn_name.size()) > fn_w) {
      int avail = fn_w - static_cast<int>(suffix.size());
      if (avail > 3) {
        fn_name =
            base.substr(0, static_cast<size_t>(avail - 3)) + "..." + suffix;
      } else if (avail > 0) {
        fn_name = base.substr(0, static_cast<size_t>(avail)) + suffix;
      } else {
        fn_name = suffix.size() > static_cast<size_t>(fn_w)
                      ? suffix.substr(0, static_cast<size_t>(fn_w))
                      : suffix;
      }
    }

    std::cout << std::left << std::setw(file_w) << r.file << "  " << std::left
              << std::setw(fn_w) << fn_name << "  ";
    bool exceeds = r.fn.complexity > (unsigned)max_complexity_allowed;
    if (painter.out_enabled) {
      std::cout << term::code(exceeds ? term::Style::red : term::Style::green)
                << std::setw(cc_w) << r.fn.complexity
                << term::code(term::Style::reset);
    } else {
      std::cout << std::setw(cc_w) << r.fn.complexity;
    }
    if ((!ignore_complexity) && exceeds) {
      std::string note =
          "  (exceeds " + std::to_string(max_complexity_allowed) + ")";
      if (painter.out_enabled) {
        std::cout << term::code(term::Style::red) << note
                  << term::code(term::Style::reset);
      } else {
        std::cout << note;
      }
    }
    std::cout << '\n';
  }
}

}  // namespace report
