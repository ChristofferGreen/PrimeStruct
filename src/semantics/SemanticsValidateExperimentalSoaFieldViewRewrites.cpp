// soa-surface-audit: exempt
#include "SemanticsValidateExperimentalSoaFieldViewRewrites.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateBuiltinSoaMetadata.h"
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

namespace primec {

void rewriteExperimentalSoaFieldViewIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewIndexStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewIndexExpr(
        stmt,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaFieldViewIndexStatements(
          stmt.bodyArguments,
          bodyBindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldNames,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding =
              extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
          binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewIndexExpr(
        arg,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
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

  if (visibleSoaFieldHelpers.count("/soa/" + fieldViewExpr.name) > 0) {
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
  const std::string getHelperName = useBorrowedGetHelper ? "get_ref" : "get";
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

  std::unordered_map<std::string, std::unordered_set<std::string>> structFieldNames;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaFieldHelpers;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  const auto specializedSoaVectorElementTypes =
      buildSpecializedExperimentalSoaVectorElementTypes(program);

  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind("/std/collections/soa/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string("/std/collections/soa/").size());
      visibleSoaFieldHelpers.insert("/soa/" + helperSuffix);
    }
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (!semantics::isStructLikeDefinition(def)) {
      continue;
    }
    structPaths.insert(def.fullPath);
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

  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    auto allBindings = bindings;
    std::function<void(const std::vector<Expr> &)> collectBindings =
        [&](const std::vector<Expr> &statements) {
          for (const Expr &stmt : statements) {
            if (stmt.isBinding) {
              if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
                allBindings[stmt.name] = *binding;
              }
            }
            if (!stmt.bodyArguments.empty()) {
              collectBindings(stmt.bodyArguments);
            }
          }
        };
    collectBindings(def.statements);
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewIndexStatements(
        def.statements,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewIndexExpr(
          *def.returnExpr,
          bindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldNames,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewHelperExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewHelperStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewHelperExpr(
        stmt,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaFieldViewHelperStatements(
          stmt.bodyArguments,
          bodyBindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldInfo,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewHelperExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewHelperExpr(
        arg,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.isBinding ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      !expr.templateArgs.empty()) {
    return;
  }

  std::string fieldName;
  if (!expr.name.empty() && expr.name.front() == '/' &&
      semantics::splitSoaFieldViewHelperPath(expr.name, &fieldName)) {
  } else {
    if (!expr.namespacePrefix.empty()) {
      return;
    }
    fieldName = expr.name;
    if (!fieldName.empty() && fieldName.front() == '/') {
      fieldName.erase(fieldName.begin());
    }
    if (fieldName.empty() || fieldName.find('/') != std::string::npos ||
        fieldName == "count" || fieldName == "count_ref" ||
        fieldName == "get" || fieldName == "get_ref" ||
        fieldName == "ref" || fieldName == "ref_ref" ||
        fieldName == "to_soa" || fieldName == "to_aos" ||
        fieldName == "to_aos_ref") {
      return;
    }
  }

  if (visibleSoaFieldHelpers.count("/soa/" + fieldName) > 0) {
    return;
  }
  if (expr.args.size() != 1) {
    return;
  }

  std::string receiverElemType;
  bool receiverNeedsDereference = false;
  bool receiverUsesCanonicalSoaVector = false;
  const Expr &receiver = expr.args.front();
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
  auto structIt = structFieldInfo.find(elementStructPath);
  if (elementStructPath.empty() || structIt == structFieldInfo.end()) {
    return;
  }
  auto fieldIt = structIt->second.find(fieldName);
  if (fieldIt == structIt->second.end()) {
    return;
  }

  Expr fieldViewCall;
  fieldViewCall.kind = Expr::Kind::Call;
  fieldViewCall.name = receiverUsesCanonicalSoaVector
                           ? "/std/collections/soa/field_view"
                           : collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorFieldView");
  fieldViewCall.templateArgs = {receiverElemType, fieldIt->second.typeText};
  auto appendReceiverValueExpr = [&](Expr &callExpr) {
    if (!receiverNeedsDereference) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    Expr dereferenceCall;
    dereferenceCall.kind = Expr::Kind::Call;
    dereferenceCall.name = "dereference";
    dereferenceCall.args.push_back(*getReceiverExpr);
    dereferenceCall.argNames.resize(dereferenceCall.args.size());
    dereferenceCall.sourceLine = getReceiverExpr->sourceLine;
    dereferenceCall.sourceColumn = getReceiverExpr->sourceColumn;
    callExpr.args.push_back(std::move(dereferenceCall));
  };
  appendReceiverValueExpr(fieldViewCall);
  fieldViewCall.args.push_back(makeI32LiteralExpr(
      static_cast<uint64_t>(fieldIt->second.index),
      expr.sourceLine,
      expr.sourceColumn));
  fieldViewCall.argNames.resize(fieldViewCall.args.size());
  fieldViewCall.sourceLine = expr.sourceLine;
  fieldViewCall.sourceColumn = expr.sourceColumn;

  expr = std::move(fieldViewCall);
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.namespacePrefix.clear();
}

bool rewriteExperimentalSoaFieldViewHelpers(Program &program, std::string &error) {
  error.clear();

  std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>> structFieldInfo;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaFieldHelpers;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  const auto specializedSoaVectorElementTypes =
      buildSpecializedExperimentalSoaVectorElementTypes(program);

  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind("/std/collections/soa/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string("/std/collections/soa/").size());
      visibleSoaFieldHelpers.insert("/soa/" + helperSuffix);
    }
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (semantics::isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }

  static const std::unordered_map<std::string, std::string> emptyImportAliases;
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
    std::unordered_map<std::string, SoaFieldViewFieldInfo> fields;
    size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticField(stmt)) {
        continue;
      }
      semantics::BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (semantics::parseBindingInfo(stmt,
                                      def.namespacePrefix,
                                      structPaths,
                                      emptyImportAliases,
                                      binding,
                                      restrictType,
                                      parseError)) {
        std::string typeText = binding.typeName;
        if (!binding.typeTemplateArg.empty()) {
          typeText += "<" + binding.typeTemplateArg + ">";
        }
        fields.emplace(stmt.name,
                       SoaFieldViewFieldInfo{
                           fieldIndex,
                           qualifySoaFieldViewTypeText(typeText,
                                                       def.namespacePrefix,
                                                       structPaths)});
      }
      ++fieldIndex;
    }
    if (!fields.empty()) {
      structFieldInfo.emplace(def.fullPath, std::move(fields));
    }
  }

  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    auto allBindings = bindings;
    std::function<void(const std::vector<Expr> &)> collectBindings =
        [&](const std::vector<Expr> &statements) {
          for (const Expr &stmt : statements) {
            if (stmt.isBinding) {
              if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
                allBindings[stmt.name] = *binding;
              }
            }
            if (!stmt.bodyArguments.empty()) {
              collectBindings(stmt.bodyArguments);
            }
          }
        };
    collectBindings(def.statements);
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewHelperStatements(
        def.statements,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewHelperExpr(
          *def.returnExpr,
          bindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldInfo,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewCarrierIndexStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, std::string> fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        stmt, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = fieldViewBindings;
      rewriteExperimentalSoaFieldViewCarrierIndexStatements(
          stmt.bodyArguments, bodyBindings, structFieldNames, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto parsed = extractParsedBindingInfo(stmt, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[stmt.name] = elemType;
        }
      }
      if (stmt.args.size() == 1 && fieldViewBindings.count(stmt.name) == 0) {
        const Expr &initializer = stmt.args.front();
        if (initializer.kind == Expr::Kind::Call && !initializer.isBinding) {
          std::string initPath = initializer.name;
          if (!initPath.empty() && initPath.front() != '/') {
            initPath.insert(initPath.begin(), '/');
          }
          if (semantics::isExperimentalSoaFieldViewHelperPath(initPath)) {
            if (initializer.templateArgs.size() >= 2) {
              fieldViewBindings[stmt.name] =
                  qualifySoaFieldViewTypeText(initializer.templateArgs[1],
                                              definitionNamespace,
                                              structPaths);
            }
          }
        }
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call ||
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
  std::string elemType;
  if (fieldViewExpr.kind == Expr::Kind::Name) {
    auto bindingIt = fieldViewBindings.find(fieldViewExpr.name);
    if (bindingIt != fieldViewBindings.end()) {
      elemType = bindingIt->second;
    }
  } else if (fieldViewExpr.kind == Expr::Kind::Call && !fieldViewExpr.isBinding) {
    std::string callPath = fieldViewExpr.name;
    if (!callPath.empty() && callPath.front() != '/') {
      callPath.insert(callPath.begin(), '/');
    }
    if (callPath == "/std/collections/soa/field_view" &&
        fieldViewExpr.templateArgs.size() >= 2 &&
        fieldViewExpr.args.size() == 2 &&
        !semantics::hasNamedArguments(fieldViewExpr.argNames)) {
      const std::string structPath = semantics::resolveStructTypePath(
          fieldViewExpr.templateArgs.front(), definitionNamespace, structPaths);
      auto fieldsIt = structFieldNames.find(structPath);
      const auto fieldIndex =
          extractNonNegativeI32LiteralIndex(fieldViewExpr.args[1]);
      if (fieldsIt != structFieldNames.end() && fieldIndex.has_value() &&
          *fieldIndex < fieldsIt->second.size()) {
        Expr getCall;
        getCall.kind = Expr::Kind::Call;
        getCall.name = "/std/collections/soa/get";
        getCall.templateArgs = {fieldViewExpr.templateArgs.front()};
        getCall.args.push_back(fieldViewExpr.args.front());
        getCall.args.push_back(expr.args[1]);
        getCall.argNames.resize(getCall.args.size());
        getCall.sourceLine = fieldViewExpr.sourceLine;
        getCall.sourceColumn = fieldViewExpr.sourceColumn;

        Expr fieldAccess;
        fieldAccess.kind = Expr::Kind::Call;
        fieldAccess.name = fieldsIt->second[*fieldIndex];
        fieldAccess.isMethodCall = true;
        fieldAccess.isFieldAccess = true;
        fieldAccess.args.push_back(std::move(getCall));
        fieldAccess.argNames.resize(fieldAccess.args.size());
        fieldAccess.sourceLine = fieldViewExpr.sourceLine;
        fieldAccess.sourceColumn = fieldViewExpr.sourceColumn;
        expr = std::move(fieldAccess);
        return;
      }
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(callPath)) {
      if (fieldViewExpr.templateArgs.size() >= 2) {
        elemType = qualifySoaFieldViewTypeText(fieldViewExpr.templateArgs[1],
                                               definitionNamespace,
                                               structPaths);
      }
    }
  }
  if (elemType.empty()) {
    return;
  }

  Expr readCall;
  readCall.kind = Expr::Kind::Call;
  readCall.name = collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRead");
  readCall.templateArgs = {elemType};
  readCall.args.push_back(fieldViewExpr);
  readCall.args.push_back(expr.args[1]);
  readCall.argNames.resize(readCall.args.size());
  readCall.sourceLine = expr.sourceLine;
  readCall.sourceColumn = expr.sourceColumn;
  expr = std::move(readCall);
}

bool rewriteExperimentalSoaFieldViewCarrierIndexes(Program &program, std::string &error) {
  error.clear();
  std::unordered_set<std::string> structPaths;
  std::unordered_map<std::string, std::vector<std::string>> structFieldNames;
  for (const Definition &def : program.definitions) {
    if (semantics::isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
      auto isStaticField = [](const Expr &stmt) {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      std::vector<std::string> fields;
      for (const Expr &stmt : def.statements) {
        if (stmt.isBinding && !isStaticField(stmt)) {
          fields.push_back(stmt.name);
        }
      }
      if (!fields.empty()) {
        structFieldNames.emplace(def.fullPath, std::move(fields));
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    for (const Expr &param : def.parameters) {
      if (auto parsed = extractParsedBindingInfo(param, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[param.name] = elemType;
        }
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewCarrierIndexStatements(
        def.statements, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          *def.returnExpr, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
  }
  for (auto &exec : program.executions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    std::string definitionNamespace;
    const size_t slash = exec.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = exec.fullPath.substr(0, slash);
    }
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewAssignTargetsExpr(Expr &expr) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(bodyArg);
  }

  if (!semantics::isAssignCall(expr) || expr.args.size() != 2) {
    return;
  }

  Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::Call && !target.isBinding) {
    // TODO-5235: built via systemHeapValue() so this magic static's backing
    // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
    static const std::string fieldRefPrefix = primec::systemHeapValue([] {
      return collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRef");
    });
    if (semantics::isExperimentalSoaFieldViewReadHelperPath(target.name) &&
        target.args.size() == 2 && !target.templateArgs.empty()) {
      Expr refCall;
      refCall.kind = Expr::Kind::Call;
      refCall.name = std::string(fieldRefPrefix);
      refCall.templateArgs = {target.templateArgs.front()};
      refCall.args = target.args;
      refCall.argNames.resize(refCall.args.size());
      refCall.sourceLine = target.sourceLine;
      refCall.sourceColumn = target.sourceColumn;

      Expr dereferenceCall;
      dereferenceCall.kind = Expr::Kind::Call;
      dereferenceCall.name = "dereference";
      dereferenceCall.args.push_back(std::move(refCall));
      dereferenceCall.argNames.resize(dereferenceCall.args.size());
      dereferenceCall.sourceLine = target.sourceLine;
      dereferenceCall.sourceColumn = target.sourceColumn;

      target = std::move(dereferenceCall);
      return;
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(target.name)) {
      return;
    }
  }
  if (target.kind != Expr::Kind::Call || !target.isFieldAccess ||
      target.args.size() != 1) {
    return;
  }

  Expr &receiver = target.args.front();
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return;
  }

  // TODO-5235: built via systemHeapValue() so these magic statics' backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::string getPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorGet");
  });
  static const std::string refPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorRef");
  });
  static const std::string fieldReadPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRead");
  });
  static const std::string fieldRefPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRef");
  });
  auto rewriteSamePathSoaGetCarrierToRef = [](std::string &path) -> bool {
    const size_t specializationSuffix = path.find("__t");
    const std::string specializationText =
        specializationSuffix == std::string::npos
            ? std::string{}
            : path.substr(specializationSuffix);
    std::string basePath =
        specializationSuffix == std::string::npos
            ? path
            : path.substr(0, specializationSuffix);
    const std::string canonicalGetPath =
        semantics::canonicalizeLegacySoaGetHelperPath(basePath);
    if (canonicalGetPath == "/std/collections/soa/get") {
      path = (basePath.rfind("/soa/", 0) == 0 ? "/soa/ref"
                                                     : "/std/collections/soa/ref") +
             specializationText;
      return true;
    }
    if (canonicalGetPath == "/std/collections/soa/get") {
      path = "/std/collections/soa/ref" + specializationText;
      return true;
    }
    if (canonicalGetPath == "/std/collections/soa/get_ref") {
      path =
          (basePath.rfind("/soa/", 0) == 0 ? "/soa/ref_ref"
                                                  : "/std/collections/soa/ref_ref") +
          specializationText;
      return true;
    }
    if (canonicalGetPath == "/std/collections/soa/get_ref") {
      path = "/std/collections/soa/ref_ref" + specializationText;
      return true;
    }
    return false;
  };
  if (!semantics::isExperimentalSoaGetLikeHelperPath(receiver.name)) {
    if (!semantics::isExperimentalSoaFieldViewReadHelperPath(receiver.name)) {
      if (!rewriteSamePathSoaGetCarrierToRef(receiver.name)) {
        return;
      }
      return;
    }
    receiver.name.replace(0, fieldReadPrefix.size(), fieldRefPrefix);
    return;
  }
  receiver.name.replace(0, getPrefix.size(), refPrefix);
}

bool rewriteExperimentalSoaFieldViewAssignTargets(Program &program,
                                                  std::string &error) {
  error.clear();
  for (Definition &def : program.definitions) {
    for (Expr &stmt : def.statements) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(stmt);
    }
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(*def.returnExpr);
    }
  }
  for (auto &exec : program.executions) {
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
  }
  return true;
}

} // namespace primec
