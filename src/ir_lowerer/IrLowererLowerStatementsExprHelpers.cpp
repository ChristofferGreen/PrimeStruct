// soa-surface-audit: exempt
// collection-surface-audit: exempt
#include "IrLowererLowerStatementsExprHelpers.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCountAccessClassifiers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"

namespace primec::ir_lowerer {

StatementsExprContext::StatementsExprContext(LowerSetupStageState &setupStageIn,
                                              LowerReturnEmitStageState &stateOutIn,
                                              const CallResolutionAdapters &callResolutionAdaptersIn,
                                              std::string &errorIn)
    : setupStage(setupStageIn),
      stateOut(stateOutIn),
      callResolutionAdapters(callResolutionAdaptersIn),
      error(errorIn),
      function(setupStageIn.function),
      semanticProgram(callResolutionAdaptersIn.semanticProgram),
      defMap(setupStageIn.defMap),
      emitExpr(stateOutIn.emitExpr),
      allocTempLocal(stateOutIn.allocTempLocal),
      inferExprKind(setupStageIn.inferenceSetupBootstrap.inferExprKind),
      inferStructExprPath(
          setupStageIn.setupLocalsOrchestration.uninitializedResolutionAdapters
              .inferStructExprPath),
      resolveExprPath(callResolutionAdaptersIn.resolveExprPath),
      resolveDefinitionCall(stateOutIn.resolveDefinitionCall),
      resolveStructTypeName(
          setupStageIn.setupLocalsOrchestration.setupTypeAndStructTypeAdapters
              .structTypeResolutionAdapters.resolveStructTypeName) {}

std::string StatementsExprContext::experimentalCollectionMemberPath(
    std::string_view collectionName, std::string_view memberName) {
  return experimentalCollectionMemberRoot(collectionName) + std::string(memberName);
}

        std::string StatementsExprContext::resolveDirectHelperPath(const Expr &callExpr) {
          if (!callExpr.name.empty() && callExpr.name.front() == '/') {
            return callExpr.name;
          }
          if (!callExpr.namespacePrefix.empty()) {
            std::string scoped = callExpr.namespacePrefix;
            if (!scoped.empty() && scoped.front() != '/') {
              scoped.insert(scoped.begin(), '/');
            }
            return scoped + "/" + callExpr.name;
          }
          return callExpr.name;
        }


        bool StatementsExprContext::matchesGeneratedSpecializedType(std::string_view path, std::string_view collectionName,
                std::string_view typeName) {
              const std::string typePath =
                  experimentalCollectionTypePath(collectionName, typeName);
              return path.rfind(typePath + "__", 0) == 0;
            }


        bool StatementsExprContext::isCollectionVectorRecordTypePath(std::string_view path) {
          return path == vectorBackingTypePath() ||
                 matchesGeneratedSpecializedType(path, "vector", "Vector");
        }


        bool StatementsExprContext::matchesDirectHelperDefinitionFamilyPath(const std::string &candidatePath, const Definition &callee) {
              return !candidatePath.empty() &&
                     (callee.fullPath == candidatePath ||
                      callee.fullPath.rfind(candidatePath + "__", 0) == 0 ||
                      callee.fullPath.rfind(candidatePath + "<", 0) == 0 ||
                      normalizeCollectionHelperPath(candidatePath) ==
                          normalizeCollectionHelperPath(callee.fullPath));
            }


        bool StatementsExprContext::isDirectHelperDefinitionFamily(const Expr &callExpr,
                                                  const Definition &callee) {
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          if (matchesDirectHelperDefinitionFamilyPath(rawPath, callee)) {
            return true;
          }
          const std::string resolvedPath = resolveExprPath(callExpr);
          return resolvedPath != rawPath &&
                 matchesDirectHelperDefinitionFamilyPath(resolvedPath, callee);
        }


        const Definition * StatementsExprContext::findDirectHelperDefinition(const std::string &rawPath) {
          auto defIt = defMap.find(rawPath);
          if (defIt != defMap.end()) {
            return defIt->second;
          }
          auto matchesGeneratedLeafDefinition = [&](const std::string &path,
                                                    const char *marker,
                                                    size_t markerSize) {
            return path.rfind(rawPath, 0) == 0 &&
                   path.compare(rawPath.size(), markerSize, marker) == 0 &&
                   path.find('/', rawPath.size() + markerSize) ==
                       std::string::npos;
          };
          for (const auto &[path, def] : defMap) {
            if (def == nullptr) {
              continue;
            }
            if (matchesGeneratedLeafDefinition(path, "__t", 3) ||
                matchesGeneratedLeafDefinition(path, "__ov", 4) ||
                matchesGeneratedLeafDefinition(path, "<", 1)) {
              return def;
            }
          }
          return nullptr;
        }


