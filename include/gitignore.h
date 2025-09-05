#ifndef GITIGNORE_H
#define GITIGNORE_H

#include <filesystem>
#include <string>
#include <vector>

namespace ignore {

struct Rule {
  std::string pattern;   // normalized pattern without leading '!'
  bool negated = false;  // true if pattern began with '!'
  bool dir_only = false; // true if pattern ended with '/'
  bool has_slash = false; // true if pattern contains '/'
  bool anchored = false; // true if pattern began with '/'
};

struct RulesFile {
  std::filesystem::path base;  // directory containing this .gitignore
  std::vector<Rule> rules;      // in order
};

// Load rules from <dir>/.gitignore if present. Returns empty rules if none.
RulesFile load_rules_for_dir(const std::filesystem::path& dir);

// Determine if path is ignored by the cumulative rules on the stack.
// The stack must be ordered from higher-level directory to the current one.
bool is_ignored(const std::vector<RulesFile>& stack,
                const std::filesystem::path& abs_path,
                bool is_dir);

}  // namespace ignore

#endif

