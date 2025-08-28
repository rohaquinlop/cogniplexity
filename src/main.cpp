#include <iostream>
#include <stdexcept>
#include <vector>

#include "../include/cli_arguments.h"

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

  return 0;
}
