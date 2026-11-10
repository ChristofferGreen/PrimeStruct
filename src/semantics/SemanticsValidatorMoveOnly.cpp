#include "SemanticsValidator.h"

namespace primec::semantics {

std::string SemanticsValidator::moveOnlyPlaceType(const Expr &value,
                                                  const std::vector<ParameterInfo> &params,
                                                  const std::unordered_map<std::string, BindingInfo> &locals) {
  const bool isPlace = value.kind == Expr::Kind::Name || (value.kind == Expr::Kind::Call && value.isFieldAccess);
  if (!isPlace || currentValidationState_.context.definitionIsUnsafe) {
    return {};
  }
  // Generic code (every template instance) and the stdlib copy values of any type bitwise by design; the rule is for
  // concrete user code.
  const std::string &definitionPath = currentValidationState_.context.definitionPath;
  if (definitionPath.rfind("/std/", 0) == 0 || definitionPath.find("__t") != std::string::npos) {
    return {};
  }
  const std::string structPath = inferStructReturnPath(value, params, locals);
  if (structPath.empty() || structNames_.count(structPath) == 0) {
    return {};
  }
  if (defMap_.count(structPath + "/Destroy") == 0 || defMap_.count(structPath + "/Copy") != 0) {
    return {};
  }
  return structPath;
}

bool SemanticsValidator::failIfMoveOnlyCopy(const Expr &diagnosticExpr,
                                            const Expr &value,
                                            const std::vector<ParameterInfo> &params,
                                            const std::unordered_map<std::string, BindingInfo> &locals) {
  const std::string moveOnlyType = moveOnlyPlaceType(value, params, locals);
  if (moveOnlyType.empty()) {
    return false;
  }
  (void)failExprDiagnostic(diagnosticExpr,
                           "move-only type " + moveOnlyType + " cannot be copied: use move(" +
                               (value.kind == Expr::Kind::Name ? value.name : std::string("...")) + ")");
  return true;
}

std::string SemanticsValidator::useAfterMoveMessage(
    const std::string &name,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  std::string message = "use-after-move: " + name;
  const auto site = currentValidationState_.moveArgumentSites.find(name);
  if (site != currentValidationState_.moveArgumentSites.end() && site->second.line > 0) {
    message += " was moved at " + std::to_string(site->second.line) + ":" +
               std::to_string(site->second.column);
  } else {
    message += " was moved";
  }
  message += "; assign " + name + " a new value before using it again";
  const BindingInfo *binding = findParamBinding(params, name);
  if (binding == nullptr) {
    if (const auto it = locals.find(name); it != locals.end()) {
      binding = &it->second;
    }
  }
  Expr place;
  place.kind = Expr::Kind::Name;
  place.name = name;
  if (binding != nullptr && moveOnlyPlaceType(place, params, locals).empty()) {
    const std::string typeText = binding->typeTemplateArg.empty()
                                     ? binding->typeName
                                     : binding->typeName + "<" + binding->typeTemplateArg + ">";
    message += ", or keep a copy before the move: [" + typeText + " mut] kept{" + name + "}";
  }
  return message;
}

bool SemanticsValidator::recordMovedBinding(const Expr &site, const std::string &name) {
  auto &moveSites = currentValidationState_.moveArgumentSites;
  currentValidationState_.movedBindings.insert(name);
  moveSites[name] = {&site, site.sourceLine, site.sourceColumn};
  return true;
}

bool SemanticsValidator::movedAtOtherSite(const std::string &name, const Expr &site) const {
  if (currentValidationState_.movedBindings.count(name) == 0) {
    return false;
  }
  const auto moved = currentValidationState_.moveArgumentSites.find(name);
  return moved == currentValidationState_.moveArgumentSites.end() || !moved->second.matches(site);
}

bool SemanticsValidator::checkBuiltinVectorMutation(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.args.empty() ||
      expr.args.size() > 2 || currentValidationState_.context.definitionIsUnsafe) {
    return true;
  }
  for (const auto &argName : expr.argNames) {
    if (argName.has_value()) {
      return true;
    }
  }
  const size_t leafStart = expr.name.rfind('/');
  const std::string leaf =
      leafStart == std::string::npos ? expr.name : expr.name.substr(leafStart + 1);
  const bool storesArgument = leaf == "push" && expr.args.size() == 2;
  if (!storesArgument && leaf != "pop" && leaf != "clear" && leaf != "reserve" &&
      leaf != "remove_at" && leaf != "remove_swap") {
    return true;
  }
  std::string receiverTypeText;
  if (!inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText)) {
    return true;
  }
  std::string receiverBase;
  std::string receiverArg;
  if (!splitTemplateTypeName(
          normalizeBindingTypeName(receiverTypeText), receiverBase, receiverArg) ||
      normalizeBindingTypeName(receiverBase) != "vector") {
    return true;
  }
  // Changing the vector can move or free the storage a view of it (or of one of its elements)
  // reads, so it waits until the view's last use (docs/spec/value-lifecycle.md, Copies).
  if (const Expr *root = containerPlaceRoot(expr.args.front()); root->kind == Expr::Kind::Name) {
    const std::string view = liveContainerViewOf(params, locals, root->name);
    if (!view.empty()) {
      return failExprDiagnostic(expr,
                                "borrowed binding: " + root->name + " (root: " + root->name +
                                    ", sink: " + view + "); " + root->name +
                                    " cannot change while " + view + " still reads it: declare " +
                                    view + " `mut` to give it its own copy");
    }
  }
  if (!storesArgument || expr.args[1].kind != Expr::Kind::Name) {
    return true;
  }
  // A named owner is moved into the vector, like a `move` argument; a borrowed parameter or a view
  // is copied (it owns nothing to hand over).
  const Expr &value = expr.args[1];
  const BindingInfo *binding = findParamBinding(params, value.name);
  if (binding != nullptr) {
    if (!binding->isCopy && !binding->isMove) {
      return true;
    }
  } else if (const auto it = locals.find(value.name); it != locals.end()) {
    binding = &it->second;
  }
  if (binding == nullptr || binding->isContainerView || binding->typeName == "Reference" ||
      !bindingOwnsResources(*binding, expr.namespacePrefix)) {
    return true;
  }
  if (movedAtOtherSite(value.name, value)) {
    return failExprDiagnostic(value, useAfterMoveMessage(value.name, params, locals));
  }
  return recordMovedBinding(value, value.name);
}

} // namespace primec::semantics
