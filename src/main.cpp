#include <iostream>
#include <stdexcept>
#include <vector>

#include "../include/cli_arguments.h"

std::vector<std::string> args_to_string(char** args, int total) {
  std::vector<std::string> strings;

  for (int i = 1; i < total; i++) {
    std::string s(args[i]);
    strings.push_back(s);
  }

  return strings;
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

  for (std::string& s : cli_args.paths) std::cout << s << " ";
  std::cout << std::endl;
  return 0;
}
