#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "../../include/builders/c_gsg_builder.h"

using std::string;
using std::string_view;

static inline string t(TSNode n) { return string(ts_node_grammar_type(n)); }

static string compute_ancestor_qual(TSNode n, const string &src) {
  std::vector<string> parts;
  TSNode cur = ts_node_parent(n);
  while (!ts_node_is_null(cur)) {
    string ty = t(cur);
    if (ty == "class_specifier" || ty == "struct_specifier" ||
        ty == "union_specifier") {
      TSNode nm = ts_node_child_by_field_name(cur, "name", 4);
      if (!ts_node_is_null(nm))
        parts.push_back(string(string_view(src).substr(
            ts_node_start_byte(nm),
            ts_node_end_byte(nm) - ts_node_start_byte(nm))));
    } else if (ty == "namespace_definition") {
      TSNode nm = ts_node_child_by_field_name(cur, "name", 4);
      if (!ts_node_is_null(nm))
        parts.push_back(string(string_view(src).substr(
            ts_node_start_byte(nm),
            ts_node_end_byte(nm) - ts_node_start_byte(nm))));
    }
    cur = ts_node_parent(cur);
  }
  if (parts.empty()) return string{};
  std::reverse(parts.begin(), parts.end());
  string out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += "::";
    out += parts[i];
  }
  return out;
}

SourceLoc CLikeGSGBuilder::loc(TSNode n) {
  TSPoint p = ts_node_start_point(n), q = ts_node_end_point(n);
  return SourceLoc{p.row, p.column, q.column};
}

string_view CLikeGSGBuilder::slice(const string &src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

std::vector<GSGNode> CLikeGSGBuilder::build_functions(TSNode root,
                                                      const string &src) {
  std::vector<GSGNode> funcs;
  collect_functions_in_scope(root, src, /*qual*/ "", funcs);
  return funcs;
}

void CLikeGSGBuilder::collect_functions_in_scope(TSNode n, const string &src,
                                                 const string &qual,
                                                 std::vector<GSGNode> &out) {
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i) {
    TSNode ch = ts_node_named_child(n, i);
    string ty = t(ch);
    // debug: std::cerr << "[CLike] node type: " << ty << "\n";
    if (ty == "function_definition") {
      string aq = compute_ancestor_qual(ch, src);
      string merged;
      if (qual.empty())
        merged = aq;
      else if (aq.empty())
        merged = qual;
      else if (aq == qual)
        merged = qual;
      else if (aq.rfind(qual + "::", 0) == 0)
        merged = aq;
      else if (qual.rfind(aq + "::", 0) == 0)
        merged = qual;
      else
        merged = qual + "::" + aq;
      out.emplace_back(build_function(ch, src, merged));
    } else if (ty == "template_declaration") {
      // Iterate through template children
      int tn = ts_node_named_child_count(ch);
      for (int ti = 0; ti < tn; ++ti) {
        TSNode inner = ts_node_named_child(ch, ti);
        string ity = t(inner);
        // std::cerr << "[CLike] template child: " << ity << "\n";
        if (ity == "function_definition") {
          out.emplace_back(build_function(inner, src, qual));
        } else if (ity == "field_declaration_list") {
          // Attempt to pair with preceding identifier as class name
          string q = qual;
          if (ti - 1 >= 0) {
            TSNode prev = ts_node_named_child(ch, ti - 1);
            if (!ts_node_is_null(prev) && t(prev) == "identifier") {
              string cn = string(slice(src, prev));
              q = q.empty() ? cn : (qual + "::" + cn);
            }
          }
          collect_functions_in_scope(inner, src, q, out);
        } else if (ity == "declaration" || ity == "class_specifier" ||
                   ity == "struct_specifier" || ity == "namespace_definition" ||
                   ity == "template_declaration") {
          collect_functions_in_scope(inner, src, qual, out);
        }
      }
    } else if (ty == "class_specifier" || ty == "struct_specifier" ||
               ty == "union_specifier") {
      // Enter class scope
      TSNode nm = ts_node_child_by_field_name(ch, "name", 4);
      string q = qual;
      if (!ts_node_is_null(nm)) {
        string cn = string(slice(src, nm));
        q = q.empty() ? cn : (qual + "::" + cn);
      }
      TSNode body = ts_node_child_by_field_name(ch, "body", 4);
      if (!ts_node_is_null(body)) collect_functions_in_scope(body, src, q, out);
    } else if (ty == "namespace_definition") {
      // Enter namespace scope
      TSNode nm = ts_node_child_by_field_name(ch, "name", 4);
      string q = qual;
      if (!ts_node_is_null(nm)) {
        string nn = string(slice(src, nm));
        // normalized namespace path may include :: already
        q = q.empty() ? nn : (qual + "::" + nn);
      }
      TSNode body = ts_node_child_by_field_name(ch, "body", 4);
      if (!ts_node_is_null(body)) collect_functions_in_scope(body, src, q, out);
    } else {
      // Recurse into other declaration lists/blocks
      collect_functions_in_scope(ch, src, qual, out);
    }
  }
}

