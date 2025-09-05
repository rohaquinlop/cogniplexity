#ifndef C_GSG_BUILDER_H
#define C_GSG_BUILDER_H

#include "../../include/gsg.h"

// Minimal C/C++ builder (reused for both languages)
class CLikeGSGBuilder : public IBuilder {
 public:
  std::vector<GSGNode> build_functions(TSNode root, const std::string &source) override;

 private:
  static SourceLoc loc(TSNode n);
  static std::string_view slice(const std::string &src, TSNode n);
  static std::string function_name_from_declarator(TSNode decl, const std::string &src);

  void collect_functions_in_scope(TSNode n, const std::string &src, const std::string &qual, std::vector<GSGNode> &out);
  GSGNode build_function(TSNode n, const std::string &src);
  GSGNode build_function(TSNode n, const std::string &src, const std::string &qual);
  void build_block_children(TSNode n, const std::string &src, std::vector<GSGNode> &out);
  GSGNode build_if(TSNode n, const std::string &src);
  GSGNode build_while(TSNode n, const std::string &src);
  GSGNode build_for(TSNode n, const std::string &src);
  GSGNode build_do_while(TSNode n, const std::string &src);

  static unsigned int c_count_bool_ops_expr(TSNode n, int nesting, const std::string &src);

  // lambdas
  static void collect_lambdas_in_node(TSNode n, const std::string &src, std::vector<GSGNode> &out);
  static GSGNode build_lambda(TSNode n, const std::string &src);
};

#endif
