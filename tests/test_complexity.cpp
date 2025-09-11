#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "../include/cognitive_complexity.h"

extern "C" {
const TSLanguage* tree_sitter_python();
const TSLanguage* tree_sitter_javascript();
const TSLanguage* tree_sitter_typescript();
const TSLanguage* tree_sitter_c();
const TSLanguage* tree_sitter_cpp();
}

static std::string read_file(const std::filesystem::path& p) {
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs) return {};
  std::string data((std::istreambuf_iterator<char>(ifs)),
                   std::istreambuf_iterator<char>());
  return data;
}

static unsigned int compute_file_complexity_lang(
    const std::filesystem::path& rel, Language lang) {
  // Resolve path relative to project root (source dir). Use this source file
  // location to find the repository root regardless of current working dir.
  static const std::filesystem::path project_root =
      std::filesystem::path(__FILE__).parent_path().parent_path();
  std::filesystem::path p = project_root / rel;
  TSParser* parser = ts_parser_new();
  switch (lang) {
    case Language::Python:
      ts_parser_set_language(parser, tree_sitter_python());
      break;
    case Language::JavaScript:
    case Language::TypeScript:
      // TS code uses the TypeScript grammar; JS uses JS grammar
      if (lang == Language::JavaScript)
        ts_parser_set_language(parser, tree_sitter_javascript());
      else
        ts_parser_set_language(parser, tree_sitter_typescript());
      break;
    case Language::C:
      ts_parser_set_language(parser, tree_sitter_c());
      break;
    case Language::Cpp:
      ts_parser_set_language(parser, tree_sitter_cpp());
      break;
    default:
      ts_parser_delete(parser);
      return 0;
  }
  std::string src = read_file(p);
  auto fns = functions_complexity_file(src, parser, lang);
  ts_parser_delete(parser);
  unsigned int sum = 0;
  for (const auto& fn : fns) sum += fn.complexity;
  return sum;
}

int main() {
  // Expected totals per file (mirrors complexipy tests). Paths are relative to
  // repository root.
  const std::map<std::string, unsigned int> expected = {
      {"tests/src/python/test_main.py", 0},
      {"tests/src/python/test_for.py", 5},
      {"tests/src/python/test_for_assign.py", 1},
      {"tests/src/python/test_if.py", 3},
      {"tests/src/python/test_match.py", 0},
      {"tests/src/python/test_multiple_func.py", 0},
      {"tests/src/python/test_nested_func.py", 2},
      {"tests/src/python/test_recursive.py", 0},
      {"tests/src/python/test_ternary_op.py", 1},
      {"tests/src/python/test_try.py", 3},
      {"tests/src/python/test_try_nested.py", 13},
      {"tests/src/python/test_break_continue.py", 3},
      {"tests/src/python/test_class.py", 1},
      {"tests/src/python/test_decorator.py", 1},
      {"tests/src/python/test_while.py", 1},
      {"tests/src/python/test.py", 9},
  };

  bool ok = true;
  for (const auto& [path, exp] : expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::Python);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // JavaScript (current implementation expectations)
  const std::map<std::string, unsigned int> js_expected = {
      {"tests/src/javascript/test_if.js", 4},
      {"tests/src/javascript/test_switch_ternary.js", 0},
  };
  for (const auto& [path, exp] : js_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::JavaScript);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // TypeScript
  const std::map<std::string, unsigned int> ts_expected = {
      {"tests/src/typescript/test_if.ts", 6},
  };
  for (const auto& [path, exp] : ts_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::TypeScript);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // C
  const std::map<std::string, unsigned int> c_expected = {
      {"tests/src/c/test_if.c", 2},
  };
  for (const auto& [path, exp] : c_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::C);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // C++
  const std::map<std::string, unsigned int> cpp_expected = {
      {"tests/src/cpp/test_if.cpp", 4},
      {"tests/src/cpp/test_operator.cpp", 3},
      {"tests/src/cpp/test_lambda.cpp", 8},
      {"tests/src/cpp/test_ctor_dtor.cpp", 2},
      {"tests/src/cpp/test_method_out_of_class.cpp", 2},
      {"tests/src/cpp/test_template_method.cpp", 2},
      {"tests/src/cpp/test_template_out_of_class.cpp", 3},
      {"tests/src/cpp/test_template_free.cpp", 1},
  };
  for (const auto& [path, exp] : cpp_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::Cpp);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  if (ok) {
    std::cout << "All complexity tests passed." << std::endl;
    return 0;
  }
  return 1;
}
