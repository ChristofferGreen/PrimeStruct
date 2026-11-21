#include "ReferenceParameterEffects.h"

#include <string_view>

namespace primec::semantics {

namespace {

bool isStructuralMutatorName(const std::string &name) {
  const size_t slash = name.rfind('/');
  const std::string_view leaf =
      slash == std::string::npos ? std::string_view(name) : std::string_view(name).substr(slash + 1);
  return leaf == "push" || leaf == "pop" || leaf == "clear" || leaf == "reserve" || leaf == "remove_at" ||
         leaf == "remove_swap" || leaf == "insert";
}

bool isLocationOf(const Expr &expr) {
  std::string pointerName;
  return expr.kind == Expr::Kind::Call && expr.args.size() == 1 && getBuiltinPointerName(expr, pointerName) &&
         pointerName == "location";
}

// The name a place expression is rooted at: `location(x)`, `dereference(x)`, `x.field` and `x[i]` all root at `x`.
const Expr *placeRoot(const Expr &place) {
  const Expr *root = &place;
  while (root->kind == Expr::Kind::Call) {
    const bool isStep = (root->isFieldAccess && root->args.size() == 1) || isLocationOf(*root) ||
                        (root->args.size() == 1 && isSimpleCallName(*root, "dereference")) ||
                        (root->args.size() == 2 &&
                         (isSimpleCallName(*root, "at") || isSimpleCallName(*root, "at_unsafe")));
    if (!isStep) {
      break;
    }
    root = &root->args.front();
  }
  return root;
}

bool isTracked(const Expr &place, const std::unordered_set<std::string> &tracked) {
  const Expr *root = placeRoot(place);
  return root->kind == Expr::Kind::Name && tracked.count(root->name) > 0;
}

bool declaresReferenceOrPointer(const Expr &binding) {
  for (const Transform &transform : binding.transforms) {
    if (transform.name == "Reference" || transform.name == "Pointer") {
      return true;
    }
  }
  return false;
}

// The index of the callee parameter that argument `argIndex` of `call` binds to.
size_t calleeParameterIndex(const Expr &call, size_t argIndex, const std::vector<ParameterInfo> &parameters) {
  if (argIndex < call.argNames.size() && call.argNames[argIndex].has_value()) {
    for (size_t candidate = 0; candidate < parameters.size(); ++candidate) {
      if (parameters[candidate].name == *call.argNames[argIndex]) {
        return candidate;
      }
    }
    return parameters.size();
  }
  return argIndex;
}

} // namespace

bool ReferenceParameterEffects::changesStructurally(const std::string &definitionPath, size_t parameterIndex) {
  const std::string key = definitionPath + "#" + std::to_string(parameterIndex);
  if (const auto cached = cache_.find(key); cached != cache_.end()) {
    return cached->second;
  }
  const bool outerAssumed = assumedCycle_;
  assumedCycle_ = false;
  const bool result = analyze(definitionPath, parameterIndex);
  if (!assumedCycle_) {
    cache_.emplace(key, result);
  }
  assumedCycle_ = assumedCycle_ || outerAssumed;
  return result;
}

bool ReferenceParameterEffects::analyze(const std::string &definitionPath, size_t parameterIndex) {
  const auto definition = definitions_.find(definitionPath);
  const auto parameters = parameters_.find(definitionPath);
  if (definition == definitions_.end() || parameters == parameters_.end() ||
      parameterIndex >= parameters->second.size()) {
    return false;
  }
  const ParameterInfo &parameter = parameters->second[parameterIndex];
  if (parameter.binding.isCopy || parameter.binding.isMove) {
    return false;
  }
  const std::string key = definitionPath + "#" + std::to_string(parameterIndex);
  if (!active_.insert(key).second) {
    assumedCycle_ = true;
    return false;
  }
  std::unordered_set<std::string> tracked{parameter.name};
  bool changes = false;
  for (const Expr &statement : definition->second->statements) {
    if (exprChanges(statement, tracked)) {
      changes = true;
      break;
    }
  }
  if (!changes && definition->second->returnExpr.has_value()) {
    changes = exprChanges(*definition->second->returnExpr, tracked);
  }
  active_.erase(key);
  return changes;
}

bool ReferenceParameterEffects::exprChanges(const Expr &expr, std::unordered_set<std::string> &tracked) {
  if (expr.isBinding && expr.args.size() == 1 && isTracked(expr.args.front(), tracked) &&
      (isLocationOf(expr.args.front()) || declaresReferenceOrPointer(expr))) {
    tracked.insert(expr.name); // an alias of the parameter's place
  }
  if (expr.kind == Expr::Kind::Call && !expr.isBinding && callChanges(expr, tracked)) {
    return true;
  }
  for (const Expr &arg : expr.args) {
    if (exprChanges(arg, tracked)) {
      return true;
    }
  }
  for (const Expr &body : expr.bodyArguments) {
    if (exprChanges(body, tracked)) {
      return true;
    }
  }
  return false;
}

bool ReferenceParameterEffects::callChanges(const Expr &call, const std::unordered_set<std::string> &tracked) {
  if (call.args.empty()) {
    return false;
  }
  const std::string &calleePath = !call.resolvedCallPath.empty() ? call.resolvedCallPath : call.name;
  const bool userDefinition = definitions_.count(calleePath) > 0 && calleePath.rfind("/std/", 0) != 0;
  // A user definition named like a container helper is an ordinary call: its own parameters say what it changes.
  if (!userDefinition && isStructuralMutatorName(call.name) && isTracked(call.args.front(), tracked)) {
    return true;
  }
  const auto parameters = parameters_.find(calleePath);
  if (definitions_.count(calleePath) == 0 || parameters == parameters_.end()) {
    return false;
  }
  for (size_t argIndex = 0; argIndex < call.args.size(); ++argIndex) {
    if (!isTracked(call.args[argIndex], tracked)) {
      continue;
    }
    const size_t parameterIndex = calleeParameterIndex(call, argIndex, parameters->second);
    if (parameterIndex >= parameters->second.size()) {
      continue;
    }
    const ParameterInfo &parameter = parameters->second[parameterIndex];
    if (parameter.binding.isCopy || parameter.binding.isMove) {
      continue;
    }
    // A `mut` parameter may replace or restructure what it borrows. The `this` of a method and a plain `Reference`
    // change it only when their own bodies do.
    if (parameter.name == "this") {
      if (changesStructurally(calleePath, parameterIndex)) {
        return true;
      }
      continue;
    }
    if (parameter.binding.typeName == "Reference") {
      if (parameter.binding.isMutable || changesStructurally(calleePath, parameterIndex)) {
        return true;
      }
      continue;
    }
    if (parameter.binding.typeName != "Pointer" && parameter.binding.isMutable) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
