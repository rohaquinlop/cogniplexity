#include <string>
#include <string_view>
#include <vector>

#include "../../include/builders/javascript_gsg_builder.h"

using std::string;
using std::string_view;

static inline string t(TSNode n) { return string(ts_node_grammar_type(n)); }

SourceLoc JavaScriptGSGBuilder::loc(TSNode n) {
  TSPoint p = ts_node_start_point(n), q = ts_node_end_point(n);
  return SourceLoc{p.row, p.column, q.column};
}

string_view JavaScriptGSGBuilder::slice(const string &src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

string_view JavaScriptGSGBuilder::name_of(TSNode n, const string &src) {
  TSNode name = ts_node_child_by_field_name(n, "name", 4);
  if (!ts_node_is_null(name)) return slice(src, name);
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i) {
    TSNode ch = ts_node_named_child(n, i);
    if (t(ch) == "identifier" || t(ch) == "property_identifier")
      return slice(src, ch);
  }
  return string_view{};
}

enum class JSBoolOp { And, Or, Not, Unknown };

static string_view js_slice(const string &src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

static JSBoolOp js_op_from_text(string_view s) {
  if (s == "&&") return JSBoolOp::And;
  if (s == "||") return JSBoolOp::Or;
  if (s == "!") return JSBoolOp::Not;
  return JSBoolOp::Unknown;
}

static TSNode js_unwrap_parens(TSNode n) {
  while (!ts_node_is_null(n) &&
         string_view{ts_node_type(n)} == "parenthesized_expression") {
    TSNode inner = ts_node_child_by_field_name(n, "expression", 10);
    if (ts_node_is_null(inner)) break;
    n = inner;
  }
  return n;
}

static JSBoolOp js_get_bool_op(TSNode n, const string &src) {
  n = js_unwrap_parens(n);
  string ty = t(n);
  if (ty == "binary_expression") {
    auto s = js_slice(src, n);
    if (s.find("&&") != string::npos) return JSBoolOp::And;
    if (s.find("||") != string::npos) return JSBoolOp::Or;
  } else if (ty == "unary_expression") {
    auto s = js_slice(src, n);
    if (!s.empty() && s[0] == '!') return JSBoolOp::Not;
  }
  return JSBoolOp::Unknown;
}

static unsigned int js_count_bool_alternations(TSNode n, const string &src) {
  n = js_unwrap_parens(n);
  string ty = t(n);
  unsigned int c = 0;
  if (ty == "binary_expression") {
    TSNode left = ts_node_child_by_field_name(n, "left", 4);
    TSNode right = ts_node_child_by_field_name(n, "right", 5);
    JSBoolOp curr = js_get_bool_op(n, src);
    JSBoolOp lb = js_get_bool_op(left, src);
    JSBoolOp rb = js_get_bool_op(right, src);
    if (lb != JSBoolOp::Unknown && curr != lb) c++;
    if (rb != JSBoolOp::Unknown && curr != rb) c++;
    c += js_count_bool_alternations(left, src);
    c += js_count_bool_alternations(right, src);
  }
  return c;
}

static bool js_has_logical_op(TSNode n, const string &src) {
  auto s = js_slice(src, n);
  return s.find("&&") != string::npos || s.find("||") != string::npos ||
         s.find("!") != string::npos;
}

unsigned int JavaScriptGSGBuilder::js_count_bool_ops_expr(TSNode n, int nesting,
                                                          const string &src) {
  if (ts_node_is_null(n)) return 0;
  string ty = t(n);
  if (ty == "binary_expression") {
    unsigned int base = js_has_logical_op(n, src) ? 1 : 0;
    unsigned int alts = js_count_bool_alternations(n, src);
    TSNode left = ts_node_child_by_field_name(n, "left", 4);
    TSNode right = ts_node_child_by_field_name(n, "right", 5);
    return base + alts + js_count_bool_ops_expr(left, nesting, src) +
           js_count_bool_ops_expr(right, nesting, src);
  }
  if (ty == "unary_expression") {
    auto s = js_slice(src, n);
    if (!s.empty() && s[0] == '!') return 1;
  }
  if (ty == "conditional_expression") {
    unsigned int c = 1 + static_cast<unsigned int>(nesting);
    int m = ts_node_named_child_count(n);
    for (int i = 0; i < m; ++i)
      c += js_count_bool_ops_expr(ts_node_named_child(n, i), nesting, src);
    return c;
  }
  unsigned int total = 0;
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i)
    total += js_count_bool_ops_expr(ts_node_named_child(n, i), nesting, src);
  return total;
}

