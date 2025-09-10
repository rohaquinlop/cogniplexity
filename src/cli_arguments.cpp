#include <algorithm>
#include <stdexcept>
#include <string>

#include "../include/cli_arguments.h"
#include "../include/gsg.h"

std::vector<std::string> args_to_string(char **args, int total) {
  std::vector<std::string> strings;

  for (int i = 1; i < total; i++) {
    std::string s(args[i]);
    strings.push_back(s);
  }

  return strings;
}

bool is_max_complexity(std::string &s) {
  return s == "--max-complexity" or s == "-mx";
}

bool is_quiet(std::string &s) { return s == "--quiet" or s == "-q"; }

bool is_ignore_complexity(std::string &s) {
  return s == "--ignore-complexity" or s == "-i";
}

bool is_detail(std::string &s) { return s == "--detail" or s == "-d"; }

bool is_sort(std::string &s) { return s == "--sort" or s == "-s"; }

bool is_output_csv(std::string &s) {
  return s == "--output-csv" or s == "-csv";
}

bool is_output_json(std::string &s) {
  return s == "--output-json" or s == "-json";
}

static bool is_lang(std::string &s) { return s == "--lang" || s == "-l"; }

static bool is_max_fn_width(std::string &s) {
  return s == "--max-fn-width" || s == "-fw";
}

static bool is_help(std::string &s) { return s == "--help" || s == "-h"; }

static bool is_version(std::string &s) { return s == "--version"; }

static bool is_exclude(std::string &s) { return s == "--exclude" || s == "-x"; }

bool is_argument(std::string &s) {
  return is_max_complexity(s) or is_quiet(s) or is_ignore_complexity(s) or
         is_detail(s) or is_sort(s) or is_output_csv(s) or is_output_json(s) ||
         is_lang(s) || is_exclude(s) || is_max_fn_width(s) || is_help(s) ||
         is_version(s);
}

