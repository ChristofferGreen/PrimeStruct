// soa-surface-audit: exempt
// collection-surface-audit: exempt
#include "IrLowererLowerEmitExprTailDispatchHelpers.h"

#include <algorithm>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"

namespace primec::ir_lowerer {

TailDispatchContext::TailDispatchContext(LowerSetupStageState &setupStageIn,
                                          LowerReturnEmitStageState &stateOutIn,
                                          const CallResolutionAdapters &callResolutionAdaptersIn,
                                          std::string &errorIn)
    : setupStage(setupStageIn),
      stateOut(stateOutIn),
      callResolutionAdapters(callResolutionAdaptersIn),
      error(errorIn),
      semanticProgram(callResolutionAdaptersIn.semanticProgram),
      defMap(setupStageIn.defMap),
      resolveExprPath(callResolutionAdaptersIn.resolveExprPath),
      resolveDefinitionCall(stateOutIn.resolveDefinitionCall),
      getMathBuiltinName(stateOutIn.getMathBuiltinName),
      resolveMethodCallDefinition(setupStageIn.inferenceSetupBootstrap.resolveMethodCallDefinition) {}

const SemanticProductIndex * TailDispatchContext::semanticIndexPtr() {
  return semanticProgram == nullptr ? nullptr : &callResolutionAdapters.semanticProductTargets.semanticIndex;
}

std::string TailDispatchContext::resolveTailDispatchDirectHelperPath(const Expr &candidate) {
          if (!candidate.name.empty() && candidate.name.front() == '/') {
            return candidate.name;
          }
          if (!candidate.namespacePrefix.empty()) {
            std::string scoped = candidate.namespacePrefix;
            if (!scoped.empty() && scoped.front() != '/') {
              scoped.insert(scoped.begin(), '/');
            }
            return scoped + "/" + candidate.name;
          }
          return candidate.name;
        };


const Definition * TailDispatchContext::resolveTailDispatchDirectHelperDefinition(const Expr &candidate) {
          auto stripGeneratedLeafSuffix = [](std::string helperPath) {
            const size_t leafStart = helperPath.find_last_of('/');
            const size_t searchStart =
                leafStart == std::string::npos ? 0 : leafStart + 1;
            const size_t generatedSuffix = helperPath.find("__", searchStart);
            if (generatedSuffix != std::string::npos) {
              helperPath.erase(generatedSuffix);
            }
            return helperPath;
          };
          auto findDirectHelperDefinition = [&](const std::string &path)
              -> const Definition * {
            auto findByPath = [&](const std::string &lookupPath)
                -> const Definition * {
              auto defIt = defMap.find(lookupPath);
              if (defIt != defMap.end()) {
                return defIt->second;
              }
              auto matchesGeneratedLeafDefinition =
                  [&](const std::string &candidatePath,
                      const char *marker,
                      size_t markerSize) {
                    return candidatePath.rfind(lookupPath, 0) == 0 &&
                           candidatePath.compare(
                               lookupPath.size(), markerSize, marker) == 0 &&
                           candidatePath.find('/',
                                              lookupPath.size() + markerSize) ==
                               std::string::npos;
                  };
              for (const auto &[candidatePath, def] : defMap) {
                if (def == nullptr) {
                  continue;
                }
                if (matchesGeneratedLeafDefinition(candidatePath, "__t", 3) ||
                    matchesGeneratedLeafDefinition(candidatePath, "__ov", 4) ||
                    matchesGeneratedLeafDefinition(candidatePath, "<", 1)) {
                  return def;
                }
              }
              return nullptr;
            };
            if (const Definition *direct = findByPath(path);
                direct != nullptr) {
              return direct;
            }
            const std::string canonicalPath = stripGeneratedLeafSuffix(path);
            if (canonicalPath != path) {
              return findByPath(canonicalPath);
            }
            return nullptr;
          };
          auto isSamePathSoaHelperPath = [&](std::string helperPath) {
            helperPath = stripGeneratedLeafSuffix(std::move(helperPath));
            return helperPath.rfind("/soa/", 0) == 0 ||
                   helperPath == "/to_aos" ||
                   helperPath == "/to_aos_ref";
          };
          if (!candidate.isMethodCall) {
            const std::string semanticTarget =
                findSemanticProductDirectCallTarget(semanticProgram, candidate);
            if (isSamePathSoaHelperPath(semanticTarget)) {
              if (const Definition *semanticDef =
                      findDirectHelperDefinition(semanticTarget)) {
                return semanticDef;
              }
            }
          }
          if (const Definition *callee = resolveDefinitionCall(candidate);
              callee != nullptr) {
            return callee;
          }
          const std::string rawPath =
              resolveTailDispatchDirectHelperPath(candidate);
          if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
              rawDef != nullptr) {
            return rawDef;
          }
          const std::string resolvedPath = resolveExprPath(candidate);
          if (resolvedPath != rawPath) {
            if (const Definition *resolvedDef =
                    findDirectHelperDefinition(resolvedPath);
                resolvedDef != nullptr) {
              return resolvedDef;
            }
          }
          if (candidate.isMethodCall) {
            const std::string semanticTarget =
                findSemanticProductMethodCallTarget(semanticProgram, candidate);
            if (!semanticTarget.empty()) {
              return findDirectHelperDefinition(semanticTarget);
            }
          }
          return nullptr;
        };


