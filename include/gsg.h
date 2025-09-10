#ifndef GSG_H
#define GSG_H

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <tree_sitter/api.h>

// Supported languages (extensible)
enum class Language {
  Python,
  C,
  Cpp,
  JavaScript,
  TypeScript,
  Java,
  Unknown,
};

// General Syntax Graph node kinds
enum class GSGNodeKind {
  Root,
  Class,
  Function,
  Block,
  If,
  ElseIf,
  Else,
  For,
  While,
  DoWhile,
  Switch,
  Case,
  With,
  Except,
  Expr,
  Try,
  Catch,
  Finally,
  Ternary,
  Return,
  Break,
  Continue,
  Unknown,
};

struct SourceLoc {
  unsigned int row{0};
  unsigned int start_col{0};
  unsigned int end_col{0};
};

struct GSGNode {
  GSGNodeKind kind{GSGNodeKind::Unknown};
  std::string name{};         // for functions/classes/cases
  SourceLoc loc{};            // for line-complexity mapping
  unsigned int addl_cost{0};  // extra cost (e.g., boolean operator changes)
  std::vector<GSGNode> children{};
};

struct IBuilder {
  virtual ~IBuilder() = default;
  // Build function-level GSG nodes found in the file/module root
  virtual std::vector<GSGNode> build_functions(TSNode root,
                                               const std::string &source) = 0;
};

std::unique_ptr<IBuilder> make_builder(Language lang);

#endif
