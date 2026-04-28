#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;
using ReturnKind = Emitter::ReturnKind;

enum class PrintTarget { Out, Err };

struct PrintBuiltin {
  PrintTarget target = PrintTarget::Out;
  bool newline = false;
  std::string name;
};

std::string joinTemplateArgs(const std::vector<std::string> &args);
ReturnKind getReturnKind(const Definition &def);
bool isPrimitiveBindingTypeName(const std::string &name);
std::string normalizeBindingTypeName(const std::string &name);
bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg);
bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool isResultBindingTypeName(const std::string &name);
std::string sourceResultCppType(bool hasValue);
std::string sourceResultStatusOkExpr();
std::string sourceResultStatusErrorExpr(const std::string &errorExpr);
std::string sourceResultStatusFromErrorExpr(const std::string &errorExpr);
std::string sourceResultStatusIsErrorExpr(const std::string &resultExpr);
std::string sourceResultStatusErrorPayloadExpr(const std::string &resultExpr);
std::string sourceResultValueOkExpr(const std::string &valueExpr);
std::string sourceResultValueErrorExpr(const std::string &errorExpr);
std::string sourceResultValueIsErrorExpr(const std::string &resultExpr);
std::string sourceResultValueErrorPayloadExpr(const std::string &resultExpr);
std::string sourceResultValueOkPayloadExpr(const std::string &resultExpr);
std::optional<std::string> trySourceResultCppType(const std::string &base,
                                                  const std::string &argText);
bool isBindingQualifierName(const std::string &name);
bool isBindingAuxTransformName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
BindingInfo getBindingInfo(const Expr &expr);
bool isReferenceCandidate(const BindingInfo &info);
std::string bindingTypeToCpp(const BindingInfo &info);
std::string bindingTypeToCpp(const std::string &typeName);
std::string bindingTypeToCpp(const BindingInfo &info,
                             const std::string &namespacePrefix,
                             const std::unordered_map<std::string, std::string> &importAliases,
                             const std::unordered_map<std::string, std::string> &structTypeMap);
std::string bindingTypeToCpp(const std::string &typeName,
                             const std::string &namespacePrefix,
                             const std::unordered_map<std::string, std::string> &importAliases,
                             const std::unordered_map<std::string, std::string> &structTypeMap);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);
std::string typeNameForReturnKind(ReturnKind kind);
ReturnKind returnKindForTypeName(const std::string &name);
ReturnKind combineNumericKinds(ReturnKind left, ReturnKind right);
ReturnKind inferPrimitiveReturnKind(const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &localTypes,
                                    const std::unordered_map<std::string, ReturnKind> &returnKinds,
                                    bool allowMathBare);

bool getBuiltinOperator(const Expr &expr, char &out);
bool getBuiltinComparison(const Expr &expr, const char *&out);
bool getBuiltinMutationName(const Expr &expr, std::string &out);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out);
bool isPathSpaceBuiltinName(const Expr &expr);
std::string stripStringLiteralSuffix(const std::string &token);
bool isBuiltinNegate(const Expr &expr);
bool isBuiltinClamp(const Expr &expr, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare);
bool isBuiltinMathConstantName(const std::string &name, bool allowBare);
bool getBuiltinPointerOperator(const Expr &expr, char &out);
bool getBuiltinMemoryName(const Expr &expr, std::string &out);
bool getBuiltinConvertName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
bool getBuiltinArrayAccessNameLocal(const Expr &expr, std::string &out);
std::string resolveExprPath(const Expr &expr);
std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, std::string> &nameMap);
bool isArrayValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isVectorValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isMapValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isStringValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isArrayCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isMapCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isStringCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isVectorCapacityCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool resolveMethodCallPath(const Expr &call,
                           const std::unordered_map<std::string, const Definition *> &defMap,
                           const std::unordered_map<std::string, BindingInfo> &localTypes,
                           const std::unordered_map<std::string, std::string> &importAliases,
                           const std::unordered_map<std::string, std::string> &structTypeMap,
                           const std::unordered_map<std::string, ReturnKind> &returnKinds,
                           const std::unordered_map<std::string, std::string> &returnStructs,
                           std::string &resolvedOut);