static Language language_from_token(std::string tok) {
  // normalize
  std::transform(tok.begin(), tok.end(), tok.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (tok == "py" || tok == "python") return Language::Python;
  if (tok == "js" || tok == "javascript") return Language::JavaScript;
  if (tok == "ts" || tok == "typescript") return Language::TypeScript;
  if (tok == "tsx") return Language::TypeScript;
  if (tok == "c") return Language::C;
  if (tok == "cpp" || tok == "c++" || tok == "cc" || tok == "cxx")
    return Language::Cpp;
  if (tok == "java") return Language::Java;
  return Language::Unknown;
}

static void parse_languages_list(const std::string &value,
                                 std::vector<Language> &out) {
  size_t start = 0;
  while (start <= value.size()) {
    size_t comma = value.find(',', start);
    std::string tok =
        value.substr(start, comma == std::string::npos ? std::string::npos
                                                       : (comma - start));
    // trim spaces
    auto trim = [](std::string &s) {
      size_t a = s.find_first_not_of(" \t\n");
      size_t b = s.find_last_not_of(" \t\n");
      if (a == std::string::npos) {
        s.clear();
        return;
      }
      s = s.substr(a, b - a + 1);
    };
    trim(tok);
    if (!tok.empty()) {
      Language lang = language_from_token(tok);
      if (lang == Language::Unknown)
        throw std::invalid_argument("Unknown language in --lang: '" + tok +
                                    "'");
      // Avoid duplicates
      if (std::find(out.begin(), out.end(), lang) == out.end())
        out.push_back(lang);
    }
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
}

CLI_ARGUMENTS load_from_vs_arguments(std::vector<std::string> &arguments) {
  int i;
  bool reading_paths = true;
  std::vector<std::string> paths;
  std::vector<std::string> excludes;
  int max_complexity_allowed = 15;
  bool quiet = false;
  bool ignore_complexity = false;
  DetailType detail = NORMAL;
  SortType sort = NAME;
  bool output_csv = false;
  bool output_json = false;
  std::vector<Language> langs_filter;
  int max_function_width = 0;
  bool show_help = false;
  bool show_version = false;

  for (i = 0; i < arguments.size() && reading_paths; i++) {
    if (!is_argument(arguments[i]))
      paths.push_back(arguments[i]);
    else {
      reading_paths = false;
      i--;
    }
  }

  // Allow --help/-h without requiring paths
  if (!reading_paths && paths.empty()) {
    bool any_help = false;
    for (int j = i; j < arguments.size(); ++j) {
      if (is_help(arguments[j])) {
        any_help = true;
        break;
      }
    }
    if (!any_help) throw std::invalid_argument("Expected at least one path");
  }

  for (i = i; i < arguments.size(); i++) {
    if (is_max_complexity(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument(
            "Expected max cognitive complexity allowed, use '-mx $number'");

      try {
        max_complexity_allowed = std::stoi(arguments[i]);
        if (max_complexity_allowed < 0) max_complexity_allowed = 0;
      } catch (const std::invalid_argument &e) {
        throw std::invalid_argument(
            "Expected a number as max complexity allowed");
      } catch (const std::out_of_range &e) {
        throw std::invalid_argument(
            "Expected a number as max complexity allowed");
      }
    } else if (is_quiet(arguments[i]))
      quiet = true;
    else if (is_ignore_complexity(arguments[i]))
      ignore_complexity = true;
    else if (is_help(arguments[i]))
      show_help = true;
    else if (is_lang(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument("Expected language list after --lang/-l");
      parse_languages_list(arguments[i], langs_filter);
    } else if (is_exclude(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument("Expected path list after --exclude/-x");
      size_t start = 0;
      const std::string &value = arguments[i];
      while (start <= value.size()) {
        size_t comma = value.find(',', start);
        std::string tok =
            value.substr(start, comma == std::string::npos ? std::string::npos
                                                           : (comma - start));
        auto trim = [](std::string &s) {
          size_t a = s.find_first_not_of(" \t\n");
          size_t b = s.find_last_not_of(" \t\n");
          if (a == std::string::npos) {
            s.clear();
            return;
          }
          s = s.substr(a, b - a + 1);
        };
        trim(tok);
        if (!tok.empty()) excludes.push_back(tok);
        if (comma == std::string::npos) break;
        start = comma + 1;
      }
    } else if (is_max_fn_width(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument("Expected number after --max-fn-width/-fw");
      try {
        max_function_width = std::stoi(arguments[i]);
        if (max_function_width < 0) max_function_width = 0;
      } catch (const std::invalid_argument &e) {
        throw std::invalid_argument(
            "Expected a number after --max-fn-width/-fw");
      }
    } else if (is_detail(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument(
            "Expected detail level, use '-d low' or '-d normal'");

      if (arguments[i] == "low")
        detail = LOW;
      else if (arguments[i] == "normal")
        detail = NORMAL;
      else
        throw std::invalid_argument(
            "Invalid detail level, use 'low' or 'normal'");
    } else if (is_sort(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument(
            "Expected sort order, use '-s asc' '-s desc' '-s name'");

      if (arguments[i] == "asc")
        sort = ASC;
      else if (arguments[i] == "desc")
        sort = DESC;
      else if (arguments[i] == "name")
        sort = NAME;
      else
        throw std::invalid_argument(
            "Invalid sort order, use '-s asc' '-s desc' '-s name'");
    } else if (is_output_csv(arguments[i]))
      output_csv = true;
    else if (is_output_json(arguments[i]))
      output_json = true;
    else
      throw std::invalid_argument("Invalid argument: '" + arguments[i] +
                                  "' on call, use the valid arguments");
  }

  return CLI_ARGUMENTS{paths,
                       excludes,
                       max_complexity_allowed,
                       quiet,
                       ignore_complexity,
                       detail,
                       sort,
                       output_csv,
                       output_json,
                       max_function_width,
                       show_help,
                       show_version,
                       langs_filter};
}

// Relaxed parsing that does not require at least one path and records presence
// of each option to support merging with config file values.
CLI_PARSE_RESULT parse_arguments_relaxed(std::vector<std::string> &arguments) {
  CLI_PARSE_RESULT res;
  int i;
  bool reading_paths = true;
  std::vector<std::string> paths;
  std::vector<std::string> excludes;
  int max_complexity_allowed = 15;
  bool quiet = false;
  bool ignore_complexity = false;
  DetailType detail = NORMAL;
  SortType sort = NAME;
  bool output_csv = false;
  bool output_json = false;
  std::vector<Language> langs_filter;
  int max_function_width = 0;
  bool show_help = false;
  bool show_version = false;

  for (i = 0; i < arguments.size() && reading_paths; i++) {
    if (!is_argument(arguments[i]))
      paths.push_back(arguments[i]);
    else {
      reading_paths = false;
      i--;
    }
  }
  if (!paths.empty()) res.has_paths = true;

  for (i = i; i < arguments.size(); i++) {
    if (is_max_complexity(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument(
            "Expected max cognitive complexity allowed, use '-mx $number'");
      try {
        max_complexity_allowed = std::stoi(arguments[i]);
        if (max_complexity_allowed < 0) max_complexity_allowed = 0;
        res.has_max_complexity = true;
      } catch (const std::invalid_argument &e) {
        throw std::invalid_argument(
            "Expected a number as max complexity allowed");
      } catch (const std::out_of_range &e) {
        throw std::invalid_argument(
            "Expected a number as max complexity allowed");
      }
    } else if (is_quiet(arguments[i])) {
      quiet = true;
      res.has_quiet = true;
    } else if (is_ignore_complexity(arguments[i])) {
      ignore_complexity = true;
      res.has_ignore_complexity = true;
    } else if (is_help(arguments[i])) {
      show_help = true;
      res.has_help = true;
    } else if (is_version(arguments[i])) {
      show_version = true;
      res.has_version = true;
    } else if (is_lang(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument("Expected language list after --lang/-l");
      parse_languages_list(arguments[i], langs_filter);
      res.has_lang = true;
    } else if (is_exclude(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument("Expected path list after --exclude/-x");
      size_t start = 0;
      const std::string &value = arguments[i];
      while (start <= value.size()) {
        size_t comma = value.find(',', start);
        std::string tok =
            value.substr(start, comma == std::string::npos ? std::string::npos
                                                           : (comma - start));
        auto trim = [](std::string &s) {
          size_t a = s.find_first_not_of(" \t\n");
          size_t b = s.find_last_not_of(" \t\n");
          if (a == std::string::npos) {
            s.clear();
            return;
          }
          s = s.substr(a, b - a + 1);
        };
        trim(tok);
        if (!tok.empty()) excludes.push_back(tok);
        if (comma == std::string::npos) break;
        start = comma + 1;
      }
      res.has_excludes = true;
    } else if (is_max_fn_width(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument("Expected number after --max-fn-width/-fw");
      try {
        max_function_width = std::stoi(arguments[i]);
        if (max_function_width < 0) max_function_width = 0;
        res.has_max_fn_width = true;
      } catch (const std::invalid_argument &e) {
        throw std::invalid_argument(
            "Expected a number after --max-fn-width/-fw");
      }
    } else if (is_detail(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument(
            "Expected detail level, use '-d low' or '-d normal'");
      if (arguments[i] == "low")
        detail = LOW;
      else if (arguments[i] == "normal")
        detail = NORMAL;
      else
        throw std::invalid_argument(
            "Invalid detail level, use 'low' or 'normal'");
      res.has_detail = true;
    } else if (is_sort(arguments[i])) {
      if (++i >= arguments.size())
        throw std::invalid_argument(
            "Expected sort order, use '-s asc' '-s desc' '-s name'");
      if (arguments[i] == "asc")
        sort = ASC;
      else if (arguments[i] == "desc")
        sort = DESC;
      else if (arguments[i] == "name")
        sort = NAME;
      else
        throw std::invalid_argument(
            "Invalid sort order, use '-s asc' '-s desc' '-s name'");
      res.has_sort = true;
    } else if (is_output_csv(arguments[i])) {
      output_csv = true;
      res.has_output_csv = true;
    } else if (is_output_json(arguments[i])) {
      output_json = true;
      res.has_output_json = true;
    } else {
      throw std::invalid_argument("Invalid argument: '" + arguments[i] +
                                  "' on call, use the valid arguments");
    }
  }

  res.args = CLI_ARGUMENTS{paths,
                           excludes,
                           max_complexity_allowed,
                           quiet,
                           ignore_complexity,
                           detail,
                           sort,
                           output_csv,
                           output_json,
                           max_function_width,
                           show_help,
                           show_version,
                           langs_filter};
  return res;
}
