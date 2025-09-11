#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "../include/gitignore.h"

namespace {

static std::string to_slash_path(const std::filesystem::path &p) {
  std::string s = p.generic_string();
  return s;
}

static std::string trim(const std::string &s) {
  size_t i = 0, j = s.size();
  while (i < j && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1]))) --j;
  return s.substr(i, j - i);
}

static bool glob_match(const std::string &pattern, const std::string &text) {
  size_t pi = 0, ti = 0;
  size_t star_pi = std::string::npos;
  size_t star_ti = std::string::npos;
  bool star_allows_slash = false;

  auto is_double_star = [&](size_t pos) -> bool {
    return pos + 1 < pattern.size() && pattern[pos] == '*' &&
           pattern[pos + 1] == '*';
  };

  while (ti < text.size()) {
    if (pi < pattern.size()) {
      char pc = pattern[pi];
      if (pc == '*') {
        bool dbl = is_double_star(pi);
        if (dbl) {
          while (pi < pattern.size() && pattern[pi] == '*') ++pi;
          star_pi = pi;
          star_ti = ti;
          star_allows_slash = true;
          continue;
        } else {
          ++pi;
          star_pi = pi;
          star_ti = ti;
          star_allows_slash = false;
          continue;
        }
      } else if (pc == '?') {
        if (text[ti] == '/') {
          if (star_pi != std::string::npos) {
            if (star_allows_slash || text[star_ti] != '/') {
              ++star_ti;
              ti = star_ti;
              pi = star_pi;
              continue;
            }
          }
          return false;
        }
        ++pi;
        ++ti;
        continue;
      } else if (pc == '\\') {
        ++pi;
        if (pi >= pattern.size()) return false;
        if (pattern[pi] == text[ti]) {
          ++pi;
          ++ti;
          continue;
        }
      } else if (pc == text[ti]) {
        ++pi;
        ++ti;
        continue;
      }
    }

    if (star_pi != std::string::npos) {
      if (star_allows_slash || text[star_ti] != '/') {
        ++star_ti;
        ti = star_ti;
        pi = star_pi;
        continue;
      }
    }
    return false;
  }

  while (pi < pattern.size() && pattern[pi] == '*') {
    ++pi;
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
  }
  return pi == pattern.size();
}

static bool match_against(const ignore::Rule &r,
                          const std::filesystem::path &base,
                          const std::filesystem::path &abs_path, bool is_dir) {
  if (r.dir_only && !is_dir) return false;

  std::error_code ec;
  auto rel = std::filesystem::relative(abs_path, base, ec);
  if (ec) return false;
  if (rel.empty()) return false;

  std::string rel_str = to_slash_path(rel);
  if (rel_str.rfind("./", 0) == 0) rel_str = rel_str.substr(2);

  if (!r.has_slash) {
    std::string name = abs_path.filename().generic_string();
    return glob_match(r.pattern, name);
  }

  return glob_match(r.pattern, rel_str);
}

}  // namespace

namespace ignore {

static bool parse_rule_line(const std::string &raw, Rule &out) {
  std::string s = trim(raw);
  if (s.empty()) return false;
  if (s[0] == '#') return false;

  Rule r;
  size_t pos = 0;
  if (s[pos] == '!') {
    r.negated = true;
    ++pos;
  }
  std::string pat = s.substr(pos);

  if (!pat.empty() && pat.back() == '/') {
    r.dir_only = true;
    pat.pop_back();
  }
  if (!pat.empty() && pat.front() == '/') {
    r.anchored = true;
    pat.erase(pat.begin());
  }
  r.has_slash = pat.find('/') != std::string::npos;
  r.pattern = pat;
  out = r;
  return !r.pattern.empty();
}

RulesFile load_rules_for_dir(const std::filesystem::path &dir) {
  RulesFile rf;
  rf.base = dir;
  std::filesystem::path file = dir / ".gitignore";
  std::ifstream in(file);
  if (!in.good()) return rf;
  std::string line;
  while (std::getline(in, line)) {
    Rule r;
    if (parse_rule_line(line, r)) {
      rf.rules.push_back(std::move(r));
    }
  }
  return rf;
}

bool is_ignored(const std::vector<RulesFile> &stack,
                const std::filesystem::path &abs_path, bool is_dir) {
  bool ignored = false;
  for (const auto &rf : stack) {
    for (const auto &r : rf.rules) {
      if (match_against(r, rf.base, abs_path, is_dir)) {
        ignored = !r.negated;
      }
    }
  }
  return ignored;
}

}  // namespace ignore
