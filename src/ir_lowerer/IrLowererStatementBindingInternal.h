#pragma once

#include "IrLowererStatementBindingHelpers.h"

namespace primec {
struct SemanticProgram;
}

namespace primec::ir_lowerer {

struct SemanticProductIndex;

bool applyErrorTypeMetadata(const std::string &typeText, LocalInfo &infoOut);
bool isFileHandleTypeText(const std::string &typeText);
bool extractResultValueTypeText(const std::string &typeText, std::string &valueTypeOut);
bool hasSoaVectorTypeTransform(const Expr &expr);
bool extractArgsPackElementTypeText(const Expr &expr, std::string &typeTextOut);
bool extractArgsPackElementTypeTextFromTypeText(const std::string &typeText, std::string &typeTextOut);
bool isExplicitPackedResultReturnExpr(const Expr &expr);
bool inferCallParameterDefaultResultInfo(
    const Expr &expr,
    const LocalMap &localsForKindInference,
    const InferBindingExprKindFn &inferExprKind,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    ResultExprInfo &infoOut,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
void applyArgsPackElementMetadata(const std::string &typeText, LocalInfo &infoOut);
void applyArgsPackElementStructMetadata(const Expr &param,
                                        const std::string &elementTypeText,
                                        const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                        const ApplyStructBindingInfoFn &applyStructValueInfo,
                                        LocalInfo &infoOut);
LocalInfo::ValueKind inferPointerMemoryIntrinsicValueKind(const Expr &expr,
                                                          const LocalMap &localsIn,
                                                          const InferBindingExprKindFn &inferExprKind);
std::string inferPointerMemoryIntrinsicStructType(const Expr &expr, const LocalMap &localsIn);
LocalInfo::ValueKind inferSpecialCallValueKind(const Expr &expr);

} // namespace primec::ir_lowerer
