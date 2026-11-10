#include "SemanticsValidator.h"

#include "SemanticsSendable.h"
#include "SemanticsValidateLambdaClosures.h"

#include <algorithm>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace primec::semantics {
namespace {

std::string task_binding_type_text(const BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool is_task_spawn_transform_name(const std::string &name) {
  std::string normalized = name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "spawn";
}

Expr without_task_spawn_transform(Expr expr) {
  expr.transforms.erase(
      std::remove_if(expr.transforms.begin(),
                     expr.transforms.end(),
                     [](const Transform &transform) {
        return is_task_spawn_transform_name(transform.name);
      }),
      expr.transforms.end());
  return expr;
}

bool has_task_spawn_transform(const Expr &expr) {
  return std::any_of(expr.transforms.begin(),
                     expr.transforms.end(),
                     [](const Transform &transform) {
    return is_task_spawn_transform_name(transform.name);
  });
}

bool is_stdlib_tuple_constructor_expr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding ||
      expr.isMethodCall || expr.isFieldAccess) {
    return false;
  }
  const std::string normalized = normalizeBindingTypeName(expr.name);
  return normalized == "/std/tuple/tuple" ||
         normalized.rfind("/std/tuple/tuple__t", 0) == 0;
}

bool is_multi_wait_tuple_materialization_expr(const Expr &expr) {
  std::string sourceName = expr.sourceName.empty() ? expr.name : expr.sourceName;
  if (!sourceName.empty() && sourceName.front() == '/') {
    sourceName.erase(sourceName.begin());
  }
  return sourceName == "wait" && is_stdlib_tuple_constructor_expr(expr);
}

struct TaskCarrierInitializer {
  const Expr *expr = nullptr;
  bool hasSpawnMarker = false;
  bool malformed = false;
};

TaskCarrierInitializer task_carrier_initializer(const Expr &expr) {
  bool hasNamedArg = false;
  bool hasSpawnLabel = false;
  for (const auto &argName : expr.argNames) {
    if (!argName.has_value()) {
      continue;
    }
    hasNamedArg = true;
    if (is_task_spawn_transform_name(*argName) && !hasSpawnLabel) {
      hasSpawnLabel = true;
      continue;
    }
    return {.malformed = true};
  }
  if (hasNamedArg && (!hasSpawnLabel || expr.args.size() != 1 ||
                      expr.argNames.size() != 1 || expr.hasBodyArguments ||
                      !expr.bodyArguments.empty())) {
    return {.malformed = true};
  }

  const Expr *initializer = nullptr;
  if (expr.args.size() == 1 && !expr.hasBodyArguments &&
      expr.bodyArguments.empty()) {
    initializer = &expr.args.front();
  } else if (expr.args.empty() && expr.hasBodyArguments &&
             expr.bodyArguments.size() == 1) {
    initializer = &expr.bodyArguments.front();
  } else if (expr.args.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty()) {
    return {};
  } else {
    return {.malformed = true};
  }

  return {.expr = initializer,
          .hasSpawnMarker = hasSpawnLabel || has_task_spawn_transform(*initializer),
          .malformed = false};
}

const BindingInfo *task_lookup_binding(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &name) {
  return findBinding(params, locals, name);
}

} // namespace

bool SemanticsValidator::exprHasExecutionTransform(const Expr &expr,
                                                   std::string_view name) const {
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return false;
  }
  return std::any_of(expr.transforms.begin(),
                     expr.transforms.end(),
                     [&](const Transform &transform) {
    return name == "spawn"
               ? is_task_spawn_transform_name(transform.name)
               : transform.name == name;
  });
}

bool SemanticsValidator::isTaskSpawnExpr(const Expr &expr) const {
  return exprHasExecutionTransform(expr, "spawn");
}

bool SemanticsValidator::isTaskWaitExpr(const Expr &expr) const {
  return expr.kind == Expr::Kind::Call && !expr.isBinding &&
         !expr.isMethodCall && !expr.isFieldAccess &&
         expr.transforms.empty() && isSimpleCallName(expr, "wait");
}

