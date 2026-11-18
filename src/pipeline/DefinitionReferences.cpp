#include "DefinitionReferences.h"

#include <algorithm>
#include <set>

namespace primec {
namespace {

bool isPathCharacter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
         c == '/';
}

class ReferenceCollector {
public:
  ReferenceCollector(const Definition &definition,
                     const std::unordered_set<std::string> &definedPaths)
      : definition_(definition), definedPaths_(definedPaths) {
  }

  std::vector<std::string> run() {
    addTransforms(definition_.transforms);
    for (const std::string &arg : definition_.templateArgs) {
      addText(arg);
    }
    for (const Expr &parameter : definition_.parameters) {
      addExpr(parameter);
    }
    for (const Expr &statement : definition_.statements) {
      addExpr(statement);
    }
    if (definition_.returnExpr.has_value()) {
      addExpr(*definition_.returnExpr);
    }
    return std::vector<std::string>(found_.begin(), found_.end());
  }

private:
  // One identifier or path: absolute, or relative to the definition's namespaces, innermost first.
  void addName(const std::string &name) {
    if (name.empty() || name == "/") {
      return;
    }
    if (name.front() == '/') {
      if (definedPaths_.count(name) != 0) {
        found_.insert(name);
      }
      return;
    }
    for (std::string scope = parentDefinitionPath(definition_.fullPath); !scope.empty();
         scope = parentDefinitionPath(scope)) {
      const std::string candidate = scope + "/" + name;
      if (definedPaths_.count(candidate) != 0) {
        found_.insert(candidate);
        return;
      }
    }
    const std::string root = "/" + name;
    if (definedPaths_.count(root) != 0) {
      found_.insert(root);
    }
  }

  // Type text such as `Reference<Point>`: every identifier or path in it.
  void addText(const std::string &text) {
    std::string token;
    for (const char c : text) {
      if (isPathCharacter(c)) {
        token += c;
        continue;
      }
      addName(token);
      token.clear();
    }
    addName(token);
  }

  void addTransforms(const std::vector<Transform> &transforms) {
    for (const Transform &transform : transforms) {
      addText(transform.name);
      for (const std::string &arg : transform.templateArgs) {
        addText(arg);
      }
    }
  }

  void addExpr(const Expr &expr) {
    addTransforms(expr.transforms);
    if (!expr.resolvedCallPath.empty()) {
      addName(expr.resolvedCallPath);
    }
    if (expr.kind == Expr::Kind::Call || expr.kind == Expr::Kind::Name) {
      addText(expr.name);
    }
    for (const std::string &arg : expr.templateArgs) {
      addText(arg);
    }
    for (const Expr &arg : expr.args) {
      addExpr(arg);
    }
    for (const Expr &body : expr.bodyArguments) {
      addExpr(body);
    }
    for (const Expr &capture : expr.lambdaInitCaptures) {
      addExpr(capture);
    }
  }

  const Definition &definition_;
  const std::unordered_set<std::string> &definedPaths_;
  std::set<std::string> found_;
};

} // namespace

std::string parentDefinitionPath(const std::string &path) {
  const size_t slash = path.rfind('/');
  if (slash == std::string::npos || slash == 0) {
    return "";
  }
  return path.substr(0, slash);
}

std::vector<std::string>
collectDefinitionReferences(const Definition &definition,
                            const std::unordered_set<std::string> &definedPaths) {
  return ReferenceCollector(definition, definedPaths).run();
}

} // namespace primec