string CLikeGSGBuilder::function_name_from_declarator(TSNode decl,
                                                      const string &src) {
  // Try textual extraction: substring of declarator up to first '('
  string_view full = slice(src, decl);
  size_t p = full.find('(');
  if (p != string::npos) {
    string pre(full.substr(0, p));
    // trim spaces
    auto trim = [](string &s) {
      size_t a = s.find_first_not_of(" \t\n");
      size_t b = s.find_last_not_of(" \t\n");
      if (a == string::npos)
        s.clear();
      else
        s = s.substr(a, b - a + 1);
    };
    trim(pre);
    // remove leading pointer/reference/parentheses wrappers
    while (!pre.empty() && (pre[0] == '*' || pre[0] == '&' || pre[0] == '('))
      pre.erase(pre.begin());
    trim(pre);
    // common pattern has qualifiers like ns::C::m or operator<<; keep as-is
    if (!pre.empty()) return pre;
  }
  // Fallback: Search for first identifier under declarator subtree
  int n = ts_node_named_child_count(decl);
  for (int i = 0; i < n; ++i) {
    TSNode ch = ts_node_named_child(decl, i);
    string ty = t(ch);
    if (ty == "identifier" || ty == "field_identifier")
      return string(slice(src, ch));
    string nested = function_name_from_declarator(ch, src);
    if (!nested.empty()) return nested;
  }
  return string{};
}

GSGNode CLikeGSGBuilder::build_function(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::Function;
  g.loc = loc(n);
  TSNode decl = ts_node_child_by_field_name(n, "declarator", 10);
  if (!ts_node_is_null(decl)) g.name = function_name_from_declarator(decl, src);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children, 0);
  return g;
}

GSGNode CLikeGSGBuilder::build_function(TSNode n, const string &src,
                                        const string &qual) {
  GSGNode g = build_function(n, src);
  if (!qual.empty() && !g.name.empty()) {
    string prefix = qual + "::";
    if (g.name.rfind(prefix, 0) != 0) g.name = prefix + g.name;
  }
  return g;
}

void CLikeGSGBuilder::build_block_children(TSNode n, const string &src,
                                           std::vector<GSGNode> &out,
                                           int nesting) {
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i) {
    TSNode s = ts_node_named_child(n, i);
    string ty = t(s);
    // Always scan for lambdas inside this statement
    collect_lambdas_in_node(s, src, out);
    if (ty == "if_statement")
      out.emplace_back(build_if(s, src));
    else if (ty == "while_statement")
      out.emplace_back(build_while(s, src));
    else if (ty == "for_statement")
      out.emplace_back(build_for(s, src));
    else if (ty == "do_statement")
      out.emplace_back(build_do_while(s, src));
    else if (ty == "switch_statement") {
      GSGNode sw;
      sw.kind = GSGNodeKind::Switch;
      sw.loc = loc(s);
      int cn = ts_node_named_child_count(s);
      for (int j = 0; j < cn; ++j) {
        TSNode cc = ts_node_named_child(s, j);
        string cty = t(cc);
        if (cty == "case_statement" || cty == "default_statement") {
          GSGNode cs;
          cs.kind = GSGNodeKind::Case;
          cs.loc = loc(cc);
          int bn = ts_node_named_child_count(cc);
          for (int k = 0; k < bn; ++k) {
            TSNode bch = ts_node_named_child(cc, k);
            // heuristically treat nested statements under case
            if (ts_node_is_named(bch))
              build_block_children(bch, src, cs.children, nesting + 1);
          }
          sw.children.emplace_back(std::move(cs));
        }
      }
      out.emplace_back(std::move(sw));
    } else if (ty == "return_statement") {
      TSNode arg = ts_node_child_by_field_name(s, "argument", 8);
      if (!ts_node_is_null(arg)) {
        unsigned int cost = c_count_bool_ops_expr(arg, nesting, src);
        if (cost) {
          GSGNode e;
          e.kind = GSGNodeKind::Expr;
          e.loc = loc(s);
          e.addl_cost = cost;
          out.emplace_back(std::move(e));
        }
      }
    } else if (ty == "expression_statement") {
      if (ts_node_named_child_count(s) > 0) {
        TSNode expr = ts_node_named_child(s, 0);
        unsigned int cost = c_count_bool_ops_expr(expr, nesting, src);
        if (cost) {
          GSGNode e;
          e.kind = GSGNodeKind::Expr;
          e.loc = loc(expr);
          e.addl_cost = cost;
          out.emplace_back(std::move(e));
        }
      }
    } else if (ty == "declaration") {
      unsigned int sum = 0;
      int dn = ts_node_named_child_count(s);
      for (int di = 0; di < dn; ++di)
        sum += c_count_bool_ops_expr(ts_node_named_child(s, di), nesting, src);
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

GSGNode CLikeGSGBuilder::build_if(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::If;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += c_count_bool_ops_expr(cond, 0, src);
  TSNode cons = ts_node_child_by_field_name(n, "consequence", 11);
  if (!ts_node_is_null(cons)) build_block_children(cons, src, g.children, 1);
  TSNode alt = ts_node_child_by_field_name(n, "alternative", 11);
  if (!ts_node_is_null(alt)) {
    string ty = t(alt);
    if (ty == "if_statement") {
      // Direct else-if
      auto eif = build_if(alt, src);
      eif.kind = GSGNodeKind::ElseIf;
      g.children.emplace_back(std::move(eif));
    } else {
      // Some grammars wrap else-if inside a single-statement block.
      // If the alternative contains exactly one named child and it is an
      // if_statement, normalize it to ElseIf to avoid extra nesting.
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
      // Regular else body
      GSGNode el;
      el.kind = GSGNodeKind::Else;
      el.loc = loc(alt);
      build_block_children(alt, src, el.children, 1);
      g.children.emplace_back(std::move(el));
    }
  }
  return g;
}

GSGNode CLikeGSGBuilder::build_while(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::While;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += c_count_bool_ops_expr(cond, 0, src);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children, 1);
  return g;
}

GSGNode CLikeGSGBuilder::build_for(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::For;
  g.loc = loc(n);
  // Python-style parity: do not add boolean cost for for-loop condition
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children, 1);
  return g;
}

