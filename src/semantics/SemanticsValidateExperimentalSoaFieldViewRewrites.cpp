#include "SemanticsValidateExperimentalSoaFieldViewRewrites.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateSoaRewriteSupport.h"
#include "SemanticsValidateExperimentalSoaMethodRewrites.h"
#include "SemanticsValidateSoaBindingExtraction.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CompileArena.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec {

void rewriteExperimentalSoaFieldViewIndexExpr(
    Expr &expr,
    const SoaBindingMap &bindings,
    const ExperimentalSoaRewriteScope &scope,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames) {
  const SoaBindingMap &allBindings = scope.allBindings;
  const SoaBindingMap &soaCollectionReturnDefinitions =
      scope.program.soaCollectionReturnDefinitions;
  const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes =
      scope.program.specializedSoaVectorElementTypes;
  const std::unordered_set<std::string> &structPaths = scope.program.structPaths;
  const std::unordered_set<std::string> &visibleSoaFieldHelpers =
      scope.program.visibleSoaFieldHelpers;
  const std::string &definitionNamespace = scope.definitionNamespace;
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewIndexExpr(arg, bindings, scope, structFieldNames);
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      expr.templateArgs.size() != 0 || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || semantics::hasNamedArguments(expr.argNames) ||
      expr.args.size() != 2) {
    return;
  }

  std::string builtinAccessName;
  if (!semantics::getBuiltinArrayAccessName(expr, builtinAccessName) ||
      builtinAccessName != "at") {
    return;
  }

  const Expr &fieldViewExpr = expr.args.front();
  if (fieldViewExpr.kind != Expr::Kind::Call || fieldViewExpr.isBinding ||
      fieldViewExpr.name.empty() ||
      fieldViewExpr.name.find('/') != std::string::npos ||
      !fieldViewExpr.templateArgs.empty() || fieldViewExpr.hasBodyArguments ||
      !fieldViewExpr.bodyArguments.empty() ||
      semantics::hasNamedArguments(fieldViewExpr.argNames) ||
      fieldViewExpr.args.size() != 1) {
    return;
  }

  if (visibleSoaFieldHelpers.count(collection_helpers::kRootedSoaPrefix + fieldViewExpr.name) > 0) {
    return;
  }

  std::string receiverElemType;
  bool receiverNeedsDereference = false;
  bool receiverUsesCanonicalSoaVector = false;
  const Expr &receiver = fieldViewExpr.args.front();
  std::optional<Expr> canonicalReceiverExpr;
  const Expr *getReceiverExpr = &receiver;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    receiverNeedsDereference =
        semantics::normalizeBindingTypeName(binding.typeName) == "Reference" ||
        semantics::normalizeBindingTypeName(binding.typeName) == "Pointer";
    auto markCanonicalSoaVector = [&](std::string typeText) {
      while (true) {
        std::string base;
        std::string argText;
        if (!semantics::splitTemplateTypeName(
                semantics::normalizeBindingTypeName(typeText), base, argText)) {
          base = semantics::normalizeBindingTypeName(typeText);
        } else {
          base = semantics::normalizeBindingTypeName(base);
        }
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) ||
              args.size() != 1) {
            return;
          }
          typeText = args.front();
          continue;
        }
        receiverUsesCanonicalSoaVector =
            semantics::isInternalOrExperimentalSoaStorageTypePath(base);
        return;
      }
    };
    markCanonicalSoaVector(
        binding.typeTemplateArg.empty()
            ? binding.typeName
            : binding.typeName + "<" + binding.typeTemplateArg + ">");
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, specializedSoaVectorElementTypes, receiverElemType);
  };
  auto candidatePathsForCall = [&](const Expr &callExpr) {
    return candidatePathsForExprCall(callExpr, definitionNamespace, &allBindings, &structPaths);
  };
  auto tryLocationReceiverBinding = [&](const Expr &locationExpr) -> bool {
    if (!semantics::isSimpleCallName(locationExpr, "location") ||
        locationExpr.args.size() != 1) {
      return false;
    }
    const Expr &locationTarget = locationExpr.args.front();
    if (locationTarget.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(locationTarget.name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
      auto allBindingIt = allBindings.find(locationTarget.name);
      if (allBindingIt != allBindings.end() &&
          tryReceiverBinding(allBindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
    } else if (locationTarget.kind == Expr::Kind::Call && !locationTarget.isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(locationTarget)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(locationTarget, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          return true;
        }
      }
    }
    return false;
  };
  if (const auto normalizedReceiver = normalizeExperimentalSoaBorrowedHelperReceiver(
          receiver, bindings, soaCollectionReturnDefinitions, definitionNamespace, structPaths);
      normalizedReceiver.has_value()) {
    canonicalReceiverExpr = *normalizedReceiver;
    getReceiverExpr = &*canonicalReceiverExpr;
    const Expr *normalizedBindingSource = getReceiverExpr;
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      receiverNeedsDereference = true;
      normalizedBindingSource = &getReceiverExpr->args.front();
    }
    if (normalizedBindingSource->kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(normalizedBindingSource->name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        // handled
      } else {
        auto allBindingIt = allBindings.find(normalizedBindingSource->name);
        if (allBindingIt != allBindings.end()) {
          tryReceiverBinding(allBindingIt->second);
        }
      }
    } else if (normalizedBindingSource->kind == Expr::Kind::Call &&
               !normalizedBindingSource->isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(*normalizedBindingSource)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          break;
        }
      }
    }
  } else if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      // handled
    } else {
      auto allBindingIt = allBindings.find(receiver.name);
      if (allBindingIt != allBindings.end()) {
        tryReceiverBinding(allBindingIt->second);
      }
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (!tryLocationReceiverBinding(receiver) &&
        semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (tryLocationReceiverBinding(borrowedSource)) {
        // handled
      } else if (borrowedSource.kind == Expr::Kind::Name) {
        const std::string &sourceName = borrowedSource.name;
        auto bindingIt = bindings.find(sourceName);
        if (bindingIt != bindings.end()) {
          tryReceiverBinding(bindingIt->second);
        }
        if (receiverElemType.empty()) {
          auto allBindingIt = allBindings.find(sourceName);
          if (allBindingIt != allBindings.end()) {
            tryReceiverBinding(allBindingIt->second);
          }
        }
      } else if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding) {
        for (const std::string &candidatePath : candidatePathsForCall(borrowedSource)) {
          auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
          if (returnIt != soaCollectionReturnDefinitions.end() &&
              tryReceiverBinding(returnIt->second)) {
            canonicalReceiverExpr = canonicalizeResolvedCallPath(borrowedSource, candidatePath);
            getReceiverExpr = &*canonicalReceiverExpr;
            break;
          }
        }
      }
    }
    if (receiverElemType.empty()) {
      for (const std::string &candidatePath : candidatePathsForCall(receiver)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          break;
        }
      }
    }
  }
  if (receiverElemType.empty()) {
    return;
  }

  const std::string normalizedElemType =
      semantics::normalizeBindingTypeName(receiverElemType);
  if (normalizedElemType.empty()) {
    return;
  }
  const std::string lookupNamespace =
      !getReceiverExpr->namespacePrefix.empty() ? getReceiverExpr->namespacePrefix : definitionNamespace;
  const std::string elementStructPath =
      semantics::resolveStructTypePath(normalizedElemType, lookupNamespace, structPaths);
  auto fieldIt = structFieldNames.find(elementStructPath);
  if (elementStructPath.empty() || fieldIt == structFieldNames.end() ||
      fieldIt->second.count(fieldViewExpr.name) == 0) {
    return;
  }

  Expr getCall;
  getCall.kind = Expr::Kind::Call;
  const bool useBorrowedGetHelper = receiverNeedsDereference;
  const std::string getHelperName = useBorrowedGetHelper ? collection_helpers::kGetRef : "get";
  getCall.name = receiverUsesCanonicalSoaVector
                     ? semantics::publicSoaHelperTargetPath(getHelperName)
                     : semantics::compatibilitySoaHelperTargetPath(getHelperName);
  getCall.templateArgs = {receiverElemType};
  auto appendReceiverValueExpr = [&](Expr &callExpr) {
    if (!useBorrowedGetHelper) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      callExpr.args.push_back(getReceiverExpr->args.front());
      return;
    }
    callExpr.args.push_back(*getReceiverExpr);
  };
  appendReceiverValueExpr(getCall);
  getCall.args.push_back(expr.args[1]);
  getCall.argNames.resize(getCall.args.size());
  getCall.sourceLine = expr.sourceLine;
  getCall.sourceColumn = expr.sourceColumn;

  expr = {};
  expr.kind = Expr::Kind::Call;
  expr.name = fieldViewExpr.name;
  expr.isMethodCall = true;
  expr.isFieldAccess = true;
  expr.args.push_back(std::move(getCall));
  expr.argNames.push_back(std::nullopt);
  expr.sourceLine = fieldViewExpr.sourceLine;
  expr.sourceColumn = fieldViewExpr.sourceColumn;
}

bool rewriteExperimentalSoaFieldViewIndexes(Program &program, std::string &error) {
  error.clear();
  const ExperimentalSoaProgramData data = collectExperimentalSoaProgramData(program);
  std::unordered_map<std::string, std::unordered_set<std::string>> structFieldNames;
  for (const Definition &def : program.definitions) {
    if (!semantics::isStructLikeDefinition(def)) {
      continue;
    }
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    std::unordered_set<std::string> fieldNames;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticField(stmt)) {
        continue;
      }
      fieldNames.insert(stmt.name);
    }
    if (fieldNames.empty()) {
      continue;
    }
    structFieldNames.emplace(def.fullPath, std::move(fieldNames));
  }
  rewriteExperimentalSoaDefinitions(
      program,
      data,
      [&](Expr &expr, const SoaBindingMap &bindings, const ExperimentalSoaRewriteScope &scope) {
        rewriteExperimentalSoaFieldViewIndexExpr(expr, bindings, scope, structFieldNames);
      });
  return true;
}

} // namespace primec