bool SemanticsValidator::isTaskTypeCarrierExpr(const Expr &expr) const {
  return expr.kind == Expr::Kind::Call && !expr.isBinding &&
         !expr.isMethodCall && !expr.isFieldAccess &&
         expr.transforms.empty() && isSimpleCallName(expr, "Task");
}

bool SemanticsValidator::isTaskBinding(const BindingInfo &binding,
                                       std::string *resultTypeOut) const {
  std::string base = normalizeBindingTypeName(binding.typeName);
  std::string arg = binding.typeTemplateArg;
  if (arg.empty()) {
    std::string splitBase;
    std::string splitArg;
    if (splitTemplateTypeName(base, splitBase, splitArg)) {
      base = normalizeBindingTypeName(splitBase);
      arg = splitArg;
    }
  }
  if (base != "Task" || arg.empty()) {
    return false;
  }
  if (resultTypeOut != nullptr) {
    *resultTypeOut = arg;
  }
  return true;
}

ReturnKind SemanticsValidator::taskResultReturnKind(
    std::string_view resultType,
    const std::string &namespacePrefix) const {
  const std::string normalized = normalizeBindingTypeName(std::string(resultType));
  ReturnKind kind = returnKindForTypeName(normalized);
  if (kind != ReturnKind::Unknown) {
    return kind;
  }
  if (!normalizeCollectionTypePath(normalized).empty() ||
      !resolveStructTypePath(normalized, namespacePrefix, structNames_).empty()) {
    return ReturnKind::Array;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(normalized, base, arg)) {
    base = normalizeBindingTypeName(base);
    if (base == "array" || base == "vector" || base == "soa" ||
        base == "Buffer" || base == "Result" || base == "Maybe" ||
        base == "tuple" || base == "std/tuple/tuple" ||
        base == "/std/tuple/tuple" || isKeyValueSurfaceTypeName(base)) {
      return ReturnKind::Array;
    }
  }
  return ReturnKind::Unknown;
}

bool SemanticsValidator::inferTaskSpawnCallBinding(
    const Expr &spawnedCall,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) {
  BindingInfo resultBinding;
  // A callee that declares return<void> is void whatever its body holds: initializer inference would
  // answer with the struct of a lambda the body creates.
  auto declaresVoidReturn = [&]() {
    const auto defIt = defMap_.find(resolveCalleePath(spawnedCall));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        return normalizeBindingTypeName(transform.templateArgs.front()) == "void";
      }
    }
    return false;
  };
  if (declaresVoidReturn()) {
    resultBinding.typeName = "void";
  } else if (!inferCallInitializerBinding(spawnedCall, params, locals, resultBinding)) {
    std::string resolvedPath = resolveCalleePath(spawnedCall);
    const std::string concretePath =
        resolveExprConcreteCallPath(params, locals, spawnedCall, resolvedPath);
    if (!concretePath.empty()) {
      resolvedPath = concretePath;
    }
    auto defIt = defMap_.find(resolvedPath);
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      (void)inferDefinitionReturnBinding(*defIt->second, resultBinding);
    }
  }
  std::string resultType = task_binding_type_text(resultBinding);
  if (resultType.empty()) {
    ReturnKind kind = inferExprReturnKind(spawnedCall, params, locals);
    if (kind != ReturnKind::Unknown && kind != ReturnKind::Void) {
      resultType = typeNameForReturnKind(kind);
    }
  }
  if (resultType.empty()) {
    return false;
  }
  bindingOut.typeName = "Task";
  bindingOut.typeTemplateArg = resultType;
  return true;
}

