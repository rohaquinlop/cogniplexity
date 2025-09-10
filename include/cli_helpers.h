#pragma once

#include <iostream>

#include "./cli_arguments.h"
#include "./config.h"
#include "./output.h"

namespace cli_helpers {

inline void print_usage() {
  std::cout
      << "Usage: cognity <paths...> [options]\n"
         "\n"
         "Options:\n"
         "  -mx, --max-complexity <int>   Max allowed complexity (default 15)\n"
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
         "       --version                Show version and exit\n"
         "\n"
         "Note: place options after paths; directories are scanned "
         "recursively.\n"
         "\n"
         "Also supports a cognity.toml file in the working directory\n"
         "to provide default values for the same options. CLI options\n"
         "override the config file.\n";
}

inline void print_version() {
#ifdef COGNITY_VERSION
  std::cout << "cognity " << COGNITY_VERSION << '\n';
#else
  std::cout << "cognity" << '\n';
#endif
}

inline void print_error(const std::string &message) {
  term::Painter p;
  p.init(false, false);
  p.print(std::cerr, term::Style::red, std::string("Error: ") + message, true);
  std::cerr << '\n';
}

inline CLI_ARGUMENTS merge_cli_and_config(const LoadedConfig &file_cfg,
                                          const CLI_PARSE_RESULT &parsed) {
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

  return cli_args;
}

}  // namespace cli_helpers