        bool StatementsExprContext::isExplicitExperimentalVectorConstructorHelper(const std::string &path) {
              const std::string slashPath =
                  experimentalCollectionMemberPath("vector", "vector");
              return path == slashPath || path == slashPath.substr(1);
            }


        const Definition * StatementsExprContext::resolveDirectHelperDefinition(const Expr &targetExpr) {
          const std::string rawPath = resolveDirectHelperPath(targetExpr);
          if (isExplicitExperimentalVectorConstructorHelper(rawPath)) {
            if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
                rawDef != nullptr) {
              return rawDef;
            }
          }
          if (const Definition *callee = resolveDefinitionCall(targetExpr);
              callee != nullptr) {
            return callee;
          }
          if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
              rawDef != nullptr) {
            return rawDef;
          }
          const std::string resolvedPath = resolveExprPath(targetExpr);
          if (resolvedPath != rawPath) {
            return findDirectHelperDefinition(resolvedPath);
          }
          return nullptr;
        }


        std::string StatementsExprContext::stripGeneratedHelperSuffix(std::string helperPath) {
          const size_t leafStart = helperPath.find_last_of('/');
          const size_t generatedSuffix =
              helperPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
          if (generatedSuffix != std::string::npos) {
            helperPath.erase(generatedSuffix);
          }
          return helperPath;
        }


        std::string StatementsExprContext::extractHelperTail(std::string helperPath) {
          helperPath = stripGeneratedHelperSuffix(std::move(helperPath));
          const size_t slash = helperPath.find_last_of('/');
          if (slash != std::string::npos) {
            helperPath = helperPath.substr(slash + 1);
          }
          return helperPath;
        }


        const StdlibSurfaceMetadata * StatementsExprContext::keyValueHelperMetadata() {
          return keyValueHelperSurfaceMetadata();
        }


        const StdlibSurfaceMetadata * StatementsExprContext::keyValueConstructorMetadata() {
          return keyValueConstructorSurfaceMetadata();
        }


        bool StatementsExprContext::resolveKeyValueHelperMemberName(std::string path,
                                                   std::string &helperNameOut) {
          helperNameOut.clear();
          const auto *metadata = keyValueHelperMetadata();
          if (metadata == nullptr) {
            return false;
          }
          path = normalizeCollectionHelperPath(path);
          return resolvePublishedStdlibSurfaceMemberName(
              path, metadata->id, helperNameOut);
        }


        std::string StatementsExprContext::keyValueHelperMethodSpelling(std::string_view memberName) {
          const auto *metadata = keyValueHelperMetadata();
          if (metadata == nullptr) {
            return std::string(memberName);
          }
          for (const StdlibSurfaceMemberAlias &alias : metadata->memberAliases) {
            if (alias.memberName == memberName &&
                alias.spelling.find('/') == std::string_view::npos) {
              return std::string(alias.spelling);
            }
          }
          return std::string(memberName);
        }


        std::string StatementsExprContext::pascalCaseHelperMember(std::string_view memberName) {
          std::string result;
          bool capitalizeNext = true;
          for (const char ch : memberName) {
            if (ch == '_') {
              capitalizeNext = true;
              continue;
            }
            if (capitalizeNext) {
              result.push_back(static_cast<char>(
                  std::toupper(static_cast<unsigned char>(ch))));
              capitalizeNext = false;
            } else {
              result.push_back(ch);
            }
          }
          return result;
        }


        std::string StatementsExprContext::keyValueImplementationMethodSpelling(const std::string &receiverStructPath,
                std::string_view memberName) {
          std::string receiverLeaf = extractHelperTail(receiverStructPath);
          constexpr std::string_view ValueSuffix = "Value";
          if (receiverLeaf.size() <= ValueSuffix.size() ||
              receiverLeaf.compare(receiverLeaf.size() - ValueSuffix.size(),
                                   ValueSuffix.size(),
                                   ValueSuffix) != 0) {
            return keyValueHelperMethodSpelling(memberName);
          }
          receiverLeaf.erase(receiverLeaf.size() - ValueSuffix.size());
          if (receiverLeaf.empty()) {
            return keyValueHelperMethodSpelling(memberName);
          }
          receiverLeaf.front() = static_cast<char>(
              std::tolower(static_cast<unsigned char>(receiverLeaf.front())));
          return receiverLeaf + pascalCaseHelperMember(memberName);
        }


        bool StatementsExprContext::isCanonicalKeyValueHelperFamilyPath(const std::string &path) {
          const auto *metadata = keyValueHelperMetadata();
          return metadata != nullptr &&
                 isCanonicalPublishedStdlibSurfaceHelperPath(
                     normalizeCollectionHelperPath(path), metadata->id);
        }


        bool StatementsExprContext::isKeyValueHelperMemberPath(const std::string &path,
                                              std::string_view expectedName) {
          std::string helperName;
          return resolveKeyValueHelperMemberName(path, helperName) &&
                 helperName == std::string(expectedName);
        }


        bool StatementsExprContext::importPathCoversPublishedTarget(const std::string &importPath, const std::string &targetPath) {
              if (importPath == targetPath) {
                return true;
              }
              if (importPath.size() >= 2 &&
                  importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
                const std::string prefix =
                    importPath.substr(0, importPath.size() - 2);
                return targetPath == prefix ||
                       targetPath.rfind(prefix + "/", 0) == 0;
              }
              return false;
            }


        bool StatementsExprContext::hasSemanticKeyValueHelperDefinition(std::string_view helperName) {
              const auto *metadata = keyValueHelperMetadata();
              if (semanticProgram == nullptr || metadata == nullptr) {
                return false;
              }
              const std::string helperPath =
                  stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
              if (helperPath.empty()) {
                return false;
              }
              if (findDirectHelperDefinition(helperPath) != nullptr) {
                return true;
              }
              auto importsHelper = [&](const std::vector<std::string> &imports) {
                return std::any_of(
                    imports.begin(),
                    imports.end(),
                    [&](const std::string &importPath) {
                      return importPathCoversPublishedTarget(importPath,
                                                             helperPath);
                    });
              };
              if (importsHelper(semanticProgram->sourceImports) ||
                  importsHelper(semanticProgram->imports)) {
                return true;
              }
              return std::any_of(
                  semanticProgram->definitions.begin(),
                  semanticProgram->definitions.end(),
                  [&](const auto &definition) {
                    return definition.fullPath == helperPath ||
                           definition.fullPath.rfind(helperPath + "__", 0) == 0;
                  });
            }


        std::string StatementsExprContext::semanticCollectionFamilyForExpr(const Expr &receiverExpr, const LocalMap &localsIn) {
          auto keyValueFamilyName = [&]() {
            const auto *metadata = keyValueHelperMetadata();
            if (metadata == nullptr || metadata->canonicalPath.empty()) {
              return std::string{};
            }
            const std::string canonicalPath(metadata->canonicalPath);
            const size_t slash = canonicalPath.find_last_of('/');
            return slash == std::string::npos
                       ? canonicalPath
                       : canonicalPath.substr(slash + 1);
          };
          if (semanticProgram == nullptr ||
              callResolutionAdapters.semanticProductTargets.semanticIndex
                  .collectionSpecializationsByExpr.empty()) {
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiverExpr.name);
              if (localIt != localsIn.end() &&
                  hasKeyValueKinds(localIt->second)) {
                return keyValueFamilyName();
              }
            }
            return std::string{};
          }
          const auto *collectionFact =
              ir_lowerer::findSemanticProductCollectionSpecialization(
                  callResolutionAdapters.semanticProductTargets.semanticIndex,
                  receiverExpr);
          if (collectionFact == nullptr && receiverExpr.sourceLine != 0 &&
              receiverExpr.sourceColumn != 0) {
            for (const auto &candidate :
                 semanticProgram->collectionSpecializations) {
              if (candidate.sourceLine == receiverExpr.sourceLine &&
                  candidate.sourceColumn == receiverExpr.sourceColumn) {
                collectionFact = &candidate;
                break;
              }
            }
          }
          if (collectionFact == nullptr) {
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiverExpr.name);
              if (localIt != localsIn.end() &&
                  hasKeyValueKinds(localIt->second)) {
                return keyValueFamilyName();
              }
            }
            return std::string{};
          }
          if (collectionFact->collectionFamilyId != InvalidSymbolId) {
            return trimTemplateTypeText(std::string(
                semanticProgramResolveCallTargetString(
                    *semanticProgram, collectionFact->collectionFamilyId)));
          }
          return trimTemplateTypeText(collectionFact->collectionFamily);
        }


        bool StatementsExprContext::populateKeyValueInfoFromCollectionFact(const SemanticProgramCollectionSpecialization &collectionFact,
                CollectionPairTypeInfo &targetInfoOut) {
          const auto *metadata = keyValueHelperMetadata();
          if (metadata == nullptr ||
              !collectionFact.helperSurfaceId.has_value() ||
              *collectionFact.helperSurfaceId != metadata->id) {
            return false;
          }
          auto resolveSemanticText = [&](const std::string &text, SymbolId textId) {
            if (semanticProgram != nullptr && textId != InvalidSymbolId) {
              return std::string(
                  semanticProgramResolveCallTargetString(*semanticProgram, textId));
            }
            return text;
          };
          targetInfoOut = {};
          targetInfoOut.isKeyValueTarget = true;
          targetInfoOut.keyValueKeyKind = valueKindFromTypeName(
              resolveSemanticText(collectionFact.keyTypeText,
                                  collectionFact.keyTypeTextId));
          targetInfoOut.keyValueValueKind = valueKindFromTypeName(
              resolveSemanticText(collectionFact.valueTypeText,
                                  collectionFact.valueTypeTextId));
          targetInfoOut.isWrappedKeyValueTarget =
              collectionFact.isReference || collectionFact.isPointer;
          targetInfoOut.structTypeName = resolveSemanticText(
              collectionFact.structPath, collectionFact.structPathId);
          return true;
        }


        bool StatementsExprContext::resolveLocalNameKeyValueInfo(const Expr &receiverExpr, CollectionPairTypeInfo &targetInfoOut) {
          if (semanticProgram == nullptr || receiverExpr.kind != Expr::Kind::Name) {
            return false;
          }
          for (const auto &collectionFact :
               semanticProgram->collectionSpecializations) {
            if (collectionFact.name != receiverExpr.name) {
              continue;
            }
            if (populateKeyValueInfoFromCollectionFact(
                    collectionFact, targetInfoOut)) {
              return true;
            }
          }
          return false;
        }


        bool StatementsExprContext::resolveKeyValueInfoFromCallReceiverQuery(const Expr &callExpr,
                const Expr &receiverExpr,
                CollectionPairTypeInfo &targetInfoOut) {
          if (semanticProgram == nullptr ||
              receiverExpr.kind != Expr::Kind::Name ||
              callExpr.sourceLine == 0 ||
              callExpr.sourceColumn == 0) {
            return false;
          }
          auto semanticQueryText =
              [&](const std::string &fallback, SymbolId textId) {
            if (textId != InvalidSymbolId) {
              std::string resolvedText = std::string(
                  semanticProgramResolveCallTargetString(
                      *semanticProgram, textId));
              if (!resolvedText.empty()) {
                return trimTemplateTypeText(resolvedText);
              }
            }
            return trimTemplateTypeText(fallback);
          };
          for (const auto &queryFact : semanticProgram->queryFacts) {
            if (queryFact.sourceLine != callExpr.sourceLine ||
                queryFact.sourceColumn != callExpr.sourceColumn) {
              continue;
            }
            const std::string receiverType = semanticQueryText(
                queryFact.receiverBindingTypeText,
                queryFact.receiverBindingTypeTextId);
            std::string receiverBase;
            std::string receiverArgsText;
            std::vector<std::string> receiverArgs;
            if (!splitTemplateTypeName(receiverType,
                                       receiverBase,
                                       receiverArgsText) ||
                normalizeCollectionBindingTypeName(receiverBase) != "map" ||
                !splitTemplateArgs(receiverArgsText, receiverArgs) ||
                receiverArgs.size() != 2) {
              continue;
            }
            targetInfoOut = {};
            targetInfoOut.isKeyValueTarget = true;
            targetInfoOut.keyValueKeyKind = valueKindFromTypeName(
                trimTemplateTypeText(receiverArgs.front()));
            targetInfoOut.keyValueValueKind = valueKindFromTypeName(
                trimTemplateTypeText(receiverArgs.back()));
            return true;
          }
          return false;
        }


        ir_lowerer::CollectionPairTypeInfo StatementsExprContext::resolveKeyValueAccessReceiverInfo(const Expr &callExpr, const Expr &receiverExpr, const LocalMap &localsIn) {
          auto resolvedInfo =
              ir_lowerer::resolveCollectionPairTypeInfo(
                  receiverExpr,
                  localsIn,
                  {},
                  semanticProgram,
                  &callResolutionAdapters.semanticProductTargets.semanticIndex);
          if (!resolvedInfo.isKeyValueTarget) {
            resolveLocalNameKeyValueInfo(receiverExpr, resolvedInfo);
          }
          if (!resolvedInfo.isKeyValueTarget) {
            resolveKeyValueInfoFromCallReceiverQuery(
                callExpr, receiverExpr, resolvedInfo);
          }
          return resolvedInfo;
        }


        bool StatementsExprContext::resolveSameFamilyKeyValueHelperMemberName(const Expr &callExpr, const Expr &receiverExpr,
                std::string &helperNameOut, const LocalMap &localsIn) {
              helperNameOut.clear();
              const auto *metadata = keyValueHelperMetadata();
              if (metadata == nullptr) {
                return false;
              }
              std::string rawPath =
                  normalizeCollectionHelperPath(resolveDirectHelperPath(callExpr));
              if (!rawPath.empty() && rawPath.front() == '/') {
                rawPath.erase(rawPath.begin());
              }
              const size_t slash = rawPath.find('/');
              if (slash == std::string::npos || slash == 0 ||
                  slash + 1 >= rawPath.size()) {
                return false;
              }
              const std::string family = semanticCollectionFamilyForExpr(receiverExpr, localsIn);
              if (family.empty() || rawPath.substr(0, slash) != family) {
                return false;
              }
              const std::string memberPath = rawPath.substr(slash + 1);
              const std::string memberLeaf = extractHelperTail(memberPath);
              const std::string_view memberName =
                  resolveStdlibSurfaceMemberName(*metadata, memberLeaf);
              if (memberName.empty()) {
                return false;
              }
              helperNameOut.assign(memberName);
              return true;
            }


        bool StatementsExprContext::isCanonicalKeyValueConstructorPath(const std::string &path) {
          const auto *metadata = keyValueConstructorMetadata();
          if (metadata == nullptr) {
            return false;
          }
          std::string constructorName;
          const std::string normalizedPath = normalizeCollectionHelperPath(path);
          return normalizedPath == metadata->canonicalPath &&
                 resolvePublishedStdlibSurfaceConstructorMemberName(
                     normalizedPath, metadata->id, constructorName) &&
                 constructorName == "map";
        }


        bool StatementsExprContext::isDirectCollectionHelperPath(const std::string &path) {
          return path.rfind("/array/", 0) == 0 ||
                 path.rfind(collectionMemberRoot("vector"), 0) == 0 ||
                 path.rfind(vectorBackingMemberRoot(), 0) == 0 ||
                 isCanonicalKeyValueHelperFamilyPath(path);
        }


        bool StatementsExprContext::hasKeyValueEntryCtorArgs(const Expr &callExpr) {
          auto isKeyValueEntryCallExpr = [&](const Expr &candidate) {
            if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
              return false;
            }
            std::string normalizedName;
            if (!candidate.name.empty() && candidate.name.front() == '/') {
              normalizedName = candidate.name.substr(1);
            } else if (!candidate.namespacePrefix.empty()) {
              normalizedName = candidate.namespacePrefix;
              if (!normalizedName.empty() && normalizedName.front() == '/') {
                normalizedName.erase(normalizedName.begin());
              }
              normalizedName += "/" + candidate.name;
            } else {
              normalizedName = candidate.name;
            }
            const auto generatedSuffix = normalizedName.find("__");
            if (generatedSuffix != std::string::npos) {
              normalizedName.erase(generatedSuffix);
            }
            return isKeyValueHelperMemberPath(normalizedName, "entry");
          };
          for (const auto &arg : callExpr.args) {
            if (isKeyValueEntryCallExpr(arg)) {
              return true;
            }
          }
          return false;
        }


        bool StatementsExprContext::isInternalSoaHelperFamilyName(const std::string &helperName) {
          return helperName.rfind("soaColumn", 0) == 0 ||
                 helperName.rfind("soaColumns", 0) == 0 ||
                 helperName.rfind("SoaColumn", 0) == 0 ||
                 helperName.rfind("SoaColumns", 0) == 0;
        }


        bool StatementsExprContext::isInternalSoaHelperFamilyPath(const std::string &path) {
          return isInternalSoaHelperFamilyName(
              extractHelperTail(normalizeCollectionHelperPath(path)));
        }


        bool StatementsExprContext::isSamePathSoaHelperPath(const std::string &path) {
          const std::string normalizedPath = stripGeneratedHelperSuffix(path);
          return normalizedPath.rfind("/soa/", 0) == 0 ||
                 normalizedPath == "/to_aos" ||
                 normalizedPath == "/to_aos_ref";
        }


        bool StatementsExprContext::isSoaWrapperHelperFamilyPath(const std::string &path) {
          const std::string normalizedPath = stripGeneratedHelperSuffix(path);
          return normalizedPath.rfind("/std/collections/soa/", 0) == 0 ||
                 normalizedPath.rfind(collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVector"), 0) == 0 ||
                 normalizedPath.rfind(collection_paths::memberPath(collection_paths::kExperimentalSoaVectorConversionsFolder, "soaVector"), 0) == 0;
        }


        const Definition * StatementsExprContext::findDirectInternalSoaDefinition(const std::string &rawPath) {
          const std::string helperName = extractHelperTail(rawPath);
          if (!isInternalSoaHelperFamilyName(helperName)) {
            return nullptr;
          }
          for (const auto &[path, def] : defMap) {
            if (def == nullptr ||
                path.rfind(collection_paths::modulePrefix(collection_paths::kInternalSoaStorageFolder), 0) != 0) {
              continue;
            }
            if (extractHelperTail(path) == helperName) {
              return def;
            }
          }
          return nullptr;
        }


        const Definition * StatementsExprContext::findDirectSoaWrapperDefinition(const Expr &callExpr,
                                                 const std::string &rawPath, const LocalMap &localsIn) {
          const std::string normalizedRawPath = stripGeneratedHelperSuffix(rawPath);
          if (const Definition *directSoaWrapper =
                  findDirectHelperDefinition(rawPath)) {
            return directSoaWrapper;
          }
          auto canonicalSamePathSoaWrapper = [](const std::string &path) {
            if (path == "/soa/count") {
              return std::string("/std/collections/soa/count");
            }
            if (path == "/soa/count_ref") {
              return std::string("/std/collections/soa/count_ref");
            }
            if (path == "/soa/get") {
              return std::string("/std/collections/soa/get");
            }
            if (path == "/soa/get_ref") {
              return std::string("/std/collections/soa/get_ref");
            }
            if (path == "/soa/ref") {
              return std::string("/std/collections/soa/ref");
            }
            if (path == "/soa/ref_ref") {
              return std::string("/std/collections/soa/ref_ref");
            }
            if (path == "/soa/reserve") {
              return std::string("/std/collections/soa/reserve");
            }
            if (path == "/soa/push") {
              return std::string("/std/collections/soa/push");
            }
            if (path == "/to_aos") {
              return std::string("/std/collections/soa/to_aos");
            }
            if (path == "/to_aos_ref") {
              return std::string("/std/collections/soa/to_aos_ref");
            }
            return std::string{};
          };
          if (const std::string canonicalPath =
                  canonicalSamePathSoaWrapper(normalizedRawPath);
              !canonicalPath.empty()) {
            return findDirectHelperDefinition(canonicalPath);
          }
          const bool isExperimentalSoaToAosCall =
              normalizedRawPath ==
              collection_paths::memberPath(collection_paths::kExperimentalSoaVectorConversionsFolder, "soaVectorToAos");
          if (isExperimentalSoaToAosCall && !callExpr.args.empty()) {
            const std::string receiverStruct =
                inferStructExprPath(callExpr.args.front(), localsIn);
            if (normalizeCollectionBindingTypeName(receiverStruct) == "soa") {
              if (const Definition *canonicalSoaToAos =
                      findDirectHelperDefinition("/std/collections/soa/to_aos")) {
                return canonicalSoaToAos;
              }
            }
          }
          return nullptr;
        }


        std::string StatementsExprContext::resolveSemanticCallTargetPath(const Expr &callExpr) {
          if (semanticProgram == nullptr || callExpr.sourceLine == 0 ||
              callExpr.sourceColumn == 0) {
            return std::string{};
          }
          for (const auto &queryFact : semanticProgram->queryFacts) {
            if (queryFact.sourceLine != callExpr.sourceLine ||
                queryFact.sourceColumn != callExpr.sourceColumn ||
                queryFact.resolvedPathId == InvalidSymbolId) {
              continue;
            }
            return std::string(semanticProgramResolveCallTargetString(
                *semanticProgram, queryFact.resolvedPathId));
          }
          return std::string{};
        }


        const Definition * StatementsExprContext::findDirectEntryKeyValueConstructorDefinition(const Expr &callExpr) {
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          const std::string normalizedRawPath =
              normalizeCollectionHelperPath(rawPath);
          if (!isCanonicalKeyValueConstructorPath(normalizedRawPath)) {
            return nullptr;
          }
          for (const auto &[path, def] : defMap) {
            if (def == nullptr || def->parameters.empty() ||
                !isArgsPackBinding(def->parameters.front())) {
              continue;
            }
            if (normalizeCollectionHelperPath(path) == normalizedRawPath &&
                extractHelperTail(path) == "map") {
              return def;
            }
          }
          return nullptr;
        }


        const Definition * StatementsExprContext::findDirectStructDefinition(const Expr &callExpr) {
          if (!ir_lowerer::isStructConstructorCallShape(callExpr)) {
            return nullptr;
          }
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
              rawDef != nullptr && ir_lowerer::isStructDefinition(*rawDef)) {
            return rawDef;
          }
          std::string directStructPath;
          if (!resolveStructTypeName(callExpr.name, callExpr.namespacePrefix, directStructPath)) {
            return nullptr;
          }
          if (const Definition *structDef = findDirectHelperDefinition(directStructPath);
              structDef != nullptr && ir_lowerer::isStructDefinition(*structDef)) {
            return structDef;
          }
          return nullptr;
        }


        bool StatementsExprContext::resolveBuiltinAccessName(const Expr &callExpr,
                                            std::string &accessNameOut) {
          if (getBuiltinArrayAccessName(callExpr, accessNameOut)) {
            return true;
          }
          const std::string resolvedAccessPath = resolveExprPath(callExpr);
          if (resolvedAccessPath == "/at") {
            accessNameOut = "at";
            return true;
          }
          if (resolvedAccessPath == "/at_unsafe") {
            accessNameOut = "at_unsafe";
            return true;
          }
          std::string vectorHelperName;
          if (resolveVectorHelperAliasName(callExpr, vectorHelperName) &&
              (vectorHelperName == "at" || vectorHelperName == "at_unsafe")) {
            accessNameOut = vectorHelperName;
            return true;
          }
          return false;
        }


        bool StatementsExprContext::resolveHelperReturnedArrayVectorAccessTargetInfo(const Expr &targetCallExpr,
                ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut, const LocalMap &localsIn) {
              targetInfoOut = {};
              auto resolveSpecializedVectorElementKind =
                  [&](const std::string &typeText,
                      ir_lowerer::LocalInfo::ValueKind &elemKindOut) {
                    elemKindOut = ir_lowerer::LocalInfo::ValueKind::Unknown;
                    std::string normalized =
                        ir_lowerer::trimTemplateTypeText(typeText);
                    if (!normalized.empty() && normalized.front() != '/') {
                      normalized.insert(normalized.begin(), '/');
                    }
                    if (!matchesGeneratedSpecializedType(
                            normalized, "vector", "Vector")) {
                      return false;
                    }
                    Expr syntheticExpr;
                    syntheticExpr.kind = Expr::Kind::Call;
                    syntheticExpr.name = normalized;
                    const Definition *structDef =
                        resolveDefinitionCall(syntheticExpr);
                    if (structDef == nullptr ||
                        !ir_lowerer::isStructDefinition(*structDef)) {
                      return false;
                    }
                    for (const auto &fieldExpr : structDef->statements) {
                      if (!fieldExpr.isBinding || fieldExpr.name != "data") {
                        continue;
                      }
                      std::string typeName;
                      std::vector<std::string> templateArgs;
                      if (!ir_lowerer::extractFirstBindingTypeTransform(
                              fieldExpr, typeName, templateArgs) ||
                          ir_lowerer::normalizeCollectionBindingTypeName(
                              typeName) != "Pointer" ||
                          templateArgs.size() != 1) {
                        continue;
                      }
                      std::string elementType =
                          ir_lowerer::trimTemplateTypeText(
                              templateArgs.front());
                      if (!ir_lowerer::extractTopLevelUninitializedTypeText(
                              elementType, elementType)) {
                        continue;
                      }
                      elemKindOut =
                          ir_lowerer::valueKindFromTypeName(elementType);
                      return elemKindOut !=
                             ir_lowerer::LocalInfo::ValueKind::Unknown;
                    }
                    return false;
                  };
              const std::string inferredReceiverStruct =
                  inferStructExprPath(targetCallExpr, localsIn);
              if (matchesGeneratedSpecializedType(
                      inferredReceiverStruct, "vector", "Vector")) {
                ir_lowerer::LocalInfo::ValueKind elemKind;
                if (!resolveSpecializedVectorElementKind(inferredReceiverStruct,
                                                        elemKind)) {
                  return false;
                }
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = true;
                targetInfoOut.elemKind = elemKind;
                targetInfoOut.structTypeName = inferredReceiverStruct;
                return true;
              }
              if (inferredReceiverStruct.rfind(
                      collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0 ||
                  normalizeCollectionBindingTypeName(inferredReceiverStruct) ==
                      "soa") {
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = false;
                targetInfoOut.isSoaVector = true;
                targetInfoOut.structTypeName = inferredReceiverStruct;
                return true;
              }
              const Definition *callee =
                  resolveDirectHelperDefinition(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee,
                                                             collectionName,
                                                             collectionArgs)) {
                return false;
              }
              if ((collectionName != "array" && collectionName != "vector" &&
                   collectionName != "soa") ||
                  collectionArgs.size() != 1) {
                return false;
              }
              targetInfoOut.isArrayOrVectorTarget = true;
              targetInfoOut.isVectorTarget = (collectionName == "vector");
              targetInfoOut.isSoaVector = (collectionName == "soa");
              targetInfoOut.elemKind =
                  ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              if (targetInfoOut.isSoaVector) {
                std::string elementTypeName =
                    trimTemplateTypeText(collectionArgs.front());
                if (!elementTypeName.empty() &&
                    elementTypeName.front() == '/') {
                  elementTypeName.erase(elementTypeName.begin());
                }
                targetInfoOut.structTypeName =
                    specializedExperimentalSoaVectorStructPathForElementType(
                        elementTypeName);
              }
              return true;
            }


        bool StatementsExprContext::tryEmitBuiltinKeyValueConstructor(const Expr &callExpr,
                const std::string &resolvedCallPath,
                bool &handledOut, const LocalMap &localsIn) {
          handledOut = false;
          std::string builtinCollectionName;
          const bool isBuiltinKeyValueConstructor =
              getBuiltinCollectionName(callExpr, builtinCollectionName) &&
              builtinCollectionName == "map";
          if (callExpr.isMethodCall ||
              (!isCanonicalKeyValueConstructorPath(resolvedCallPath) &&
               !isBuiltinKeyValueConstructor)) {
            return true;
          }
          if (callExpr.templateArgs.size() != 2) {
            return true;
          }
          const std::string diagnosticCallPath =
              isBuiltinKeyValueConstructor ? canonicalKeyValueConstructorPath()
                                           : resolvedCallPath;
          if ((callExpr.args.size() % 2) != 0) {
            error = "argument count mismatch for " +
                    normalizeCollectionHelperPath(diagnosticCallPath);
            handledOut = true;
            return false;
          }

          const auto keyKind =
              ir_lowerer::valueKindFromTypeName(callExpr.templateArgs.front());
          if (keyKind == ir_lowerer::LocalInfo::ValueKind::Unknown) {
            error = "native backend requires typed map constructor keys";
            handledOut = true;
            return false;
          }

          const int32_t mapLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(mapLocal)});

          for (size_t argIndex = 0; argIndex < callExpr.args.size(); argIndex += 2) {
            const int32_t keyLocal = allocTempLocal();
            if (!emitExpr(callExpr.args[argIndex], localsIn)) {
              handledOut = true;
              return false;
            }
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(keyLocal)});

            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(callExpr.args[argIndex + 1], localsIn)) {
              handledOut = true;
              return false;
            }
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});

            if (!ir_lowerer::emitBuiltinCanonicalMapInsertOverwriteOrGrow(
                    mapLocal,
                    -1,
                    mapLocal,
                    keyLocal,
                    valueLocal,
                    keyKind,
                    [&]() { return allocTempLocal(); },
                    [&]() { return function.instructions.size(); },
                    [&](IrOpcode op, uint64_t imm) {
                      function.instructions.push_back({op, imm});
                    },
                    [&](size_t indexToPatch, uint64_t target) {
                      function.instructions[indexToPatch].imm = target;
                    })) {
              error = "failed to lower builtin canonical map constructor";
              handledOut = true;
              return false;
            }
          }

          function.instructions.push_back(
              {IrOpcode::LoadLocal, static_cast<uint64_t>(mapLocal)});
          handledOut = true;
          return true;
        }


} // namespace primec::ir_lowerer
