#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"

#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/SoaPathHelpers.h"

#include <cctype>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererBindingTypeHelpersFileLocal.h"

namespace primec::ir_lowerer {
using namespace binding_type_helpers_file_local;

std::string resolveSemanticProductTypeText(const SemanticProgram *semanticProgram,
                                           const std::string &text,
                                           SymbolId textId) {
  if (semanticProgram != nullptr && textId != InvalidSymbolId) {
    std::string resolvedTypeText = std::string(
        semanticProgramResolveCallTargetString(*semanticProgram, textId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(text);
}


bool validateSemanticProductBindingCoverage(const Program &program,
                                           const SemanticProgram *semanticProgram,
                                           std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  auto validateBindingExpr = [&](const std::string &scopePath,
                                 const std::string &siteKind,
                                 const Expr &expr) {
    if (expr.semanticNodeId == 0) {
      return true;
    }
    const SemanticProgramBindingFact *bindingFact =
        findSemanticProductBindingFact(semanticIndex, expr);
    const std::string bindingTypeText =
        bindingFact != nullptr
            ? resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact)
            : std::string{};
    if (bindingFact == nullptr || bindingTypeText.empty()) {
      error = "missing semantic-product binding fact: " +
              describeBindingSite(scopePath, siteKind, expr);
      return false;
    }
    const std::string siteDescription = describeBindingSite(scopePath, siteKind, expr);
    if (!validateInternedSemanticTextMetadata(*semanticProgram,
                                              bindingFact->bindingTypeTextId,
                                              bindingFact->bindingTypeText,
                                              "binding",
                                              "type",
                                              siteDescription,
                                              error) ||
        !validateInternedSemanticTextMetadata(*semanticProgram,
                                              bindingFact->referenceRootId,
                                              bindingFact->referenceRoot,
                                              "binding",
                                              "reference root",
                                              siteDescription,
                                              error)) {
      return false;
    }
    if (bindingFact->resolvedPathId == InvalidSymbolId ||
        semanticProgramBindingFactResolvedPath(*semanticProgram, *bindingFact).empty()) {
      error = "missing semantic-product binding resolved path id: " +
              siteDescription;
      return false;
    }
    return true;
  };

  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.isBinding && !validateBindingExpr(scopePath, "local", expr)) {
      return false;
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    for (const auto &param : def.parameters) {
      if (!validateBindingExpr(def.fullPath, "parameter", param)) {
        return false;
      }
    }
    if (!validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductLocalAutoCoverage(const Program &program,
                                              const SemanticProgram *semanticProgram,
                                              std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);

  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.isBinding && expr.args.size() == 1 && expr.semanticNodeId != 0 &&
        isLocalAutoBindingCandidate(expr)) {
      const SemanticProgramLocalAutoFact *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr);
      const std::string localAutoBindingTypeText =
          localAutoFact != nullptr
              ? resolveSemanticProductTypeText(semanticProgram,
                                               localAutoFact->bindingTypeText,
                                               localAutoFact->bindingTypeTextId)
              : std::string{};
      if (localAutoFact == nullptr || localAutoBindingTypeText.empty()) {
        error = "missing semantic-product local-auto fact: " +
                describeBindingSite(scopePath, "local", expr);
        return false;
      }
      const std::string siteDescription = describeBindingSite(scopePath, "local", expr);
      if (!validateInternedSemanticTextMetadata(*semanticProgram,
                                                localAutoFact->bindingTypeTextId,
                                                localAutoFact->bindingTypeText,
                                                "local-auto",
                                                "binding type",
                                                siteDescription,
                                                error)) {
        return false;
      }
      if (localAutoFact != nullptr &&
          localAutoFact->initializerResolvedPathId != InvalidSymbolId &&
          semanticProgramLocalAutoFactInitializerResolvedPath(*semanticProgram, *localAutoFact).empty()) {
        error = "missing semantic-product local-auto initializer path id: " +
                siteDescription;
        return false;
      }
      auto validateInitializerCallPath = [&](SymbolId pathId,
                                             SymbolId returnKindId,
                                             std::optional<StdlibSurfaceId> callSurfaceId,
                                             const char *missingLabel,
                                             const char *staleLabel) {
        if (pathId == InvalidSymbolId) {
          return true;
        }
        if (semanticProgramResolveCallTargetString(*semanticProgram, pathId).empty()) {
          error = std::string("missing semantic-product local-auto ") + missingLabel +
                  " path id: " + siteDescription;
          return false;
        }
        const std::string_view callPath =
            semanticProgramResolveCallTargetString(*semanticProgram, pathId);
        const std::string_view initializerPath =
            localAutoFact->initializerResolvedPathId != InvalidSymbolId
                ? semanticProgramResolveCallTargetString(
                      *semanticProgram, localAutoFact->initializerResolvedPathId)
                : std::string_view{};
        const bool sameInitializerPath =
            localAutoFact->initializerResolvedPathId == InvalidSymbolId ||
            pathId == localAutoFact->initializerResolvedPathId ||
            (!initializerPath.empty() &&
             stripResolvedPathSpecializationSuffix(callPath) ==
                 stripResolvedPathSpecializationSuffix(initializerPath));
        const bool samePublishedStdlibSurface =
            localAutoFact->initializerStdlibSurfaceId.has_value() &&
            callSurfaceId.has_value() &&
            *localAutoFact->initializerStdlibSurfaceId == *callSurfaceId;
        if (!sameInitializerPath && !samePublishedStdlibSurface) {
          error = std::string("stale semantic-product local-auto ") + staleLabel +
                  " fact: " + siteDescription;
          return false;
        }
        if (returnKindId != InvalidSymbolId) {
          const std::string_view returnKind =
              semanticProgramResolveCallTargetString(*semanticProgram, returnKindId);
          if (returnKind.empty()) {
            error = std::string("missing semantic-product local-auto ") + missingLabel +
                    " return-kind id: " + siteDescription;
            return false;
          }
          const auto *summary =
              semanticProgramLookupPublishedCallableSummaryByPathId(*semanticProgram, pathId);
          if (summary != nullptr) {
            const std::string_view expectedReturnKind =
                summary->returnKindId != InvalidSymbolId
                    ? semanticProgramResolveCallTargetString(*semanticProgram,
                                                             summary->returnKindId)
                    : std::string_view(summary->returnKind);
            if (!expectedReturnKind.empty() && returnKind != expectedReturnKind) {
              error = std::string("stale semantic-product local-auto ") + staleLabel +
                      " return-kind fact: " + siteDescription;
              return false;
            }
          }
        }
        return true;
      };
      if (!validateInitializerCallPath(localAutoFact->initializerDirectCallResolvedPathId,
                                       localAutoFact->initializerDirectCallReturnKindId,
                                       localAutoFact->initializerDirectCallStdlibSurfaceId,
                                       "direct-call",
                                       "direct-call")) {
        return false;
      }
      if (!validateInitializerCallPath(localAutoFact->initializerMethodCallResolvedPathId,
                                       localAutoFact->initializerMethodCallReturnKindId,
                                       localAutoFact->initializerMethodCallStdlibSurfaceId,
                                       "method-call",
                                       "method-call")) {
        return false;
      }
      const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(semanticIndex, expr);
      const std::string bindingTypeText =
          bindingFact != nullptr
              ? resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact)
              : std::string{};
      if (bindingFact != nullptr &&
          !bindingTypeText.empty() &&
          !semanticTypeTextsMatchForLocalAuto(localAutoBindingTypeText,
                                              bindingTypeText)) {
        error = "stale semantic-product local-auto fact: " +
                siteDescription;
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductCollectionSpecializationCoverage(
    const Program &program,
    const SemanticProgram *semanticProgram,
    std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  auto validateBindingExpr = [&](const std::string &scopePath,
                                 const std::string &siteKind,
                                 const Expr &expr) {
    if (expr.semanticNodeId == 0) {
      return true;
    }
    const SemanticProgramBindingFact *bindingFact =
        findSemanticProductBindingFact(semanticIndex, expr);
    const std::string bindingTypeText =
        bindingFact != nullptr
            ? resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact)
            : std::string{};
    if (bindingFact == nullptr || bindingTypeText.empty()) {
      return true;
    }

    ExpectedCollectionSpecialization expected;
    if (!extractExpectedCollectionSpecialization(
            bindingTypeText, expected)) {
      return true;
    }

    const SemanticProgramCollectionSpecialization *collectionFact =
        findSemanticProductCollectionSpecialization(semanticIndex, expr);
    if (collectionFact == nullptr) {
      error = "missing semantic-product collection specialization: " +
              describeBindingSite(scopePath, siteKind, expr);
      return false;
    }
    const std::string siteDescription = describeBindingSite(scopePath, siteKind, expr);
    if (!validateInternedSemanticTextMetadata(*semanticProgram,
                                              collectionFact->collectionFamilyId,
                                              collectionFact->collectionFamily,
                                              "collection specialization",
                                              "family",
                                              siteDescription,
                                              error,
                                              false) ||
        !validateInternedSemanticTextMetadata(*semanticProgram,
                                              collectionFact->bindingTypeTextId,
                                              collectionFact->bindingTypeText,
                                              "collection specialization",
                                              "binding type",
                                              siteDescription,
                                              error,
                                              false) ||
        !validateInternedSemanticTextMetadata(*semanticProgram,
                                              collectionFact->elementTypeTextId,
                                              collectionFact->elementTypeText,
                                              "collection specialization",
                                              "element type",
                                              siteDescription,
                                              error,
                                              false) ||
        !validateInternedSemanticTextMetadata(*semanticProgram,
                                              collectionFact->keyTypeTextId,
                                              collectionFact->keyTypeText,
                                              "collection specialization",
                                              "key type",
                                              siteDescription,
                                              error,
                                              false) ||
        !validateInternedSemanticTextMetadata(*semanticProgram,
                                              collectionFact->valueTypeTextId,
                                              collectionFact->valueTypeText,
                                              "collection specialization",
                                              "value type",
                                              siteDescription,
                                              error,
                                              false) ||
        !validateInternedSemanticTextMetadata(*semanticProgram,
                                              collectionFact->structPathId,
                                              collectionFact->structPath,
                                              "collection specialization",
                                              "struct path",
                                              siteDescription,
                                              error)) {
      return false;
    }
    if (!collectionSpecializationMatchesExpected(*collectionFact, expected)) {
      error = "stale semantic-product collection specialization: " +
              siteDescription;
      return false;
    }
    return true;
  };

  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.isBinding && !validateBindingExpr(scopePath, "local", expr)) {
      return false;
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    for (const auto &param : def.parameters) {
      if (!validateBindingExpr(def.fullPath, "parameter", param)) {
        return false;
      }
    }
    if (!validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductArrayExtentCoverage(const Program &program,
                                                const SemanticProgram *semanticProgram,
                                                std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  std::unordered_map<std::string, std::string> extentExpressionsByResolvedPath;
  for (const auto &def : program.definitions) {
    collectArrayExtentExpressionsForBindings(def.fullPath,
                                             def.statements,
                                             extentExpressionsByResolvedPath);
  }
  auto requireTextMetadata = [&](SymbolId textId,
                                 const std::string &fallback,
                                 const std::string &expected,
                                 std::string_view fieldLabel,
                                 const std::string &siteDescription) {
    if (textId == InvalidSymbolId) {
      error = "missing semantic-product array extent " +
              std::string(fieldLabel) + " id: " + siteDescription;
      return false;
    }
    const std::string actual =
        resolveSemanticTextOrFallback(*semanticProgram, textId, fallback);
    if (actual.empty()) {
      error = "missing semantic-product array extent " +
              std::string(fieldLabel) + " id: " + siteDescription;
      return false;
    }
    if (!expected.empty() && actual != expected) {
      error = "stale semantic-product array extent " +
              std::string(fieldLabel) + " metadata: " + siteDescription;
      return false;
    }
    return true;
  };

  auto validateArrayExtentFact = [&](const SemanticProgramArrayExtentFact *arrayFact,
                                     const ExpectedArrayExtent &expected,
                                     const std::string &expectedSiteKind,
                                     const std::string &expectedTargetName,
                                     const std::string &expectedTargetResolvedPath,
                                     uint64_t expectedTargetSemanticNodeId,
                                     const std::string &expectedBindingTypeText,
                                     const std::string &expectedExtentExpression,
                                     const std::string &siteDescription) {
    if (arrayFact == nullptr) {
      error = "missing semantic-product array extent fact: " + siteDescription;
      return false;
    }
    if (arrayFact->isReference != expected.isReference ||
        arrayFact->targetSemanticNodeId != expectedTargetSemanticNodeId) {
      error = "stale semantic-product array extent fact: " + siteDescription;
      return false;
    }
    if (!requireTextMetadata(arrayFact->siteKindId,
                             arrayFact->siteKind,
                             expectedSiteKind,
                             "site kind",
                             siteDescription) ||
        !requireTextMetadata(arrayFact->targetNameId,
                             arrayFact->targetName,
                             expectedTargetName,
                             "target name",
                             siteDescription) ||
        !requireTextMetadata(arrayFact->targetResolvedPathId,
                             arrayFact->targetResolvedPath,
                             expectedTargetResolvedPath,
                             "target path",
                             siteDescription) ||
        !requireTextMetadata(arrayFact->bindingTypeTextId,
                             arrayFact->bindingTypeText,
                             expectedBindingTypeText,
                             "binding type",
                             siteDescription) ||
        !requireTextMetadata(arrayFact->elementTypeTextId,
                             arrayFact->elementTypeText,
                             expected.elementTypeText,
                             "element type",
                             siteDescription) ||
        !requireTextMetadata(arrayFact->extentExpressionId,
                             arrayFact->extentExpression,
                             expectedExtentExpression,
                             "expression",
                             siteDescription)) {
      return false;
    }
    return true;
  };

  auto validateBindingFact = [&](const SemanticProgramBindingFact &bindingFact) {
    const std::string bindingTypeText =
        resolveSemanticBindingFactTypeText(semanticProgram, bindingFact);
    ExpectedArrayExtent expected;
    if (!extractExpectedArrayExtent(bindingTypeText, expected)) {
      return true;
    }
    const std::string siteKind =
        resolveSemanticTextOrFallback(*semanticProgram,
                                      bindingFact.siteKindId,
                                      bindingFact.siteKind);
    if (!arrayExtentBindingSiteRequiresFact(siteKind, expected.isReference)) {
      return true;
    }
    const std::string scopePath =
        resolveSemanticTextOrFallback(*semanticProgram,
                                      bindingFact.scopePathId,
                                      bindingFact.scopePath);
    const std::string targetName =
        resolveSemanticTextOrFallback(*semanticProgram,
                                      bindingFact.nameId,
                                      bindingFact.name);
    const std::string targetResolvedPath =
        std::string(semanticProgramBindingFactResolvedPath(*semanticProgram,
                                                           bindingFact));
    const std::string siteDescription =
        describeArrayExtentSite(scopePath, siteKind, targetName);
    if (targetResolvedPath.empty()) {
      error = "missing semantic-product array extent target path: " +
              siteDescription;
      return false;
    }
    std::string expectedExtentExpression = "count(" + targetName + ")";
    if (const auto extentExpression =
            extentExpressionsByResolvedPath.find(targetResolvedPath);
        extentExpression != extentExpressionsByResolvedPath.end()) {
      expectedExtentExpression = extentExpression->second;
    }
    const SemanticProgramArrayExtentFact *arrayFact =
        semanticProgramLookupPublishedArrayExtentFactBySemanticId(
            *semanticProgram, bindingFact.semanticNodeId);
    return validateArrayExtentFact(arrayFact,
                                   expected,
                                   expectedArrayExtentSiteKind(siteKind,
                                                               expected.isReference),
                                   targetName,
                                   targetResolvedPath,
                                   bindingFact.semanticNodeId,
                                   bindingTypeText,
                                   expectedExtentExpression,
                                   siteDescription);
  };

  for (const auto *bindingFact : semanticProgramBindingFactView(*semanticProgram)) {
    if (bindingFact != nullptr && !validateBindingFact(*bindingFact)) {
      return false;
    }
  }

  auto countTarget = [](const Expr &expr) -> const Expr * {
    if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
      return nullptr;
    }
    std::string_view name = expr.name;
    if (const std::size_t slash = name.find_last_of('/');
        slash != std::string_view::npos) {
      name.remove_prefix(slash + 1);
    }
    if (name != "count") {
      return nullptr;
    }
    return &expr.args.front();
  };

  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (const Expr *target = countTarget(expr);
        target != nullptr && target->kind == Expr::Kind::Name &&
        expr.semanticNodeId != 0) {
      const SemanticProgramBindingFact *targetBindingFact =
          findSemanticProductBindingFact(semanticIndex, *target);
      const std::string targetBindingTypeText =
          targetBindingFact != nullptr
              ? resolveSemanticBindingFactTypeText(semanticProgram, *targetBindingFact)
              : std::string{};
      ExpectedArrayExtent expected;
      if (targetBindingFact != nullptr &&
          extractExpectedArrayExtent(targetBindingTypeText, expected)) {
        const std::string targetResolvedPath =
            std::string(semanticProgramBindingFactResolvedPath(*semanticProgram,
                                                               *targetBindingFact));
        const std::string siteDescription =
            describeArrayExtentSite(scopePath, "count", target->name);
        if (targetResolvedPath.empty()) {
          error = "missing semantic-product array extent target path: " +
                  siteDescription;
          return false;
        }
        const SemanticProgramArrayExtentFact *arrayFact =
            semanticProgramLookupPublishedArrayExtentFactBySemanticId(
                *semanticProgram, expr.semanticNodeId);
        if (!validateArrayExtentFact(arrayFact,
                                     expected,
                                     "count-expression",
                                     target->name,
                                     targetResolvedPath,
                                     target->semanticNodeId,
                                     targetBindingTypeText,
                                     "count(" + target->name + ")",
                                     siteDescription)) {
          return false;
        }
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    for (const auto &param : def.parameters) {
      if (!validateExprs(def.fullPath, param.args) ||
          !validateExprs(def.fullPath, param.bodyArguments)) {
        return false;
      }
    }
    if (!validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  return true;
}

std::string normalizeCollectionBindingTypeName(const std::string &name) {
  if (isBuiltinCollectionTypeName(name, "vector") ||
      isExperimentalCollectionTypeName(name, "vector", "Vector")) {
    return "vector";
  }
  if (isBuiltinCollectionTypeName(name, "map") ||
      isExperimentalCollectionTypeName(name, "map", "Map")) {
    return "map";
  }
  if (name == "soa" || collection_helpers::isCollectionFamilyRoot(name, collection_helpers::CollectionFamily::Soa) ||
      name.rfind("soa<", 0) == 0 || name.rfind("/soa<", 0) == 0 ||
      name == "std/collections/soa" || name == collection_helpers::kCanonicalSoa ||
      name.rfind("std/collections/soa<", 0) == 0 ||
      name.rfind("/std/collections/soa<", 0) == 0 ||
      collection_helpers::isCollectionFamilyRoot(name, collection_helpers::CollectionFamily::Soa) || name == "std/collections/soa" ||
      name == collection_helpers::kCanonicalSoa || name == "SoaVector" ||
      name == "/SoaVector" ||
      name == collection_paths::memberPathBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
      name == collection_paths::memberPath(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
      soa_paths::isExperimentalColumnarVectorTypePath(name)) {
    return "soa";
  }
  if (name == "Buffer" || name == "std/gfx/Buffer" || name == "/std/gfx/Buffer" ||
      name == "std/gfx/experimental/Buffer" || name == "/std/gfx/experimental/Buffer") {
    return "Buffer";
  }
  if (name == "/File" || name == "std/file/File" || name == "/std/file/File") {
    return "File";
  }
  if (name == "args") {
    return "array";
  }
  // TODO-5250: Slice<T, Capability> desugars to array<T> - see the matching
  // comment in SemanticsBindingTypeHelpers.cpp's normalizeBindingTypeName.
  if (name == "Slice") {
    return "array";
  }
  return name;
}

bool typeTextUsesRawBuiltinSoaVectorLayout(const std::string &typeText) {
  const std::string normalized = trimTemplateTypeText(typeText);
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(normalized, base, arg)) {
    const std::string trimmedBase = trimTemplateTypeText(base);
    if (trimmedBase == "soa" || collection_helpers::isCollectionFamilyRoot(trimmedBase, collection_helpers::CollectionFamily::Soa) ||
        trimmedBase == "std/collections/soa" ||
        trimmedBase == collection_helpers::kCanonicalSoa) {
      return true;
    }
    std::vector<std::string> templateArgs;
    if (splitTemplateArgs(arg, templateArgs)) {
      for (const std::string &nested : templateArgs) {
        if (typeTextUsesRawBuiltinSoaVectorLayout(nested)) {
          return true;
        }
      }
    } else if (!arg.empty() && typeTextUsesRawBuiltinSoaVectorLayout(arg)) {
      return true;
    }
    return false;
  }
  return normalized == "soa" || collection_helpers::isCollectionFamilyRoot(normalized, collection_helpers::CollectionFamily::Soa) ||
         normalized == "std/collections/soa" ||
         normalized == collection_helpers::kCanonicalSoa;
}

bool exprUsesRawBuiltinSoaVectorLayout(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" ||
        isBindingQualifierName(transform.name)) {
      continue;
    }
    if (typeTextUsesRawBuiltinSoaVectorLayout(transform.name)) {
      return true;
    }
    for (const std::string &templateArg : transform.templateArgs) {
      if (typeTextUsesRawBuiltinSoaVectorLayout(templateArg)) {
        return true;
      }
    }
  }
  return false;
}

BindingTypeAdapters makeBindingTypeAdapters(const SemanticProgram *semanticProgram) {
  BindingTypeAdapters adapters;
  // Build the (potentially large) semantic-product index once and share it by
  // pointer across every adapter closure below via a shared_ptr - the same
  // pattern already used for this type in IrLowererStringLiteralHelpers.cpp,
  // IrLowererCountAccessHelpers.cpp, IrLowererNativeTailDispatch.cpp, and
  // IrLowererInlineNativeCallDispatch.cpp. Capturing the index itself by
  // value in each of the six lambdas below (as before) copy-constructed the
  // full multi-map index (up to 8 unordered_maps) once per lambda - a
  // shared_ptr capture is a cheap refcount bump instead.
  const auto semanticIndex =
      std::make_shared<const SemanticProductIndex>(buildSemanticProductIndex(semanticProgram));
  adapters.isBindingMutable = [](const Expr &expr) {
    return ir_lowerer::isBindingMutable(expr);
  };
  adapters.bindingKind = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramCollectionSpecialization *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndex, expr);
        collectionFact != nullptr) {
      return bindingKindFromCollectionSpecialization(semanticProgram, *collectionFact);
    }
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, expr);
        bindingFact != nullptr) {
      const std::string bindingTypeText =
          resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact);
      if (!bindingTypeText.empty()) {
        return bindingKindFromTypeText(bindingTypeText);
      }
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return LocalInfo::Kind::Value;
    }
    return bindingKindFromTransforms(expr);
  };
  adapters.hasExplicitBindingTypeTransform = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramLocalAutoFact *localAutoFact =
            findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, expr);
        localAutoFact != nullptr) {
      return false;
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, expr);
      if (bindingFact == nullptr ||
          resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact).empty()) {
        return false;
      }
      return true;
    }
    return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
  };
  adapters.isStringBinding = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, expr);
        bindingFact != nullptr) {
      const std::string bindingTypeText =
          resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact);
      if (!bindingTypeText.empty()) {
        return isStringBindingTypeText(bindingTypeText);
      }
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return false;
    }
    return isStringBindingType(expr);
  };
  adapters.isFileErrorBinding = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, expr);
        bindingFact != nullptr) {
      const std::string bindingTypeText =
          resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact);
      if (!bindingTypeText.empty()) {
        return isFileErrorBindingTypeText(bindingTypeText);
      }
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return false;
    }
    return isFileErrorBindingType(expr);
  };
  adapters.bindingValueKind = [semanticProgram, semanticIndex](const Expr &expr, LocalInfo::Kind kind) {
    if (const SemanticProgramCollectionSpecialization *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndex, expr);
        collectionFact != nullptr) {
      return bindingValueKindFromCollectionSpecialization(semanticProgram, *collectionFact);
    }
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, expr);
        bindingFact != nullptr) {
      const std::string bindingTypeText =
          resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact);
      if (!bindingTypeText.empty()) {
        return bindingValueKindFromTypeText(bindingTypeText, kind);
      }
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return LocalInfo::ValueKind::Unknown;
    }
    return bindingValueKindFromTransforms(expr, kind);
  };
  adapters.setReferenceArrayInfo = [semanticProgram, semanticIndex](const Expr &expr, LocalInfo &info) {
    if (const SemanticProgramCollectionSpecialization *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndex, expr);
        collectionFact != nullptr) {
      setReferenceCollectionInfoFromSpecialization(semanticProgram, *collectionFact, info);
      return;
    }
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, expr);
        bindingFact != nullptr) {
      const std::string bindingTypeText =
          resolveSemanticBindingFactTypeText(semanticProgram, *bindingFact);
      if (!bindingTypeText.empty()) {
        setReferenceArrayInfoFromTypeText(bindingTypeText, info);
      }
      return;
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return;
    }
    setReferenceArrayInfoFromTransforms(expr, info);
  };
  return adapters;
}