const StdlibSurfaceMetadata * TailDispatchContext::tailDispatchKeyValueHelperMetadata() {
          return primec::ir_lowerer::keyValueHelperSurfaceMetadata();
        };


std::optional<primec::StdlibSurfaceId> TailDispatchContext::tailDispatchKeyValueHelperSurfaceId() {
          const auto *metadata = tailDispatchKeyValueHelperMetadata();
          if (metadata == nullptr) {
            return std::nullopt;
          }
          return metadata->id;
        };


bool TailDispatchContext::isTailDispatchKeyValueHelperSurface(std::optional<primec::StdlibSurfaceId> surfaceId) {
          const auto keyValueSurfaceId = tailDispatchKeyValueHelperSurfaceId();
          return keyValueSurfaceId.has_value() && surfaceId == keyValueSurfaceId;
        };


bool TailDispatchContext::isTailDispatchKeyValueImportAliasHelperPath(std::string_view path, std::string_view helperName) {
          const auto *metadata = tailDispatchKeyValueHelperMetadata();
          if (metadata == nullptr) {
            return false;
          }
          const std::string_view resolvedHelperName =
              primec::resolveStdlibSurfaceMemberName(*metadata, path);
          if (resolvedHelperName != helperName) {
            return false;
          }
          for (std::string_view alias : metadata->importAliasSpellings) {
            if (alias.empty() || alias.find('/') != std::string_view::npos) {
              continue;
            }
            std::string rootedAlias = "/" + std::string(alias) + "/";
            if (path.rfind(rootedAlias, 0) == 0) {
              return true;
            }
          }
          return false;
        };


bool TailDispatchContext::hasPublishedSemanticKeyValueSurface(const Expr &callExpr) {
          return isTailDispatchKeyValueHelperSurface(
                     findSemanticProductDirectCallStdlibSurfaceId(
                         semanticProgram, callExpr)) ||
                 isTailDispatchKeyValueHelperSurface(
                     findSemanticProductBridgePathChoiceStdlibSurfaceId(
                         semanticProgram, callExpr));
        };


bool TailDispatchContext::resolvePublishedTailDispatchKeyValueHelperName(const Expr &callExpr, std::string &helperNameOut) {
              helperNameOut.clear();
              const auto keyValueSurfaceId = tailDispatchKeyValueHelperSurfaceId();
              if (!keyValueSurfaceId.has_value()) {
                return false;
              }
              return ir_lowerer::resolvePublishedSemanticStdlibSurfaceMemberName(
                         semanticProgram,
                         callExpr,
                         *keyValueSurfaceId,
                         helperNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceExprMemberName(
                         callExpr,
                         *keyValueSurfaceId,
                         helperNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                         resolveExprPath(callExpr),
                         *keyValueSurfaceId,
                         helperNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                         resolveTailDispatchDirectHelperPath(callExpr),
                         *keyValueSurfaceId,
                         helperNameOut);
            };


bool TailDispatchContext::resolveBuiltinKeyValueHelperName(const Expr &callExpr, bool allowMethodCall, std::string &helperNameOut) {
              helperNameOut.clear();
              if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
                return false;
              }
              if (allowMethodCall && callExpr.isMethodCall) {
                if (resolvePublishedTailDispatchKeyValueHelperName(callExpr, helperNameOut)) {
                  return true;
                }
                if (callExpr.namespacePrefix.empty() &&
                    callExpr.name.find('/') == std::string::npos &&
                    (callExpr.name == "count" || callExpr.name == "contains" ||
                    callExpr.name == "tryAt" || callExpr.name == "at" ||
                     callExpr.name == "at_unsafe")) {
                  helperNameOut = callExpr.name;
                  return true;
                }
                return false;
              }
              if (callExpr.isMethodCall) {
                return false;
              }
              if (ir_lowerer::resolveKeyValueHelperAliasName(callExpr, helperNameOut)) {
                return true;
              }
              return resolvePublishedTailDispatchKeyValueHelperName(callExpr, helperNameOut);
            };


bool TailDispatchContext::inferCallKeyValueTargetInfo(const Expr &targetExpr, ir_lowerer::CollectionPairTypeInfo &out, const LocalMap &localsIn) {
          out = {};
          const Definition *callee =
              resolveTailDispatchDirectHelperDefinition(targetExpr);
          if (callee == nullptr) {
            return false;
          }
          std::string collectionName;
          std::vector<std::string> collectionArgs;
          if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) ||
              collectionName != "map" ||
              collectionArgs.size() != 2) {
            return ir_lowerer::inferForwardedCollectionPairTypeInfo(
                targetExpr, *callee, localsIn, {}, out);
          }
          out.isKeyValueTarget = true;
          out.keyValueKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
          out.keyValueValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
          return out.keyValueKeyKind != ir_lowerer::LocalInfo::ValueKind::Unknown &&
                 out.keyValueValueKind != ir_lowerer::LocalInfo::ValueKind::Unknown;
        };


