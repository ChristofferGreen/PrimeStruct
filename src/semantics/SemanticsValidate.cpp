#include "primec/Semantics.h"

#include "SemanticsValidator.h"
#include "primec/TransformRegistry.h"

#include <string>
#include <vector>

namespace primec {

namespace {
bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "struct" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "public" || name == "private" ||
         name == "package" || name == "static" || name == "single_type_to_return" || name == "stack" ||
         name == "heap" || name == "buffer";
}

bool isSingleTypeReturnCandidate(const Transform &transform) {
  if (!transform.arguments.empty()) {
    return false;
  }
  if (isNonTypeTransformName(transform.name)) {
    return false;
  }
  return true;
}

std::string formatTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

std::string formatTransformType(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + formatTemplateArgs(transform.templateArgs) + ">";
}

bool validateTransformPhaseList(const std::vector<Transform> &transforms,
                                const std::string &context,
                                std::string &error) {
  for (const auto &transform : transforms) {
    const bool isText = primec::isTextTransformName(transform.name);
    const bool isSemantic = primec::isSemanticTransformName(transform.name);
    if (transform.phase == TransformPhase::Text) {
      if (!isText && isSemantic) {
        error = "semantic transform cannot appear in text(...) group on " + context;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Semantic) {
      if (!isSemantic && isText) {
        error = "text transform cannot appear in semantic(...) group on " + context;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Auto && isText && isSemantic) {
      error = "ambiguous transform name on " + context + ": " + transform.name;
      return false;
    }
  }
  return true;
}

bool validateTransformListContext(const std::vector<Transform> &transforms,
                                  const std::string &context,
                                  bool allowSingleTypeToReturn,
                                  std::string &error) {
  if (!validateTransformPhaseList(transforms, context, error)) {
    return false;
  }
  if (!allowSingleTypeToReturn) {
    for (const auto &transform : transforms) {
      if (transform.name == "single_type_to_return") {
        error = "single_type_to_return is only valid on definitions: " + context;
        return false;
      }
    }
  }
  return true;
}

bool applySingleTypeToReturn(std::vector<Transform> &transforms,
                             bool force,
                             const std::string &context,
                             std::string &error) {
  size_t markerIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (transforms[i].name != "single_type_to_return") {
      continue;
    }
    if (markerIndex != transforms.size()) {
      error = "duplicate single_type_to_return transform on " + context;
      return false;
    }
    if (!transforms[i].templateArgs.empty()) {
      error = "single_type_to_return does not accept template arguments on " + context;
      return false;
    }
    if (!transforms[i].arguments.empty()) {
      error = "single_type_to_return does not accept arguments on " + context;
      return false;
    }
    markerIndex = i;
  }
  const bool hasExplicitMarker = markerIndex != transforms.size();
  if (!hasExplicitMarker && !force) {
    return true;
  }
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      if (hasExplicitMarker) {
        error = "single_type_to_return cannot be combined with return transform on " + context;
        return false;
      }
      return true;
    }
  }
  size_t typeIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (!isSingleTypeReturnCandidate(transforms[i])) {
      continue;
    }
    if (typeIndex != transforms.size()) {
      if (hasExplicitMarker) {
        error = "single_type_to_return requires a single type transform on " + context;
        return false;
      }
      return true;
    }
    typeIndex = i;
  }
  if (typeIndex == transforms.size()) {
    if (hasExplicitMarker) {
      error = "single_type_to_return requires a type transform on " + context;
      return false;
    }
    return true;
  }
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.phase = TransformPhase::Semantic;
  returnTransform.templateArgs.push_back(formatTransformType(transforms[typeIndex]));
  if (!hasExplicitMarker) {
    transforms[typeIndex] = std::move(returnTransform);
    return true;
  }
  std::vector<Transform> rewritten;
  rewritten.reserve(transforms.size() - 1);
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (i == typeIndex) {
      rewritten.push_back(std::move(returnTransform));
      continue;
    }
    rewritten.push_back(std::move(transforms[i]));
  }
  transforms = std::move(rewritten);
  return true;
}

bool validateExprTransforms(const Expr &expr, const std::string &context, std::string &error) {
  if (!validateTransformListContext(expr.transforms, context, false, error)) {
    return false;
  }
  for (const auto &arg : expr.args) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  return true;
}

bool applySemanticTransforms(Program &program,
                             const std::vector<std::string> &semanticTransforms,
                             std::string &error) {
  bool forceSingleTypeToReturn = false;
  for (const auto &name : semanticTransforms) {
    if (name == "single_type_to_return") {
      forceSingleTypeToReturn = true;
      continue;
    }
    error = "unsupported semantic transform: " + name;
    return false;
  }

  for (auto &def : program.definitions) {
    if (!validateTransformListContext(def.transforms, def.fullPath, true, error)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!validateExprTransforms(param, def.fullPath, error)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!validateExprTransforms(stmt, def.fullPath, error)) {
        return false;
      }
    }
    if (!applySingleTypeToReturn(def.transforms, forceSingleTypeToReturn, def.fullPath, error)) {
      return false;
    }
  }

  for (auto &exec : program.executions) {
    if (!validateTransformListContext(exec.transforms, exec.fullPath, false, error)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
  }

  return true;
}
} // namespace

bool Semantics::validate(Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects,
                         const std::vector<std::string> &semanticTransforms) const {
  if (!applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  semantics::SemanticsValidator validator(program, entryPath, error, defaultEffects);
  return validator.run();
}

} // namespace primec