bool SemanticsValidator::inferTaskSpawnBinding(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) {
  if (isTaskTypeCarrierExpr(expr)) {
    TaskCarrierInitializer initializer = task_carrier_initializer(expr);
    if (initializer.malformed || initializer.expr == nullptr ||
        !initializer.hasSpawnMarker || initializer.expr->kind != Expr::Kind::Call ||
        initializer.expr->isBinding || expr.templateArgs.size() != 1) {
      return false;
    }
    Expr spawnedCall = without_task_spawn_transform(*initializer.expr);
    BindingInfo inferredTaskBinding;
    if (!inferTaskSpawnCallBinding(spawnedCall, params, locals,
                                   inferredTaskBinding) ||
        !errorTypesMatch(expr.templateArgs.front(),
                         inferredTaskBinding.typeTemplateArg,
                         expr.namespacePrefix)) {
      return false;
    }
    bindingOut = std::move(inferredTaskBinding);
    return true;
  }
  if (!isTaskSpawnExpr(expr)) {
    return false;
  }
  Expr spawnedCall = without_task_spawn_transform(expr);
  return inferTaskSpawnCallBinding(spawnedCall, params, locals, bindingOut);
}

bool SemanticsValidator::inferTaskWaitBinding(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) const {
  if (!isTaskWaitExpr(expr) || expr.args.empty()) {
    return false;
  }
  std::vector<std::string> resultTypes;
  resultTypes.reserve(expr.args.size());
  for (const Expr &arg : expr.args) {
    if (arg.kind != Expr::Kind::Name) {
      return false;
    }
    const BindingInfo *taskBinding = task_lookup_binding(params, locals, arg.name);
    std::string resultType;
    if (taskBinding == nullptr || !isTaskBinding(*taskBinding, &resultType)) {
      return false;
    }
    resultTypes.push_back(std::move(resultType));
  }
  if (resultTypes.size() > 1) {
    bindingOut.typeName = "/std/tuple/tuple";
    bindingOut.typeTemplateArg = joinTemplateArgs(resultTypes);
    return true;
  }
  std::string resultType = std::move(resultTypes.front());
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(normalizeBindingTypeName(resultType), base, arg) &&
      !base.empty()) {
    bindingOut.typeName = normalizeBindingTypeName(base);
    bindingOut.typeTemplateArg = arg;
    return true;
  }
  bindingOut.typeName = normalizeBindingTypeName(resultType);
  bindingOut.typeTemplateArg.clear();
  return true;
}

bool SemanticsValidator::validateTaskSpawnExpr(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::vector<Expr> *enclosingStatements,
    size_t statementIndex) {
  for (const Transform &transform : expr.transforms) {
    if (!is_task_spawn_transform_name(transform.name)) {
      continue;
    }
    if (!transform.templateArgs.empty() || !transform.arguments.empty()) {
      return failExprDiagnostic(expr, "spawn transform does not accept arguments");
    }
  }
  if (currentValidationState_.context.activeEffects.count("task") == 0) {
    return failExprDiagnostic(expr, "spawn requires task effect");
  }
  Expr spawnedCall = without_task_spawn_transform(expr);
  return validateTaskSpawnCall(params,
                               locals,
                               expr,
                               spawnedCall,
                               enclosingStatements,
                               statementIndex);
}

bool SemanticsValidator::validateTaskSpawnCall(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &diagnosticExpr,
    const Expr &spawnedCall,
    const std::vector<Expr> *enclosingStatements,
    size_t statementIndex) {
  if (currentValidationState_.context.activeEffects.count("task") == 0) {
    return failExprDiagnostic(diagnosticExpr, "spawn requires task effect");
  }
  // A spawn argument is copied into the task, as in any call, so a `mut` binding is an ordinary argument; what may
  // cross is decided by type below. A task handle is rejected by name first: its state is tracked per spawning frame.
  for (const Expr &arg : spawnedCall.args) {
    if (arg.kind != Expr::Kind::Name) {
      continue;
    }
    const BindingInfo *binding = task_lookup_binding(params, locals, arg.name);
    if (binding != nullptr && isTaskBinding(*binding)) {
      return failExprDiagnostic(
          arg, "task handles cannot be captured by spawned task: " + arg.name);
    }
  }

  if (!validateExpr(params, locals, spawnedCall, enclosingStatements, statementIndex)) {
    return false;
  }
  if (!validateTaskSpawnArgumentsSendable(params, locals, spawnedCall)) {
    return false;
  }
  BindingInfo taskBinding;
  if (!inferTaskSpawnCallBinding(spawnedCall, params, locals, taskBinding)) {
    return failExprDiagnostic(diagnosticExpr, "spawn requires call target with inferred return type");
  }
  return true;
}