bool TailDispatchContext::rewriteBuiltinKeyValueInsertBuiltinExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.size() != 3) {
            return false;
          }
          auto matchesPublishedKeyValueInsertPath = [&](const Expr &candidate) {
            std::string helperName;
            if (hasPublishedSemanticKeyValueSurface(candidate) &&
                resolvePublishedTailDispatchKeyValueHelperName(candidate, helperName) &&
                (helperName == "insert" || helperName == "insert_ref")) {
              return true;
            }
            const std::string resolvedPath = resolveExprPath(candidate);
            const auto keyValueSurfaceId = tailDispatchKeyValueHelperSurfaceId();
            return keyValueSurfaceId.has_value() &&
                   ir_lowerer::isPublishedStdlibSurfaceLoweringPath(
                       resolvedPath, *keyValueSurfaceId) &&
                   ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                       resolvedPath, *keyValueSurfaceId, helperName) &&
                   (helperName == "insert" || helperName == "insert_ref");
          };
          auto stripGeneratedHelperSuffix = [](std::string helperName) {
            const size_t generatedSuffix = helperName.find("__");
            if (generatedSuffix != std::string::npos) {
              helperName.erase(generatedSuffix);
            }
            return helperName;
          };
          auto isDirectBareKeyValueInsertHelperStem = [&](const Expr &candidate) {
            if (candidate.isMethodCall || candidate.name.empty()) {
              return false;
            }
            std::string helperName =
                resolveTailDispatchDirectHelperPath(candidate);
            if (!helperName.empty() && helperName.front() == '/') {
              helperName.erase(helperName.begin());
            }
            if (helperName.find('/') != std::string::npos) {
              return false;
            }
            helperName = stripGeneratedHelperSuffix(std::move(helperName));
            return helperName == "insert" || helperName == "insert_ref" ||
                   helperName == "Insert" || helperName == "InsertRef";
          };

          size_t receiverIndex = 0;
          if (callExpr.isMethodCall) {
            std::string helperName;
            if (!resolveBuiltinKeyValueHelperName(callExpr, true, helperName) ||
                (helperName != "insert" && helperName != "insert_ref")) {
              return false;
            }
          } else {
            std::string helperName;
            if ((!ir_lowerer::resolveKeyValueHelperAliasName(callExpr, helperName) ||
                 (helperName != "insert" && helperName != "insert_ref")) &&
                !isDirectBareKeyValueInsertHelperStem(callExpr)) {
              return false;
            }
            if (hasNamedArguments(callExpr.argNames)) {
              for (size_t i = 0; i < callExpr.args.size(); ++i) {
                if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
                    *callExpr.argNames[i] == "values") {
                  receiverIndex = i;
                  break;
                }
              }
            }
          }

          if (receiverIndex >= callExpr.args.size()) {
            return false;
          }
          const auto targetInfo =
              ir_lowerer::resolveCollectionPairTypeInfo(
                  callExpr.args[receiverIndex],
                  localsIn,
                  [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                    return inferCallKeyValueTargetInfo(keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                  },
                  semanticProgram,
                  semanticIndexPtr());
          if (!targetInfo.isKeyValueTarget &&
              !matchesPublishedKeyValueInsertPath(callExpr)) {
            return false;
          }
          if (targetInfo.isWrappedKeyValueTarget ||
              ir_lowerer::isKeyValueStorageStructPath(targetInfo.structTypeName)) {
            return false;
          }

          rewrittenExpr = callExpr;
          rewrittenExpr.name = ir_lowerer::canonicalKeyValueHelperPath("insert");
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.isFieldAccess = false;
          // Force rewritten rooted helper calls to resolve from their new path
          // instead of stale semantic-product call-target facts.
          rewrittenExpr.semanticNodeId = 0;
          if (rewrittenExpr.templateArgs.empty() &&
              targetInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
              targetInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
            rewrittenExpr.templateArgs = {
                ir_lowerer::typeNameForValueKind(targetInfo.keyValueKeyKind),
                ir_lowerer::typeNameForValueKind(targetInfo.keyValueValueKind),
            };
          }
          return true;
        };


