#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "../include/config.h"
#include "../include/gsg.h"

using std::string;

static inline string ltrim(string s) {
  size_t i = 0;
  while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
  return s.substr(i);
}
static inline string rtrim(string s) {
  if (s.empty()) return s;
  size_t i = s.size();
  while (i > 0 && std::isspace((unsigned char)s[i - 1])) --i;
  return s.substr(0, i);
}
static inline string trim(string s) { return rtrim(ltrim(std::move(s))); }

static inline string to_lower(string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

static inline bool ieq(const string &a, const string &b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
      return false;
  return true;
}

static std::optional<string> parse_string_value(const string &s, size_t &pos) {
  if (pos >= s.size() || s[pos] != '"') return std::nullopt;
  ++pos;  // skip opening quote
  std::ostringstream out;
  while (pos < s.size()) {
    char c = s[pos++];
    if (c == '"') break;
    if (c == '\\' && pos < s.size()) {
      char n = s[pos++];
      switch (n) {
        case 'n':
          out << '\n';
          break;
        case 'r':
          out << '\r';
          break;
        case 't':
          out << '\t';
          break;
        case '"':
          out << '"';
          break;
        case '\\':
          out << '\\';
          break;
        default:
          out << n;
          break;  // minimal escaping
      }
    } else {
      out << c;
    }
  }
  return out.str();
}

static std::optional<long long> parse_int_value(const string &s) {
  if (s.empty()) return std::nullopt;
  char *end = nullptr;
  long long v = std::strtoll(s.c_str(), &end, 10);
  if (end && *end == '\0') return v;
  return std::nullopt;
}

static std::optional<bool> parse_bool_value(const string &s) {
  string t = to_lower(trim(s));
  if (t == "true") return true;
  if (t == "false") return false;
  return std::nullopt;
}

static std::vector<string> split_csv(const string &s) {
  std::vector<string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t comma = s.find(',', start);
    string tok =
        s.substr(start, comma == string::npos ? string::npos : (comma - start));
    tok = trim(tok);
    if (!tok.empty()) out.push_back(tok);
    if (comma == string::npos) break;
    start = comma + 1;
  }
  return out;
}

static Language language_from_token(string tok) {
  tok = to_lower(trim(tok));
  if (tok == "py" || tok == "python") return Language::Python;
  if (tok == "js" || tok == "javascript") return Language::JavaScript;
  if (tok == "ts" || tok == "typescript" || tok == "tsx")
    return Language::TypeScript;
  if (tok == "c") return Language::C;
  if (tok == "cpp" || tok == "c++" || tok == "cc" || tok == "cxx")
    return Language::Cpp;
  if (tok == "java") return Language::Java;
  return Language::Unknown;
}

static void parse_languages_list(const string &value,
                                 std::vector<Language> &out) {
  auto toks = split_csv(value);
  for (auto &tok : toks) {
    Language lang = language_from_token(tok);
    if (lang != Language::Unknown &&
        std::find(out.begin(), out.end(), lang) == out.end())
      out.push_back(lang);
  }
}

static std::vector<string> parse_array_of_strings(const string &raw) {
  std::vector<string> out;
  size_t i = 0;
  // Expect [ ... ]
  while (i < raw.size() && std::isspace((unsigned char)raw[i])) ++i;
  if (i >= raw.size() || raw[i] != '[') return out;
  ++i;
  while (i < raw.size()) {
    while (i < raw.size() && std::isspace((unsigned char)raw[i])) ++i;
    if (i < raw.size() && raw[i] == ']') {
      ++i;
      break;
    }
    if (i >= raw.size()) break;
    if (raw[i] == '"') {
      auto v = parse_string_value(raw, i);
      if (v) out.push_back(*v);
    } else {
      // unquoted token until comma or ]
      size_t j = i;
      while (j < raw.size() && raw[j] != ',' && raw[j] != ']') ++j;
      out.push_back(trim(raw.substr(i, j - i)));
      i = j;
    }
    while (i < raw.size() && std::isspace((unsigned char)raw[i])) ++i;
    if (i < raw.size() && raw[i] == ',') ++i;
  }
  return out;
}

