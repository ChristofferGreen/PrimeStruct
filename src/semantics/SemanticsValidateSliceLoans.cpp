#include "SemanticsValidateSliceLoans.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {

namespace {

std::string leafName(const std::string &name) {
  const size_t slash = name.find_last_of('/');
  return slash == std::string::npos ? name : name.substr(slash + 1);
}

bool isViewTypeName(const std::string &typeName) {
  const std::string leaf = leafName(typeName);
  return leaf == "Slice" || leaf == "ReadSlice" || leaf == "WriteSlice" || leaf == "StringView";
}

bool isViewBinding(const Expr &expr) {
  if (!expr.isBinding) {
    return false;
  }
  for (const Transform &transform : expr.transforms) {
    if (isViewTypeName(transform.name)) {
      return true;
    }
  }
  return false;
}

bool isViewType(const std::vector<Transform> &transforms) {
  for (const Transform &transform : transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      std::string base = transform.templateArgs.front();
      const size_t angle = base.find('<');
      if (angle != std::string::npos) {
        base.resize(angle);
      }
      if (isViewTypeName(base)) {
        return true;
      }
    }
  }
  return false;
}

// Calls that change the structure or the contents of their receiver.
bool isMutatingName(const std::string &name) {
  static const std::set<std::string> exact = {"push",   "pop",    "insert",   "erase",    "reserve",      "clear",
                                              "remove", "resize", "shrinkToFit", "pushByte", "popByte", "pushCodePoint",
                                              "swap",   "set",    "fill",     "copyFrom", "reverse",      "sort"};
  if (exact.count(name) > 0) {
    return true;
  }
  return name.rfind("append", 0) == 0 || name.rfind("insert", 0) == 0 || name.rfind("replace", 0) == 0 ||
         name.rfind("assign", 0) == 0 || name.rfind("remove", 0) == 0 || name.rfind("push", 0) == 0;
}

// The binding a place expression starts at: `v`, `v.field`, `v.view()`.
const Expr *placeBase(const Expr &expr) {
  const Expr *current = &expr;
  while (current->kind == Expr::Kind::Call && !current->args.empty() && (current->isFieldAccess || current->isMethodCall)) {
    current = &current->args.front();
  }
  return current;
}

bool mentionsName(const Expr &expr, const std::string &name) {
  if (expr.kind == Expr::Kind::Name && expr.name == name) {
    return true;
  }
  for (const Expr &arg : expr.args) {
    if (mentionsName(arg, name)) {
      return true;
    }
  }
  for (const Expr &arg : expr.bodyArguments) {
    if (mentionsName(arg, name)) {
      return true;
    }
  }
  return false;
}

// Whether `expr` structurally changes (or assigns, or moves) the binding `root`.
bool mutatesRoot(const Expr &expr, const std::string &root) {
  if (expr.kind == Expr::Kind::Call && !expr.isBinding && !expr.args.empty()) {
    const std::string leaf = leafName(expr.name);
    const Expr *base = placeBase(expr.args.front());
    const bool onRoot = base->kind == Expr::Kind::Name && base->name == root;
    if (onRoot && !expr.isFieldAccess) {
      if (leaf == "assign" || leaf == "move" || isMutatingName(leaf)) {
        return true;
      }
    }
  }
  for (const Expr &arg : expr.args) {
    if (mutatesRoot(arg, root)) {
      return true;
    }
  }
  for (const Expr &arg : expr.bodyArguments) {
    if (mutatesRoot(arg, root)) {
      return true;
    }
  }
  return false;
}

struct Checker {
  const Definition &def;
  std::set<std::string> parameterNames;
  bool returnsView = false;
  std::string error;
  // view binding -> the root binding it borrows from
  std::map<std::string, std::string> loanRoots;
  std::set<std::string> returnedRoots;

  explicit Checker(const Definition &definition) : def(definition) {
    for (const Expr &param : def.parameters) {
      parameterNames.insert(param.name);
    }
    parameterNames.insert("this");
    returnsView = isViewType(def.transforms);
  }

  bool fail(const std::string &message) {
    if (error.empty()) {
      error = message + " in " + def.fullPath;
    }
    return false;
  }

  // The root binding a view-producing expression borrows from, or empty.
  std::string rootOf(const Expr &expr) const {
    if (expr.kind == Expr::Kind::Name) {
      const auto it = loanRoots.find(expr.name);
      return it == loanRoots.end() ? std::string() : it->second;
    }
    if (expr.kind == Expr::Kind::Call && !expr.isBinding && !expr.args.empty()) {
      const Expr *base = placeBase(expr.args.front());
      if (base->kind == Expr::Kind::Name) {
        const auto it = loanRoots.find(base->name);
        return it == loanRoots.end() ? base->name : it->second;
      }
    }
    return {};
  }

