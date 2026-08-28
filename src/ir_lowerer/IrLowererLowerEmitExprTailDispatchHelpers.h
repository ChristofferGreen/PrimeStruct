#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "IrLowererCallHelpers.h"
#include "IrLowererCallHelperTypes.h"
#include "IrLowererLowerReturnEmitStage.h"
#include "IrLowererSetupMathHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {

class TailDispatchContext {
public:
  TailDispatchContext(LowerSetupStageState &setupStage,
                      LowerReturnEmitStageState &stateOut,
                      const CallResolutionAdapters &callResolutionAdapters,
                      std::string &error);

  std::string resolveTailDispatchDirectHelperPath(const Expr &candidate);
  const Definition * resolveTailDispatchDirectHelperDefinition(const Expr &candidate);
  const StdlibSurfaceMetadata * tailDispatchKeyValueHelperMetadata();
  std::optional<primec::StdlibSurfaceId> tailDispatchKeyValueHelperSurfaceId();
  bool isTailDispatchKeyValueHelperSurface(std::optional<primec::StdlibSurfaceId> surfaceId);
  bool isTailDispatchKeyValueImportAliasHelperPath(std::string_view path, std::string_view helperName);
  bool hasPublishedSemanticKeyValueSurface(const Expr &callExpr);
  bool resolvePublishedTailDispatchKeyValueHelperName(const Expr &callExpr, std::string &helperNameOut);
  bool resolveBuiltinKeyValueHelperName(const Expr &callExpr, bool allowMethodCall, std::string &helperNameOut);
  bool inferCallKeyValueTargetInfo(const Expr &targetExpr, ir_lowerer::CollectionPairTypeInfo &out, const LocalMap &localsIn);
  bool rewriteBuiltinKeyValueInsertBuiltinExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn);
  bool rewriteCanonicalKeyValueHelperForExperimentalReceiverExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn);
  bool isVectorStructPath(const std::string &structPath);
  bool publishedKeyValueAccessHelperReturnsString(std::string_view helperName);
  bool hasExplicitStdKeyValueHelperSpelling(const Expr &callExpr);
  bool rewriteExplicitKeyValueHelperBuiltinExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn);
  std::string resolveSemanticReceiverTypeText(const std::string &typeText, SymbolId typeTextId);
  bool rewriteSameFamilyKeyValueCountExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn);
  bool rewriteCanonicalKeyValueHelperDefinitionExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn);
  bool rewriteImplicitBorrowedKeyValueReceiverExpr(const Expr &callExpr, Expr &rewrittenExpr, const LocalMap &localsIn);
  bool resolveCanonicalMathBuiltinName(const Expr &callExpr, std::string &mathBuiltinName);
  std::string resolveSemanticQueryFactTypeText(const SemanticProgramQueryFact &queryFact);
  std::string normalizeInternalSoaMetadataType(std::string typeText);
  std::string internalSoaMetadataCallLeaf(const Expr &callExpr);
  bool isInternalSoaMetadataReceiver(const Expr &receiverExpr, const Expr &callExpr, const LocalMap &localsIn);

private:
  const SemanticProductIndex * semanticIndexPtr();

  [[maybe_unused]] LowerSetupStageState &setupStage;
  [[maybe_unused]] LowerReturnEmitStageState &stateOut;
  const CallResolutionAdapters &callResolutionAdapters;
  [[maybe_unused]] std::string &error;
  const SemanticProgram *const &semanticProgram;
  std::unordered_map<std::string, const Definition *> &defMap;
  const ResolveExprPathFn &resolveExprPath;
  const ResolveDefinitionCallFn &resolveDefinitionCall;
  const GetSetupMathBuiltinNameFn &getMathBuiltinName;
  const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition;
};

} // namespace primec::ir_lowerer