GSGNode CLikeGSGBuilder::build_do_while(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::DoWhile;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += c_count_bool_ops_expr(cond, 0, src);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, src, g.children);
  return g;
}

// Expression costs: handle logical ops and ternary
static inline bool has_logical_token(const string &s) {
  return s.find("&&") != string::npos || s.find("||") != string::npos ||
         s.find('!') != string::npos;
}

static inline string_view sv_slice(const string &src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

static unsigned int c_count_bool_alternations(TSNode n, const string &src) {
  string ty = t(n);
  unsigned int c = 0;
  if (ty == "binary_expression") {
    TSNode left = ts_node_child_by_field_name(n, "left", 4);
    TSNode right = ts_node_child_by_field_name(n, "right", 5);
    auto s = sv_slice(src, n);
    int curr = (s.find("&&") != string::npos)
                   ? 0
                   : ((s.find("||") != string::npos) ? 1 : -1);
    auto sl = sv_slice(src, left);
    auto sr = sv_slice(src, right);
    int lb = (sl.find("&&") != string::npos)
                 ? 0
                 : ((sl.find("||") != string::npos) ? 1 : -1);
    int rb = (sr.find("&&") != string::npos)
                 ? 0
                 : ((sr.find("||") != string::npos) ? 1 : -1);
    if (lb != -1 && curr != lb) c++;
    if (rb != -1 && curr != rb) c++;
    c += c_count_bool_alternations(left, src);
    c += c_count_bool_alternations(right, src);
  }
  return c;
}

unsigned int CLikeGSGBuilder::c_count_bool_ops_expr(TSNode n, int nesting,
                                                    const string &src) {
  if (ts_node_is_null(n)) return 0;
  string ty = t(n);
  if (ty == "binary_expression") {
    auto s = sv_slice(src, n);
    unsigned int base = has_logical_token(string(s)) ? 1 : 0;
    unsigned int alts = c_count_bool_alternations(n, src);
    TSNode left = ts_node_child_by_field_name(n, "left", 4);
    TSNode right = ts_node_child_by_field_name(n, "right", 5);
    return base + alts + c_count_bool_ops_expr(left, nesting, src) +
           c_count_bool_ops_expr(right, nesting, src);
  }
  if (ty == "unary_expression") {
    auto s = sv_slice(src, n);
    if (!s.empty() && s.front() == '!') return 1;
  }
  if (ty == "conditional_expression") {
    unsigned int c = 1 + static_cast<unsigned int>(nesting);
    int m = ts_node_named_child_count(n);
    for (int i = 0; i < m; ++i)
      c += c_count_bool_ops_expr(ts_node_named_child(n, i), nesting, src);
    return c;
  }
  unsigned int total = 0;
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i)
    total += c_count_bool_ops_expr(ts_node_named_child(n, i), nesting, src);
  return total;
}

void CLikeGSGBuilder::collect_lambdas_in_node(TSNode n, const string &src,
                                              std::vector<GSGNode> &out) {
  if (ts_node_is_null(n)) return;
  string ty = t(n);
  if (ty == "lambda_expression") {
    out.emplace_back(build_lambda(n, src));
    return;  // don't double-collect children
  }
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i)
    collect_lambdas_in_node(ts_node_named_child(n, i), src, out);
}

GSGNode CLikeGSGBuilder::build_lambda(TSNode n, const string &src) {
  GSGNode g;
  g.kind = GSGNodeKind::Function;
  g.loc = loc(n);
  // Name as lambda@row:col
  g.name = "lambda@" + std::to_string(g.loc.row) + ":" +
           std::to_string(g.loc.start_col);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) {
    // Use a local builder object to recurse
    CLikeGSGBuilder tmp;
    tmp.build_block_children(body, src, g.children, 0);
  }
  return g;
}
