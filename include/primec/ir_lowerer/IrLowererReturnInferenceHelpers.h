#pragma once

#include <functional>
#include <string>

#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

enum class MissingReturnBehavior { Error, Void };

struct ReturnInferenceOptions {
  MissingReturnBehavior missingReturnBehavior = MissingReturnBehavior::Error;
  bool includeDefinitionReturnExpr = false;
  bool deferUnknownReturnDependencyErrors = false;
};

struct EntryReturnConfig {
  bool hasReturnTransform = false;
  bool returnsVoid = false;
  bool hasResultInfo = false;
  ResultReturnInfo resultInfo{};
};

using InferBindingIntoLocalsFn = std::function<bool(const Expr &, bool, LocalMap &, std::string &)>;
using ExpandMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using BindingKindForInferenceFn = std::function<LocalInfo::Kind(const Expr &)>;
using BindingValueKindForInferenceFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using InferStructExprPathFromLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using ResolveStructTypeNameForReturnFn = std::function<bool(const std::string &, const std::string &, std::string &)>;
using ResolveStructArrayInfoForReturnFn = std::function<bool(const std::string &, StructArrayTypeInfo &)>;

bool analyzeEntryReturnTransforms(const Definition &entryDef,
                                  const std::string &entryPath,
                                  EntryReturnConfig &out,
                                  std::string &error);
bool analyzeEntryReturnTransforms(const Definition &entryDef,
                                  const SemanticProgram *semanticProgram,
                                  const std::string &entryPath,
                                  EntryReturnConfig &out,
                                  std::string &error);
void analyzeDeclaredReturnTransforms(const Definition &def,
                                     const ResolveStructTypeNameForReturnFn &resolveStructTypeName,
                                     const ResolveStructArrayInfoForReturnFn &resolveStructArrayInfoFromPath,
                                     ReturnInfo &info,
                                     bool &hasReturnTransform,
                                     bool &hasReturnAuto);

bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const ExprLocalsValueKindFn &inferExprKindFromLocals,
                               const ExprLocalsValueKindFn &inferArrayElementKindFromLocals,
                               const InferStructExprPathFromLocalsFn &inferStructExprPathFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error,
                               bool *sawUnresolvedDependencyOut = nullptr);
bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const ExprLocalsValueKindFn &inferExprKindFromLocals,
                               const ExprLocalsValueKindFn &inferArrayElementKindFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error,
                               bool *sawUnresolvedDependencyOut = nullptr);
bool inferReturnInferenceBindingIntoLocals(const Expr &bindingExpr,
                                           bool isParameter,
                                           const std::string &definitionPath,
                                           LocalMap &activeLocals,
                                           const ExprPredicateFn &isBindingMutable,
                                           const BindingKindForInferenceFn &bindingKind,
                                           const ExprPredicateFn
                                               &hasExplicitBindingTypeTransform,
                                           const BindingValueKindForInferenceFn &bindingValueKind,
                                           const ExprLocalsValueKindFn &inferExprKindFromLocals,
                                           const ExprPredicateFn &isFileErrorBinding,
                                           const ExprLocalInfoVisitorFn &setReferenceArrayInfo,
                                           const ExprLocalInfoVisitorFn &applyStructArrayInfo,
                                           const ExprLocalInfoVisitorFn &applyStructValueInfo,
                                           const InferStructExprPathFromLocalsFn &inferStructExprPathFromLocals,
                                           const ExprPredicateFn &isStringBinding,
                                           std::string &error);

} // namespace primec::ir_lowerer