bool TailDispatchContext::rewriteCanonicalKeyValueHelperForExperimentalReceiverExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn) {
          std::string helperName;
          if (!resolveBuiltinKeyValueHelperName(callExpr, true, helperName)) {
            return false;
          }
          if (hasPublishedSemanticKeyValueSurface(callExpr)) {
            return false;
          }
          const auto keyValueSurfaceId = tailDispatchKeyValueHelperSurfaceId();
          if (!keyValueSurfaceId.has_value()) {
            return false;
          }
          if (ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  resolveTailDispatchDirectHelperPath(callExpr),
                  *keyValueSurfaceId) ||
              (hasPublishedSemanticKeyValueSurface(callExpr) &&
               ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                   resolveExprPath(callExpr), *keyValueSurfaceId))) {
            return false;
          }
          const size_t expectedArgCount = helperName == "count" ? 1u : 2u;
          if (callExpr.args.size() != expectedArgCount) {
            return false;
          }
          const SemanticProductIndex *const canonicalKeyValueHelperSemanticIndexPtr =
              semanticIndexPtr();
          auto isSpecializedExperimentalKeyValueStructPath =
              [](const std::string &structPath) {
                if (!ir_lowerer::isKeyValueStorageStructPath(structPath)) {
                  return false;
                }
                return structPath.find("__") != std::string::npos;
              };
          auto hasCanonicalKeyValueHelperSemanticReceiverFact = [&](const Expr &receiverExpr) {
            return semanticProgram != nullptr &&
                   canonicalKeyValueHelperSemanticIndexPtr != nullptr &&
                   receiverExpr.semanticNodeId != 0 &&
                   (ir_lowerer::findSemanticProductCollectionSpecialization(
                        *canonicalKeyValueHelperSemanticIndexPtr, receiverExpr) != nullptr ||
                    ir_lowerer::findSemanticProductQueryFact(
                        semanticProgram, *canonicalKeyValueHelperSemanticIndexPtr, receiverExpr) != nullptr ||
                    ir_lowerer::findSemanticProductBindingFact(
                        *canonicalKeyValueHelperSemanticIndexPtr, receiverExpr) != nullptr ||
                    ir_lowerer::findSemanticProductLocalAutoFact(
                        semanticProgram, *canonicalKeyValueHelperSemanticIndexPtr, receiverExpr) != nullptr);
          };
          const auto receiverKeyValueTargetInfo =
              ir_lowerer::resolveCollectionPairTypeInfo(
                  callExpr.args.front(),
                  localsIn,
                  [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                    return inferCallKeyValueTargetInfo(keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                  },
                  semanticProgram,
                  canonicalKeyValueHelperSemanticIndexPtr);
          if (receiverKeyValueTargetInfo.isWrappedKeyValueTarget) {
            return false;
          }

          auto inferExperimentalKeyValueStructPath = [&](const Expr &receiverExpr) {
            const auto keyValueTargetInfo =
                ir_lowerer::resolveCollectionPairTypeInfo(
                    receiverExpr,
                    localsIn,
                    [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                      return inferCallKeyValueTargetInfo(keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                    },
                    semanticProgram,
                    canonicalKeyValueHelperSemanticIndexPtr);
            if (isSpecializedExperimentalKeyValueStructPath(keyValueTargetInfo.structTypeName)) {
              return keyValueTargetInfo.structTypeName;
            }
            if (hasCanonicalKeyValueHelperSemanticReceiverFact(receiverExpr)) {
              return std::string{};
            }
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(receiverExpr.name);
              if (it != localsIn.end() &&
                  !it->second.isArgsPack &&
                  it->second.kind != LocalInfo::Kind::Reference &&
                  it->second.kind != LocalInfo::Kind::Pointer &&
                  isSpecializedExperimentalKeyValueStructPath(it->second.structTypeName)) {
                return it->second.structTypeName;
              }
            }
            const auto accessTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(
                    receiverExpr,
                    localsIn,
                    {},
                    semanticProgram,
                    canonicalKeyValueHelperSemanticIndexPtr);
            if (accessTargetInfo.isWrappedKeyValueTarget) {
              if (isSpecializedExperimentalKeyValueStructPath(keyValueTargetInfo.structTypeName)) {
                return keyValueTargetInfo.structTypeName;
              }
              return std::string{};
            }
            if (isSpecializedExperimentalKeyValueStructPath(accessTargetInfo.structTypeName)) {
              return accessTargetInfo.structTypeName;
            }
            return std::string{};
          };
          const std::string receiverStructPath =
              inferExperimentalKeyValueStructPath(callExpr.args.front());
          if (receiverStructPath.empty()) {
            return false;
          }

          Expr methodExpr = callExpr;
          methodExpr.isMethodCall = true;
          methodExpr.name = helperName;
          methodExpr.namespacePrefix.clear();
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee == nullptr) {
            return false;
          }
          const bool isExperimentalKeyValueHelper =
              isSpecializedExperimentalKeyValueStructPath(callee->fullPath);
          if (!isExperimentalKeyValueHelper) {
            return false;
          }

          rewrittenExpr = callExpr;
          rewrittenExpr.name = callee->fullPath;
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.semanticNodeId = 0;
          rewrittenExpr.templateArgs.clear();
          return true;
        };


bool TailDispatchContext::isVectorStructPath(const std::string &structPath) {
          const std::string vectorTypePath = collection_paths::memberPath(
              collection_paths::kVectorFolder, collection_paths::kVectorTypeName);
          return structPath == "/vector" ||
                 structPath == vectorTypePath ||
                 structPath.rfind(vectorTypePath + "__", 0) == 0;
        };


