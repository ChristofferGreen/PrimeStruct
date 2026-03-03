#pragma once

#include <functional>
#include <string>

#include "IrLowererFlowHelpers.h"
#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

enum class MissingReturnBehavior { Error, Void };

struct ReturnInferenceOptions {
  MissingReturnBehavior missingReturnBehavior = MissingReturnBehavior::Error;
  bool includeDefinitionReturnExpr = false;
};

struct EntryReturnConfig {
  bool hasReturnTransform = false;
  bool returnsVoid = false;
  bool hasResultInfo = false;
  ResultReturnInfo resultInfo;
};

using InferBindingIntoLocalsFn = std::function<bool(const Expr &, bool, LocalMap &, std::string &)>;
using InferValueKindFromLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ExpandMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;

bool analyzeEntryReturnTransforms(const Definition &entryDef,
                                  const std::string &entryPath,
                                  EntryReturnConfig &out,
                                  std::string &error);

bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                               const InferValueKindFromLocalsFn &inferArrayElementKindFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error);

} // namespace primec::ir_lowerer
