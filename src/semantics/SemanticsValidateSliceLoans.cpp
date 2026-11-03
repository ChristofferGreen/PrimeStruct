#include "SemanticsValidateSliceLoans.h"

#include <algorithm>
#include <array>
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

bool hasTypeLeaf(const Expr &expr, std::string_view leaf) {
  if (!expr.isBinding) {
    return false;
  }
  for (const Transform &transform : expr.transforms) {
    if (leafName(transform.name) == leaf) {
      return true;
    }
  }
  return false;
}

// Structs with a view field (directly or through another such struct) carry borrows, by leaf name.
using CarrierSet = std::set<std::string>;

bool isViewTypeName(const std::string &typeName, const CarrierSet &carriers) {
  return isViewTypeName(typeName) || carriers.count(leafName(typeName)) > 0;
}

bool isViewBinding(const Expr &expr, const CarrierSet &carriers) {
  if (!expr.isBinding) {
    return false;
  }
  for (const Transform &transform : expr.transforms) {
    if (isViewTypeName(transform.name, carriers)) {
      return true;
    }
  }
  return false;
}

bool isViewType(const std::vector<Transform> &transforms, const CarrierSet &carriers) {
  for (const Transform &transform : transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      std::string base = transform.templateArgs.front();
      const size_t angle = base.find('<');
      if (angle != std::string::npos) {
        base.resize(angle);
      }
      if (isViewTypeName(base, carriers)) {
        return true;
      }
    }
  }
  return false;
}

CarrierSet collectCarriers(const Program &program) {
  CarrierSet carriers;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const Definition &def : program.definitions) {
      const std::string name = leafName(def.fullPath);
      if (carriers.count(name) > 0) {
        continue;
      }
      bool isStruct = false;
      for (const Transform &transform : def.transforms) {
        isStruct = isStruct || transform.name == "struct";
      }
      if (!isStruct) {
        continue;
      }
      for (const Expr &statement : def.statements) {
        if (isViewBinding(statement, carriers)) {
          carriers.insert(name);
          changed = true;
          break;
        }
      }
    }
  }
  return carriers;
}

// The names in these tables are string views over literals: a function-local static std::set would allocate from the
// compile arena, which is released between compilations (docs/CompilerArenaAllocator.md).
template <size_t N>
bool listed(const std::array<std::string_view, N> &names, const std::string &name) {
  return std::find(names.begin(), names.end(), std::string_view(name)) != names.end();
}

