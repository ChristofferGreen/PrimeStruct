#include "OperatorHelperPrediction.h"

#include "primec/support/OperatorRewriteTargets.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>

namespace primec {

namespace {

// The path of a math type spelled `Vec2` or `/std/math/Vec2`, empty for any other spelling.
std::string mathTypePath(std::string_view spelling) {
  constexpr std::string_view Namespace = "/std/math/";
  std::string_view leaf = spelling;
  if (leaf.substr(0, Namespace.size()) == Namespace) {
    leaf.remove_prefix(Namespace.size());
  }
  for (const std::string_view known : {"Vec2", "Vec3", "Vec4", "Mat2", "Mat3", "Mat4", "Quat"}) {
    if (leaf == known) {
      return std::string(Namespace) + std::string(known);
    }
  }
  return {};
}

bool isVectorPath(const std::string &path) {
  return path.rfind("/std/math/Vec", 0) == 0;
}

class OperatorSiteScanner {
public:
  std::vector<std::string> scan(const Program &program) {
    for (const Definition &definition : program.definitions) {
      locals_.clear();
      for (const Expr &parameter : definition.parameters) {
        bindLocal(parameter);
      }
      for (const Expr &statement : definition.statements) {
        visit(statement);
      }
      if (definition.returnExpr.has_value()) {
        visit(*definition.returnExpr);
      }
    }
    std::sort(demand_.begin(), demand_.end());
    demand_.erase(std::unique(demand_.begin(), demand_.end()), demand_.end());
    return std::move(demand_);
  }

private:
  // The math type of a binding or parameter: its declared type (`Vec2`, `Reference<Vec2>`) or, when it declares
  // none, the type of its initializer.
  std::string bindingType(const Expr &binding) {
    for (const Transform &transform : binding.transforms) {
      std::string path = mathTypePath(transform.name);
      if (path.empty() && transform.templateArgs.size() == 1) {
        path = mathTypePath(transform.templateArgs.front());
      }
      if (!path.empty()) {
        return path;
      }
    }
    return binding.args.size() == 1 ? typeOf(binding.args.front()) : std::string();
  }

  void bindLocal(const Expr &binding) {
    std::string path = bindingType(binding);
    if (!path.empty()) {
      locals_[binding.name] = std::move(path);
    }
  }

  static std::string operatorName(const Expr &expr) {
    std::string_view name = expr.name;
    if (!name.empty() && name.front() == '/') {
      name.remove_prefix(1);
    }
    return std::string(name);
  }

  static bool isOperatorCall(const Expr &expr) {
    if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 || expr.isBinding || expr.isMethodCall ||
        expr.isFieldAccess || expr.isBraceConstructor) {
      return false;
    }
    const std::string name = operatorName(expr);
    return name == "plus" || name == "minus" || name == "multiply" || name == "divide";
  }

  // The math type an expression evaluates to, empty for a scalar or for a shape this scan does not follow.
  std::string typeOf(const Expr &expr) {
    if (expr.kind == Expr::Kind::Name) {
      const auto found = locals_.find(expr.name);
      return found == locals_.end() ? std::string() : found->second;
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return {};
    }
    if (isOperatorCall(expr)) {
      const std::string left = typeOf(expr.args[0]);
      const std::string right = typeOf(expr.args[1]);
      const std::string name = operatorName(expr);
      if (left.empty()) {
        return name == "multiply" ? right : std::string();
      }
      if (right.empty() || left == right) {
        return left;
      }
      // A matrix on a vector gives the vector, a quaternion on a vector the vector.
      return isVectorPath(right) ? right : left;
    }
    return expr.isMethodCall || expr.isFieldAccess ? std::string() : mathTypePath(expr.name);
  }

  void visit(const Expr &expr) {
    if (expr.isBinding) {
      for (const Expr &arg : expr.args) {
        visit(arg);
      }
      bindLocal(expr);
    } else {
      if (isOperatorCall(expr)) {
        const std::string helper = primec::operatorRewriteTargetPath(
            operatorName(expr), typeOf(expr.args[0]), typeOf(expr.args[1]));
        if (!helper.empty()) {
          demand_.push_back(helper);
        }
      }
      for (const Expr &arg : expr.args) {
        visit(arg);
      }
    }
    for (const Expr &body : expr.bodyArguments) {
      visit(body);
    }
  }

  std::unordered_map<std::string, std::string> locals_;
  std::vector<std::string> demand_;
};

} // namespace

std::vector<std::string> predictOperatorRewriteDemand(const Program &program) {
  return OperatorSiteScanner().scan(program);
}

} // namespace primec
