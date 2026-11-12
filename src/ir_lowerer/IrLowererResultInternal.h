#pragma once

#include "primec/frontend/SemanticProduct.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"

namespace primec::ir_lowerer {

bool usesInlineBufferResultErrorDiscriminator(const ResultExprInfo &resultInfo);

// The query fact's result error type as text, preferring the semantic product's interned
// string when one is available.
std::string
resolveSemanticQueryResultErrorTypeText(const SemanticProductTargetAdapter *semanticProductTargets,
                                        const SemanticProgramQueryFact &queryFact);
// Whether two result error type texts name the same type, ignoring a leading slash.
bool resultErrorTypesMatch(std::string left, std::string right);

bool populateMetadataBindingInfo(const Expr &bindingExpr,
                                 LocalMap &localsIn,
                                 const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                 const ResolveCallDefinitionFn &resolveDefinitionCall,
                                 const LookupReturnInfoFn &lookupReturnInfo,
                                 const ExprLocalsValueKindFn &inferExprKind,
                                 const SemanticProgram *semanticProgram = nullptr,
                                 const SemanticProductIndex *semanticIndex = nullptr,
                                 std::string *errorOut = nullptr);
bool isInlineBodyBlockEnvelope(const Expr &candidate, const ResolveCallDefinitionFn &resolveDefinitionCall);
bool isInlineBodyValueEnvelope(const Expr &candidate, const ResolveCallDefinitionFn &resolveDefinitionCall);
bool mergeControlFlowResultInfos(const ResultExprInfo &first,
                                 const ResultExprInfo &second,
                                 ResultExprInfo &out);
bool inferDirectResultValueStructType(const Expr &expr,
                                      const LocalMap &localsIn,
                                      const ResolveCallDefinitionFn &resolveDefinitionCall,
                                      std::string &structTypeOut,
                                      bool suppressCallDefinitionFallback = false);
bool inferDirectResultValueCollectionInfo(const Expr &expr,
                                          const LocalMap &localsIn,
                                          const ResolveCallDefinitionFn &resolveDefinitionCall,
                                          LocalInfo::Kind &collectionKindOut,
                                          LocalInfo::ValueKind &valueKindOut,
                                          LocalInfo::ValueKind &keyValueKeyKindOut,
                                          bool suppressCallDefinitionFallback = false);
void applyResultValueInfoToLocal(const ResultExprInfo &resultInfo, LocalInfo &paramInfo);
void applyDirectResultValueMetadata(const Expr &valueExpr,
                                    const LocalMap &localsIn,
                                    const ResolveCallDefinitionFn &resolveDefinitionCall,
                                    const ExprLocalsValueKindFn &inferExprKind,
                                    const SemanticProgram *semanticProgram,
                                    const SemanticProductIndex *semanticIndex,
                                    ResultExprInfo &out);
bool resolveBodyResultExprInfo(const std::vector<Expr> &bodyExprs,
                               const LocalMap &localsIn,
                               const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                               const ResolveCallDefinitionFn &resolveDefinitionCall,
                               const LookupReturnInfoFn &lookupReturnInfo,
                               const ExprLocalsValueKindFn &inferExprKind,
                               ResultExprInfo &out,
                               const SemanticProgram *semanticProgram = nullptr,
                               const SemanticProductIndex *semanticIndex = nullptr,
                               std::string *errorOut = nullptr);
bool resolveResultLambdaValueExprForMetadata(const Expr &lambdaExpr,
                                             LocalMap &lambdaLocals,
                                             const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                             const ResolveCallDefinitionFn &resolveDefinitionCall,
                                             const LookupReturnInfoFn &lookupReturnInfo,
                                             const ExprLocalsValueKindFn &inferExprKind,
                                             const Expr *&valueExprOut,
                                             const SemanticProgram *semanticProgram = nullptr,
                                             const SemanticProductIndex *semanticIndex = nullptr,
                                             std::string *errorOut = nullptr);

} // namespace primec::ir_lowerer
