#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

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

#include "../include/cli_arguments.h"
#include "../include/cognitive_complexity.h"
#include "../include/file_operations.h"
#include "../include/gsg.h"
#include "../tree-sitter/lib/include/tree_sitter/api.h"

extern "C" {
const TSLanguage *tree_sitter_python();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_typescript();
const TSLanguage *tree_sitter_tsx();
}

// Simple cross-platform console color helper using ANSI codes.
// On Windows, attempts to enable Virtual Terminal Processing; if not available,
// colors are disabled (fallback to plain text).
namespace term {
enum class Style { reset, bold, dim, red, green, yellow, blue, magenta, cyan };

struct ColorConfig {
  bool ansi_enabled_stdout = false;
  bool ansi_enabled_stderr = false;
};

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

static inline const char *code(Style s) {
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

struct Painter {
  ColorConfig cfg;
  bool out_enabled = false;
  bool err_enabled = false;

  void init(bool json_mode, bool csv_mode) {
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

void parse_python() {
  TSParser *parser = ts_parser_new();

  ts_parser_set_language(parser, tree_sitter_python());

  const char *source_code = "[1, 2, 3, 4]";
  TSTree *tree =
      ts_parser_parse_string(parser, NULL, source_code, strlen(source_code));

  TSNode root_node = ts_tree_root_node(tree);
  TSNode array_node = ts_node_named_child(root_node, 0);
  TSNode number_node = ts_node_named_child(array_node, 0);

  std::cout << ts_node_grammar_type(root_node) << std::endl;
  std::cout << ts_node_grammar_type(array_node) << std::endl;
  std::cout << ts_node_grammar_type(number_node) << std::endl;

  ts_tree_delete(tree);
  ts_parser_delete(parser);
}

static Language detect_language_from_path(const std::string &path) {
  auto ends_with = [&](const char *suf) {
    size_t n = strlen(suf);
    return path.size() >= n && path.compare(path.size() - n, n, suf) == 0;
  };
  if (ends_with(".py")) return Language::Python;
  if (ends_with(".c")) return Language::C;
  if (ends_with(".cpp") || ends_with(".cc") || ends_with(".cxx"))
    return Language::Cpp;
  if (ends_with(".js") || ends_with(".mjs") || ends_with(".cjs"))
    return Language::JavaScript;
  if (ends_with(".ts") || ends_with(".tsx")) return Language::TypeScript;
  if (ends_with(".java")) return Language::Java;
  return Language::Unknown;
}

static bool language_is_selected(Language lang,
                                 const std::vector<Language> &filter) {
  if (filter.empty()) return true;
  for (auto l : filter)
    if (l == lang) return true;
  return false;
}

static void collect_source_files(const std::vector<std::string> &inputs,
                                 const std::vector<Language> &filter,
                                 std::vector<std::string> &out) {
  namespace fs = std::filesystem;
  for (const auto &p : inputs) {
    fs::path path(p);
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
      for (fs::recursive_directory_iterator it(path, ec), end; it != end;
           it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file()) continue;
        std::string fpath = it->path().string();
        Language lang = detect_language_from_path(fpath);
        if (lang == Language::Unknown) continue;
        if (!language_is_selected(lang, filter)) continue;
        out.push_back(fpath);
      }
    } else if (fs::is_regular_file(path, ec)) {
      Language lang = detect_language_from_path(p);
      if (lang != Language::Unknown && language_is_selected(lang, filter))
        out.push_back(p);
    } else {
      // Ignore non-existing inputs silently
    }
  }
}

int main(int argc, char **argv) {
  CLI_ARGUMENTS cli_args;
  if (argc <= 1) {
    term::Painter p;
    p.init(false, false);
    p.print(std::cerr, term::Style::red, "Error: expected at least one path",
            true);
    std::cerr << std::endl;
    return 1;
  }
  std::vector<std::string> args = args_to_string(argv, argc);

  try {
    cli_args = load_from_vs_arguments(args);
  } catch (const std::invalid_argument &e) {
    term::Painter p;
    p.init(false, false);
    p.print(std::cerr, term::Style::red, std::string("Error: ") + e.what(),
            true);
    std::cerr << std::endl;
    return 1;
  }

  if (cli_args.show_help) {
    std::cout
        << "Usage: cognity <paths...> [options]\n"
           "\n"
           "Options:\n"
           "  -mx, --max-complexity <int>   Max allowed complexity (default "
           "15)\n"
           "  -q,  --quiet                  Quiet mode (only offenders unless "
           "-i)\n"
           "  -i,  --ignore-complexity      Ignore max complexity threshold\n"
           "  -d,  --detail <low|normal>    Detail level (default normal)\n"
           "  -s,  --sort <asc|desc|name>   Sort order (default name)\n"
           "  -csv, --output-csv            Output CSV\n"
           "  -json, --output-json          Output JSON\n"
           "  -l,  --lang <list>            Comma-separated languages filter "
           "(e.g. py,js)\n"
           "  -fw, --max-fn-width <int>     Truncate function names to width "
           "when printing\n"
           "  -h,  --help                   Show this help and exit\n"
           "\n"
           "Note: place options after paths; directories are scanned "
           "recursively.\n";
    return 0;
  }

  TSParser *parser = ts_parser_new();
  std::string source_code;

  // Expand inputs: allow directories and language filtering
  std::vector<std::string> files;
  collect_source_files(cli_args.paths, cli_args.languages, files);
  if (files.empty()) {
    term::Painter p;
    p.init(false, false);
    p.print(std::cerr, term::Style::red, "No matching source files found",
            true);
    std::cerr << std::endl;
    ts_parser_delete(parser);
    return 1;
  }

  // If JSON/CSV is requested, aggregate across files first
  struct Row {
    std::string file;
    FunctionComplexity fn;
  };
  std::vector<Row> all_rows;

  for (std::string &path : files) {
    Language lang = detect_language_from_path(path);
    switch (lang) {
      case Language::Python:
        ts_parser_set_language(parser, tree_sitter_python());
        break;
      case Language::JavaScript:
        ts_parser_set_language(parser, tree_sitter_javascript());
        break;
      case Language::C:
        ts_parser_set_language(parser, tree_sitter_c());
        break;
      case Language::Cpp:
        ts_parser_set_language(parser, tree_sitter_cpp());
        break;
      case Language::TypeScript: {
        if (path.size() >= 4 && path.rfind(".tsx") == path.size() - 4) {
          ts_parser_set_language(parser, tree_sitter_tsx());
        } else {
          ts_parser_set_language(parser, tree_sitter_typescript());
        }
        break;
      }
      default:
        // Should not happen because we pre-filter files
        continue;
    }
    try {
      source_code = load_file_content(path);
    } catch (const std::runtime_error &e) {
      term::Painter p;
      p.init(false, false);
      p.print(std::cerr, term::Style::red, std::string("Error: ") + e.what(),
              true);
      std::cerr << std::endl;
      return 1;
    }

    std::vector<FunctionComplexity> functions_complexity =
        functions_complexity_file(source_code, parser, lang);

    // Sorting within a file according to CLI preference
    auto cmp_name = [](const FunctionComplexity &a,
                       const FunctionComplexity &b) { return a.name < b.name; };
    auto cmp_asc = [](const FunctionComplexity &a,
                      const FunctionComplexity &b) {
      return a.complexity < b.complexity;
    };
    auto cmp_desc = [](const FunctionComplexity &a,
                       const FunctionComplexity &b) {
      return a.complexity > b.complexity;
    };
    switch (cli_args.sort) {
      case NAME:
        std::sort(functions_complexity.begin(), functions_complexity.end(),
                  cmp_name);
        break;
      case ASC:
        std::sort(functions_complexity.begin(), functions_complexity.end(),
                  cmp_asc);
        break;
      case DESC:
        std::sort(functions_complexity.begin(), functions_complexity.end(),
                  cmp_desc);
        break;
    }

    // Collect rows and also print text output if not JSON/CSV
    for (const auto &fn : functions_complexity) {
      all_rows.push_back(Row{path, fn});
    }
  }

  // Determine if any function exceeds the threshold (for exit status)
  bool any_exceeds = false;
  if (!cli_args.ignore_complexity) {
    for (const auto &r : all_rows) {
      if (r.fn.complexity > (unsigned)cli_args.max_complexity_allowed) {
        any_exceeds = true;
        break;
      }
    }
  }

  // Handle JSON/CSV outputs first (take precedence over text)
  if (cli_args.output_json) {
    std::cout << "[";
    for (size_t i = 0; i < all_rows.size(); ++i) {
      const auto &r = all_rows[i];
      std::cout << (i ? ",\n" : "\n");
      std::cout << "  {\"file\": \"" << r.file << "\", "
                << "\"function\": \"" << r.fn.name << "@" << r.fn.row + 1
                << "\", "
                << "\"complexity\": " << r.fn.complexity << ", "
                << "\"line\": " << r.fn.row + 1 << " }";
    }
    if (!all_rows.empty()) std::cout << "\n";
    std::cout << "]" << std::endl;
    ts_parser_delete(parser);
    return any_exceeds ? 2 : 0;
  }

  if (cli_args.output_csv) {
    std::cout << "file,function,complexity,line" << std::endl;
    for (const auto &r : all_rows) {
      std::cout << r.file << "," << r.fn.name << "@" << r.fn.row + 1 << ","
                << r.fn.complexity << "," << r.fn.row + 1 << std::endl;
    }
    ts_parser_delete(parser);
    return any_exceeds ? 2 : 0;
  }

  // If quiet and not ignoring complexity, only keep offenders
  if (cli_args.quiet && !cli_args.ignore_complexity) {
    all_rows.erase(std::remove_if(all_rows.begin(), all_rows.end(),
                                  [&](const Row &r) {
                                    return r.fn.complexity <=
                                           cli_args.max_complexity_allowed;
                                  }),
                   all_rows.end());
  }

  // Default minimal
  // Re-group rows by file (preserve original in-file order from chosen sort)
  std::stable_sort(all_rows.begin(), all_rows.end(),
                   [](const Row &a, const Row &b) { return a.file < b.file; });

  // Helper to compute number of digits for unsigned integers
  auto digits = [](unsigned int v) -> int {
    int d = 1;
    while (v >= 10) {
      v /= 10;
      ++d;
    }
    return d;
  };

  // Print grouped by file with dynamic column widths
  std::size_t i = 0;
  const std::string cc_header = "cognitive complexity";
  while (i < all_rows.size()) {
    static term::Painter painter;
    static bool painter_initialized = false;
    if (!painter_initialized) {
      painter.init(false, false);
      painter_initialized = true;
    }
    const std::string current_file = all_rows[i].file;

    // Determine the range [i, j) for this file
    std::size_t j = i;
    // Column widths (ensure minimums based on header labels)
    int max_cc_width = static_cast<int>(cc_header.size());
    int max_fn_width = 8;  // at least length of "Function"
    while (j < all_rows.size() && all_rows[j].file == current_file) {
      const auto &fn = all_rows[j].fn;
      max_cc_width = std::max(max_cc_width, digits(fn.complexity));
      // Account for function name plus @line suffix in width
      std::string suffix = "@" + std::to_string(fn.row + 1);
      max_fn_width = std::max(max_fn_width,
                              static_cast<int>(fn.name.size() + suffix.size()));
      ++j;
    }

    // Apply optional truncation for function column
    if (cli_args.max_function_width > 0) {
      // Keep at least header width
      max_fn_width =
          std::max(8, std::min(max_fn_width, cli_args.max_function_width));
    }

    if (i) std::cout << "\n";
    painter.print(std::cout, term::Style::cyan, current_file);
    std::cout << std::endl;
    if (painter.out_enabled) std::cout << term::code(term::Style::bold);
    std::cout << "  " << std::left << std::setw(max_fn_width) << "Function"
              << std::left << "  " << std::setw(max_cc_width) << cc_header;
    if (painter.out_enabled) std::cout << term::code(term::Style::reset);
    std::cout << std::endl;

    for (std::size_t k = i; k < j; ++k) {
      const auto &r = all_rows[k];
      std::string suffix = "@" + std::to_string(r.fn.row + 1);
      std::string base = r.fn.name;
      std::string fn_name = base + suffix;
      if (static_cast<int>(fn_name.size()) > max_fn_width) {
        int avail = max_fn_width - static_cast<int>(suffix.size());
        if (avail > 3) {
          fn_name =
              base.substr(0, static_cast<size_t>(avail - 3)) + "..." + suffix;
        } else if (avail > 0) {
          fn_name = base.substr(0, static_cast<size_t>(avail)) + suffix;
        } else {
          fn_name = suffix.size() > static_cast<size_t>(max_fn_width)
                        ? suffix.substr(0, static_cast<size_t>(max_fn_width))
                        : suffix;
        }
      }

      std::cout << "  " << std::left << std::setw(max_fn_width) << fn_name
                << std::left << "  ";
      bool exceeds =
          r.fn.complexity > (unsigned)cli_args.max_complexity_allowed;
      if (painter.out_enabled) {
        std::cout << term::code(exceeds ? term::Style::red : term::Style::green)
                  << std::setw(max_cc_width) << r.fn.complexity
                  << term::code(term::Style::reset);
      } else {
        std::cout << std::setw(max_cc_width) << r.fn.complexity;
      }
      if ((!cli_args.ignore_complexity) && exceeds) {
        std::string note = "  (exceeds " +
                           std::to_string(cli_args.max_complexity_allowed) +
                           ")";
        if (painter.out_enabled) {
          std::cout << term::code(term::Style::red) << note
                    << term::code(term::Style::reset);
        } else {
          std::cout << note;
        }
      }
      std::cout << std::endl;
    }

    i = j;
  }

  ts_parser_delete(parser);

  return any_exceeds ? 2 : 0;
}