std::string SemanticsValidator::diagnosticTypeText(const std::string &typeText) {
  const std::string normalized = normalizeBindingTypeName(typeText);
  auto joinArgs = [&](const std::vector<std::string> &args) {
    std::string text;
    for (size_t i = 0; i < args.size(); ++i) {
      text += (i == 0 ? "" : ", ") + diagnosticTypeText(args[i]);
    }
    return text;
  };
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalized, base, argText)) {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(argText, args)) {
      return diagnosticTypeText(base) + "<" + joinArgs(args) + ">";
    }
    return normalized;
  }
  std::string path = normalized;
  if (defMap_.count(path) == 0) {
    path = resolveStructTypePath(normalized, "", structNames_);
  }
  if (path.find(LambdaClosureStructPrefix) != std::string::npos) {
    return "closure";
  }
  // Generated types read as written: `/__Function1__t...` and `__Function1` are `Function`.
  if (const auto defIt = defMap_.find(path); defIt != defMap_.end() && defIt->second != nullptr &&
                                              !defIt->second->instanceOfTemplate.empty()) {
    const Definition &def = *defIt->second;
    const size_t slash = def.instanceOfTemplate.find_last_of('/');
    return userFacingLoweredTypeLeaf(
               def.instanceOfTemplate.substr(slash == std::string::npos ? 0 : slash + 1)) +
           "<" + joinArgs(def.instanceTemplateArgs) + ">";
  }
  if (!normalized.empty() && normalized.front() == '/') {
    return userFacingLoweredTypeLeaf(normalized.substr(normalized.find_last_of('/') + 1));
  }
  return userFacingLoweredTypeLeaf(normalized);
}

std::string SemanticsValidator::diagnosticDefinitionText(const std::string &path) {
  const auto defIt = defMap_.find(path);
  if (defIt == defMap_.end() || defIt->second == nullptr || defIt->second->instanceOfTemplate.empty()) {
    return path;
  }
  const Definition &def = *defIt->second;
  std::string text = def.instanceOfTemplate + "<";
  for (size_t i = 0; i < def.instanceTemplateArgs.size(); ++i) {
    text += (i == 0 ? "" : ", ") + diagnosticTypeText(def.instanceTemplateArgs[i]);
  }
  return text + ">";
}

std::string SemanticsValidator::diagnosticInstantiationChain(const std::string &path) {
  std::string chain;
  std::unordered_set<std::string> seen;
  std::string current = path;
  while (seen.insert(current).second) {
    const auto defIt = defMap_.find(current);
    if (defIt == defMap_.end() || defIt->second == nullptr || defIt->second->instanceRequestedBy.empty()) {
      break;
    }
    current = defIt->second->instanceRequestedBy;
    chain += (chain.empty() ? "" : ", ") + std::string("instantiated from ") + diagnosticDefinitionText(current);
  }
  return chain.empty() ? chain : " (" + chain + ")";
}