std::vector<GSGNode> JavaScriptGSGBuilder::build_functions(TSNode root,
                                                           const string &src) {
  std::vector<GSGNode> funcs;
  int n = ts_node_named_child_count(root);
  for (int i = 0; i < n; ++i) {
    TSNode ch = ts_node_named_child(root, i);
    string ty = t(ch);
    if (ty == "function_declaration") {
      funcs.emplace_back(build_function(ch, src));
    } else if (ty == "class_declaration") {
      TSNode body = ts_node_child_by_field_name(ch, "body", 4);
      int m = ts_node_named_child_count(body);
      for (int j = 0; j < m; ++j) {
        TSNode mem = ts_node_named_child(body, j);
        if (t(mem) == "method_definition") {
          funcs.emplace_back(build_function(mem, src));
        }
      }
    }
  }
  return funcs;
}

GSGNode JavaScriptGSGBuilder::build_function(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::Function;
  g.name = string(name_of(n, src));
  g.loc = loc(n);

  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children, 0);
  return g;
}

void JavaScriptGSGBuilder::build_block_children(TSNode n, const string &src,
                                                std::vector<GSGNode> &out,
                                                int nesting) {
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i) {
    TSNode s = ts_node_named_child(n, i);
    string ty = t(s);
    if (ty == "if_statement")
      out.emplace_back(build_if(s, src));
    else if (ty == "while_statement")
      out.emplace_back(build_while(s, src));
    else if (ty == "for_statement")
      out.emplace_back(build_for(s, src));
    else if (ty == "do_statement")
      out.emplace_back(build_do_while(s, src));
    else if (ty == "function_declaration" || ty == "method_definition")
      out.emplace_back(build_function(s, src));
    else if (ty == "switch_statement") {
      GSGNode sw;
      sw.kind = GSGNodeKind::Switch;
      sw.loc = loc(s);
      TSNode body = ts_node_child_by_field_name(s, "body", 4);
      if (!ts_node_is_null(body)) {
        int kmax = ts_node_named_child_count(body);
        for (int k = 0; k < kmax; ++k) {
          TSNode cc = ts_node_named_child(body, k);
          string cty = t(cc);
          if (cty == "switch_case" || cty == "switch_default") {
            GSGNode cs;
            cs.kind = GSGNodeKind::Case;
            cs.loc = loc(cc);
            TSNode cbody = ts_node_child_by_field_name(cc, "consequent", 10);
            if (!ts_node_is_null(cbody)) {
              string cbty = t(cbody);
              if (cbty == "return_statement") {
                TSNode arg = ts_node_child_by_field_name(cbody, "argument", 8);
                if (ts_node_is_null(arg) &&
                    ts_node_named_child_count(cbody) > 0)
                  arg = ts_node_named_child(cbody, 0);
                if (!ts_node_is_null(arg)) {
                  unsigned int cost =
                      js_count_bool_ops_expr(arg, nesting + 1, src);
                  if (cost) {
                    GSGNode e;
                    e.kind = GSGNodeKind::Expr;
                    e.loc = loc(cbody);
                    e.addl_cost = cost;
                    cs.children.emplace_back(std::move(e));
                  }
                }
              } else if (cbty == "expression_statement") {
                if (ts_node_named_child_count(cbody) > 0) {
                  TSNode expr = ts_node_named_child(cbody, 0);
                  unsigned int cost =
                      js_count_bool_ops_expr(expr, nesting + 1, src);
                  if (cost) {
                    GSGNode e;
                    e.kind = GSGNodeKind::Expr;
                    e.loc = loc(expr);
                    e.addl_cost = cost;
                    cs.children.emplace_back(std::move(e));
                  }
                }
              } else {
                build_block_children(cbody, src, cs.children, nesting + 1);
              }
            }
            sw.children.emplace_back(std::move(cs));
          }
        }
      }
      out.emplace_back(std::move(sw));
    } else if (ty == "expression_statement") {
      if (ts_node_named_child_count(s) > 0) {
        TSNode expr = ts_node_named_child(s, 0);
        unsigned int cost = js_count_bool_ops_expr(expr, nesting, src);
        if (cost) {
          GSGNode e;
          e.kind = GSGNodeKind::Expr;
          e.loc = loc(expr);
          e.addl_cost = cost;
          out.emplace_back(std::move(e));
        }
      }
    } else if (ty == "return_statement") {
      TSNode arg = ts_node_child_by_field_name(s, "argument", 8);
      if (ts_node_is_null(arg) && ts_node_named_child_count(s) > 0)
        arg = ts_node_named_child(s, 0);
      if (!ts_node_is_null(arg)) {
        unsigned int cost = js_count_bool_ops_expr(arg, nesting, src);
        if (cost) {
          GSGNode e;
          e.kind = GSGNodeKind::Expr;
          e.loc = loc(s);
          e.addl_cost = cost;
          out.emplace_back(std::move(e));
        }
      }
    } else if (ty == "throw_statement") {
      TSNode arg = ts_node_child_by_field_name(s, "argument", 8);
      if (!ts_node_is_null(arg)) {
        unsigned int cost = js_count_bool_ops_expr(arg, nesting, src);
        if (cost) {
          GSGNode e;
          e.kind = GSGNodeKind::Expr;
          e.loc = loc(s);
          e.addl_cost = cost;
          out.emplace_back(std::move(e));
        }
      }
    } else if (ty == "lexical_declaration" || ty == "variable_declaration") {
      int dn = ts_node_named_child_count(s);
      unsigned int sum = 0;
      for (int di = 0; di < dn; ++di) {
        TSNode d = ts_node_named_child(s, di);
        sum += js_count_bool_ops_expr(d, nesting, src);
      }
      if (sum) {
        GSGNode e;
        e.kind = GSGNodeKind::Expr;
        e.loc = loc(s);
        e.addl_cost = sum;
        out.emplace_back(std::move(e));
      }
    }
  }
}

