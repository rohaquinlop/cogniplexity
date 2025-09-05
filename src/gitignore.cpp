#include "../include/gitignore.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

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

// Wildcard matching supporting '*', '?', and '**'.
// '*' and '?' do not match '/'; '**' matches across '/'.
static bool glob_match(const std::string &pattern, const std::string &text) {
  size_t pi = 0, ti = 0;
  size_t star_pi = std::string::npos;
  size_t star_ti = std::string::npos;
  bool star_allows_slash = false; // true for '**'

  auto is_double_star = [&](size_t pos) -> bool {
    return pos + 1 < pattern.size() && pattern[pos] == '*' && pattern[pos + 1] == '*';
  };

  while (ti < text.size()) {
    if (pi < pattern.size()) {
      char pc = pattern[pi];
      if (pc == '*') {
        bool dbl = is_double_star(pi);
        if (dbl) {
          // collapse multiple '*' in case of '***'
          while (pi < pattern.size() && pattern[pi] == '*') ++pi;
          star_pi = pi; // position after stars
          star_ti = ti;
          star_allows_slash = true;
          continue;
        } else {
          // single '*'
          ++pi;
          star_pi = pi;
          star_ti = ti;
          star_allows_slash = false;
          continue;
        }
      } else if (pc == '?') {
        if (text[ti] == '/') {
          // '?' does not match '/'
          if (star_pi != std::string::npos) {
            // backtrack if prior star can consume more
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
        // escape next char literally if present
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
      // backtrack: let star consume one more char if allowed
      if (star_allows_slash || text[star_ti] != '/') {
        ++star_ti;
        ti = star_ti;
        pi = star_pi;
        continue;
      }
    }
    return false;
  }

  // consume remaining '*' at end of pattern
  while (pi < pattern.size() && pattern[pi] == '*') {
    // handle possible '***'
    ++pi;
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
  }
  return pi == pattern.size();
}

static bool match_against(const ignore::Rule &r,
                          const std::filesystem::path &base,
                          const std::filesystem::path &abs_path,
                          bool is_dir) {
  if (r.dir_only && !is_dir) return false;

  // Only consider paths under base
  std::error_code ec;
  auto rel = std::filesystem::relative(abs_path, base, ec);
  if (ec) return false;
  if (rel.empty()) return false;

  std::string rel_str = to_slash_path(rel);
  // Remove leading "./" that relative might add
  if (rel_str.rfind("./", 0) == 0) rel_str = rel_str.substr(2);

  // If rule has no slash, match only the basename
  if (!r.has_slash) {
    std::string name = abs_path.filename().generic_string();
    return glob_match(r.pattern, name);
  }

  // With slashes: match against relative path string
  // For anchored rules, ensure match is from beginning (glob_match does that).
  // For non-anchored rules we also match from beginning per gitignore semantics
  // when pattern contains '/' (relative to base dir).
  return glob_match(r.pattern, rel_str);
}

}  // namespace

namespace ignore {

static bool parse_rule_line(const std::string &raw, Rule &out) {
  std::string s = trim(raw);
  if (s.empty()) return false;
  if (s[0] == '#') return false; // comment

  Rule r;
  size_t pos = 0;
  if (s[pos] == '!') {
    r.negated = true;
    ++pos;
  }
  // Not handling escaped leading '#', '\\' escapes in general; keep simple
  std::string pat = s.substr(pos);

  // Directory-only if ends with '/'
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
  // Determine match status by last matching rule across the stack (parents first)
  // Start as not ignored
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

