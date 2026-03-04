#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "IrLowererStructTypeHelpers.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

struct ResultExprInfo {
  bool isResult = false;
  bool hasValue = false;
  std::string errorType;
};

struct LocalResultInfo {
  bool found = false;
  bool isResult = false;
  bool resultHasValue = false;
  std::string resultErrorType;
  bool isFileHandle = false;
};

using LookupLocalResultInfoFn = std::function<LocalResultInfo(const std::string &)>;
using ResolveCallDefinitionFn = std::function<const Definition *(const Expr &)>;
using LookupDefinitionResultInfoFn = std::function<bool(const std::string &, ResultExprInfo &)>;
using ResolveMethodCallWithLocalsFn = std::function<const Definition *(const Expr &, const LocalMap &)>;
using LookupReturnInfoFn = std::function<bool(const std::string &, ReturnInfo &)>;
using ResolveResultExprInfoWithLocalsFn =
    std::function<bool(const Expr &, const LocalMap &, ResultExprInfo &)>;

bool resolveResultExprInfo(const Expr &expr,
                           const LookupLocalResultInfoFn &lookupLocal,
                           const ResolveCallDefinitionFn &resolveMethodCall,
                           const ResolveCallDefinitionFn &resolveDefinitionCall,
                           const LookupDefinitionResultInfoFn &lookupDefinitionResult,
                           ResultExprInfo &out);
bool resolveResultExprInfoFromLocals(const Expr &expr,
                                     const LocalMap &localsIn,
                                     const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                     const LookupReturnInfoFn &lookupReturnInfo,
                                     ResultExprInfo &out);
ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo);
bool resolveResultWhyCallInfo(const Expr &expr,
                              const LocalMap &localsIn,
                              const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                              ResultExprInfo &resultInfo,
                              std::string &error);
bool emitResultWhyLocalsFromValueExpr(
    const Expr &valueExpr,
    const LocalMap &localsIn,
    bool resultHasValue,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    int32_t &errorLocalOut);
bool isSupportedResultWhyErrorKind(LocalInfo::ValueKind kind);
std::string normalizeResultWhyErrorName(const std::string &errorType, LocalInfo::ValueKind errorKind);
void emitResultWhyErrorLocalFromResult(
    int32_t resultLocal,
    bool resultHasValue,
    int32_t errorLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
bool emitResultWhyEmptyString(
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
Expr makeResultWhyErrorValueExpr(int32_t errorLocal,
                                 LocalInfo::ValueKind valueKind,
                                 const std::string &namespacePrefix,
                                 int32_t tempOrdinal,
                                 LocalMap &callLocals);
Expr makeResultWhyBoolErrorExpr(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t tempOrdinal,
    LocalMap &callLocals,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
enum class ResultWhyCallEmitResult {
  Emitted,
  Error,
};
struct ResultWhyCallOps {
  std::function<bool(const std::string &, const std::string &, std::string &)> resolveStructTypeName;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<LocalInfo::Kind(const Expr &)> bindingKind;
  std::function<bool(const Expr &, std::string &, std::vector<std::string> &)> extractFirstBindingTypeTransform;
  std::function<bool(const std::string &, StructSlotLayoutInfo &)> resolveStructSlotLayout;
  std::function<LocalInfo::ValueKind(const std::string &)> valueKindFromTypeName;
  std::function<Expr(LocalMap &, LocalInfo::ValueKind)> makeErrorValueExpr;
  std::function<Expr(LocalMap &)> makeBoolErrorExpr;
  std::function<bool(const Expr &, const Definition &, const LocalMap &)> emitInlineDefinitionCall;
  std::function<bool(int32_t)> emitFileErrorWhy;
  std::function<bool()> emitEmptyString;
};
ResultWhyCallEmitResult emitResolvedResultWhyCall(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyCallOps &ops,
    std::string &error);

} // namespace primec::ir_lowerer
