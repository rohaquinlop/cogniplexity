#include <functional>

#include "../include/builders/c_gsg_builder.h"
#include "../include/builders/javascript_gsg_builder.h"
#include "../include/builders/python_gsg_builder.h"
#include "../include/cognitive_complexity.h"
#include "../include/gsg.h"

static inline LineComplexity build_line_complexity_from_loc(
    const SourceLoc &loc, unsigned int c) {
  return LineComplexity{loc.row, loc.start_col, loc.end_col, c};
}

// Compute cognitive complexity from a GSG subtree
std::pair<unsigned int, std::vector<LineComplexity>>
compute_cognitive_complexity_gsg(const GSGNode &node, int nesting_level) {
  unsigned int complexity = 0;
  std::vector<LineComplexity> lines;

  auto count_children = [&](int next_nesting) {
    for (const auto &ch : node.children) {
      auto [c, l] = compute_cognitive_complexity_gsg(ch, next_nesting);
      complexity += c;
      if (!l.empty()) lines.insert(lines.end(), l.begin(), l.end());
    }
  };

  switch (node.kind) {
    case GSGNodeKind::Function: {
      // Compute body; do NOT add nested function complexities to parent
      for (const auto &ch : node.children) {
        if (ch.kind == GSGNodeKind::Function) continue;
        auto [c, l] = compute_cognitive_complexity_gsg(ch, nesting_level);
        complexity += c;
        if (!l.empty()) lines.insert(lines.end(), l.begin(), l.end());
      }
      break;
    }
    case GSGNodeKind::For:
    case GSGNodeKind::While:
    case GSGNodeKind::DoWhile: {
      unsigned int stmt = 1 + nesting_level + node.addl_cost;
      complexity += stmt;
      lines.push_back(build_line_complexity_from_loc(node.loc, stmt));
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::If: {
      unsigned int stmt = 1 + nesting_level + node.addl_cost;
      complexity += stmt;
      lines.push_back(build_line_complexity_from_loc(node.loc, stmt));
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::ElseIf: {
      unsigned int stmt = node.addl_cost;  // only bool ops for elif
      complexity += stmt;
      lines.push_back(build_line_complexity_from_loc(node.loc, stmt));
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::Switch:
    case GSGNodeKind::Case:
    case GSGNodeKind::Ternary: {
      unsigned int stmt = 1 + nesting_level + node.addl_cost;
      complexity += stmt;
      lines.push_back(build_line_complexity_from_loc(node.loc, stmt));
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::Else: {
      // else body contributes only via nested constructs
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::With:
    case GSGNodeKind::Except:
    case GSGNodeKind::Expr: {
      if (node.addl_cost) {
        complexity += node.addl_cost;
        lines.push_back(
            build_line_complexity_from_loc(node.loc, node.addl_cost));
      }
      count_children(nesting_level + 1);
      break;
    }
    default: {
      // Other nodes just traverse
      count_children(nesting_level);
      break;
    }
  }

  return {complexity, lines};
}

// Builder factory
std::unique_ptr<IBuilder> make_builder(Language lang) {
  switch (lang) {
    case Language::Python:
      return std::make_unique<PythonGSGBuilder>();
    case Language::JavaScript:
      return std::make_unique<JavaScriptGSGBuilder>();
    case Language::TypeScript:
      // Reuse JS builder for TS
      return std::make_unique<JavaScriptGSGBuilder>();
    case Language::C:
    case Language::Cpp:
      return std::make_unique<CLikeGSGBuilder>();
    default:
      return nullptr;
  }
}

// Entry point: produce function complexities for a given language
std::vector<FunctionComplexity> functions_complexity_file(
    const std::string &source_code, TSParser *parser, Language lang) {
  std::vector<FunctionComplexity> functions;

  TSTree *tree = ts_parser_parse_string(parser, NULL, source_code.c_str(),
                                        strlen(source_code.c_str()));
  TSNode root_node = ts_tree_root_node(tree);

  auto builder = make_builder(lang);
  if (!builder) {
    ts_tree_delete(tree);
    return functions;
  }

  auto func_nodes = builder->build_functions(root_node, source_code);
  // helper to collect function complexities recursively, applying nesting to
  // nested functions
  std::function<void(const GSGNode &, int)> collect;
  collect = [&](const GSGNode &fn, int nesting) {
    auto [c, lines] = compute_cognitive_complexity_gsg(fn, nesting);
    functions.push_back(FunctionComplexity{.name = fn.name,
                                           .complexity = c,
                                           .row = fn.loc.row,
                                           .start_col = fn.loc.start_col,
                                           .end_col = fn.loc.end_col,
                                           .lines = lines});
    for (const auto &ch : fn.children)
      if (ch.kind == GSGNodeKind::Function) collect(ch, nesting + 1);
  };
  for (const auto &fn : func_nodes) collect(fn, 0);

  ts_tree_delete(tree);
  return functions;
}
