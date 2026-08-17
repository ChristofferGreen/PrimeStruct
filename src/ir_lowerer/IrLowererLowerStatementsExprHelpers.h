#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "IrLowererCallHelpers.h"
#include "IrLowererCallHelperTypes.h"
#include "IrLowererLowerReturnEmitStage.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {

class StatementsExprContext {
public:
  StatementsExprContext(LowerSetupStageState &setupStage,
                        LowerReturnEmitStageState &stateOut,
                        const CallResolutionAdapters &callResolutionAdapters,
                        std::string &error);

  std::string resolveDirectHelperPath(const Expr &callExpr);
  bool matchesGeneratedSpecializedType(std::string_view path, std::string_view collectionName, std::string_view typeName);
  bool isCollectionVectorRecordTypePath(std::string_view path);
  bool matchesDirectHelperDefinitionFamilyPath(const std::string &candidatePath, const Definition &callee);
  bool isDirectHelperDefinitionFamily(const Expr &callExpr, const Definition &callee);
  const Definition * findDirectHelperDefinition(const std::string &rawPath);
  bool isExplicitExperimentalVectorConstructorHelper(const std::string &path);
  const Definition * resolveDirectHelperDefinition(const Expr &targetExpr);
  std::string stripGeneratedHelperSuffix(std::string helperPath);
  std::string extractHelperTail(std::string helperPath);
  const StdlibSurfaceMetadata * keyValueHelperMetadata();
  const StdlibSurfaceMetadata * keyValueConstructorMetadata();
  bool resolveKeyValueHelperMemberName(std::string path, std::string &helperNameOut);
  std::string keyValueHelperMethodSpelling(std::string_view memberName);
  std::string pascalCaseHelperMember(std::string_view memberName);
  std::string keyValueImplementationMethodSpelling(const std::string &receiverStructPath, std::string_view memberName);
  bool isCanonicalKeyValueHelperFamilyPath(const std::string &path);
  bool isKeyValueHelperMemberPath(const std::string &path, std::string_view expectedName);
  bool importPathCoversPublishedTarget(const std::string &importPath, const std::string &targetPath);
  bool hasSemanticKeyValueHelperDefinition(std::string_view helperName);
  std::string semanticCollectionFamilyForExpr(const Expr &receiverExpr, const LocalMap &localsIn);
  bool populateKeyValueInfoFromCollectionFact(const SemanticProgramCollectionSpecialization &collectionFact, CollectionPairTypeInfo &targetInfoOut);
  bool resolveLocalNameKeyValueInfo(const Expr &receiverExpr, CollectionPairTypeInfo &targetInfoOut);
  bool resolveKeyValueInfoFromCallReceiverQuery(const Expr &callExpr, const Expr &receiverExpr, CollectionPairTypeInfo &targetInfoOut);
  ir_lowerer::CollectionPairTypeInfo resolveKeyValueAccessReceiverInfo(const Expr &callExpr, const Expr &receiverExpr, const LocalMap &localsIn);
  bool resolveSameFamilyKeyValueHelperMemberName(const Expr &callExpr, const Expr &receiverExpr, std::string &helperNameOut, const LocalMap &localsIn);
  bool isCanonicalKeyValueConstructorPath(const std::string &path);
  bool isDirectCollectionHelperPath(const std::string &path);
  bool hasKeyValueEntryCtorArgs(const Expr &callExpr);
  bool isInternalSoaHelperFamilyName(const std::string &helperName);
  bool isInternalSoaHelperFamilyPath(const std::string &path);
  bool isSamePathSoaHelperPath(const std::string &path);
  bool isSoaWrapperHelperFamilyPath(const std::string &path);
  const Definition * findDirectInternalSoaDefinition(const std::string &rawPath);
  const Definition * findDirectSoaWrapperDefinition(const Expr &callExpr, const std::string &rawPath, const LocalMap &localsIn);
  std::string resolveSemanticCallTargetPath(const Expr &callExpr);
  const Definition * findDirectEntryKeyValueConstructorDefinition(const Expr &callExpr);
  const Definition * findDirectStructDefinition(const Expr &callExpr);
  bool resolveBuiltinAccessName(const Expr &callExpr, std::string &accessNameOut);
  bool resolveHelperReturnedArrayVectorAccessTargetInfo(const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut, const LocalMap &localsIn);
  bool tryEmitBuiltinKeyValueConstructor(const Expr &callExpr, const std::string &resolvedCallPath, bool &handledOut, const LocalMap &localsIn);
  std::string experimentalCollectionMemberPath(std::string_view collectionName, std::string_view memberName);

private:
  LowerSetupStageState &setupStage;
  LowerReturnEmitStageState &stateOut;
  const CallResolutionAdapters &callResolutionAdapters;
  std::string &error;
  IrFunction &function;
  const SemanticProgram *const &semanticProgram;
  std::unordered_map<std::string, const Definition *> &defMap;
  const std::function<bool(const Expr &, const LocalMap &)> &emitExpr;
  const std::function<int32_t()> &allocTempLocal;
  const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind;
  const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath;
  const ResolveExprPathFn &resolveExprPath;
  const ResolveDefinitionCallFn &resolveDefinitionCall;
  const ResolveStructTypeNameFn &resolveStructTypeName;
};

} // namespace primec::ir_lowerer