bool isBuiltinAssign(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool getVectorMutatorName(const Expr &expr,
                          const std::unordered_map<std::string, std::string> &nameMap,
                          std::string &out);
std::vector<const Expr *> orderCallArguments(const Expr &expr,
                                             const std::string &resolvedPath,
                                             const std::vector<Expr> &params,
                                             const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isBuiltinIf(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool isBuiltinBlock(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool isLoopCall(const Expr &expr);
bool isWhileCall(const Expr &expr);
bool isForCall(const Expr &expr);
bool isRepeatCall(const Expr &expr);
bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);
bool isReturnCall(const Expr &expr);

bool runEmitterEmitSetupMathImport(const Program &program);

enum class EmitterLifecycleHelperKind { Create, Destroy, Copy, Move };

struct EmitterLifecycleHelperMatch {
  std::string parentPath;
  EmitterLifecycleHelperKind kind = EmitterLifecycleHelperKind::Create;
  std::string placement;
};

std::optional<EmitterLifecycleHelperMatch> runEmitterEmitSetupLifecycleHelperMatchStep(const std::string &fullPath);

std::optional<std::string> runEmitterExprControlNameStep(
    const Expr &expr,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    bool allowMathBare);
std::optional<std::string> runEmitterExprControlFloatLiteralStep(const Expr &expr);
std::optional<std::string> runEmitterExprControlIntegerLiteralStep(const Expr &expr);
std::optional<std::string> runEmitterExprControlBoolLiteralStep(const Expr &expr);
std::optional<std::string> runEmitterExprControlStringLiteralStep(const Expr &expr);

using EmitFieldAccessReceiverFn = std::function<std::string(const Expr &)>;
using ResolveFieldAccessStaticReceiverFn = std::function<std::optional<std::string>(const Expr &)>;
std::optional<std::string> runEmitterExprControlFieldAccessStep(
    const Expr &expr,
    const EmitFieldAccessReceiverFn &emitReceiverExpr,
    const ResolveFieldAccessStaticReceiverFn &resolveStaticReceiverExpr);

std::optional<std::string> runEmitterExprControlCallPathStep(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, std::string> &nameMap,
    const std::unordered_map<std::string, std::string> &importAliases);

using EmitterExprControlIsCountLikeCallFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, Emitter::BindingInfo> &)>;
using EmitterExprControlResolveMethodPathFn = std::function<bool(std::string &)>;
std::optional<std::string> runEmitterExprControlMethodPathStep(
    const Expr &expr,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    const EmitterExprControlIsCountLikeCallFn &isArrayCountCall,
    const EmitterExprControlIsCountLikeCallFn &isMapCountCall,
    const EmitterExprControlIsCountLikeCallFn &isStringCountCall,
    const EmitterExprControlResolveMethodPathFn &resolveMethodPath);

using EmitterExprControlCountRewriteIsCountLikeCallFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, Emitter::BindingInfo> &)>;
using EmitterExprControlCountRewriteResolveMethodPathFn = std::function<bool(const Expr &, std::string &)>;
using EmitterExprControlCountRewriteIsCollectionAccessReceiverFn = std::function<bool(const Expr &)>;
std::optional<std::string> runEmitterExprControlCountRewriteStep(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, std::string> &nameMap,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isArrayCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isMapCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isStringCountCall,
    const EmitterExprControlCountRewriteResolveMethodPathFn &resolveMethodPath,
    const EmitterExprControlCountRewriteIsCollectionAccessReceiverFn &isCollectionAccessReceiverExpr = {});

using EmitterExprControlBuiltinBlockPreludeIsBuiltinBlockFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, std::string> &)>;
using EmitterExprControlBuiltinBlockPreludeHasNamedArgumentsFn =
    std::function<bool(const std::vector<std::optional<std::string>> &)>;
struct EmitterExprControlBuiltinBlockPreludeStepResult {
  bool matched = false;
  std::optional<std::string> earlyReturnExpr;
};
EmitterExprControlBuiltinBlockPreludeStepResult runEmitterExprControlBuiltinBlockPreludeStep(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &nameMap,
    const EmitterExprControlBuiltinBlockPreludeIsBuiltinBlockFn &isBuiltinBlock,
    const EmitterExprControlBuiltinBlockPreludeHasNamedArgumentsFn &hasNamedArguments);

using EmitterExprControlBuiltinBlockEarlyReturnIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlBuiltinBlockEarlyReturnEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlBuiltinBlockEarlyReturnStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlBuiltinBlockEarlyReturnStepResult runEmitterExprControlBuiltinBlockEarlyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlBuiltinBlockEarlyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlBuiltinBlockEarlyReturnEmitExprFn &emitExpr);

using EmitterExprControlBuiltinBlockFinalValueIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlBuiltinBlockFinalValueEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlBuiltinBlockFinalValueStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlBuiltinBlockFinalValueStepResult runEmitterExprControlBuiltinBlockFinalValueStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlBuiltinBlockFinalValueIsReturnCallFn &isReturnCall,
    const EmitterExprControlBuiltinBlockFinalValueEmitExprFn &emitExpr);

