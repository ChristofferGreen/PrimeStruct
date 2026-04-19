#pragma once

#include "IrLowererResultHelpers.h"

namespace primec::ir_lowerer {

bool usesInlineBufferResultErrorDiscriminator(const ResultExprInfo &resultInfo);

bool populateMetadataBindingInfo(const Expr &bindingExpr,
                                 LocalMap &localsIn,
                                 const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                 const ResolveCallDefinitionFn &resolveDefinitionCall,
                                 const LookupReturnInfoFn &lookupReturnInfo,
                                 const InferExprKindWithLocalsFn &inferExprKind,
                                 const SemanticProductTargetAdapter *semanticProductTargets = nullptr,
                                 std::string *errorOut = nullptr);
bool isInlineBodyBlockEnvelope(const Expr &candidate, const ResolveCallDefinitionFn &resolveDefinitionCall);
bool isInlineBodyValueEnvelope(const Expr &candidate, const ResolveCallDefinitionFn &resolveDefinitionCall);
bool mergeControlFlowResultInfos(const ResultExprInfo &first,
                                 const ResultExprInfo &second,
                                 ResultExprInfo &out);
bool inferDirectResultValueStructType(const Expr &expr,
                                      const LocalMap &localsIn,
                                      const ResolveCallDefinitionFn &resolveDefinitionCall,
                                      std::string &structTypeOut);
bool inferDirectResultValueCollectionInfo(const Expr &expr,
                                          const LocalMap &localsIn,
                                          const ResolveCallDefinitionFn &resolveDefinitionCall,
                                          LocalInfo::Kind &collectionKindOut,
                                          LocalInfo::ValueKind &valueKindOut,
                                          LocalInfo::ValueKind &mapKeyKindOut);
void applyResultValueInfoToLocal(const ResultExprInfo &resultInfo, LocalInfo &paramInfo);
void applyDirectResultValueMetadata(const Expr &valueExpr,
                                    const LocalMap &localsIn,
                                    const ResolveCallDefinitionFn &resolveDefinitionCall,
                                    const InferExprKindWithLocalsFn &inferExprKind,
                                    const SemanticProductTargetAdapter *semanticProductTargets,
                                    ResultExprInfo &out);
bool resolveBodyResultExprInfo(const std::vector<Expr> &bodyExprs,
                               const LocalMap &localsIn,
                               const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                               const ResolveCallDefinitionFn &resolveDefinitionCall,
                               const LookupReturnInfoFn &lookupReturnInfo,
                               const InferExprKindWithLocalsFn &inferExprKind,
                               ResultExprInfo &out,
                               const SemanticProductTargetAdapter *semanticProductTargets = nullptr,
                               std::string *errorOut = nullptr);
bool resolveResultLambdaValueExprForMetadata(const Expr &lambdaExpr,
                                             LocalMap &lambdaLocals,
                                             const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                             const ResolveCallDefinitionFn &resolveDefinitionCall,
                                             const LookupReturnInfoFn &lookupReturnInfo,
                                             const InferExprKindWithLocalsFn &inferExprKind,
                                             const Expr *&valueExprOut,
                                             const SemanticProductTargetAdapter *semanticProductTargets = nullptr,
                                             std::string *errorOut = nullptr);

} // namespace primec::ir_lowerer
