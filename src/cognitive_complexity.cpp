#include <cstdint>

#include "../include/cognitive_complexity.h"

std::vector<FunctionComplexity> functions_complexity_file(
    const std::string& source_code, TSParser* parser) {
  int i;
  TSTree* tree = ts_parser_parse_string(parser, NULL, source_code.c_str(),
                                        strlen(source_code.c_str()));

  TSNode root_node = ts_tree_root_node(tree), child_node;
  int total_children = ts_node_child_count(root_node);
  std::string child_type;
  std::vector<FunctionComplexity> functions;
  TSPoint p, q;

  for (i = 0; i < total_children; i++) {
    child_node = ts_node_named_child(root_node, i);
    child_type = ts_node_grammar_type(child_node);

    if (child_type == "function_definition") {
      auto [complexity, lines_complexity] =
          compute_cognitive_complexity(child_node, 0);

      p = ts_node_start_point(child_node);
      q = ts_node_end_point(child_node);

      functions.push_back(FunctionComplexity{
          .name = get_function_name(child_node, source_code),
          .complexity = complexity,
          .row = p.row,
          .start_col = p.column,
          .end_col = q.column,
      });
    }
  }

  ts_tree_delete(tree);
  return functions;
}

std::pair<unsigned int, std::vector<LineComplexity>>
compute_cognitive_complexity(TSNode current_node, int nesting_level) {
  unsigned int complexity = 0;
  std::vector<LineComplexity> lines_complexity;

  if (is_decorator(current_node))
    return compute_cognitive_complexity(ts_node_named_child(current_node, 0),
                                        nesting_level);
  std::string node_type = ts_node_grammar_type(current_node), child_type;
  TSNode child_node;
  int total_children = ts_node_child_count(current_node);
  TSPoint p, q;

  if (node_type == "class_definition") {
    TSNode body = ts_node_child_by_field_name(current_node, "body", 4);
    total_children = ts_node_child_count(body);

    for (int i = 0; i < total_children; i++) {
      child_node = ts_node_named_child(body, i);
      child_type = ts_node_grammar_type(child_node);

      if (child_type == "function_definition") {
        auto [child_complexity, child_lines_complexity] =
            compute_cognitive_complexity(child_node, nesting_level);

        complexity += child_complexity;
        if (!child_lines_complexity.empty())
          lines_complexity.insert(lines_complexity.end(),
                                  child_lines_complexity.begin(),
                                  child_lines_complexity.end());
      }
    }

  } else if (node_type == "function_definition") {
    TSNode body = ts_node_child_by_field_name(current_node, "body", 4);
    total_children = ts_node_child_count(body);

    for (int i = 0; i < total_children; i++) {
      child_node = ts_node_named_child(body, i);
      child_type = ts_node_grammar_type(child_node);

      if (child_type == "function_definition") nesting_level++;

      auto [child_complexity, child_lines_complexity] =
          compute_cognitive_complexity(child_node, nesting_level);

      complexity += child_complexity;
      if (!child_lines_complexity.empty())
        lines_complexity.insert(lines_complexity.end(),
                                child_lines_complexity.begin(),
                                child_lines_complexity.end());

      if (child_type == "function_definition") nesting_level--;
    }
  } else if (node_type == "for_statement") {
    unsigned int stmt_complexity = 1 + nesting_level;
    p = ts_node_start_point(child_node);
    q = ts_node_end_point(child_node);
    lines_complexity.push_back(LineComplexity{
        .row = p.row,
        .start_col = p.column,
        .end_col = q.column,
    });
  }

  return {complexity, lines_complexity};
}

static bool is_decorator(TSNode& current_node) {
  std::string node_type = ts_node_grammar_type(current_node);

  if (node_type != "function_definition" or
      ts_node_child_count(current_node) != 2)
    return false;

  TSNode frst_child = ts_node_named_child(current_node, 0),
         scnd_child = ts_node_named_child(current_node, 1);

  std::string frst_child_type = ts_node_grammar_type(frst_child),
              scnd_child_type = ts_node_grammar_type(scnd_child);
  return frst_child_type == "function_definition" and
         scnd_child_type == "return_statement";
}

static std::string_view get_function_name(TSNode node,
                                          const std::string& source_code) {
  TSNode name = ts_node_child_by_field_name(node, "name", 4);
  return slice_source(source_code, name);
}

static std::string_view slice_source(const std::string& source_code,
                                     TSNode node) {
  const uint32_t a = ts_node_start_byte(node);
  const uint32_t b = ts_node_end_byte(node);
  return std::string_view(source_code).substr(a, b - a);
}