SendableTypeLookup SemanticsValidator::sendableTypeLookup() {
  SendableTypeLookup lookup;
  lookup.resolveDefinition = [this](const std::string &typeText,
                                    const std::string &namespacePrefix) -> const Definition * {
    std::string path = resolveStructTypePath(typeText, namespacePrefix, structNames_);
    if (path.empty()) {
      path = resolveStructTypePath(typeText, namespacePrefix, sumNames_);
    }
    if (path.empty()) {
      return nullptr;
    }
    const auto defIt = defMap_.find(path);
    return defIt == defMap_.end() ? nullptr : defIt->second;
  };
  lookup.fieldType =
      [this](const Definition &structDef, const Expr &fieldStmt, std::string &typeTextOut) {
        if (isCompileTimeTypeBinding(fieldStmt)) {
          return false;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(structDef, fieldStmt, fieldBinding)) {
          return false;
        }
        typeTextOut = task_binding_type_text(fieldBinding);
        return true;
      };
  lookup.display = [this](const std::string &typeText) { return diagnosticTypeText(typeText); };
  return lookup;
}

bool SemanticsValidator::validateFunctionBoxSendable(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isMethodCall || expr.isFieldAccess ||
      expr.args.size() != 1) {
    return true;
  }
  const size_t slash = expr.name.find_last_of('/');
  const std::string_view leaf = slash == std::string::npos
                                    ? std::string_view(expr.name)
                                    : std::string_view(expr.name).substr(slash + 1);
  if (leaf.rfind("__fn_box", 0) != 0) {
    return true;
  }
  const std::string boxPath = resolveCalleePath(expr);
  const auto paramsIt = paramsByDef_.find(boxPath);
  if (paramsIt == paramsByDef_.end() || paramsIt->second.empty()) {
    return true;
  }
  const std::string closureType = task_binding_type_text(paramsIt->second.front().binding);
  if (closureType.empty()) {
    return true;
  }
  const auto defIt = defMap_.find(boxPath);
  const std::string boxNamespace = defIt == defMap_.end() || defIt->second == nullptr
                                       ? std::string()
                                       : defIt->second->namespacePrefix;
  const std::string violation = sendableViolation(closureType, boxNamespace, sendableTypeLookup());
  if (violation.empty()) {
    return true;
  }
  return failExprDiagnostic(expr, "closure cannot be a Function: " + violation);
}

