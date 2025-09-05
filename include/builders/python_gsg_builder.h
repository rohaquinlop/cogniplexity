#ifndef PYTHON_GSG_BUILDER_H
#define PYTHON_GSG_BUILDER_H

#include "../../include/gsg.h"

class PythonGSGBuilder : public IBuilder {
 public:
  std::vector<GSGNode> build_functions(TSNode root,
                                       const std::string &source) override;

 private:
  // node mappers
  GSGNode build_function(TSNode node, const std::string &source);
  void build_block_children(TSNode block, const std::string &source,
                            std::vector<GSGNode> &out, int nesting);
  GSGNode build_for(TSNode node, const std::string &source, int nesting);
  GSGNode build_while(TSNode node, const std::string &source, int nesting);
  GSGNode build_if(TSNode node, const std::string &source, int nesting);
  void build_try(TSNode node, const std::string &source, std::vector<GSGNode> &out, int nesting);

  // helpers
  static SourceLoc loc_from_node(TSNode node);
  static std::string_view slice_source(const std::string &source, TSNode node);
  static std::string_view get_identifier(TSNode node,
                                         const std::string &source);
  static unsigned int count_bool_operators(TSNode node,
                                           const std::string &source);
  static unsigned int count_bool_ops_expr(TSNode node, int nesting,
                                          const std::string &source);
};

#endif