// Calls that change the structure or the contents of their receiver.
bool isMutatingName(const std::string &name) {
  static constexpr std::array<std::string_view, 18> exact = {
      "push", "pop", "insert", "erase", "reserve", "clear", "remove", "resize", "shrinkToFit",
      "pushByte", "popByte", "pushCodePoint", "swap", "set", "fill", "copyFrom", "reverse", "sort"};
  if (listed(exact, name)) {
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
  const CarrierSet &carriers;
  std::set<std::string> parameterNames;
  bool returnsView = false;
  std::string error;
  // view binding -> the root bindings it borrows from
  std::map<std::string, std::set<std::string>> loanRoots;
  std::set<std::string> returnedRoots;
  std::set<std::string> viewParameters;
  std::set<std::string> viewLocals;
  // Open string arenas: binding name and the scope depth that holds it; Strings made while one is open.
  std::vector<std::pair<std::string, size_t>> arenas;
  std::map<std::string, size_t> arenaStrings;  // String binding -> index into `arenas`
  std::map<std::string, size_t> declarationOrder;  // binding -> position in the order of declaration
  size_t declarationCount = 0;
  std::vector<size_t> arenaOrder;  // declaration position of each open arena
  // names declared by each enclosing block, innermost last
  std::vector<std::set<std::string>> scopes;

  Checker(const Definition &definition, const CarrierSet &carrierSet) : def(definition), carriers(carrierSet) {
    for (const Expr &param : def.parameters) {
      parameterNames.insert(param.name);
      if (isViewBinding(param, carriers)) {
        viewParameters.insert(param.name);
      }
    }
    parameterNames.insert("this");
    returnsView = isViewType(def.transforms, carriers);
  }

  bool fail(const std::string &message) {
    if (error.empty()) {
      error = message + " in " + def.fullPath;
    }
    return false;
  }

  // The root bindings a view-producing expression borrows from. A constructor takes the roots of all its arguments; a
  // call on a binding takes that binding's roots (or the binding itself when it is not a view).
  std::set<std::string> rootsOf(const Expr &expr) const {
    std::set<std::string> roots;
    if (expr.kind == Expr::Kind::Name) {
      const auto it = loanRoots.find(expr.name);
      if (it != loanRoots.end()) {
        roots = it->second;
      }
      return roots;
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return roots;
    }
    if (expr.isBraceConstructor) {
      for (const Expr &arg : expr.args) {
        const std::set<std::string> inner = viewRootsOf(arg);
        roots.insert(inner.begin(), inner.end());
      }
      return roots;
    }
    if (!expr.args.empty()) {
      const Expr *base = placeBase(expr.args.front());
      if (base->kind == Expr::Kind::Name) {
        const auto it = loanRoots.find(base->name);
        if (it != loanRoots.end()) {
          roots = it->second;
        } else if (viewLocals.count(base->name) == 0) {
          roots.insert(base->name); // an owner; a view of unknown provenance borrows nothing we can name
        }
      }
    }
    return roots;
  }

  // Whether `name` is a view or a carrier struct (a view-typed local or parameter).
  bool isViewish(const std::string &name) const { return loanRoots.count(name) > 0 || viewParameters.count(name) > 0; }

  // The roots of a constructor argument, only when the argument is itself a view: a view name, or a call that makes a
  // view (`s.view()`, `v.slice(a, b)`) or works on one. Arithmetic on parameters is not a borrow.
  std::set<std::string> viewRootsOf(const Expr &expr) const {
    if (expr.kind == Expr::Kind::Name) {
      const auto it = loanRoots.find(expr.name);
      if (it != loanRoots.end()) {
        return it->second;
      }
      if (viewParameters.count(expr.name) > 0) {
        return {expr.name};
      }
      return {};
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.args.empty()) {
      return {};
    }
    static constexpr std::array<std::string_view, 15> makers = {
        "view", "slice", "sliceMut", "readOnly", "takeFirst", "dropFirst", "takeLast", "dropLast",
        "head", "tail", "trim", "trimStart", "trimEnd", "lineAt", "splitPart"};
    const Expr *base = placeBase(expr.args.front());
    if (listed(makers, leafName(expr.name)) || (base->kind == Expr::Kind::Name && isViewish(base->name))) {
      return rootsOf(expr);
    }
    // A call that takes a view (`parseRequest(buffer.view())`) hands back a borrow of what the view borrows.
    std::set<std::string> roots;
    for (const Expr &arg : expr.args) {
      const std::set<std::string> inner = viewRootsOf(arg);
      roots.insert(inner.begin(), inner.end());
    }
    return roots;
  }

  bool declaredInOuterScope(const std::string &name) const {
    for (size_t i = 0; i + 1 < scopes.size(); ++i) {
      if (scopes[i].count(name) > 0) {
        return scopes.back().count(name) == 0;
      }
    }
    return false;
  }

  bool checkBlock(const std::vector<Expr> &statements) {
    scopes.emplace_back();
    const bool ok = checkStatements(statements);
    while (!arenas.empty() && arenas.back().second == scopes.size()) {
      for (auto it = arenaStrings.begin(); it != arenaStrings.end();) {
        it = it->second == arenas.size() - 1 ? arenaStrings.erase(it) : std::next(it);
      }
      arenas.pop_back();
      arenaOrder.pop_back();
    }
    scopes.pop_back();
    return ok;
  }

  bool checkStatements(const std::vector<Expr> &statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      const Expr &statement = statements[i];
      if (statement.isBinding) {
        scopes.back().insert(statement.name);
        declarationOrder[statement.name] = ++declarationCount;
        if (hasTypeLeaf(statement, "StringArena")) {
          arenas.emplace_back(statement.name, scopes.size());
          arenaOrder.push_back(declarationCount);
        } else if (hasTypeLeaf(statement, "String") && !arenas.empty()) {
          arenaStrings[statement.name] = arenas.size() - 1;
        }
      }
      if (isViewBinding(statement, carriers)) {
        viewLocals.insert(statement.name);
      }
      if (isViewBinding(statement, carriers) && !statement.args.empty()) {
        std::set<std::string> roots = rootsOf(statement.args.front());
        roots.erase(statement.name);
        if (roots.empty()) {
          loanRoots.erase(statement.name);
        } else {
          loanRoots[statement.name] = roots;
          size_t lastUse = i;
          for (size_t j = i + 1; j < statements.size(); ++j) {
            if (mentionsName(statements[j], statement.name)) {
              lastUse = j;
            }
          }
          for (size_t j = i + 1; j < lastUse; ++j) {
            for (const std::string &root : roots) {
              if (mutatesRoot(statements[j], root)) {
                return fail("borrowed binding: " + root + " (root: " + root + ", sink: " + statement.name + ")");
              }
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

  static std::string firstOf(const std::set<std::string> &roots) { return *roots.begin(); }

  // A String made while an arena is open cannot be returned, assigned to a binding made before the arena, or pushed
  // into a container made before it: its memory is freed when the arena ends.
  bool checkArenaEscapes(const Expr &statement, const std::string &leaf) {
    if (arenaStrings.empty()) {
      return true;
    }
    auto arenaOf = [&](const Expr &value) -> const std::pair<std::string, size_t> * {
      if (value.kind != Expr::Kind::Name) {
        return nullptr;
      }
      const auto it = arenaStrings.find(value.name);
      return it == arenaStrings.end() ? nullptr : &arenas[it->second];
    };
    auto arenaIndexOf = [&](const Expr &value) -> size_t {
      return arenaStrings.find(value.name)->second;
    };
    auto madeBefore = [&](const std::string &name, size_t arenaIndex) {
      const auto it = declarationOrder.find(name);
      return it == declarationOrder.end() || it->second < arenaOrder[arenaIndex];
    };
    if (leaf == "return" && statement.args.size() == 1) {
      if (const auto *arena = arenaOf(statement.args.front())) {
        return fail("string escapes its arena via return (arena: " + arena->first + ")");
      }
    }
    if (leaf == "assign" && statement.args.size() == 2 && statement.args[0].kind == Expr::Kind::Name) {
      if (const auto *arena = arenaOf(statement.args[1])) {
        if (madeBefore(statement.args[0].name, arenaIndexOf(statement.args[1]))) {
          return fail("string escapes its arena via outer binding " + statement.args[0].name + " (arena: " + arena->first + ")");
        }
      }
    }
    if ((leaf == "push" || leaf == "insert") && statement.args.size() >= 2) {
      const Expr *base = placeBase(statement.args.front());
      for (size_t k = 1; k < statement.args.size(); ++k) {
        if (const auto *arena = arenaOf(statement.args[k])) {
          if (base->kind == Expr::Kind::Name && madeBefore(base->name, arenaIndexOf(statement.args[k]))) {
            return fail("string escapes its arena via container " + base->name + " (arena: " + arena->first + ")");
          }
        }
      }
    }
    return true;
  }

  // A view cannot be stored in a field, a container or an outer binding, and cannot be returned when it borrows a local.
  bool checkEscapes(const Expr &statement) {
    if (statement.kind != Expr::Kind::Call || statement.isBinding) {
      return true;
    }
    const std::string leaf = leafName(statement.name);
    if (!checkArenaEscapes(statement, leaf)) {
      return false;
    }
    if (leaf == "assign" && statement.args.size() == 2) {
      const Expr &target = statement.args[0];
      const Expr &value = statement.args[1];
      const std::set<std::string> roots = viewRootsOf(value);
      if (!roots.empty() && value.kind == Expr::Kind::Name && target.kind == Expr::Kind::Call && target.isFieldAccess) {
        return fail("slice escapes via field " + target.name + " (root: " + firstOf(roots) + ")");
      }
      if (!roots.empty() && target.kind == Expr::Kind::Name && declaredInOuterScope(target.name)) {
        for (const std::string &root : roots) {
          if (scopes.back().count(root) > 0) {
            return fail("slice escapes via outer binding " + target.name + " (root: " + root + ")");
          }
        }
      }
    }
    if ((leaf == "push" || leaf == "insert") && statement.args.size() >= 2) {
      for (size_t k = 1; k < statement.args.size(); ++k) {
        const Expr &value = statement.args[k];
        if (value.kind == Expr::Kind::Name && loanRoots.count(value.name) > 0) {
          return fail("slice escapes via container (root: " + firstOf(loanRoots[value.name]) + ")");
        }
      }
    }
    if (leaf == "return" && statement.args.size() == 1) {
      return checkReturned(statement.args.front());
    }
    return true;
  }

  bool checkReturned(const Expr &value) {
    if (!returnsView) {
      return true;
    }
    for (const std::string &root : viewRootsOf(value)) {
      returnedRoots.insert(root);
      if (parameterNames.count(root) == 0) {
        return fail("slice escapes via return (root: " + root + ")");
      }
    }
    return true;
  }

  bool run() {
    if (!checkBlock(def.statements)) {
      return false;
    }
    if (def.returnExpr.has_value() && !checkReturned(*def.returnExpr)) {
      return false;
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
  const CarrierSet carriers = collectCarriers(program);
  for (const Definition &def : program.definitions) {
    if (def.statements.empty() && !def.returnExpr.has_value()) {
      continue;
    }
    Checker checker(def, carriers);
    if (!checker.run()) {
      error = checker.error;
      return false;
    }
  }
  return true;
}

} // namespace primec::semantics
