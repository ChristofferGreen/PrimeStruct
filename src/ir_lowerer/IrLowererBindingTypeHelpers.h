#pragma once

#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

using BindingKindFromTransformsFn = std::function<LocalInfo::Kind(const Expr &)>;
using BindingValueKindFromTransformsFn =
    std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;

struct BindingTypeAdapters {
  BindingKindFromTransformsFn bindingKind{};
  ExprPredicateFn isBindingMutable{};
  ExprPredicateFn hasExplicitBindingTypeTransform{};
  ExprPredicateFn isStringBinding{};
  ExprPredicateFn isFileErrorBinding{};
  BindingValueKindFromTransformsFn bindingValueKind{};
  ExprLocalInfoVisitorFn setReferenceArrayInfo{};
};

bool validateSemanticProductBindingCoverage(const Program &program,
                                            const SemanticProgram *semanticProgram,
                                            std::string &error);
bool validateSemanticProductLocalAutoCoverage(const Program &program,
                                              const SemanticProgram *semanticProgram,
                                              std::string &error);
bool validateSemanticProductCollectionSpecializationCoverage(
    const Program &program,
    const SemanticProgram *semanticProgram,
    std::string &error);
bool validateSemanticProductArrayExtentCoverage(const Program &program,
                                                const SemanticProgram *semanticProgram,
                                                std::string &error);
BindingTypeAdapters makeBindingTypeAdapters(const SemanticProgram *semanticProgram = nullptr);

std::string resolveSemanticProductTypeText(const SemanticProgram *semanticProgram,
                                           const std::string &text,
                                           SymbolId textId);
std::string normalizeCollectionBindingTypeName(const std::string &name);
bool typeTextUsesRawBuiltinSoaVectorLayout(const std::string &typeText);
bool exprUsesRawBuiltinSoaVectorLayout(const Expr &expr);
LocalInfo::Kind bindingKindFromTransforms(const Expr &expr);
bool isStringTypeName(const std::string &name);
bool isStringBindingType(const Expr &expr);
bool isFileErrorBindingType(const Expr &expr);
LocalInfo::ValueKind bindingValueKindFromTransforms(const Expr &expr, LocalInfo::Kind kind);
void setReferenceArrayInfoFromTransforms(const Expr &expr, LocalInfo &info);

} // namespace primec::ir_lowerer