bool TailDispatchContext::publishedKeyValueAccessHelperReturnsString(std::string_view helperName) {
          if (semanticProgram == nullptr ||
              (helperName != "at" && helperName != "at_unsafe")) {
            return false;
          }
          const std::string helperPath =
              ir_lowerer::canonicalKeyValueHelperPath(helperName);
          const auto helperPathId =
              semanticProgramLookupCallTargetStringId(*semanticProgram, helperPath);
          if (!helperPathId.has_value()) {
            return false;
          }
          const SemanticProgramReturnFact *returnFact =
              semanticProgramLookupPublishedReturnFactByDefinitionPathId(
                  *semanticProgram, *helperPathId);
          if (returnFact == nullptr) {
            return false;
          }
          auto resolveReturnTypeText = [&](const std::string &text,
                                           SymbolId textId) {
            if (textId != InvalidSymbolId) {
              const std::string_view resolved =
                  semanticProgramResolveCallTargetString(*semanticProgram,
                                                         textId);
              if (!resolved.empty()) {
                return ir_lowerer::trimTemplateTypeText(std::string(resolved));
              }
            }
            return ir_lowerer::trimTemplateTypeText(text);
          };
          const std::string structPath =
              resolveReturnTypeText(returnFact->structPath,
                                    returnFact->structPathId);
          const std::string bindingType =
              resolveReturnTypeText(returnFact->bindingTypeText,
                                    returnFact->bindingTypeTextId);
          return structPath == "string" || structPath == "/string" ||
                 bindingType == "string" || bindingType == "/string";
        };


bool TailDispatchContext::hasExplicitStdKeyValueHelperSpelling(const Expr &callExpr) {
          auto hasStdKeyValuePrefix = [](std::string_view text) {
            return text.rfind(ir_lowerer::collectionMemberRoot("map"), 0) == 0 ||
                   text.rfind(ir_lowerer::collectionMemberRoot("map", false), 0) == 0;
          };
          return hasStdKeyValuePrefix(callExpr.name) ||
                 hasStdKeyValuePrefix(callExpr.namespacePrefix);
        };


bool TailDispatchContext::rewriteExplicitKeyValueHelperBuiltinExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn) {
          std::string helperName;
          if (!resolveBuiltinKeyValueHelperName(callExpr, false, helperName) ||
              (helperName != "count" && helperName != "contains" &&
               helperName != "tryAt" && helperName != "at" &&
               helperName != "at_unsafe")) {
            return false;
          }
          if (callExpr.sourceIsMethodCall &&
              !hasExplicitStdKeyValueHelperSpelling(callExpr) &&
              publishedKeyValueAccessHelperReturnsString(helperName)) {
            return false;
          }
          const auto keyValueSurfaceId = tailDispatchKeyValueHelperSurfaceId();
          if (!keyValueSurfaceId.has_value()) {
            return false;
          }
          const SemanticProductIndex *const explicitKeyValueHelperSemanticIndexPtr =
              semanticIndexPtr();
          const auto keyValueTargetInfo =
              ir_lowerer::resolveCollectionPairTypeInfo(
                  callExpr.args.front(),
                  localsIn,
                  [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                    return inferCallKeyValueTargetInfo(keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                  },
                  semanticProgram,
                  explicitKeyValueHelperSemanticIndexPtr);
          const auto receiverTargetInfo =
              ir_lowerer::resolveArrayVectorAccessTargetInfo(
                  callExpr.args.front(),
                  localsIn,
                  {},
                  semanticProgram,
                  explicitKeyValueHelperSemanticIndexPtr);
          const bool isCanonicalStdKeyValueHelperPath =
              ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  resolveTailDispatchDirectHelperPath(callExpr),
                  *keyValueSurfaceId) ||
              (hasPublishedSemanticKeyValueSurface(callExpr) &&
               ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                   resolveExprPath(callExpr), *keyValueSurfaceId));
          if (isCanonicalStdKeyValueHelperPath) {
            // Keep canonical stdlib helper calls intact so direct overrides on
            // the published key/value helper surface are honored.
            return false;
          }
          const std::string rawPath = resolveTailDispatchDirectHelperPath(callExpr);
          if ((helperName == "count" || helperName == "contains" ||
               helperName == "tryAt" || helperName == "at" ||
               helperName == "at_unsafe") &&
              isTailDispatchKeyValueImportAliasHelperPath(rawPath, helperName) &&
              normalizeCollectionHelperPath(rawPath) ==
                  normalizeCollectionHelperPath(resolveExprPath(callExpr))) {
            return false;
          }
          if ((helperName == "count" || helperName == "contains" ||
               helperName == "tryAt" || helperName == "at" ||
               helperName == "at_unsafe" || helperName == "insert" ||
               helperName == "insert_ref") &&
              resolveDefinitionCall(callExpr) != nullptr &&
              !isCanonicalStdKeyValueHelperPath) {
            return false;
          }
          const std::string resolvedHelperPath = resolveExprPath(callExpr);
          if (ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  resolvedHelperPath, *keyValueSurfaceId) &&
              !isCanonicalStdKeyValueHelperPath) {
            // Keep canonical stdlib helper paths intact so custom overrides on
            // the published key/value helper surface retain their resolved return types.
            return false;
          }
          std::string receiverAccessName;
          const bool receiverIsIndexedArgsPackElement =
              callExpr.args.front().kind == Expr::Kind::Call &&
              getBuiltinArrayAccessName(callExpr.args.front(), receiverAccessName) &&
              callExpr.args.front().args.size() == 2;
          if (helperName == "at" &&
              receiverTargetInfo.isArgsPackTarget &&
              receiverTargetInfo.argsPackElementKind == LocalInfo::Kind::Value &&
              receiverTargetInfo.isKeyValueTarget) {
            rewrittenExpr = callExpr;
            rewrittenExpr.name = "at";
            rewrittenExpr.namespacePrefix.clear();
            return true;
          }
          if (receiverTargetInfo.isArgsPackTarget &&
              receiverTargetInfo.argsPackElementKind == LocalInfo::Kind::Value &&
              receiverTargetInfo.isKeyValueTarget &&
              !receiverIsIndexedArgsPackElement) {
            return false;
          }
          if (!keyValueTargetInfo.isKeyValueTarget) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = helperName;
          rewrittenExpr.namespacePrefix.clear();
          // Force later call resolution to use the rewritten builtin helper
          // shape instead of stale semantic-product direct-call targets.
          rewrittenExpr.semanticNodeId = 0;
          rewrittenExpr.templateArgs.clear();
          return true;
        };


