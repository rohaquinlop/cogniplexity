#include <string>
#include <string_view>
#include <vector>

#include "../../include/builders/python_gsg_builder.h"

using std::string;
using std::string_view;

static inline string node_type(TSNode n) {
  return string(ts_node_grammar_type(n));
}

SourceLoc PythonGSGBuilder::loc_from_node(TSNode node) {
  TSPoint p = ts_node_start_point(node), q = ts_node_end_point(node);
  return SourceLoc{p.row, p.column, q.column};
}

string_view PythonGSGBuilder::slice_source(const string &source, TSNode node) {
  const uint32_t a = ts_node_start_byte(node);
  const uint32_t b = ts_node_end_byte(node);
  return string_view(source).substr(a, b - a);
}

string_view PythonGSGBuilder::get_identifier(TSNode node,
                                             const string &source) {
  TSNode name = ts_node_child_by_field_name(node, "name", 4);
  return slice_source(source, name);
}

// No-op helpers removed; we'll compute expression costs directly where needed

// Boolean operator helpers (rough, language-agnostic text match)
enum class BoolOp { And, Or, Not, Unknown };

static BoolOp from_text_get_bool_op(string_view s) {
  if (s == "and" || s == "&&") return BoolOp::And;
  if (s == "or" || s == "||") return BoolOp::Or;
  if (s == "not" || s == "!") return BoolOp::Not;
  return BoolOp::Unknown;
}

static string_view slice_src(const string &source, TSNode node) {
  const uint32_t a = ts_node_start_byte(node);
  const uint32_t b = ts_node_end_byte(node);
  return string_view(source).substr(a, b - a);
}

static BoolOp get_boolean_op_for_node(TSNode node, const string &source) {
  return from_text_get_bool_op(slice_src(source, node));
}

unsigned int PythonGSGBuilder::count_bool_operators(TSNode node,
                                                    const string &source) {
  if (ts_node_is_null(node)) return 0;
  string t = node_type(node);
  unsigned int complexity = 0;
  if (t != "boolean_operator" && t != "not_operator") return 0;

  BoolOp current_bool = get_boolean_op_for_node(node, source);

  if (t == "boolean_operator") {
    TSNode left = ts_node_child_by_field_name(node, "left", 4);
    TSNode right = ts_node_child_by_field_name(node, "right", 5);

    BoolOp left_bool = get_boolean_op_for_node(left, source);
    BoolOp right_bool = get_boolean_op_for_node(right, source);

    if (left_bool != BoolOp::Unknown && current_bool != left_bool) complexity++;
    if (right_bool != BoolOp::Unknown && current_bool != right_bool)
      complexity++;

    complexity += count_bool_operators(left, source);
    complexity += count_bool_operators(right, source);
  }
  return complexity;
}

// General expression complexity similar to complexipy::count_bool_ops
unsigned int PythonGSGBuilder::count_bool_ops_expr(TSNode node, int nesting,
                                                   const string &source) {
  if (ts_node_is_null(node)) return 0;
  string t = node_type(node);
  if (t == "boolean_operator") {
    return 1 + count_bool_operators(node, source);
  }
  if (t == "not_operator") {
    return 1;  // count negation
  }
  if (t == "conditional_expression") {
    unsigned int c = 1 + static_cast<unsigned int>(nesting);
    int m = ts_node_named_child_count(node);
    for (int i = 0; i < m; ++i) {
      c += count_bool_ops_expr(ts_node_named_child(node, i), nesting, source);
    }
    return c;
  }
  if (t == "comparison_operator") {
    unsigned int total = 0;
    int m = ts_node_named_child_count(node);
    for (int i = 0; i < m; ++i)
      total +=
          count_bool_ops_expr(ts_node_named_child(node, i), nesting, source);
    return total;
  }
  // Default: recurse into named children to find nested constructs
  unsigned int total = 0;
  int m = ts_node_named_child_count(node);
  for (int i = 0; i < m; ++i)
    total += count_bool_ops_expr(ts_node_named_child(node, i), nesting, source);
  return total;
}

