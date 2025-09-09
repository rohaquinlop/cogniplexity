#ifndef COGNITY_CONFIG_H
#define COGNITY_CONFIG_H

#include <string>

#include "./cli_arguments.h"

struct ConfigPresence {
  bool paths = false;
  bool excludes = false;
  bool max_complexity = false;
  bool quiet = false;
  bool ignore_complexity = false;
  bool detail = false;
  bool sort = false;
  bool output_csv = false;
  bool output_json = false;
  bool max_fn_width = false;
  bool languages = false;
};

struct LoadedConfig {
  bool loaded = false;  // true if cognity.toml was found and parsed
  CLI_ARGUMENTS
  args{};  // values parsed (only meaningful where present.* = true)
  ConfigPresence present{};  // which keys were explicitly provided
};

// Load config from the given file path. The parser supports a small TOML
// subset:
// - key = value pairs (int, bool, string)
// - arrays of strings/ints for paths/languages
// - comments starting with '#'
// Supported keys (case-insensitive):
//   paths, max_complexity | max_complexity_allowed, quiet, ignore_complexity,
//   detail, sort, output_csv, output_json, max_fn_width | max_function_width,
//   lang | languages, exclude
LoadedConfig load_cognity_toml(const std::string &filepath);

#endif