GSGNode JavaScriptGSGBuilder::build_if(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::If;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += js_count_bool_ops_expr(cond, 0, src);
  TSNode cons = ts_node_child_by_field_name(n, "consequence", 11);
  if (!ts_node_is_null(cons)) build_block_children(cons, src, g.children, 1);
  TSNode alt = ts_node_child_by_field_name(n, "alternative", 11);
  if (!ts_node_is_null(alt)) {
    string ty = t(alt);
    if (ty == "if_statement") {
      auto eif = build_if(alt, src);
      eif.kind = GSGNodeKind::ElseIf;
      g.children.emplace_back(std::move(eif));
    } else {
      int an = ts_node_named_child_count(alt);
      if (an == 1) {
        TSNode only = ts_node_named_child(alt, 0);
        if (!ts_node_is_null(only) && t(only) == "if_statement") {
          auto eif = build_if(only, src);
          eif.kind = GSGNodeKind::ElseIf;
          g.children.emplace_back(std::move(eif));
          return g;
        }
      }
      GSGNode el;
      el.kind = GSGNodeKind::Else;
      el.loc = loc(alt);
      build_block_children(alt, src, el.children, 1);
      g.children.emplace_back(std::move(el));
    }
  }
  return g;
}

GSGNode JavaScriptGSGBuilder::build_while(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::While;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += js_count_bool_ops_expr(cond, 0, src);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children, 1);
  return g;
}

GSGNode JavaScriptGSGBuilder::build_for(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::For;
  g.loc = loc(n);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children, 1);
  return g;
}

GSGNode JavaScriptGSGBuilder::build_do_while(TSNode n, const string &src) {
  (void)src;
  GSGNode g;
  g.kind = GSGNodeKind::DoWhile;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += js_count_bool_alternations(cond, src);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children, 1);
  return g;
}