using EmitterExprControlBuiltinBlockBindingPreludeGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlBuiltinBlockBindingPreludeHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlBuiltinBlockBindingPreludeInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlBuiltinBlockBindingPreludeTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
struct EmitterExprControlBuiltinBlockBindingPreludeStepResult {
  bool handled = false;
  Emitter::BindingInfo binding;
  bool hasExplicitType = false;
  bool lambdaInit = false;
  bool useAuto = false;
};
EmitterExprControlBuiltinBlockBindingPreludeStepResult runEmitterExprControlBuiltinBlockBindingPreludeStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &blockTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const EmitterExprControlBuiltinBlockBindingPreludeGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlBuiltinBlockBindingPreludeHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlBuiltinBlockBindingPreludeInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlBuiltinBlockBindingPreludeTypeNameForReturnKindFn &typeNameForReturnKind);

using EmitterExprControlBuiltinBlockBindingQualifiersIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
struct EmitterExprControlBuiltinBlockBindingQualifiersStepResult {
  bool needsConst = false;
  bool useRef = false;
};
EmitterExprControlBuiltinBlockBindingQualifiersStepResult runEmitterExprControlBuiltinBlockBindingQualifiersStep(
    const Emitter::BindingInfo &binding,
    bool hasInitializer,
    const EmitterExprControlBuiltinBlockBindingQualifiersIsReferenceCandidateFn &isReferenceCandidate);

using EmitterExprControlBuiltinBlockBindingAutoEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlBuiltinBlockBindingAutoStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlBuiltinBlockBindingAutoStepResult runEmitterExprControlBuiltinBlockBindingAutoStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool useAuto,
    const EmitterExprControlBuiltinBlockBindingAutoEmitExprFn &emitExpr);

using EmitterExprControlBuiltinBlockBindingExplicitEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlBuiltinBlockBindingExplicitStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlBuiltinBlockBindingExplicitStepResult runEmitterExprControlBuiltinBlockBindingExplicitStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const std::string &namespacePrefix,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlBuiltinBlockBindingExplicitEmitExprFn &emitExpr);

using EmitterExprControlBuiltinBlockBindingFallbackEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlBuiltinBlockBindingFallbackStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlBuiltinBlockBindingFallbackStepResult runEmitterExprControlBuiltinBlockBindingFallbackStep(
    const Expr &stmt,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const EmitterExprControlBuiltinBlockBindingFallbackEmitExprFn &emitExpr);

using EmitterExprControlBuiltinBlockStatementEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlBuiltinBlockStatementStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlBuiltinBlockStatementStepResult runEmitterExprControlBuiltinBlockStatementStep(
    const Expr &stmt,
    const EmitterExprControlBuiltinBlockStatementEmitExprFn &emitExpr);

using EmitterExprControlBodyWrapperIsBuiltinBlockFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, std::string> &)>;
using EmitterExprControlBodyWrapperEmitExprFn = std::function<std::string(const Expr &)>;
std::optional<std::string> runEmitterExprControlBodyWrapperStep(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &nameMap,
    const EmitterExprControlBodyWrapperIsBuiltinBlockFn &isBuiltinBlock,
    const EmitterExprControlBodyWrapperEmitExprFn &emitExpr);

bool runEmitterExprControlIfBlockEnvelopeStep(const Expr &candidate);

using EmitterExprControlIfBlockEarlyReturnIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBlockEarlyReturnEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBlockEarlyReturnStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlIfBlockEarlyReturnStepResult runEmitterExprControlIfBlockEarlyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBlockEarlyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBlockEarlyReturnEmitExprFn &emitExpr);

using EmitterExprControlIfBlockFinalValueIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBlockFinalValueEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBlockFinalValueStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlIfBlockFinalValueStepResult runEmitterExprControlIfBlockFinalValueStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBlockFinalValueIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBlockFinalValueEmitExprFn &emitExpr);

using EmitterExprControlIfBlockBindingPreludeGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBlockBindingPreludeHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBlockBindingPreludeInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBlockBindingPreludeTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
struct EmitterExprControlIfBlockBindingPreludeStepResult {
  bool handled = false;
  Emitter::BindingInfo binding;
  bool hasExplicitType = false;
  bool lambdaInit = false;
  bool useAuto = false;
};
EmitterExprControlIfBlockBindingPreludeStepResult runEmitterExprControlIfBlockBindingPreludeStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const EmitterExprControlIfBlockBindingPreludeGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBlockBindingPreludeHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBlockBindingPreludeInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBlockBindingPreludeTypeNameForReturnKindFn &typeNameForReturnKind);

