#include "IrLowererScalarCallBlocker.h"

#include "IrLowererRealCallBlockerText.h"
#include "IrLowererRecursionAnalysis.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

std::string scalarOnlyParameterBlocker(const Definition &def) {
  for (const Expr &param : def.parameters) {
    const std::string subject = "parameter `" + param.name + "` ";
    if (isArgsPackBinding(param)) {
      return subject + "is an argument pack";
    }
    const std::string typeName = extractParameterTypeNameStatic(param);
    if (!typeName.empty() && isSupportedScalarTypeName(typeName)) {
      if (paramHasMutTransform(param)) {
        return subject + "is a `" + typeName +
               " mut`: a scalar-only call copies the value, so the caller would not see the write";
      }
      continue;
    }
    const Transform *type = taskParameterTypeTransform(param);
    std::string typeText = typeName;
    if (type != nullptr) {
      typeText = type->name;
      if (!type->templateArgs.empty()) {
        typeText += "<" + trimTemplateTypeText(type->templateArgs.front()) + ">";
      }
    }
    return subject + "is a `" + typeText +
           "`, which a call on this target does not take (only scalar values)";
  }
  return {};
}

RealCallBlocker scalarDefinitionShapeBlocker(const Definition &def) {
  static constexpr std::pair<const char *, AddressAbiBlocker> Shapes[] = {
      {"host", AddressAbiBlocker::HostDefinition},
      {"struct", AddressAbiBlocker::StructDefinition},
      {"sum", AddressAbiBlocker::SumDefinition},
      {"compute", AddressAbiBlocker::ComputeDefinition},
  };
  for (const auto &[transformName, blocker] : Shapes) {
    if (definitionHasTransform(def, transformName)) {
      return {addressAbiBlockerCode(blocker), describeAddressAbiBlocker(blocker, def)};
    }
  }
  if (definitionHasTransform(def, "on_error")) {
    return {"on-error-handler",
            "it has an `on_error` handler, which a call on this target does not take"};
  }
  return {};
}

} // namespace primec::ir_lowerer