  bool checkBlock(const std::vector<Expr> &statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      const Expr &statement = statements[i];
      if (isViewBinding(statement) && !statement.args.empty()) {
        const std::string root = rootOf(statement.args.front());
        if (root.empty() || root == statement.name) {
          loanRoots.erase(statement.name);
        } else {
          loanRoots[statement.name] = root;
          size_t lastUse = i;
          for (size_t j = i + 1; j < statements.size(); ++j) {
            if (mentionsName(statements[j], statement.name)) {
              lastUse = j;
            }
          }
          for (size_t j = i + 1; j < lastUse; ++j) {
            if (mutatesRoot(statements[j], root)) {
              return fail("borrowed binding: " + root + " (root: " + root + ", sink: " + statement.name + ")");
            }
          }
        }
      }
      if (!checkEscapes(statement)) {
        return false;
      }
      if (!checkNested(statement)) {
        return false;
      }
    }
    return true;
  }

  bool checkNested(const Expr &statement) {
    if (!statement.bodyArguments.empty() && !checkBlock(statement.bodyArguments)) {
      return false;
    }
    for (const Expr &arg : statement.args) {
      if (arg.kind == Expr::Kind::Call && !arg.bodyArguments.empty() && !checkNested(arg)) {
        return false;
      }
    }
    return true;
  }

  // A view cannot be stored in a field or a container, and cannot be returned when it borrows a local.
  bool checkEscapes(const Expr &statement) {
    if (statement.kind != Expr::Kind::Call || statement.isBinding) {
      return true;
    }
    const std::string leaf = leafName(statement.name);
    if (leaf == "assign" && statement.args.size() == 2) {
      const Expr &target = statement.args[0];
      const std::string root = rootOf(statement.args[1]);
      if (!root.empty() && statement.args[1].kind == Expr::Kind::Name && target.kind == Expr::Kind::Call &&
          target.isFieldAccess) {
        return fail("slice escapes via field " + target.name + " (root: " + root + ")");
      }
    }
    if ((leaf == "push" || leaf == "insert") && statement.args.size() >= 2) {
      for (size_t k = 1; k < statement.args.size(); ++k) {
        const Expr &value = statement.args[k];
        if (value.kind == Expr::Kind::Name) {
          const std::string root = rootOf(value);
          if (!root.empty() && loanRoots.count(value.name) > 0) {
            return fail("slice escapes via container (root: " + root + ")");
          }
        }
      }
    }
    if (leaf == "return" && statement.args.size() == 1) {
      const std::string root = rootOf(statement.args.front());
      if (!root.empty() && returnsView) {
        returnedRoots.insert(root);
        if (parameterNames.count(root) == 0) {
          return fail("slice escapes via return (root: " + root + ")");
        }
      }
    }
    return true;
  }

  bool run() {
    if (!checkBlock(def.statements)) {
      return false;
    }
    if (def.returnExpr.has_value() && returnsView) {
      const std::string root = rootOf(*def.returnExpr);
      if (!root.empty()) {
        returnedRoots.insert(root);
        if (parameterNames.count(root) == 0) {
          return fail("slice escapes via return (root: " + root + ")");
        }
      }
    }
    if (returnsView) {
      std::set<std::string> borrowedParameters;
      for (const std::string &root : returnedRoots) {
        if (parameterNames.count(root) > 0) {
          borrowedParameters.insert(root);
        }
      }
      std::string named;
      for (const Transform &transform : def.transforms) {
        if (transform.name == "returns_borrow" && transform.templateArgs.size() == 1) {
          named = transform.templateArgs.front();
        }
      }
      if (!named.empty() && parameterNames.count(named) == 0) {
        return fail("returns_borrow names an unknown parameter: " + named);
      }
      if (named.empty() && borrowedParameters.size() > 1) {
        return fail("returned view could borrow from more than one parameter; add [returns_borrow<name>]");
      }
    }
    return true;
  }
};

} // namespace

bool validateSliceLoans(const Program &program, std::string &error) {
  for (const Definition &def : program.definitions) {
    if (def.statements.empty() && !def.returnExpr.has_value()) {
      continue;
    }
    Checker checker(def);
    if (!checker.run()) {
      error = checker.error;
      return false;
    }
  }
  return true;
}

} // namespace primec::semantics
