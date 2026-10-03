#include "IrLowererCountAccessHelpers.h"
#include "IrLowererCountAccessClassifiers.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/ir/SoaPathHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererCountAccessInternal.h"

namespace primec::ir_lowerer {
using namespace count_access_internal;

bool resolveEntryArgsParameter(const Definition &entryDef,
                               const SemanticProgram *semanticProgram,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error) {
  if (semanticProgram != nullptr) {
    return resolveEntryArgsParameterFromSemanticProduct(
        entryDef, semanticProgram, hasEntryArgsOut, entryArgsNameOut, error);
  }

  hasEntryArgsOut = false;
  entryArgsNameOut.clear();
  if (entryDef.parameters.empty()) {
    return true;
  }
  if (entryDef.parameters.size() != 1) {
    error = "native backend only supports a single array<string> entry parameter";
    return false;
  }
  const Expr &param = entryDef.parameters.front();
  if (!isEntryArgsParam(param)) {
    error = "native backend entry parameter must be array<string>";
    return false;
  }
  if (!param.args.empty()) {
    error = "native backend does not allow entry parameter defaults";
    return false;
  }
  hasEntryArgsOut = true;
  entryArgsNameOut = param.name;
  return true;
}

bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error) {
  return resolveEntryArgsParameter(entryDef, nullptr, hasEntryArgsOut, entryArgsNameOut, error);
}

bool buildEntryCountAccessSetup(const Definition &entryDef,
                                const SemanticProgram *semanticProgram,
                                EntryCountAccessSetup &out,
                                std::string &error) {
  out = {};
  if (!resolveEntryArgsParameter(entryDef, semanticProgram, out.hasEntryArgs, out.entryArgsName, error)) {
    return false;
  }
  out.classifiers =
      makeCountAccessClassifiers(out.hasEntryArgs, out.entryArgsName, semanticProgram);
  return true;
}

bool buildEntryCountAccessSetup(const Definition &entryDef, EntryCountAccessSetup &out, std::string &error) {
  return buildEntryCountAccessSetup(entryDef, nullptr, out, error);
}

CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs, const std::string &entryArgsName) {
  return makeCountAccessClassifiers(hasEntryArgs, entryArgsName, nullptr);
}

CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs,
                                                  const std::string &entryArgsName,
                                                  const SemanticProgram *semanticProgram) {
  CountAccessClassifiers classifiers{};
  classifiers.isEntryArgsName = makeIsEntryArgsName(hasEntryArgs, entryArgsName);
  classifiers.isArrayCountCall = makeIsArrayCountCall(hasEntryArgs, entryArgsName, semanticProgram);
  classifiers.isVectorCapacityCall = makeIsVectorCapacityCall(semanticProgram);
  classifiers.isStringCountCall = makeIsStringCountCall(semanticProgram);
  return classifiers;
}

ExprLocalsPredicateFn makeIsEntryArgsName(bool hasEntryArgs, const std::string &entryArgsName) {
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isEntryArgsName(expr, localsIn, hasEntryArgs, entryArgsName);
  };
}

ExprLocalsPredicateFn makeIsArrayCountCall(bool hasEntryArgs, const std::string &entryArgsName) {
  return makeIsArrayCountCall(hasEntryArgs, entryArgsName, nullptr);
}

ExprLocalsPredicateFn makeIsArrayCountCall(bool hasEntryArgs,
                                        const std::string &entryArgsName,
                                        const SemanticProgram *semanticProgram) {
  auto semanticIndex = semanticProgram == nullptr
                           ? std::shared_ptr<SemanticProductIndex>{}
                           : std::make_shared<SemanticProductIndex>(
                                 buildSemanticProductIndex(semanticProgram));
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isArrayCountCall(expr,
                            localsIn,
                            hasEntryArgs,
                            entryArgsName,
                            semanticProgram,
                            semanticIndex.get());
  };
}

ExprLocalsPredicateFn makeIsVectorCapacityCall() {
  return makeIsVectorCapacityCall(nullptr);
}

ExprLocalsPredicateFn makeIsVectorCapacityCall(const SemanticProgram *semanticProgram) {
  auto semanticIndex = semanticProgram == nullptr
                           ? std::shared_ptr<SemanticProductIndex>{}
                           : std::make_shared<SemanticProductIndex>(
                                 buildSemanticProductIndex(semanticProgram));
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isVectorCapacityCall(expr, localsIn, semanticProgram, semanticIndex.get());
  };
}

ExprLocalsPredicateFn makeIsStringCountCall() {
  return makeIsStringCountCall(nullptr);
}

ExprLocalsPredicateFn makeIsStringCountCall(const SemanticProgram *semanticProgram) {
  auto semanticIndex = semanticProgram == nullptr
                           ? std::shared_ptr<SemanticProductIndex>{}
                           : std::make_shared<SemanticProductIndex>(
                                 buildSemanticProductIndex(semanticProgram));
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isStringCountCall(expr, localsIn, semanticProgram, semanticIndex.get());
  };
}

} // namespace primec::ir_lowerer
