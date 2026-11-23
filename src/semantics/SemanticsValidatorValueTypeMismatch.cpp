#include "SemanticsValidator.h"

namespace primec::semantics {

// The type check a value gets against a declared type: a `push` argument against the element type, and a struct
// field's initializer against the field. The same rule as a local's initializer (a scalar of another kind, a
// `string` for another type, a struct for another struct), restricted to what the validator infers for sure.

namespace {

bool isPlainScalarKind(ReturnKind kind) {
  switch (kind) {
  case ReturnKind::Int:
  case ReturnKind::Int64:
  case ReturnKind::UInt64:
  case ReturnKind::UInt8:
  case ReturnKind::Float32:
  case ReturnKind::Float64:
  case ReturnKind::Bool:
  case ReturnKind::String:
    return true;
  default:
    return false;
  }
}

} // namespace

std::string
SemanticsValidator::valueTypeMismatch(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &value,
                                      const std::string &expectedTypeText) {
  const std::string expectedType = normalizeBindingTypeName(expectedTypeText);
  if (expectedType.empty()) {
    return {};
  }
  const ReturnKind expectedKind = returnKindForTypeName(expectedType);
  const ReturnKind actualKind = inferExprReturnKind(value, params, locals);
  if (expectedKind != ReturnKind::Unknown) {
    // Only a scalar kind of the same family converts (an `f32` into an `f64` element), as a binding does.
    const bool bothFloat =
        (expectedKind == ReturnKind::Float32 || expectedKind == ReturnKind::Float64) &&
        (actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64);
    if (actualKind != ReturnKind::Unknown && actualKind != expectedKind && !bothFloat &&
        isPlainScalarKind(actualKind)) {
      return typeNameForReturnKind(actualKind);
    }
    if (actualKind == ReturnKind::String && expectedKind != ReturnKind::String) {
      return "string";
    }
    return {};
  }
  const std::string expectedStruct =
      resolveStructTypePath(expectedType, value.namespacePrefix, structNames_);
  if (expectedStruct.empty() || structNames_.count(expectedStruct) == 0) {
    return {};
  }
  const std::string actualStruct = inferStructReturnPath(value, params, locals);
  if (structNames_.count(actualStruct) > 0) {
    if (actualStruct.substr(0, actualStruct.find("__t")) !=
        expectedStruct.substr(0, expectedStruct.find("__t"))) {
      return actualStruct.front() == '/' ? actualStruct.substr(1) : actualStruct;
    }
    return {};
  }
  if (actualKind == ReturnKind::String) {
    return "string";
  }
  if (actualKind != ReturnKind::Unknown && isPlainScalarKind(actualKind)) {
    return typeNameForReturnKind(actualKind);
  }
  return {};
}

bool SemanticsValidator::checkIndexedAssignmentTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr) {
  const Expr &target = expr.args.front();
  const std::string &definitionPath = currentValidationState_.context.definitionPath;
  noteVectorElementPlaces(definitionPath, target, false, vectorElementPlaceSites_);
  if (!isStdlibVectorElementRead(target)) {
    return true;
  }
  BindingInfo elementInfo;
  if (inferBindingTypeFromInitializer(target, params, locals, elementInfo)) {
    const std::string expectedType = expectedBindingTypeText(elementInfo);
    const std::string actualType = valueTypeMismatch(params, locals, expr.args[1], expectedType);
    if (!actualType.empty()) {
      return failExprDiagnostic(
          expr, "assign value type mismatch: expected " + expectedType + ", got " + actualType);
    }
  }
  vectorElementPlaceSites_.addAssignedElement(definitionPath, target);
  return true;
}

} // namespace primec::semantics
