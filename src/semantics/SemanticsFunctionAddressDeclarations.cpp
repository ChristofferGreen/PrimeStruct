#include "SemanticsFunctionAddressDeclarations.h"

#include "primec/semantics/FunctionAddressDefinitions.h"

namespace primec::semantics {

std::string functionAddressDeclarationProblem(const Definition &def,
                                              const std::vector<ParameterInfo> &params,
                                              const std::function<bool(const std::string &)> &definitionExists) {
  const bool indirect = isIndirectCallDefinition(def);
  const std::string marker = indirect ? "indirect_call" : "function_address";
  if (indirect && isFunctionAddressDefinition(def)) {
    return "indirect_call and function_address cannot mark one definition";
  }
  if (!def.templateArgs.empty()) {
    return marker + " declaration cannot be generic";
  }
  if (!def.statements.empty() || def.returnExpr.has_value()) {
    return marker + " declaration must have an empty body";
  }
  const Transform *returnTransform = nullptr;
  for (const Transform &transform : def.transforms) {
    if (transform.name == "return") {
      returnTransform = &transform;
    }
  }
  if (returnTransform == nullptr) {
    return marker + " declaration requires an explicit return type";
  }
  if (indirect) {
    if (!indirectCallTransform(def)->arguments.empty()) {
      return marker + " takes no arguments";
    }
    if (params.empty() || params.front().binding.typeName != "u64" || !params.front().binding.typeTemplateArg.empty()) {
      return marker + " takes the function address as its first parameter, a u64";
    }
    return {};
  }
  const std::string target = functionAddressTarget(def);
  if (target.empty()) {
    return marker + " takes the path of one definition, as in function_address(\"/path\")";
  }
  if (!params.empty()) {
    return marker + " declaration takes no parameters";
  }
  if (returnTransform->templateArgs.size() != 1 || returnTransform->templateArgs.front() != "u64") {
    return marker + " declaration returns u64";
  }
  if (!definitionExists(target)) {
    return marker + " names no definition (" + target + ")";
  }
  return {};
}

} // namespace primec::semantics
