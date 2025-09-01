#include <cstdint>
#include <string_view>

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
          .lines = lines_complexity,
      });
    }
  }

  ts_tree_delete(tree);
  return functions;
}

std::pair<unsigned int, std::vector<LineComplexity>>
compute_cognitive_complexity(TSNode current_node, int nesting_level) {
  unsigned int complexity = 0, stmt_complexity;
  std::vector<LineComplexity> lines_complexity;

  if (is_decorator(current_node))
    return compute_cognitive_complexity(ts_node_named_child(current_node, 0),
                                        nesting_level);
  std::string node_type = ts_node_grammar_type(current_node), child_type;
  TSNode child_node;
  TSPoint p, q;

  if (node_type == "class_definition") {
    auto [body, total_children] = get_body(current_node);

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
    auto [body, total_children] = get_body(current_node);

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
  } else if (node_type == "for_statement" or node_type == "while_statement") {
    stmt_complexity = 1 + nesting_level;
    complexity += stmt_complexity;
    lines_complexity.push_back(
        build_line_complexity(current_node, stmt_complexity));

    auto [body, total_children] = get_body(current_node);

    // TODO: check while condition

    for (int i = 0; i < total_children; i++) {
      child_node = ts_node_named_child(body, i);
      auto [child_complexity, child_lines_complexity] =
          compute_cognitive_complexity(child_node, nesting_level + 1);

      complexity += child_complexity;
      if (!child_lines_complexity.empty())
        lines_complexity.insert(lines_complexity.end(),
                                child_lines_complexity.begin(),
                                child_lines_complexity.end());
    }
  } else if (node_type == "if_statement") {
    stmt_complexity = 1 + nesting_level;
    complexity += stmt_complexity;
    lines_complexity.push_back(
        build_line_complexity(current_node, stmt_complexity));
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

static LineComplexity build_line_complexity(TSNode node,
                                            unsigned int complexity) {
  TSPoint p = ts_node_start_point(node), q = ts_node_end_point(node);

  return LineComplexity{
      .row = p.row,
      .start_col = p.column,
      .end_col = q.column,
      .complexity = complexity,
  };
}

static std::pair<TSNode, int> get_body(TSNode node) {
  TSNode body = ts_node_child_by_field_name(node, "body", 4);
  int total_children = ts_node_child_count(body);

  return {body, total_children};
}

static unsigned int count_bool_operators(TSNode node,
                                         const std::string& source_code) {
  std::string node_type = ts_node_grammar_type(node);
  unsigned int complexity = 0;

  if (node_type != "boolean_operator" and node_type != "not_operator") return 0;

  BoolOp current_bool = get_boolean_op(node, source_code);

  if (node_type == "boolean_operator") {
    TSNode left = ts_node_child_by_field_name(node, "left", 4),
           right = ts_node_child_by_field_name(node, "right", 5);

    BoolOp left_bool = get_boolean_op(left, source_code),
           right_bool = get_boolean_op(right, source_code);

    if (left_bool != BoolOp::Unknown and current_bool != left_bool)
      complexity++;
    if (right_bool != BoolOp::Unknown and current_bool != right_bool)
      complexity++;

    complexity += count_bool_operators(left, source_code);
    complexity += count_bool_operators(right, source_code);
  } else if (node_type == "not_operator") {
    //
  }

  return complexity;
}

static BoolOp get_boolean_op(TSNode node, const std::string& source_code) {
  std::string_view s = slice_source(source_code, node);
  return from_text_get_bool_op(s);
}

static BoolOp from_text_get_bool_op(std::string_view& s) {
  if (s == "and" or s == "&&") return BoolOp::And;
  if (s == "or" or s == "||") return BoolOp::Or;
  if (s == "not" or s == "!") return BoolOp::Not;

  return BoolOp::Unknown;
}

static TSNode unwrap_parens(TSNode node) {
  TSNode inner;
  while (!ts_node_is_null(node) and
         std::string_view{ts_node_type(node)} == "parenthesized_expression") {
    inner = ts_node_child_by_field_name(node, "expression", 10);
    if (ts_node_is_null(inner)) {
      if (!ts_node_named_child_count(node)) break;
      inner = ts_node_named_child(node, 0);
    }
    node = inner;
  }

  return node;
}
