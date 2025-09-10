#include <stdexcept>
#include <string>
#include <vector>

#include "../include/cli_arguments.h"
#include "../include/cli_helpers.h"
#include "../include/cognitive_complexity.h"
#include "../include/config.h"
#include "../include/file_operations.h"
#include "../include/output.h"
#include "../include/sourcing.h"

int main(int argc, char **argv) {
  LoadedConfig file_cfg = load_cognity_toml("cognity.toml");

  std::vector<std::string> args = args_to_string(argv, argc);
  CLI_PARSE_RESULT parsed;
  try {
    parsed = parse_arguments_relaxed(args);
  } catch (const std::invalid_argument &e) {
    cli_helpers::print_error(e.what());
    return 1;
  }

  if (parsed.has_help && parsed.args.show_help) {
    cli_helpers::print_usage();
    return 0;
  }

  if (parsed.has_version && parsed.args.show_version) {
    cli_helpers::print_version();
    return 0;
  }

  // Merge config + CLI (CLI overrides)
  CLI_ARGUMENTS cli_args = cli_helpers::merge_cli_and_config(file_cfg, parsed);

  if (cli_args.paths.empty()) {
    cli_helpers::print_error(
        "expected at least one path (via CLI or cognity.toml)");
    return 1;
  }

  TSParser *parser = ts_parser_new();
  std::string source_code;

  std::vector<std::string> files;
  collect_source_files(cli_args.paths, cli_args.languages, cli_args.excludes,
                       files);
  if (files.empty()) {
    cli_helpers::print_error("No matching source files found");
    ts_parser_delete(parser);
    return 1;
  }

  std::vector<report::Row> all_rows;

  for (std::string &path : files) {
    Language lang = detect_language_from_path(path);
    set_ts_language_for_file(parser, lang, path);
    if (lang == Language::Unknown) continue;
    try {
      source_code = load_file_content(path);
    } catch (const std::runtime_error &e) {
      cli_helpers::print_error(e.what());
      return 1;
    }

    std::vector<FunctionComplexity> functions_complexity =
        functions_complexity_file(source_code, parser, lang);
    report::sort_functions(functions_complexity, cli_args.sort);

    for (const auto &fn : functions_complexity) {
      all_rows.push_back(report::Row{path, fn});
    }
  }

  bool any_exceeds = report::any_exceeds(
      all_rows, cli_args.max_complexity_allowed, cli_args.ignore_complexity);

  auto rows_cmp_name = [](const report::Row &a, const report::Row &b) {
    if (a.file != b.file) return a.file < b.file;
    if (a.fn.name != b.fn.name) return a.fn.name < b.fn.name;
    return a.fn.row < b.fn.row;
  };

  if (cli_args.output_json) {
    report::print_json(all_rows, cli_args.sort);
    ts_parser_delete(parser);
    return any_exceeds ? 2 : 0;
  }

  if (cli_args.output_csv) {
    report::print_csv(all_rows, cli_args.sort);
    ts_parser_delete(parser);
    return any_exceeds ? 2 : 0;
  }

  report::print_table(all_rows, cli_args.sort, cli_args.max_function_width,
                      cli_args.max_complexity_allowed,
                      cli_args.ignore_complexity, cli_args.quiet,
                      cli_args.detail);

  ts_parser_delete(parser);

  return any_exceeds ? 2 : 0;
}
