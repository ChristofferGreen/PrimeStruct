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
#include "primec/support/CollectionHelperNames.h"

namespace primec {

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
        collection_helpers::isCountHelperName(fieldName) ||
        collection_helpers::isGetHelperName(fieldName) ||
        collection_helpers::isRefHelperName(fieldName) ||
        fieldName == "to_soa" || collection_helpers::isToAosHelperName(fieldName)) {
      return;
    }
  }

  if (visibleSoaFieldHelpers.count(collection_helpers::kRootedSoaPrefix + fieldName) > 0) {
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
                           ? collection_helpers::kCanonicalSoaFieldView
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
    if (collection_helpers::isRootedSoaPath(def.fullPath)) {
      visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind(collection_helpers::kCanonicalSoaPrefix, 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string(collection_helpers::kCanonicalSoaPrefix).size());
      visibleSoaFieldHelpers.insert(collection_helpers::kRootedSoaPrefix + helperSuffix);
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

} // namespace primec