std::string TailDispatchContext::resolveSemanticReceiverTypeText(const std::string &typeText, SymbolId typeTextId) {
          if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
            std::string resolvedTypeText = std::string(
                semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
            if (!resolvedTypeText.empty()) {
              return ir_lowerer::trimTemplateTypeText(resolvedTypeText);
            }
          }
          return ir_lowerer::trimTemplateTypeText(typeText);
        };


bool TailDispatchContext::rewriteSameFamilyKeyValueCountExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall ||
              callExpr.args.size() != 1) {
            return false;
          }
          const auto *metadata = tailDispatchKeyValueHelperMetadata();
          if (metadata == nullptr || metadata->canonicalPath.empty()) {
            return false;
          }
          std::string rawPath =
              normalizeCollectionHelperPath(
                  resolveTailDispatchDirectHelperPath(callExpr));
          if (!rawPath.empty() && rawPath.front() == '/') {
            rawPath.erase(rawPath.begin());
          }
          const size_t slash = rawPath.find('/');
          if (slash == std::string::npos || slash == 0 ||
              slash + 1 >= rawPath.size()) {
            return false;
          }
          const std::string memberLeaf =
              rawPath.substr(rawPath.find_last_of('/') + 1);
          if (resolveStdlibSurfaceMemberName(*metadata, memberLeaf) != "count") {
            return false;
          }
          const SemanticProductIndex *const keyValueCountSemanticIndexPtr =
              semanticIndexPtr();
          const auto keyValueTargetInfo =
              ir_lowerer::resolveCollectionPairTypeInfo(
                  callExpr.args.front(),
                  localsIn,
                  [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                    return inferCallKeyValueTargetInfo(keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                  },
                  semanticProgram,
                  keyValueCountSemanticIndexPtr);
          if (!keyValueTargetInfo.isKeyValueTarget) {
            return false;
          }
          auto keyValueFamilyName = [&]() {
            const std::string canonicalPath(metadata->canonicalPath);
            const size_t familySlash = canonicalPath.find_last_of('/');
            return familySlash == std::string::npos
                       ? canonicalPath
                       : canonicalPath.substr(familySlash + 1);
          };
          std::string family = keyValueFamilyName();
          if (semanticProgram != nullptr && keyValueCountSemanticIndexPtr != nullptr) {
            if (const auto *collectionFact =
                    ir_lowerer::findSemanticProductCollectionSpecialization(
                        *keyValueCountSemanticIndexPtr,
                        callExpr.args.front());
                collectionFact != nullptr) {
              family = collectionFact->collectionFamilyId != InvalidSymbolId
                           ? trimTemplateTypeText(std::string(
                                 semanticProgramResolveCallTargetString(
                                     *semanticProgram,
                                     collectionFact->collectionFamilyId)))
                           : trimTemplateTypeText(
                                 collectionFact->collectionFamily);
            }
          }
          if (family.empty() || rawPath.substr(0, slash) != family) {
            return false;
          }
          const std::string canonicalCountPath =
              stdlibSurfaceCanonicalHelperPath(metadata->id, "count");
          if (canonicalCountPath.empty()) {
            return false;
          }
          auto importsCanonicalCount = [&]() {
            if (semanticProgram == nullptr) {
              return false;
            }
            auto importsPath = [&](const std::vector<std::string> &imports) {
              return std::any_of(
                  imports.begin(),
                  imports.end(),
                  [&](const std::string &importPath) {
                    if (importPath == canonicalCountPath) {
                      return true;
                    }
                    if (importPath.size() >= 2 &&
                        importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
                      const std::string prefix =
                          importPath.substr(0, importPath.size() - 2);
                      return canonicalCountPath == prefix ||
                             canonicalCountPath.rfind(prefix + "/", 0) == 0;
                    }
                    return false;
                  });
            };
            return importsPath(semanticProgram->sourceImports) ||
                   importsPath(semanticProgram->imports);
          };
          if (!importsCanonicalCount()) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = canonicalCountPath;
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.semanticNodeId = 0;
          return true;
        };