LocalInfo::Kind bindingKindFromTransforms(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    const std::string normalizedName = normalizeCollectionBindingTypeName(transform.name);
    if (normalizedName == "Reference") {
      return LocalInfo::Kind::Reference;
    }
    if (normalizedName == "Pointer") {
      return LocalInfo::Kind::Pointer;
    }
    if (normalizedName == "array") {
      return LocalInfo::Kind::Array;
    }
    if (normalizedName == "vector" || normalizedName == "soa") {
      return LocalInfo::Kind::Vector;
    }
    if (normalizedName == "map") {
      return LocalInfo::Kind::Value;
    }
    if (normalizedName == "Buffer") {
      return LocalInfo::Kind::Buffer;
    }
  }
  return LocalInfo::Kind::Value;
}

bool isStringTypeName(const std::string &name) {
  return name == "string";
}

bool isStringBindingType(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (isStringTypeName(transform.name)) {
      return true;
    }
    if ((transform.name == "Pointer" || transform.name == "Reference") && transform.templateArgs.size() == 1 &&
        isStringTypeName(transform.templateArgs.front())) {
      return true;
    }
  }
  return false;
}

bool isFileErrorBindingType(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (transform.name == "FileError") {
      return true;
    }
    if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1 &&
        unwrapTopLevelUninitializedTypeText(transform.templateArgs.front()) == "FileError") {
      return true;
    }
  }
  return false;
}

