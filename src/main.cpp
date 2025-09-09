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
#include "../include/config.h"
#include "../include/file_operations.h"
#include "../include/gitignore.h"
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

static void collect_dir_with_gitignore(
    const std::filesystem::path &dir, const std::vector<Language> &filter,
    const std::vector<std::filesystem::path> &exclude_dirs,
    const std::vector<std::filesystem::path> &exclude_files,
    std::vector<std::string> &out, std::vector<ignore::RulesFile> &stack) {
  namespace fs = std::filesystem;
  auto rf = ignore::load_rules_for_dir(dir);
  bool pushed = !rf.rules.empty();
  if (pushed) stack.push_back(std::move(rf));

  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    const fs::directory_entry &ent = *it;
    fs::path p = ent.path();
    bool is_dir = ent.is_directory(ec);
    if (ec) is_dir = false;
    bool is_reg = ent.is_regular_file(ec);
    if (ec) is_reg = false;

    if (is_dir && p.filename() == ".git") continue;

    // Exclude directories early (skip recursion)
    if (is_dir) {
      bool skip_dir = false;
      for (const auto &ed : exclude_dirs) {
        std::error_code ec2;
        auto rel = fs::relative(p, ed, ec2);
        if (!ec2 && !rel.empty() && rel.is_relative() &&
            rel.native().rfind("..", 0) != 0) {
          skip_dir = true;
          break;
        }
        std::error_code ec3;
        auto pnorm = fs::weakly_canonical(p, ec3);
        auto dnorm = fs::weakly_canonical(ed, ec3);
        if (!ec3 && pnorm == dnorm) {
          skip_dir = true;
          break;
        }
      }
      if (skip_dir) continue;
    }

    if (ignore::is_ignored(stack, p, is_dir)) {
      if (is_dir) continue;
      if (is_reg) continue;
    }

    if (is_dir) {
      collect_dir_with_gitignore(p, filter, exclude_dirs, exclude_files, out,
                                 stack);
      continue;
    }

    if (is_reg) {
      std::string fpath = p.string();
      // Exclude files
      bool skip_file = false;
      for (const auto &ef : exclude_files) {
        std::error_code ec3;
        auto pnorm = fs::weakly_canonical(p, ec3);
        auto fnorm = fs::weakly_canonical(ef, ec3);
        if (!ec3 && pnorm == fnorm) {
          skip_file = true;
          break;
        }
      }
      if (skip_file) continue;
      Language lang = detect_language_from_path(fpath);
      if (lang == Language::Unknown) continue;
      if (!language_is_selected(lang, filter)) continue;
      out.push_back(fpath);
    }
  }

  if (pushed) stack.pop_back();
}

static void collect_source_files(const std::vector<std::string> &inputs,
                                 const std::vector<Language> &filter,
                                 const std::vector<std::string> &excludes,
                                 std::vector<std::string> &out) {
  namespace fs = std::filesystem;
  // Prepare exclude lists
  std::vector<fs::path> exclude_dirs;
  std::vector<fs::path> exclude_files;
  for (const auto &e : excludes) {
    std::error_code ec;
    fs::path ep = fs::absolute(fs::path(e), ec);
    if (ec) ep = fs::path(e);
    if (fs::is_directory(ep, ec))
      exclude_dirs.push_back(ep);
    else
      exclude_files.push_back(ep);
  }
  for (const auto &p : inputs) {
    fs::path path(p);
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
      std::vector<ignore::RulesFile> stack;
      // Skip top-level directory if excluded
      bool skip_dir = false;
      for (const auto &ed : exclude_dirs) {
        std::error_code ec2;
        auto rel = fs::weakly_canonical(path, ec2);
        auto dnorm = fs::weakly_canonical(ed, ec2);
        if (!ec2 && rel == dnorm) {
          skip_dir = true;
          break;
        }
      }
      if (skip_dir) continue;
      collect_dir_with_gitignore(path, filter, exclude_dirs, exclude_files, out,
                                 stack);
    } else if (fs::is_regular_file(path, ec)) {
      // Skip if explicitly excluded
      bool skip = false;
      for (const auto &ef : exclude_files) {
        std::error_code ec3;
        auto pnorm = fs::weakly_canonical(path, ec3);
        auto fnorm = fs::weakly_canonical(ef, ec3);
        if (!ec3 && pnorm == fnorm) {
          skip = true;
          break;
        }
      }
      if (skip) continue;
      Language lang = detect_language_from_path(p);
      if (lang != Language::Unknown && language_is_selected(lang, filter))
        out.push_back(p);
    } else {
      // Ignore non-existing inputs silently
    }
  }
}