using EmitterExprControlIfBlockBindingQualifiersIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
struct EmitterExprControlIfBlockBindingQualifiersStepResult {
  bool needsConst = false;
  bool useRef = false;
};
EmitterExprControlIfBlockBindingQualifiersStepResult runEmitterExprControlIfBlockBindingQualifiersStep(
    const Emitter::BindingInfo &binding,
    bool hasInitializer,
    const EmitterExprControlIfBlockBindingQualifiersIsReferenceCandidateFn &isReferenceCandidate);

using EmitterExprControlIfBlockBindingAutoEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBlockBindingAutoStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlIfBlockBindingAutoStepResult runEmitterExprControlIfBlockBindingAutoStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool useAuto,
    const EmitterExprControlIfBlockBindingAutoEmitExprFn &emitExpr);

using EmitterExprControlIfBlockBindingExplicitEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBlockBindingExplicitStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlIfBlockBindingExplicitStepResult runEmitterExprControlIfBlockBindingExplicitStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const std::string &namespacePrefix,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBlockBindingExplicitEmitExprFn &emitExpr);

using EmitterExprControlIfBlockBindingFallbackEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBlockBindingFallbackStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlIfBlockBindingFallbackStepResult runEmitterExprControlIfBlockBindingFallbackStep(
    const Expr &stmt,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const EmitterExprControlIfBlockBindingFallbackEmitExprFn &emitExpr);

using EmitterExprControlIfBlockStatementEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBlockStatementStepResult {
  bool handled = false;
  std::string emittedStatement;
};
EmitterExprControlIfBlockStatementStepResult runEmitterExprControlIfBlockStatementStep(
    const Expr &stmt,
    const EmitterExprControlIfBlockStatementEmitExprFn &emitExpr);

struct EmitterExprControlIfBranchBodyEmitResult {
  bool handled = false;
  std::string emittedStatement;
  bool shouldBreak = false;
};
using EmitterExprControlIfBranchBodyEmitStatementFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &, bool isLast)>;
struct EmitterExprControlIfBranchBodyStepResult {
  bool handled = false;
  std::string emittedBody;
};
EmitterExprControlIfBranchBodyStepResult runEmitterExprControlIfBranchBodyStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchBodyEmitStatementFn &emitStatement);

using EmitterExprControlIfBranchPreludeIsBlockEnvelopeFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchPreludeEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBranchPreludeStepResult {
  bool handled = false;
  std::string emittedExpr;
};
EmitterExprControlIfBranchPreludeStepResult runEmitterExprControlIfBranchPreludeStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchPreludeIsBlockEnvelopeFn &isBlockEnvelope,
    const EmitterExprControlIfBranchPreludeEmitExprFn &emitExpr);

using EmitterExprControlIfBranchWrapperEmitBodyFn = std::function<std::string()>;
struct EmitterExprControlIfBranchWrapperStepResult {
  bool handled = false;
  std::string emittedExpr;
};
EmitterExprControlIfBranchWrapperStepResult runEmitterExprControlIfBranchWrapperStep(
    const EmitterExprControlIfBranchWrapperEmitBodyFn &emitBody);

using EmitterExprControlIfBranchBodyReturnIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyReturnEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBranchBodyReturnStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};
EmitterExprControlIfBranchBodyReturnStepResult runEmitterExprControlIfBranchBodyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBranchBodyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBranchBodyReturnEmitExprFn &emitExpr);

using EmitterExprControlIfBranchBodyBindingGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBranchBodyBindingHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyBindingInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBranchBodyBindingTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
using EmitterExprControlIfBranchBodyBindingIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
using EmitterExprControlIfBranchBodyBindingEmitExprFn =
    std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBranchBodyBindingStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};
EmitterExprControlIfBranchBodyBindingStepResult runEmitterExprControlIfBranchBodyBindingStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBranchBodyBindingGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBranchBodyBindingHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBranchBodyBindingInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBranchBodyBindingTypeNameForReturnKindFn &typeNameForReturnKind,
    const EmitterExprControlIfBranchBodyBindingIsReferenceCandidateFn &isReferenceCandidate,
    const EmitterExprControlIfBranchBodyBindingEmitExprFn &emitExpr);

using EmitterExprControlIfBranchBodyDispatchReturnFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &, bool isLast)>;
using EmitterExprControlIfBranchBodyDispatchBindingFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &)>;
using EmitterExprControlIfBranchBodyDispatchStatementFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &)>;
struct EmitterExprControlIfBranchBodyDispatchStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};
EmitterExprControlIfBranchBodyDispatchStepResult runEmitterExprControlIfBranchBodyDispatchStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBranchBodyDispatchReturnFn &emitReturn,
    const EmitterExprControlIfBranchBodyDispatchBindingFn &emitBinding,
    const EmitterExprControlIfBranchBodyDispatchStatementFn &emitStatement);