bool SemanticsValidator::validateTaskSpawnArgumentsSendable(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &spawnedCall) {
  std::string calleePath = resolveCalleePath(spawnedCall);
  if (const std::string concretePath = resolveExprConcreteCallPath(params, locals, spawnedCall, calleePath);
      !concretePath.empty()) {
    calleePath = concretePath;
  }
  if (spawnedCall.isMethodCall && !spawnedCall.args.empty()) {
    std::string methodPath;
    bool isBuiltin = false;
    if (resolveMethodTarget(params, locals, spawnedCall.namespacePrefix, spawnedCall.args.front(),
                            spawnedCall.name, methodPath, isBuiltin) &&
        !isBuiltin) {
      calleePath = methodPath;
    }
  }
  const auto paramsIt = paramsByDef_.find(calleePath);
  if (paramsIt == paramsByDef_.end()) {
    return true;
  }
  const std::vector<ParameterInfo> &calleeParams = paramsIt->second;

  const SendableTypeLookup lookup = sendableTypeLookup();

  const std::string calleeNamespace = [&]() -> std::string {
    const auto defIt = defMap_.find(calleePath);
    return defIt == defMap_.end() || defIt->second == nullptr ? std::string() : defIt->second->namespacePrefix;
  }();
  size_t positional = 0;
  for (size_t argIndex = 0; argIndex < spawnedCall.args.size(); ++argIndex) {
    const Expr &arg = spawnedCall.args[argIndex];
    const ParameterInfo *param = nullptr;
    if (argIndex < spawnedCall.argNames.size() && spawnedCall.argNames[argIndex].has_value()) {
      for (const ParameterInfo &candidate : calleeParams) {
        if (candidate.name == *spawnedCall.argNames[argIndex]) {
          param = &candidate;
          break;
        }
      }
    } else if (positional < calleeParams.size()) {
      param = &calleeParams[positional++];
    } else if (!calleeParams.empty()) {
      param = &calleeParams.back();  // further elements of a variadic pack
    }
    if (param == nullptr) {
      continue;
    }
    const bool isPlace = arg.kind == Expr::Kind::Name || (arg.kind == Expr::Kind::Call && arg.isFieldAccess);
    // A method's receiver is the struct itself: its `Reference<Self>` is how the method sees it, not a value the
    // spawner hands over.
    const bool isReceiver = param == &calleeParams.front() && param->name == "this";
    // A borrowed `mut` parameter would write the spawner's place; a task works on copies (`[T copy mut]`).
    if (param->binding.isMutable && !param->binding.isCopy && !param->binding.isMove && isPlace) {
      if (isReceiver) {
        return failExprDiagnostic(arg,
                                  "spawn receiver of " + diagnosticDefinitionText(calleePath) +
                                      " is a mutable borrow of the spawner's value: a task works on copies");
      }
      return failExprDiagnostic(arg,
                                "spawn argument `" + param->name + "` of " + diagnosticDefinitionText(calleePath) +
                                    " is a mutable borrow of the spawner's value: a task works on copies, take it as `[" +
                                    diagnosticTypeText(task_binding_type_text(param->binding)) + " copy mut]`");
    }
    std::string typeText = isReceiver ? param->binding.typeTemplateArg : task_binding_type_text(param->binding);
    std::string packBase;
    std::string packArg;
    if (splitTemplateTypeName(normalizeBindingTypeName(typeText), packBase, packArg) &&
        normalizeBindingTypeName(packBase) == "args") {
      typeText = packArg;
    }
    if (typeText.empty()) {
      continue;
    }
    const std::string violation = sendableViolation(typeText, calleeNamespace, lookup);
    if (violation.empty()) {
      continue;
    }
    const std::string subject = isReceiver ? std::string("spawn receiver of ")
                                           : "spawn argument `" + param->name + "` of ";
    return failExprDiagnostic(arg,
                              subject + diagnosticDefinitionText(calleePath) + " is not sendable: " + violation +
                                  diagnosticInstantiationChain(calleePath) + "; " + SendableSharingNote);
  }
  return true;
}

bool SemanticsValidator::validateTaskWaitExpr(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr) {
  if (currentValidationState_.context.activeEffects.count("task") == 0) {
    return failExprDiagnostic(expr, "wait requires task effect");
  }
  if (expr.args.empty() || hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() ||
      !expr.templateArgs.empty()) {
    return failExprDiagnostic(expr, "wait requires at least one task handle");
  }
  for (const Expr &arg : expr.args) {
    if (arg.kind != Expr::Kind::Name) {
      return failExprDiagnostic(arg, "wait requires a task handle binding");
    }
    const BindingInfo *binding = task_lookup_binding(params, locals, arg.name);
    std::string resultType;
    if (binding == nullptr || !isTaskBinding(*binding, &resultType)) {
      return failExprDiagnostic(arg, "wait requires a task handle binding");
    }
    auto stateIt = currentValidationState_.taskHandles.find(arg.name);
    if (stateIt == currentValidationState_.taskHandles.end()) {
      stateIt = currentValidationState_.taskHandles
                    .emplace(arg.name,
                             ValidationState::TaskHandleState{resultType, true, false})
                    .first;
    }
    if (stateIt->second.waited || !stateIt->second.live) {
      return failExprDiagnostic(arg, "task handle already waited: " + arg.name);
    }
    stateIt->second.live = false;
    stateIt->second.waited = true;
  }
  return true;
}