bool TailDispatchContext::rewriteCanonicalKeyValueHelperDefinitionExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }
          std::string helperName;
          if (callExpr.isMethodCall &&
              resolveBuiltinKeyValueHelperName(callExpr, true, helperName)) {
            if (helperName == "contains_ref") {
              helperName = "contains";
            } else if (helperName == "tryAt_ref") {
              helperName = "tryAt";
            } else if (helperName == "count_ref") {
              helperName = "count";
            }
          } else if (!callExpr.isMethodCall &&
                     callExpr.namespacePrefix.empty() &&
                     callExpr.name.find('/') == std::string::npos &&
                     (callExpr.name == "contains" ||
                      callExpr.name == "tryAt" ||
                      callExpr.name == "count") &&
                     resolveTailDispatchDirectHelperDefinition(callExpr) ==
                         nullptr) {
            helperName = callExpr.name;
          }
          if (helperName.empty()) {
            return false;
          }
          if (!((helperName == "contains" && callExpr.args.size() == 2) ||
                (helperName == "tryAt" && callExpr.args.size() == 2) ||
                (helperName == "count" && !callExpr.args.empty()))) {
            return false;
          }
          if (helperName == "count" &&
              callExpr.args.front().kind == Expr::Kind::Call) {
            return false;
          }
          const SemanticProductIndex *const canonicalKeyValueHelperSemanticIndexPtr =
              semanticIndexPtr();
          const auto keyValueTargetInfo =
              ir_lowerer::resolveCollectionPairTypeInfo(
                  callExpr.args.front(),
                  localsIn,
                  [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                    return inferCallKeyValueTargetInfo(keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                  },
                  semanticProgram,
                  canonicalKeyValueHelperSemanticIndexPtr);
          if (!keyValueTargetInfo.isKeyValueTarget) {
            return false;
          }
          Expr candidate = callExpr;
          candidate.isMethodCall = false;
          candidate.isFieldAccess = false;
          candidate.namespacePrefix.clear();
          candidate.name = ir_lowerer::canonicalKeyValueHelperPath(helperName);
          candidate.semanticNodeId = 0;
          candidate.templateArgs.clear();
          const Definition *callee =
              resolveTailDispatchDirectHelperDefinition(candidate);
          if (callee == nullptr) {
            return false;
          }
          rewrittenExpr = std::move(candidate);
          return true;
        };


bool TailDispatchContext::rewriteImplicitBorrowedKeyValueReceiverExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }

          const SemanticProductIndex *const borrowedKeyValueReceiverSemanticIndexPtr =
              semanticIndexPtr();
          auto resolveBorrowedKeyValueReceiverInfo =
              [&](const Expr &receiverExpr) {
                return ir_lowerer::resolveCollectionPairTypeInfo(
                    receiverExpr,
                    localsIn,
                    [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                      return inferCallKeyValueTargetInfo(keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                    },
                    semanticProgram,
                    borrowedKeyValueReceiverSemanticIndexPtr);
              };
          auto isBorrowedOrPointerKeyValueReceiver = [&](const Expr &receiverExpr) {
            if (receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
                receiverExpr.args.size() == 1) {
              return false;
            }
            const auto keyValueTargetInfo = resolveBorrowedKeyValueReceiverInfo(receiverExpr);
            return keyValueTargetInfo.isKeyValueTarget && keyValueTargetInfo.isWrappedKeyValueTarget;
          };

          auto shouldRewriteReceiver = [&](const Expr &candidate) {
            if (candidate.isMethodCall) {
              std::string helperName;
              return resolveBuiltinKeyValueHelperName(candidate, true, helperName) &&
                     (helperName == "count" || helperName == "contains" ||
                      helperName == "tryAt" || helperName == "at" ||
                      helperName == "at_unsafe");
            }
            if (!candidate.args.empty() &&
                candidate.namespacePrefix.empty() &&
                candidate.name.find('/') == std::string::npos &&
                (candidate.name == "count" || candidate.name == "contains" ||
                 candidate.name == "tryAt" || candidate.name == "at" ||
                 candidate.name == "at_unsafe")) {
              return resolveBorrowedKeyValueReceiverInfo(candidate.args.front()).isKeyValueTarget;
            }
            std::string helperName;
            return resolveBuiltinKeyValueHelperName(candidate, false, helperName) &&
                   (helperName == "count" || helperName == "contains" ||
                    helperName == "tryAt" || helperName == "at" ||
                    helperName == "at_unsafe" || helperName == "insert" ||
                    helperName == "insert_ref");
          };

          if (!shouldRewriteReceiver(callExpr) || !isBorrowedOrPointerKeyValueReceiver(callExpr.args.front())) {
            return false;
          }

          Expr derefExpr;
          derefExpr.kind = Expr::Kind::Call;
          derefExpr.name = "dereference";
          derefExpr.args.push_back(callExpr.args.front());

          rewrittenExpr = callExpr;
          rewrittenExpr.args.front() = derefExpr;
          return true;
        };


