#include "TemplateMonomorphContext.h"

#include "SemanticsHelpers.h"

namespace primec {

const std::set<std::string> &sourceDefsFamilyPathIndex(const Context &ctx) {
  if (!ctx.sourceDefsFamilyPathIndexValid) {
    ctx.sourceDefsFamilyPathIndex.clear();
    for (const auto &[defPath, definition] : ctx.sourceDefs) {
      (void)definition;
      ctx.sourceDefsFamilyPathIndex.insert(defPath);
    }
    ctx.sourceDefsFamilyPathIndexValid = true;
  }
  return ctx.sourceDefsFamilyPathIndex;
}

// True if any key in sourceDefs starts with `prefix`. Relies on
// lexicographic ordering: every element sharing `prefix` sorts
// contiguously starting at lower_bound(prefix).
bool anySourceDefStartsWith(const Context &ctx, const std::string &prefix) {
  const std::set<std::string> &index = sourceDefsFamilyPathIndex(ctx);
  const auto it = index.lower_bound(prefix);
  return it != index.end() && it->compare(0, prefix.size(), prefix) == 0;
}

bool isArgsPackParameterExpr(const Expr &param) {
  for (const Transform &transform : param.transforms) {
    if (transform.name == "args" && transform.templateArgs.size() == 1) {
      return true;
    }
  }
  return false;
}

bool definitionHasVariadicParameter(const Definition &def) {
  return !def.parameters.empty() && isArgsPackParameterExpr(def.parameters.back());
}

bool definitionHasTypePackParameter(const Definition &def) {
  for (bool isPack : def.templateArgIsPack) {
    if (isPack) {
      return true;
    }
  }
  return false;
}

bool isStructDefinition(const Definition &def) {
  bool isStruct = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return false;
    }
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (semantics::isStructTransformName(transform.name)) {
      isStruct = true;
    }
  }
  if (isStruct) {
    return true;
  }
  if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
}

} // namespace primec