bool SemanticsValidator::validateTaskTypeCarrierExpr(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::vector<Expr> *enclosingStatements,
    size_t statementIndex) {
  if (expr.templateArgs.size() != 1) {
    return failExprDiagnostic(expr, "Task requires exactly one template argument");
  }
  TaskCarrierInitializer initializer = task_carrier_initializer(expr);
  if (!initializer.malformed && initializer.expr == nullptr) {
    return true;
  }
  if (initializer.malformed || initializer.expr == nullptr ||
      !initializer.hasSpawnMarker || initializer.expr->kind != Expr::Kind::Call ||
      initializer.expr->isBinding) {
    return failExprDiagnostic(expr,
                              "Task<T> carrier only accepts a spawn initializer");
  }
  Expr spawnedCall = without_task_spawn_transform(*initializer.expr);
  if (!validateTaskSpawnCall(params,
                             locals,
                             expr,
                             spawnedCall,
                             enclosingStatements,
                             statementIndex)) {
    return false;
  }
  BindingInfo inferredTaskBinding;
  if (!inferTaskSpawnCallBinding(spawnedCall, params, locals,
                                 inferredTaskBinding) ||
      !errorTypesMatch(expr.templateArgs.front(),
                       inferredTaskBinding.typeTemplateArg,
                       expr.namespacePrefix)) {
    return failExprDiagnostic(expr, "Task<T> initializer type mismatch");
  }
  return true;
}

void SemanticsValidator::recordTaskHandleBinding(const std::string &name,
                                                 const BindingInfo &binding) {
  std::string resultType;
  if (!isTaskBinding(binding, &resultType)) {
    currentValidationState_.taskHandles.erase(name);
    return;
  }
  currentValidationState_.taskHandles[name] =
      ValidationState::TaskHandleState{std::move(resultType), true, false};
}

bool SemanticsValidator::validateTaskHandleArgumentEscapes(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr) {
  if (isTaskWaitExpr(expr)) {
    return true;
  }
  if (is_multi_wait_tuple_materialization_expr(expr)) {
    return validateTaskWaitExpr(params, locals, expr);
  }
  for (const Expr &arg : expr.args) {
    if (arg.kind != Expr::Kind::Name) {
      continue;
    }
    const BindingInfo *binding = task_lookup_binding(params, locals, arg.name);
    if (binding != nullptr && isTaskBinding(*binding)) {
      return failExprDiagnostic(
          arg, "task handles cannot escape their spawning function: " + arg.name);
    }
  }
  return true;
}

bool SemanticsValidator::validateTaskHandleReturnEscape(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &returnExpr,
    const Expr &returnStmt) {
  if (returnExpr.kind != Expr::Kind::Name) {
    return true;
  }
  const BindingInfo *binding =
      task_lookup_binding(params, locals, returnExpr.name);
  if (binding == nullptr || !isTaskBinding(*binding)) {
    return true;
  }
  return failExprDiagnostic(
      returnStmt,
      "task handles cannot escape their spawning function: " +
          returnExpr.name);
}

bool SemanticsValidator::validateNoLiveTaskHandlesAtReturn(
    const Expr &returnStmt) {
  std::vector<std::string> liveNames;
  for (const auto &[name, state] : currentValidationState_.taskHandles) {
    if (state.live && !state.waited) {
      liveNames.push_back(name);
    }
  }
  std::sort(liveNames.begin(), liveNames.end());
  if (liveNames.empty()) {
    return true;
  }
  return failExprDiagnostic(returnStmt,
                            "task handle must be waited before return: " +
                                liveNames.front());
}

bool SemanticsValidator::validateNoLiveTaskHandlesAtDefinitionEnd() {
  std::vector<std::string> liveNames;
  for (const auto &[name, state] : currentValidationState_.taskHandles) {
    if (state.live && !state.waited) {
      liveNames.push_back(name);
    }
  }
  std::sort(liveNames.begin(), liveNames.end());
  if (liveNames.empty()) {
    return true;
  }
  if (currentDefinitionContext_ != nullptr) {
    return failDefinitionDiagnostic(
        *currentDefinitionContext_,
        "task handle must be waited before definition end: " +
            liveNames.front());
  }
  return failUncontextualizedDiagnostic(
      "task handle must be waited before definition end: " + liveNames.front());
}

} // namespace primec::semantics
