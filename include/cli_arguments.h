#ifndef CLI_ARGUMENTS_H
#define CLI_ARGUMENTS_H

#include <string>
#include <vector>

enum DetailType { LOW, NORMAL };

enum SortType { ASC, DESC, NAME };

struct CLI_ARGUMENTS {
  std::vector<std::string> paths;
  int max_complexity_allowed = 15;  // --max-complexity-allowed -mx
  bool quiet = false;               // --quiet -q
  bool ignore_complexity = false;   // --ignore-complexity -i
  DetailType detail = NORMAL;       // --detail -d
  SortType sort = NAME;             // --sort -s
  bool output_csv = false;          // --output-csv -csv
  bool output_json = false;         // --output-json -json
};

std::vector<std::string> args_to_string(char**, int);
CLI_ARGUMENTS load_from_vs_arguments(std::vector<std::string>&);

#endif
