#ifndef JAVASCRIPT_GSG_BUILDER_H
#define JAVASCRIPT_GSG_BUILDER_H

#include "../../include/gsg.h"

class JavaScriptGSGBuilder : public IBuilder {
 public:
  std::vector<GSGNode> build_functions(TSNode root,
                                       const std::string &source) override;

 private:
  static SourceLoc loc(TSNode n);
  static std::string_view slice(const std::string &src, TSNode n);
  static std::string_view name_of(TSNode n, const std::string &src);

  GSGNode build_function(TSNode n, const std::string &src);
  void build_block_children(TSNode n, const std::string &src,
                            std::vector<GSGNode> &out);
  GSGNode build_if(TSNode n, const std::string &src);
  GSGNode build_while(TSNode n, const std::string &src);
  GSGNode build_for(TSNode n, const std::string &src);
  GSGNode build_do_while(TSNode n, const std::string &src);

  // expression costs
  static unsigned int js_count_bool_ops_expr(TSNode n, int nesting,
                                             const std::string &src);
};

#endif
