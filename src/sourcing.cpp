#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "../include/gitignore.h"
#include "../include/sourcing.h"

extern "C" {
const TSLanguage *tree_sitter_python();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_typescript();
const TSLanguage *tree_sitter_tsx();
const TSLanguage *tree_sitter_c();
const TSLanguage *tree_sitter_cpp();
}

Language detect_language_from_path(const std::string &path) {
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

void collect_source_files(const std::vector<std::string> &inputs,
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

void set_ts_language_for_file(TSParser *parser, Language lang,
                              const std::string &path) {
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
      break;
  }
}