std::vector<GSGNode> PythonGSGBuilder::build_functions(TSNode root,
                                                       const string &source) {
  std::vector<GSGNode> funcs;
  int n = ts_node_named_child_count(root);
  for (int i = 0; i < n; ++i) {
    TSNode child = ts_node_named_child(root, i);
    auto t = node_type(child);
    if (t == "function_definition") {
      funcs.emplace_back(build_function(child, source));
    } else if (t == "class_definition") {
      // Collect nested functions inside classes too
      TSNode body = ts_node_child_by_field_name(child, "body", 4);
      if (!ts_node_is_null(body)) {
        int m = ts_node_named_child_count(body);
        for (int j = 0; j < m; ++j) {
          TSNode member = ts_node_named_child(body, j);
          if (node_type(member) == "function_definition")
            funcs.emplace_back(build_function(member, source));
        }
      }
    }
  }
  return funcs;
}

GSGNode PythonGSGBuilder::build_function(TSNode node, const string &source) {
  GSGNode f;
  f.kind = GSGNodeKind::Function;
  f.name = std::string(get_identifier(node, source));
  f.loc = loc_from_node(node);

  TSNode body = ts_node_child_by_field_name(node, "body", 4);
  if (!ts_node_is_null(body)) {
    build_block_children(body, source, f.children, /*nesting=*/0);
  }
  return f;
}