LoadedConfig load_cognity_toml(const std::string &filepath) {
  LoadedConfig cfg;
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(filepath, ec) || !fs::is_regular_file(filepath, ec)) {
    return cfg;  // not loaded
  }

  std::ifstream in(filepath);
  if (!in.is_open()) return cfg;

  cfg.loaded = true;
  string line;
  while (std::getline(in, line)) {
    // strip comments (# ...), naive (ignores quotes); acceptable for simple use
    auto hash = line.find('#');
    if (hash != string::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;
    // tables not supported; expect key = value
    auto eq = line.find('=');
    if (eq == string::npos) continue;
    string key = trim(line.substr(0, eq));
    string value = trim(line.substr(eq + 1));
    if (key.empty() || value.empty()) continue;
    string k = to_lower(key);

    if (ieq(k, "paths")) {
      // support ["a", "b"] or comma-separated string
      std::vector<string> vals;
      if (!value.empty() && value.front() == '[') {
        vals = parse_array_of_strings(value);
      } else if (!value.empty() && value.front() == '"') {
        size_t pos = 0;
        auto s = parse_string_value(value, pos);
        if (s) vals = split_csv(*s);
      } else {
        vals = split_csv(value);
      }
      if (!vals.empty()) {
        cfg.args.paths = vals;
        cfg.present.paths = true;
      }
      continue;
    }

    if (ieq(k, "max_complexity") || ieq(k, "max_complexity_allowed") ||
        ieq(k, "max-complexity")) {
      if (auto v = parse_int_value(value)) {
        cfg.args.max_complexity_allowed = (int)std::max(0LL, *v);
        cfg.present.max_complexity = true;
      }
      continue;
    }

    if (ieq(k, "quiet")) {
      if (auto v = parse_bool_value(value)) {
        cfg.args.quiet = *v;
        cfg.present.quiet = true;
      }
      continue;
    }

    if (ieq(k, "ignore_complexity") || ieq(k, "ignore-complexity")) {
      if (auto v = parse_bool_value(value)) {
        cfg.args.ignore_complexity = *v;
        cfg.present.ignore_complexity = true;
      }
      continue;
    }

    if (ieq(k, "detail")) {
      string v = to_lower(value);
      if (!v.empty() && v.front() == '"' && v.back() == '"' && v.size() >= 2)
        v = to_lower(v.substr(1, v.size() - 2));
      if (v == "low")
        cfg.args.detail = LOW;
      else if (v == "normal")
        cfg.args.detail = NORMAL;
      else
        continue;
      cfg.present.detail = true;
      continue;
    }

    if (ieq(k, "sort")) {
      string v = to_lower(value);
      if (!v.empty() && v.front() == '"' && v.back() == '"' && v.size() >= 2)
        v = to_lower(v.substr(1, v.size() - 2));
      if (v == "asc")
        cfg.args.sort = ASC;
      else if (v == "desc")
        cfg.args.sort = DESC;
      else if (v == "name")
        cfg.args.sort = NAME;
      else
        continue;
      cfg.present.sort = true;
      continue;
    }

    if (ieq(k, "output_csv") || ieq(k, "output-csv")) {
      if (auto v = parse_bool_value(value)) {
        cfg.args.output_csv = *v;
        cfg.present.output_csv = true;
      }
      continue;
    }

    if (ieq(k, "output_json") || ieq(k, "output-json")) {
      if (auto v = parse_bool_value(value)) {
        cfg.args.output_json = *v;
        cfg.present.output_json = true;
      }
      continue;
    }

    if (ieq(k, "max_fn_width") || ieq(k, "max-function-width") ||
        ieq(k, "max_function_width")) {
      if (auto v = parse_int_value(value)) {
        cfg.args.max_function_width = (int)std::max(0LL, *v);
        cfg.present.max_fn_width = true;
      }
      continue;
    }

    if (ieq(k, "lang") || ieq(k, "languages")) {
      std::vector<string> vals;
      if (!value.empty() && value.front() == '[') {
        vals = parse_array_of_strings(value);
      } else if (!value.empty() && value.front() == '"') {
        size_t pos = 0;
        auto s = parse_string_value(value, pos);
        if (s) vals = split_csv(*s);
      } else {
        vals = split_csv(value);
      }
      cfg.args.languages.clear();
      for (auto &tok : vals) {
        Language l = language_from_token(tok);
        if (l != Language::Unknown &&
            std::find(cfg.args.languages.begin(), cfg.args.languages.end(),
                      l) == cfg.args.languages.end()) {
          cfg.args.languages.push_back(l);
        }
      }
      if (!cfg.args.languages.empty()) cfg.present.languages = true;
      continue;
    }
  }

  return cfg;
}