LocalInfo::ValueKind bindingValueKindFromTransforms(const Expr &expr, LocalInfo::Kind kind) {
  bool sawTypeTransform = false;
  for (const auto &transform : expr.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    sawTypeTransform = true;
    const std::string normalizedName = normalizeCollectionBindingTypeName(transform.name);
    if (normalizedName == "Pointer" || normalizedName == "Reference") {
      // TODO-5249: an optional second template argument names a capability
      // marker (Reference<T, Capability>/Pointer<T, Capability>); only the
      // first argument (T) determines the value kind.
      if (transform.templateArgs.size() == 1 || transform.templateArgs.size() == 2) {
        return valueKindFromTypeName(unwrapTopLevelUninitializedTypeText(transform.templateArgs.front()));
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "array" || normalizedName == "vector" ||
        normalizedName == "soa" || normalizedName == "Buffer") {
      if (transform.templateArgs.size() == 1) {
        return valueKindFromTypeName(transform.templateArgs.front());
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "map") {
      if (transform.templateArgs.size() == 2) {
        return valueKindFromTypeName(transform.templateArgs[1]);
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "Result") {
      if (transform.templateArgs.size() == 1) {
        return LocalInfo::ValueKind::Int32;
      }
      if (transform.templateArgs.size() == 2) {
        return LocalInfo::ValueKind::Int64;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "File") {
      return LocalInfo::ValueKind::Int64;
    }
    LocalInfo::ValueKind kindValue = valueKindFromTypeName(normalizedName);
    if (kindValue != LocalInfo::ValueKind::Unknown) {
      return kindValue;
    }
  }
  if (kind != LocalInfo::Kind::Value || sawTypeTransform) {
    return LocalInfo::ValueKind::Unknown;
  }
  return LocalInfo::ValueKind::Int32;
}

void setReferenceArrayInfoFromTransforms(const Expr &expr, LocalInfo &info) {
  if (info.kind != LocalInfo::Kind::Reference && info.kind != LocalInfo::Kind::Pointer) {
    return;
  }
  for (const auto &transform : expr.transforms) {
    if ((info.kind == LocalInfo::Kind::Reference && transform.name != "Reference") ||
        (info.kind == LocalInfo::Kind::Pointer && transform.name != "Pointer") ||
        transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string targetType = unwrapTopLevelUninitializedTypeText(transform.templateArgs.front());
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(targetType, base, arg)) {
      return;
    }
    base = normalizeCollectionBindingTypeName(base);
    if (base == "array") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToArray = true;
      } else {
        info.pointerToArray = true;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
      }
      return;
    }
    if (base == "vector") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToVector = true;
      } else {
        info.pointerToVector = true;
      }
      const std::string elementType = trimTemplateTypeText(arg);
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(elementType);
      }
      if (info.structTypeName.empty() && valueKindFromTypeName(elementType) == LocalInfo::ValueKind::Unknown) {
        info.structTypeName =
            specializedCollectionVectorRecordPathForElementType(elementType);
      }
      return;
    }
    if (base == "soa") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToVector = true;
      } else {
        info.pointerToVector = true;
      }
      info.isSoaVector = true;
      const std::string elementType = trimTemplateTypeText(arg);
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(elementType);
      }
      if (info.structTypeName.empty() && valueKindFromTypeName(elementType) == LocalInfo::ValueKind::Unknown) {
        info.structTypeName =
            specializedExperimentalSoaVectorStructPathForElementType(elementType);
      }
      return;
    }
    if (base == "Buffer") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToBuffer = true;
      } else {
        info.pointerToBuffer = true;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
      }
      return;
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(arg, args) || args.size() != 2) {
        return;
      }
      info.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
      info.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = info.keyValueValueKind;
      }
      return;
    }
    if (base == "File") {
      info.isFileHandle = true;
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = LocalInfo::ValueKind::Int64;
      }
    }
    return;
  }
}

} // namespace primec::ir_lowerer