using EmitterExprControlIfBranchBodyHandlersIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyHandlersGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBranchBodyHandlersHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyHandlersInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBranchBodyHandlersTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
using EmitterExprControlIfBranchBodyHandlersIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
using EmitterExprControlIfBranchBodyHandlersEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBranchBodyHandlersStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};
EmitterExprControlIfBranchBodyHandlersStepResult runEmitterExprControlIfBranchBodyHandlersStep(
    const Expr &stmt,
    bool isLast,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBranchBodyHandlersIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBranchBodyHandlersGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBranchBodyHandlersHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBranchBodyHandlersInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBranchBodyHandlersTypeNameForReturnKindFn &typeNameForReturnKind,
    const EmitterExprControlIfBranchBodyHandlersIsReferenceCandidateFn &isReferenceCandidate,
    const EmitterExprControlIfBranchBodyHandlersEmitExprFn &emitExpr);

using EmitterExprControlIfBranchValueIsEnvelopeFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchValueEmitExprFn = std::function<std::string(const Expr &)>;
using EmitterExprControlIfBranchValueEmitStatementFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &, bool isLast)>;
struct EmitterExprControlIfBranchValueStepResult {
  bool handled = false;
  std::string emittedExpr;
};
EmitterExprControlIfBranchValueStepResult runEmitterExprControlIfBranchValueStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchValueIsEnvelopeFn &isIfBlockEnvelope,
    const EmitterExprControlIfBranchValueEmitExprFn &emitExpr,
    const EmitterExprControlIfBranchValueEmitStatementFn &emitStatement);

using EmitterExprControlIfBranchEmitIsEnvelopeFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchEmitIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchEmitGetBindingInfoFn = std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBranchEmitHasExplicitTypeFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchEmitInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBranchEmitTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
using EmitterExprControlIfBranchEmitIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
using EmitterExprControlIfBranchEmitEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBranchEmitStepResult {
  bool handled = false;
  std::string emittedExpr;
};
EmitterExprControlIfBranchEmitStepResult runEmitterExprControlIfBranchEmitStep(
    const Expr &candidate,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBranchEmitIsEnvelopeFn &isIfBlockEnvelope,
    const EmitterExprControlIfBranchEmitIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBranchEmitGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBranchEmitHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBranchEmitInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBranchEmitTypeNameForReturnKindFn &typeNameForReturnKind,
    const EmitterExprControlIfBranchEmitIsReferenceCandidateFn &isReferenceCandidate,
    const EmitterExprControlIfBranchEmitEmitExprFn &emitExpr);

using EmitterExprControlIfBranchBodyStatementEmitExprFn = std::function<std::string(const Expr &)>;
struct EmitterExprControlIfBranchBodyStatementStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};
EmitterExprControlIfBranchBodyStatementStepResult runEmitterExprControlIfBranchBodyStatementStep(
    const Expr &stmt,
    const EmitterExprControlIfBranchBodyStatementEmitExprFn &emitExpr);

using EmitterExprControlIfTernaryEmitConditionFn = std::function<std::string()>;
using EmitterExprControlIfTernaryEmitThenFn = std::function<std::string()>;
using EmitterExprControlIfTernaryEmitElseFn = std::function<std::string()>;
struct EmitterExprControlIfTernaryStepResult {
  bool handled = false;
  std::string emittedExpr;
};
EmitterExprControlIfTernaryStepResult runEmitterExprControlIfTernaryStep(
    const EmitterExprControlIfTernaryEmitConditionFn &emitCondition,
    const EmitterExprControlIfTernaryEmitThenFn &emitThen,
    const EmitterExprControlIfTernaryEmitElseFn &emitElse);

using EmitterExprControlIfTernaryFallbackEmitConditionFn = std::function<std::string()>;
using EmitterExprControlIfTernaryFallbackEmitThenFn = std::function<std::string()>;
using EmitterExprControlIfTernaryFallbackEmitElseFn = std::function<std::string()>;
struct EmitterExprControlIfTernaryFallbackStepResult {
  bool handled = false;
  std::string emittedExpr;
};
EmitterExprControlIfTernaryFallbackStepResult runEmitterExprControlIfTernaryFallbackStep(
    const EmitterExprControlIfTernaryFallbackEmitConditionFn &emitCondition,
    const EmitterExprControlIfTernaryFallbackEmitThenFn &emitThen,
    const EmitterExprControlIfTernaryFallbackEmitElseFn &emitElse);

} // namespace primec::emitter