bool TailDispatchContext::resolveCanonicalMathBuiltinName(const Expr &callExpr, std::string &mathBuiltinName) {
          if (getMathBuiltinName(callExpr, mathBuiltinName)) {
            return true;
          }

          Expr resolvedCallExpr = callExpr;
          resolvedCallExpr.name = resolveExprPath(callExpr);
          resolvedCallExpr.namespacePrefix.clear();
          return getMathBuiltinName(resolvedCallExpr, mathBuiltinName);
        };


std::string TailDispatchContext::resolveSemanticQueryFactTypeText(const SemanticProgramQueryFact &queryFact) {
              auto resolveFactTypeText = [&](const std::string &typeText,
                                             SymbolId typeTextId) {
                if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
                  std::string resolvedTypeText = std::string(
                      semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
                  if (!resolvedTypeText.empty()) {
                    return ir_lowerer::trimTemplateTypeText(resolvedTypeText);
                  }
                }
                return ir_lowerer::trimTemplateTypeText(typeText);
              };
              std::string bindingType =
                  resolveFactTypeText(queryFact.bindingTypeText, queryFact.bindingTypeTextId);
              if (bindingType.empty()) {
                bindingType =
                    resolveFactTypeText(queryFact.queryTypeText, queryFact.queryTypeTextId);
              }
              return bindingType;
            };


std::string TailDispatchContext::normalizeInternalSoaMetadataType(std::string typeText) {
          typeText = ir_lowerer::trimTemplateTypeText(typeText);
          for (std::string_view wrapper : {"Reference<", "Pointer<"}) {
            if (typeText.rfind(wrapper, 0) == 0 &&
                typeText.size() > wrapper.size() &&
                typeText.back() == '>') {
              typeText = ir_lowerer::trimTemplateTypeText(
                  typeText.substr(wrapper.size(),
                                  typeText.size() - wrapper.size() - 1));
              break;
            }
          }
          const size_t templateStart = typeText.find('<');
          if (templateStart != std::string::npos) {
            typeText.erase(templateStart);
          }
          const size_t leafStart = typeText.find_last_of('/');
          const size_t suffixStart =
              typeText.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
          if (suffixStart != std::string::npos) {
            typeText.erase(suffixStart);
          }
          return typeText;
        };


std::string TailDispatchContext::internalSoaMetadataCallLeaf(const Expr &callExpr) {
          std::string path = callExpr.name;
          if (path.find('/') == std::string::npos && !callExpr.namespacePrefix.empty()) {
            path = callExpr.namespacePrefix == "/" ? "/" + path
                                                   : callExpr.namespacePrefix + "/" + path;
          }
          const size_t leafStart = path.find_last_of('/');
          std::string leaf =
              leafStart == std::string::npos ? path : path.substr(leafStart + 1);
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          if (leaf == "field_count" || leaf == "field_capacity") {
            return leaf;
          }
          return std::string{};
        };


bool TailDispatchContext::isInternalSoaMetadataReceiver(const Expr &receiverExpr,
                                                 const Expr &callExpr, const LocalMap &localsIn) {
          auto isInternalSoaType = [&](const std::string &typeText) {
            const std::string normalized =
                normalizeInternalSoaMetadataType(typeText);
            return normalized == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName) ||
                   normalized == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView");
          };
          if (semanticProgram != nullptr && semanticIndexPtr() != nullptr) {
            const auto *queryFact =
                ir_lowerer::findSemanticProductQueryFact(
                    semanticProgram, *semanticIndexPtr(), callExpr);
            if (queryFact != nullptr) {
              const std::string receiverType = resolveSemanticReceiverTypeText(
                  queryFact->receiverBindingTypeText,
                  queryFact->receiverBindingTypeTextId);
              return isInternalSoaType(receiverType);
            }
          }
          if (receiverExpr.kind != Expr::Kind::Name) {
            return false;
          }
          auto localIt = localsIn.find(receiverExpr.name);
          return localIt != localsIn.end() &&
                 isInternalSoaType(localIt->second.structTypeName);
        };


} // namespace primec::ir_lowerer