void PythonGSGBuilder::build_block_children(TSNode block, const string &source,
                                            std::vector<GSGNode> &out,
                                            int nesting) {
  int n = ts_node_named_child_count(block);
  for (int i = 0; i < n; ++i) {
    TSNode stmt = ts_node_named_child(block, i);
    string t = node_type(stmt);
    // debug
    // std::cerr << "stmt type: " << t << "\n";
    if (t == "for_statement")
      out.emplace_back(build_for(stmt, source, nesting));
    else if (t == "while_statement")
      out.emplace_back(build_while(stmt, source, nesting));
    else if (t == "if_statement")
      out.emplace_back(build_if(stmt, source, nesting));
    else if (t == "match_statement") {
      // Traverse case bodies only (no base cost)
      int mc = ts_node_named_child_count(stmt);
      for (int k = 0; k < mc; ++k) {
        TSNode ch = ts_node_named_child(stmt, k);
        if (node_type(ch) == "case_clause") {
          TSNode cbody = ts_node_child_by_field_name(ch, "body", 4);
          if (!ts_node_is_null(cbody))
            build_block_children(cbody, source, out, nesting + 1);
        }
      }
    } else if (t == "try_statement") {
      // body
      TSNode body = ts_node_child_by_field_name(stmt, "body", 4);
      if (!ts_node_is_null(body))
        build_block_children(body, source, out, nesting + 1);
      // handlers
      int m = ts_node_named_child_count(stmt);
      for (int j = 0; j < m; ++j) {
        TSNode ch = ts_node_named_child(stmt, j);
        if (node_type(ch) == "except_clause") {
          GSGNode ex;
          ex.kind = GSGNodeKind::Except;
          ex.loc = loc_from_node(ch);
          ex.addl_cost = 1;
          TSNode exbody = ts_node_child_by_field_name(ch, "body", 4);
          if (!ts_node_is_null(exbody))
            build_block_children(exbody, source, ex.children, nesting + 1);
          out.emplace_back(std::move(ex));
        }
      }
      // else and finally
      TSNode orelse = ts_node_child_by_field_name(stmt, "alternative", 11);
      if (!ts_node_is_null(orelse))
        build_block_children(orelse, source, out, nesting + 1);
      TSNode finalizer = ts_node_child_by_field_name(stmt, "finalizer", 9);
      if (!ts_node_is_null(finalizer))
        build_block_children(finalizer, source, out, nesting + 1);
    } else if (t == "return_statement") {
      TSNode value = ts_node_named_child(stmt, 0);
      if (!ts_node_is_null(value) && node_type(value) == "expression") {
        GSGNode e;
        e.kind = GSGNodeKind::Expr;
        e.loc = loc_from_node(stmt);
        e.addl_cost = count_bool_ops_expr(value, nesting, source);
        out.emplace_back(std::move(e));
      }
    } else if (t == "raise_statement") {
      GSGNode e;
      e.kind = GSGNodeKind::Expr;
      e.loc = loc_from_node(stmt);
      unsigned int rc = 0;
      int m = ts_node_named_child_count(stmt);
      for (int j = 0; j < m; ++j) {
        TSNode ch = ts_node_named_child(stmt, j);
        rc += count_bool_ops_expr(ch, nesting, source);
      }
      e.addl_cost = rc;
      out.emplace_back(std::move(e));
    } else if (t == "assert_statement") {
      GSGNode e;
      e.kind = GSGNodeKind::Expr;
      e.loc = loc_from_node(stmt);
      unsigned int ac = 0;
      int m = ts_node_named_child_count(stmt);
      for (int j = 0; j < m; ++j) {
        TSNode ch = ts_node_named_child(stmt, j);
        ac += count_bool_ops_expr(ch, nesting, source);
      }
      e.addl_cost = ac;
      out.emplace_back(std::move(e));
    } else if (t == "with_statement") {
      GSGNode w;
      w.kind = GSGNodeKind::With;
      w.loc = loc_from_node(stmt);
      unsigned int wc = 0;
      int m = ts_node_named_child_count(stmt);
      for (int j = 0; j < m; ++j) {
        TSNode ch = ts_node_named_child(stmt, j);
        wc += count_bool_ops_expr(ch, nesting, source);
      }
      w.addl_cost = wc;
      TSNode wbody = ts_node_child_by_field_name(stmt, "body", 4);
      if (!ts_node_is_null(wbody))
        build_block_children(wbody, source, w.children, nesting + 1);
      out.emplace_back(std::move(w));
    } else if (t == "assignment") {
      TSNode right = ts_node_child_by_field_name(stmt, "right", 5);
      if (!ts_node_is_null(right)) {
        GSGNode e;
        e.kind = GSGNodeKind::Expr;
        e.loc = loc_from_node(stmt);
        e.addl_cost = count_bool_ops_expr(right, nesting, source);
        out.emplace_back(std::move(e));
      }
    } else if (t == "augmented_assignment") {
      TSNode right = ts_node_child_by_field_name(stmt, "right", 5);
      if (!ts_node_is_null(right)) {
        GSGNode e;
        e.kind = GSGNodeKind::Expr;
        e.loc = loc_from_node(stmt);
        e.addl_cost = count_bool_ops_expr(right, nesting, source);
        out.emplace_back(std::move(e));
      }
    } else if (t == "_simple_statement" || t == "expression_statement") {
      int sN = ts_node_named_child_count(stmt);
      for (int sj = 0; sj < sN; ++sj) {
        TSNode sub = ts_node_named_child(stmt, sj);
        string st = node_type(sub);
        // std::cerr << "  simple sub: " << st << "\n";
        if (st == "assignment") {
          TSNode right = ts_node_child_by_field_name(sub, "right", 5);
          unsigned int cost = 0;
          if (!ts_node_is_null(right))
            cost = count_bool_ops_expr(right, nesting, source);
          else
            cost = count_bool_ops_expr(sub, nesting, source);
          if (cost) {
            GSGNode e;
            e.kind = GSGNodeKind::Expr;
            e.loc = loc_from_node(sub);
            e.addl_cost = cost;
            out.emplace_back(std::move(e));
          }
        } else if (st == "augmented_assignment") {
          TSNode right = ts_node_child_by_field_name(sub, "right", 5);
          unsigned int cost = 0;
          if (!ts_node_is_null(right))
            cost = count_bool_ops_expr(right, nesting, source);
          else
            cost = count_bool_ops_expr(sub, nesting, source);
          if (cost) {
            GSGNode e;
            e.kind = GSGNodeKind::Expr;
            e.loc = loc_from_node(sub);
            e.addl_cost = cost;
            out.emplace_back(std::move(e));
          }
        } else if (st == "return_statement") {
          TSNode value = ts_node_named_child(sub, 0);
          unsigned int cost = 0;
          if (!ts_node_is_null(value))
            cost = count_bool_ops_expr(value, nesting, source);
          else
            cost = count_bool_ops_expr(sub, nesting, source);
          if (cost) {
            GSGNode e;
            e.kind = GSGNodeKind::Expr;
            e.loc = loc_from_node(sub);
            e.addl_cost = cost;
            out.emplace_back(std::move(e));
          }
        } else if (st == "assert_statement") {
          unsigned int ac = 0;
          int cm = ts_node_named_child_count(sub);
          for (int cj = 0; cj < cm; ++cj)
            ac += count_bool_ops_expr(ts_node_named_child(sub, cj), nesting,
                                      source);
          if (ac) {
            GSGNode e;
            e.kind = GSGNodeKind::Expr;
            e.loc = loc_from_node(sub);
            e.addl_cost = ac;
            out.emplace_back(std::move(e));
          }
        } else if (st == "raise_statement") {
          unsigned int rc = 0;
          int rm = ts_node_named_child_count(sub);
          for (int rj = 0; rj < rm; ++rj)
            rc += count_bool_ops_expr(ts_node_named_child(sub, rj), nesting,
                                      source);
          if (rc) {
            GSGNode e;
            e.kind = GSGNodeKind::Expr;
            e.loc = loc_from_node(sub);
            e.addl_cost = rc;
            out.emplace_back(std::move(e));
          }
        } else if (st == "conditional_expression") {
          // Rare: bare ternary expression as a statement – count per complexipy
          // as expression
          unsigned int cost = count_bool_ops_expr(sub, nesting, source);
          if (cost) {
            GSGNode e;
            e.kind = GSGNodeKind::Expr;
            e.loc = loc_from_node(sub);
            e.addl_cost = cost;
            out.emplace_back(std::move(e));
          }
        }
      }
    } else if (t == "function_definition")
      out.emplace_back(build_function(stmt, source));
    else {
      // ignore
    }
  }
}

