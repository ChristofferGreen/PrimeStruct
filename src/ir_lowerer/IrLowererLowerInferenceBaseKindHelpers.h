#pragma once

#include "IrLowererLowerInferenceSetup.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

bool isIndexedArgsPackFileHandleReceiver(const Expr &receiverExpr, const LocalMap &localsIn);
bool isIndexedBorrowedArgsPackFileHandleReceiver(const Expr &receiverExpr, const LocalMap &localsIn);
bool isIndexedPointerArgsPackFileHandleReceiver(const Expr &receiverExpr, const LocalMap &localsIn);

bool isMapTryAtCallName(const Expr &expr);
bool isMapContainsCallName(const Expr &expr);
bool inferMapTryAtResultValueKind(const Expr &expr,
                                  const LocalMap &localsIn,
                                  LocalInfo::ValueKind &kindOut);
bool inferMapContainsResultKind(const Expr &expr,
                                const LocalMap &localsIn,
                                LocalInfo::ValueKind &kindOut);

bool inferLiteralOrNameExprKindImpl(const Expr &expr,
                                    const LocalMap &localsIn,
                                    const GetSetupMathConstantNameFn &getMathConstantName,
                                    LocalInfo::ValueKind &kindOut);
bool inferCallExprBaseKindImpl(const Expr &expr,
                               const LocalMap &localsIn,
                               const InferStructExprWithLocalsFn &inferStructExprPath,
                               const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                               const std::function<bool(const Expr &,
                                                        const LocalMap &,
                                                        UninitializedStorageAccessInfo &,
                                                        bool &)> &resolveUninitializedStorage,
                               const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                               const ResolveCallDefinitionFn *resolveDefinitionCall,
                               const LookupReturnInfoFn *lookupReturnInfo,
                               const SemanticProgram *semanticProgram,
                               const SemanticProductIndex *semanticIndex,
                               const InferExprKindWithLocalsFn *fallbackInferExprKind,
                               LocalInfo::ValueKind &kindOut);

} // namespace primec::ir_lowerer
