#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/frontend/SemanticProduct.h"

namespace primec::ir_lowerer {

struct SemanticProductIndex;


struct CountAccessClassifiers {
  ExprLocalsPredicateFn isEntryArgsName{};
  ExprLocalsPredicateFn isArrayCountCall{};
  ExprLocalsPredicateFn isVectorCapacityCall{};
  ExprLocalsPredicateFn isStringCountCall{};
};

struct EntryCountAccessSetup {
  bool hasEntryArgs = false;
  std::string entryArgsName{};
  CountAccessClassifiers classifiers{};
};
enum class StringCountCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class CountAccessCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};

bool resolveEntryArgsParameter(const Definition &entryDef,
                               const SemanticProgram *semanticProgram,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error);
bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error);
bool buildEntryCountAccessSetup(const Definition &entryDef,
                                const SemanticProgram *semanticProgram,
                                EntryCountAccessSetup &out,
                                std::string &error);
bool buildEntryCountAccessSetup(const Definition &entryDef, EntryCountAccessSetup &out, std::string &error);
CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs, const std::string &entryArgsName);
CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs,
                                                  const std::string &entryArgsName,
                                                  const SemanticProgram *semanticProgram);
ExprLocalsPredicateFn makeIsEntryArgsName(bool hasEntryArgs, const std::string &entryArgsName);
ExprLocalsPredicateFn makeIsArrayCountCall(bool hasEntryArgs, const std::string &entryArgsName);
ExprLocalsPredicateFn makeIsArrayCountCall(bool hasEntryArgs,
                                        const std::string &entryArgsName,
                                        const SemanticProgram *semanticProgram);
ExprLocalsPredicateFn makeIsVectorCapacityCall();
ExprLocalsPredicateFn makeIsVectorCapacityCall(const SemanticProgram *semanticProgram);
ExprLocalsPredicateFn makeIsStringCountCall();
ExprLocalsPredicateFn makeIsStringCountCall(const SemanticProgram *semanticProgram);
bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isArrayCountCall(const Expr &expr,
                      const LocalMap &localsIn,
                      bool hasEntryArgs,
                      const std::string &entryArgsName,
                      const SemanticProgram *semanticProgram);
// Takes a caller-owned, pre-built semantic index instead of rebuilding one
// on every call - prefer this overload whenever a SemanticProductIndex is
// already available in scope (e.g. callResolutionAdapters.semanticProductTargets.semanticIndex).
bool isArrayCountCall(const Expr &expr,
                      const LocalMap &localsIn,
                      bool hasEntryArgs,
                      const std::string &entryArgsName,
                      const SemanticProgram *semanticProgram,
                      const SemanticProductIndex *semanticIndex);
bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn);
bool isVectorCapacityCall(const Expr &expr,
                          const LocalMap &localsIn,
                          const SemanticProgram *semanticProgram,
                          const SemanticProductIndex *semanticIndex);
bool isStringCountCall(const Expr &expr, const LocalMap &localsIn);
bool isStringCountCall(const Expr &expr,
                       const LocalMap &localsIn,
                       const SemanticProgram *semanticProgram);
// Takes a caller-owned, pre-built semantic index instead of rebuilding one
// on every call - see isArrayCountCall's equivalent overload above.
bool isStringCountCall(const Expr &expr,
                       const LocalMap &localsIn,
                       const SemanticProgram *semanticProgram,
                       const SemanticProductIndex *semanticIndex);
StringCountCallEmitResult tryEmitStringCountCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isStringCountCall,
    const ExprLocalsValueKindFn &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    std::string &error);
CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &isStringCountCall,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const ExprLocalsPredicateFn &isDynamicCollectionCountTarget,
    const ExprLocalsPredicateFn &isDynamicVectorCountTarget,
    const ExprLocalsPredicateFn &isDynamicVectorCapacityTarget,
    const ExprLocalsValueKindFn &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInstructionFn &emitInstruction,
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &isStringCountCall,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInstructionFn &emitInstruction,
    std::string &error);
CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &isStringCountCall,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const ExprLocalsValueKindFn &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInstructionFn &emitInstruction,
    std::string &error);

} // namespace primec::ir_lowerer