int main(int argc, char **argv) {
  LoadedConfig file_cfg = load_cognity_toml("cognity.toml");

  std::vector<std::string> args = args_to_string(argv, argc);
  CLI_PARSE_RESULT parsed;
  try {
    parsed = parse_arguments_relaxed(args);
  } catch (const std::invalid_argument &e) {
    term::Painter p;
    p.init(false, false);
    p.print(std::cerr, term::Style::red, std::string("Error: ") + e.what(),
            true);
    std::cerr << std::endl;
    return 1;
  }

  if (parsed.has_help && parsed.args.show_help) {
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
           "  -x,  --exclude <list>         Comma-separated files/dirs to "
           "exclude\n"
           "  -fw, --max-fn-width <int>     Truncate function names to width "
           "when printing\n"
           "  -h,  --help                   Show this help and exit\n"
           "\n"
           "Note: place options after paths; directories are scanned "
           "recursively.\n"
           "\n"
           "Also supports a cognity.toml file in the working directory\n"
           "to provide default values for the same options. CLI options\n"
           "override the config file.\n";
    return 0;
  }

  // Merge config + CLI (CLI overrides)
  CLI_ARGUMENTS cli_args;  // start with defaults
  if (file_cfg.loaded) {
    if (file_cfg.present.max_complexity)
      cli_args.max_complexity_allowed = file_cfg.args.max_complexity_allowed;
    if (file_cfg.present.quiet) cli_args.quiet = file_cfg.args.quiet;
    if (file_cfg.present.ignore_complexity)
      cli_args.ignore_complexity = file_cfg.args.ignore_complexity;
    if (file_cfg.present.detail) cli_args.detail = file_cfg.args.detail;
    if (file_cfg.present.sort) cli_args.sort = file_cfg.args.sort;
    if (file_cfg.present.output_csv)
      cli_args.output_csv = file_cfg.args.output_csv;
    if (file_cfg.present.output_json)
      cli_args.output_json = file_cfg.args.output_json;
    if (file_cfg.present.max_fn_width)
      cli_args.max_function_width = file_cfg.args.max_function_width;
    if (file_cfg.present.languages)
      cli_args.languages = file_cfg.args.languages;
    if (file_cfg.present.paths) cli_args.paths = file_cfg.args.paths;
    if (file_cfg.present.excludes) cli_args.excludes = file_cfg.args.excludes;
  }

  // Apply CLI overrides where present
  if (parsed.has_max_complexity)
    cli_args.max_complexity_allowed = parsed.args.max_complexity_allowed;
  if (parsed.has_quiet) cli_args.quiet = parsed.args.quiet;
  if (parsed.has_ignore_complexity)
    cli_args.ignore_complexity = parsed.args.ignore_complexity;
  if (parsed.has_detail) cli_args.detail = parsed.args.detail;
  if (parsed.has_sort) cli_args.sort = parsed.args.sort;
  if (parsed.has_output_csv) cli_args.output_csv = parsed.args.output_csv;
  if (parsed.has_output_json) cli_args.output_json = parsed.args.output_json;
  if (parsed.has_max_fn_width)
    cli_args.max_function_width = parsed.args.max_function_width;
  if (parsed.has_lang) cli_args.languages = parsed.args.languages;
  if (parsed.has_paths) cli_args.paths = parsed.args.paths;
  if (parsed.has_excludes) cli_args.excludes = parsed.args.excludes;

  if (cli_args.paths.empty()) {
    term::Painter p;
    p.init(false, false);
    p.print(std::cerr, term::Style::red,
            "Error: expected at least one path (via CLI or cognity.toml)",
            true);
    std::cerr << std::endl;
    return 1;
  }

  TSParser *parser = ts_parser_new();
  std::string source_code;

  std::vector<std::string> files;
  collect_source_files(cli_args.paths, cli_args.languages, cli_args.excludes,
                       files);
  if (files.empty()) {
    term::Painter p;
    p.init(false, false);
    p.print(std::cerr, term::Style::red, "No matching source files found",
            true);
    std::cerr << std::endl;
    ts_parser_delete(parser);
    return 1;
  }

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

    for (const auto &fn : functions_complexity) {
      all_rows.push_back(Row{path, fn});
    }
  }

  bool any_exceeds = false;
  if (!cli_args.ignore_complexity) {
    for (const auto &r : all_rows) {
      if (r.fn.complexity > (unsigned)cli_args.max_complexity_allowed) {
        any_exceeds = true;
        break;
      }
    }
  }

  auto rows_cmp_name = [](const Row &a, const Row &b) {
    if (a.file != b.file) return a.file < b.file;
    if (a.fn.name != b.fn.name) return a.fn.name < b.fn.name;
    return a.fn.row < b.fn.row;
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

  auto apply_global_sort_for_rows = [&](std::vector<Row> &rows) {
    switch (cli_args.sort) {
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
  };

  if (cli_args.output_json) {
    apply_global_sort_for_rows(all_rows);
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
    apply_global_sort_for_rows(all_rows);
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

  apply_global_sort_for_rows(all_rows);

  term::Painter painter;
  painter.init(false, false);

  const std::string file_header = "File";
  const std::string func_header = "Function";
  const std::string cc_header = "cognitive complexity";

  // Helper to compute number of digits for unsigned integers
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
  for (const auto &r : all_rows) {
    file_w = std::max(file_w, static_cast<int>(r.file.size()));
    std::string suffix = "@" + std::to_string(r.fn.row + 1);
    fn_w = std::max(fn_w, static_cast<int>(r.fn.name.size() + suffix.size()));
    cc_w = std::max(cc_w, digits(r.fn.complexity));
  }
  if (cli_args.max_function_width > 0)
    fn_w = std::max(8, std::min(fn_w, cli_args.max_function_width));

  if (painter.out_enabled) std::cout << term::code(term::Style::bold);
  std::cout << std::left << std::setw(file_w) << file_header << "  "
            << std::left << std::setw(fn_w) << func_header << "  " << std::left
            << std::setw(cc_w) << cc_header;
  if (painter.out_enabled) std::cout << term::code(term::Style::reset);
  std::cout << std::endl;

  for (const auto &r : all_rows) {
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
    bool exceeds = r.fn.complexity > (unsigned)cli_args.max_complexity_allowed;
    if (painter.out_enabled) {
      std::cout << term::code(exceeds ? term::Style::red : term::Style::green)
                << std::setw(cc_w) << r.fn.complexity
                << term::code(term::Style::reset);
    } else {
      std::cout << std::setw(cc_w) << r.fn.complexity;
    }
    if ((!cli_args.ignore_complexity) && exceeds) {
      std::string note =
          "  (exceeds " + std::to_string(cli_args.max_complexity_allowed) + ")";
      if (painter.out_enabled) {
        std::cout << term::code(term::Style::red) << note
                  << term::code(term::Style::reset);
      } else {
        std::cout << note;
      }
    }
    std::cout << std::endl;
  }

  ts_parser_delete(parser);

  return any_exceeds ? 2 : 0;
}
