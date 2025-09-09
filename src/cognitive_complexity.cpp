#include "../include/builders/c_gsg_builder.h"
#include "../include/builders/javascript_gsg_builder.h"
#include "../include/builders/python_gsg_builder.h"
#include "../include/cognitive_complexity.h"
#include "../include/gsg.h"

static inline LineComplexity build_line_complexity_from_loc(
    const SourceLoc &loc, unsigned int c) {
  return LineComplexity{loc.row, loc.start_col, loc.end_col, c};
}

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
      // Strict decorator-factory pattern: exactly two statements, a nested
      // function followed by a return.
      if (node.children.size() == 2 &&
          node.children[0].kind == GSGNodeKind::Function &&
          node.children[1].kind == GSGNodeKind::Expr &&
          node.children[1].addl_cost == 0) {
        const auto &inner = node.children[0];
        for (const auto &ich : inner.children) {
          int next_nest = (ich.kind == GSGNodeKind::Function)
                              ? nesting_level + 1
                              : nesting_level;
          auto [c2, l2] = compute_cognitive_complexity_gsg(ich, next_nest);
          complexity += c2;
          if (!l2.empty()) lines.insert(lines.end(), l2.begin(), l2.end());
        }
        break;
      }
      for (const auto &ch : node.children) {
        int next_nest = (ch.kind == GSGNodeKind::Function) ? nesting_level + 1
                                                           : nesting_level;
        auto [c, l] = compute_cognitive_complexity_gsg(ch, next_nest);
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
    case GSGNodeKind::Switch: {
      count_children(nesting_level);
      break;
    }
    case GSGNodeKind::Case: {
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::Else: {
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::Try: {
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::Finally: {
      count_children(nesting_level + 1);
      break;
    }
    case GSGNodeKind::With:
    case GSGNodeKind::Except:
    case GSGNodeKind::Expr:
    case GSGNodeKind::Ternary: {
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
  for (const auto &fn : func_nodes) {
    auto [c, lines] = compute_cognitive_complexity_gsg(fn, 0);
    functions.push_back(FunctionComplexity{.name = fn.name,
                                           .complexity = c,
                                           .row = fn.loc.row,
                                           .start_col = fn.loc.start_col,
                                           .end_col = fn.loc.end_col,
                                           .lines = lines});
  }

  ts_tree_delete(tree);
  return functions;
}
