#include <iostream>
#include <stdexcept>
#include <vector>

#include "../include/cli_arguments.h"
#include "../tree-sitter/lib/include/tree_sitter/api.h"

extern "C" {
const TSLanguage* tree_sitter_python();
}

void parse_python() {
  TSParser* parser = ts_parser_new();

  ts_parser_set_language(parser, tree_sitter_python());

  const char* source_code = "[1, 2, 3, 4]";
  TSTree* tree =
      ts_parser_parse_string(parser, NULL, source_code, strlen(source_code));

  TSNode root_node = ts_tree_root_node(tree);
  TSNode array_node = ts_node_named_child(root_node, 0);
  TSNode number_node = ts_node_named_child(array_node, 0);

  std::cout << ts_node_grammar_type(root_node) << std::endl;
  std::cout << ts_node_grammar_type(array_node) << std::endl;
  std::cout << ts_node_grammar_type(number_node) << std::endl;

  ts_tree_delete(tree);
  ts_parser_delete(parser);
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

  parse_python();

  return 0;
}
