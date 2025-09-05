#include <iostream>
#include <stdexcept>
#include <vector>
#include <filesystem>

#include "../include/cli_arguments.h"
#include "../include/cognitive_complexity.h"
#include "../include/file_operations.h"
#include "../include/gsg.h"
#include "../tree-sitter/lib/include/tree_sitter/api.h"

extern "C" {
const TSLanguage* tree_sitter_python();
const TSLanguage* tree_sitter_javascript();
const TSLanguage* tree_sitter_typescript();
const TSLanguage* tree_sitter_tsx();
}

void parse_python() {
  TSParser* parser = ts_parser_new();

  ts_parser_set_language(parser, tree_sitter_python());

  const char* source_code = "[1, 2, 3, 4]";
  TSTree* tree =
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

static bool language_is_selected(Language lang, const std::vector<Language> &filter) {
  if (filter.empty()) return true;
  for (auto l : filter) if (l == lang) return true;
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
      for (fs::recursive_directory_iterator it(path, ec), end; it != end; it.increment(ec)) {
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
      if (lang != Language::Unknown && language_is_selected(lang, filter)) out.push_back(p);
    } else {
      // Ignore non-existing inputs silently
    }
  }
}

int main(int argc, char** argv) {
  CLI_ARGUMENTS cli_args;
  if (argc <= 1) {
    std::cerr << "Error: expected at least one path" << std::endl;
    return 1;
  }
  std::vector<std::string> args = args_to_string(argv, argc);

  try {
    cli_args = load_from_vs_arguments(args);
  } catch (const std::invalid_argument& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  TSParser* parser = ts_parser_new();
  std::string source_code;

  // Expand inputs: allow directories and language filtering
  std::vector<std::string> files;
  collect_source_files(cli_args.paths, cli_args.languages, files);
  if (files.empty()) {
    std::cerr << "No matching source files found" << std::endl;
    ts_parser_delete(parser);
    return 1;
  }

  for (std::string& path : files) {
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
    } catch (const std::runtime_error& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
    }

    std::vector<FunctionComplexity> functions_complexity =
        functions_complexity_file(source_code, parser, lang);

    for (FunctionComplexity& function : functions_complexity) {
      std::cout << "name: " << function.name
                << " - cognitive complexity: " << function.complexity
                << std::endl;
    }
  }

  ts_parser_delete(parser);

  return 0;
}