GSGNode PythonGSGBuilder::build_for(TSNode node, const string &source,
                                    int nesting) {
  (void)source;
  GSGNode g;
  g.kind = GSGNodeKind::For;
  g.loc = loc_from_node(node);
  TSNode body = ts_node_child_by_field_name(node, "body", 4);
  if (!ts_node_is_null(body))
    build_block_children(body, source, g.children, /*nesting=*/nesting + 1);
  return g;
}

GSGNode PythonGSGBuilder::build_while(TSNode node, const string &source,
                                      int nesting) {
  GSGNode g;
  g.kind = GSGNodeKind::While;
  g.loc = loc_from_node(node);

  TSNode cond = ts_node_child_by_field_name(node, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += count_bool_ops_expr(cond, /*nesting=*/nesting, source);

  TSNode body = ts_node_child_by_field_name(node, "body", 4);
  if (!ts_node_is_null(body))
    build_block_children(body, source, g.children, /*nesting=*/nesting + 1);
  return g;
}

GSGNode PythonGSGBuilder::build_if(TSNode node, const string &source,
                                   int nesting) {
  GSGNode g;
  g.kind = GSGNodeKind::If;
  g.loc = loc_from_node(node);

  TSNode cond = ts_node_child_by_field_name(node, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += count_bool_ops_expr(cond, /*nesting=*/nesting, source);

  TSNode cons = ts_node_child_by_field_name(node, "consequence", 11);
  if (!ts_node_is_null(cons))
    build_block_children(cons, source, g.children, /*nesting=*/nesting + 1);

  // Alternatives: multiple elif_clause and optionally else_clause
  int n = ts_node_named_child_count(node);
  for (int i = 0; i < n; ++i) {
    TSNode ch = ts_node_named_child(node, i);
    string t = node_type(ch);
    if (t == "elif_clause") {
      // Represent elif as ElseIf with its own body
      GSGNode eif;
      eif.kind = GSGNodeKind::ElseIf;
      eif.loc = loc_from_node(ch);
      TSNode econd = ts_node_child_by_field_name(ch, "condition", 9);
      if (!ts_node_is_null(econd))
        eif.addl_cost +=
            count_bool_ops_expr(econd, /*nesting=*/nesting, source);
      TSNode ebody = ts_node_child_by_field_name(ch, "consequence", 11);
      if (!ts_node_is_null(ebody))
        build_block_children(ebody, source, eif.children,
                             /*nesting=*/nesting + 1);
      g.children.emplace_back(std::move(eif));
    } else if (t == "else_clause") {
      GSGNode el;
      el.kind = GSGNodeKind::Else;
      el.loc = loc_from_node(ch);
      TSNode ebody = ts_node_child_by_field_name(ch, "body", 4);
      if (!ts_node_is_null(ebody))
        build_block_children(ebody, source, el.children,
                             /*nesting=*/nesting + 1);
      g.children.emplace_back(std::move(el));
    }
  }
  return g;
}
