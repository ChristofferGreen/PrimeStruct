#include "EmitterHelpers.h"
#include "EmitterBuiltinCallPathHelpersInternal.h"

#include <cctype>

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;
using BindingVisibility = Emitter::BindingVisibility;
using ReturnKind = Emitter::ReturnKind;

std::string joinTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

ReturnKind getReturnKind(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "struct" || transform.name == "pod" || transform.name == "handle" ||
        transform.name == "gpu_lane" || transform.name == "no_padding" ||
        transform.name == "platform_independent_padding") {
      return ReturnKind::Void;
    }
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &typeName = transform.templateArgs.front();
    if (typeName == "void") {
      return ReturnKind::Void;
    }
    if (typeName == "int" || typeName == "i32") {
      return ReturnKind::Int;
    }
    if (typeName == "bool") {
      return ReturnKind::Bool;
    }
    if (typeName == "string") {
      return ReturnKind::String;
    }
    if (typeName == "i64") {
      return ReturnKind::Int64;
    }
    if (typeName == "u64") {
      return ReturnKind::UInt64;
    }
    if (typeName == "float" || typeName == "f32") {
      return ReturnKind::Float32;
    }
    if (typeName == "f64") {
      return ReturnKind::Float64;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeName, base, arg) && base == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    break;
  }
  return ReturnKind::Unknown;
}

bool isPrimitiveBindingTypeName(const std::string &name) {
  return name == "int" || name == "i32" || name == "i64" || name == "u64" || name == "float" || name == "f32" ||
         name == "f64" || name == "bool" || name == "string" || name == "FileError";
}

std::string normalizeBindingTypeName(const std::string &name) {
  if (name == "int") {
    return "i32";
  }
  if (name == "float") {
    return "f32";
  }
  if (name == "/map" || name == "std/collections/map" || name == "/std/collections/map") {
    return "map";
  }
  if (name.rfind("/map<", 0) == 0) {
    return name.substr(1);
  }
  if (name.rfind("std/collections/map<", 0) == 0) {
    return "map" + name.substr(std::string("std/collections/map").size());
  }
  if (name.rfind("/std/collections/map<", 0) == 0) {
    return "map" + name.substr(std::string("/std/collections/map").size());
  }
  return name;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
bool getBuiltinOperator(const Expr &expr, char &out);
bool getBuiltinComparison(const Expr &expr, const char *&out);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool isBuiltinClamp(const Expr &expr, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare);
bool isBuiltinMathConstantName(const std::string &name, bool allowBare);
std::string resolveExprPath(const Expr &expr);

namespace {

bool matchesScopedBuiltinSimpleCall(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  const std::string resolved = resolveExprPath(expr);
  if (normalizeInternalSoaStorageBuiltinAlias(resolved) == nameToMatch) {
    return true;
  }
  return isSimpleCallName(expr, nameToMatch);
}

} // namespace

bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg) {
  base.clear();
  arg.clear();
  const size_t open = text.find('<');
  if (open == std::string::npos || open == 0 || text.back() != '>') {
    return false;
  }
  base = text.substr(0, open);
  int depth = 0;
  size_t start = open + 1;
  for (size_t i = start; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      depth++;
      continue;
    }
    if (c == '>') {
      if (depth == 0) {
        if (i + 1 != text.size()) {
          return false;
        }
        arg = text.substr(start, i - start);
        return true;
      }
      depth--;
    }
  }
  return false;
}

bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.push_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (c == ',' && depth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  for (const auto &seg : out) {
    if (seg.empty()) {
      return false;
    }
  }
  return !out.empty();
}

bool isResultBindingTypeName(const std::string &name) {
  const std::string normalized = normalizeBindingTypeName(name);
  return normalized == "Result" || normalized == "/std/result/Result" ||
         normalized == "std/result/Result";
}

std::string sourceResultCppType(bool hasValue) {
  return hasValue ? "ps_result_value" : "ps_result_status";
}

std::string sourceResultStatusOkExpr() {
  return "ps_result_status_ok()";
}

std::string sourceResultStatusErrorExpr(const std::string &errorExpr) {
  return "ps_result_status_error(" + errorExpr + ")";
}

std::string sourceResultStatusFromErrorExpr(const std::string &errorExpr) {
  return "ps_result_status_from_error(" + errorExpr + ")";
}

std::string sourceResultStatusIsErrorExpr(const std::string &resultExpr) {
  return "ps_result_status_is_error(" + resultExpr + ")";
}

std::string sourceResultStatusErrorPayloadExpr(const std::string &resultExpr) {
  return "ps_result_status_error_payload(" + resultExpr + ")";
}

std::string sourceResultValueOkExpr(const std::string &valueExpr) {
  return "ps_result_value_ok(" + valueExpr + ")";
}

std::string sourceResultValueErrorExpr(const std::string &errorExpr) {
  return "ps_result_value_error(" + errorExpr + ")";
}

std::string sourceResultIsErrorExpr(const std::string &resultExpr) {
  return "ps_result_is_error(" + resultExpr + ")";
}

std::string sourceResultErrorPayloadExpr(const std::string &resultExpr) {
  return "ps_result_error_payload(" + resultExpr + ")";
}

std::string sourceResultValuePayloadExpr(const std::string &resultExpr) {
  return "ps_result_payload(" + resultExpr + ")";
}

std::optional<std::string> trySourceResultCppType(const std::string &base,
                                                  const std::string &argText) {
  if (!isResultBindingTypeName(base)) {
    return std::nullopt;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args)) {
    return sourceResultCppType(false);
  }
  return sourceResultCppType(args.size() == 2);
}

bool isBindingQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "static";
}

bool isBindingAuxTransformName(const std::string &name) {
  return name == "mut" || name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || isBindingQualifierName(name);
}

bool hasExplicitBindingTypeTransform(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return true;
  }
  return false;
}

BindingInfo getBindingInfo(const Expr &expr) {
  BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut" && transform.templateArgs.empty() && transform.arguments.empty()) {
      info.isMutable = true;
      continue;
    }
    if (transform.name == "copy" && transform.templateArgs.empty() && transform.arguments.empty()) {
      info.isCopy = true;
      continue;
    }
    if (transform.name == "static" && transform.templateArgs.empty() && transform.arguments.empty()) {
      info.isStatic = true;
      continue;
    }
    if (transform.name == "public" && transform.templateArgs.empty() && transform.arguments.empty()) {
      info.visibility = BindingVisibility::Public;
      continue;
    }
    if (transform.name == "private" && transform.templateArgs.empty() && transform.arguments.empty()) {
      info.visibility = BindingVisibility::Private;
      continue;
    }
    if (transform.name == "copy" || transform.name == "restrict" || transform.name == "align_bytes" ||
        transform.name == "align_kbytes") {
      continue;
    }
    if (transform.arguments.empty()) {
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (transform.templateArgs.empty()) {
        info.typeName = normalizeBindingTypeName(transform.name);
      } else if (info.typeName.empty()) {
        info.typeName = normalizeBindingTypeName(transform.name);
        info.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
      }
    }
  }
  if (info.typeName.empty()) {
    if (expr.args.size() == 1) {
      const Expr &init = expr.args.front();
      if (init.kind == Expr::Kind::Literal) {
        if (init.isUnsigned) {
          info.typeName = "u64";
        } else {
          info.typeName = init.intWidth == 64 ? "i64" : "i32";
        }
      } else if (init.kind == Expr::Kind::BoolLiteral) {
        info.typeName = "bool";
      } else if (init.kind == Expr::Kind::FloatLiteral) {
        info.typeName = init.floatWidth == 64 ? "f64" : "f32";
      } else if (init.kind == Expr::Kind::Call) {
        std::string convertName;
        if (getBuiltinConvertName(init, convertName) && init.templateArgs.size() == 1) {
          info.typeName = init.templateArgs.front();
        } else {
          std::string collection;
          if (getBuiltinCollectionName(init, collection)) {
            if ((collection == "array" || collection == "vector") && init.templateArgs.size() == 1) {
              info.typeName = collection;
              info.typeTemplateArg = init.templateArgs.front();
            } else if (collection == "map" && init.templateArgs.size() == 2) {
              info.typeName = "map";
              info.typeTemplateArg = joinTemplateArgs(init.templateArgs);
            }
          }
        }
      }
    }
    if (info.typeName.empty()) {
      info.typeName = "int";
    }
  }
  return info;
}

bool isReferenceCandidate(const BindingInfo &info) {
  const std::string typeName = normalizeBindingTypeName(info.typeName);
  if (typeName == "File" || typeName == "Result" || typeName == "FileError") {
    return false;
  }
  if (typeName == "Reference" || typeName == "Pointer") {
    return false;
  }
  if (typeName == "array" || typeName == "vector" || typeName == "args" || typeName == "map") {
    return true;
  }
  if (typeName == "string") {
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    return false;
  }
  return true;
}

bool isAliasingBinding(const BindingInfo &info) {
  const std::string typeName = normalizeBindingTypeName(info.typeName);
  return typeName == "Reference" || typeName == "Pointer";
}

std::string bindingTypeToCpp(const BindingInfo &info);

std::string bindingTypeToCpp(const std::string &typeName) {
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(typeName, base, arg)) {
    base = normalizeBindingTypeName(base);
    if (auto resultType = trySourceResultCppType(base, arg)) {
      return *resultType;
    }
    if (base == "uninitialized") {
      return bindingTypeToCpp(arg);
    }
    if (base == "Pointer") {
      std::string elemType = bindingTypeToCpp(arg);
      if (elemType.empty()) {
        elemType = "void";
      }
      return elemType + " *";
    }
    if (base == "Reference") {
      std::string elemType = bindingTypeToCpp(arg);
      if (elemType.empty()) {
        elemType = "void";
      }
      return "std::reference_wrapper<" + elemType + ">";
    }
    if (base == "array" || base == "vector" || base == "args") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        std::string elemType = bindingTypeToCpp(args[0]);
        return "std::vector<" + elemType + ">";
      }
      return "std::vector<int>";
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
        std::string keyType = bindingTypeToCpp(args[0]);
        std::string valueType = bindingTypeToCpp(args[1]);
        return "std::unordered_map<" + keyType + ", " + valueType + ">";
      }
      return "std::unordered_map<int, int>";
    }
  }
  const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
  if (normalizedTypeName == "i32" || normalizedTypeName == "int") {
    return "int";
  }
  if (normalizedTypeName == "i64") {
    return "int64_t";
  }
  if (normalizedTypeName == "u64") {
    return "uint64_t";
  }
  if (normalizedTypeName == "bool") {
    return "bool";
  }
  if (normalizedTypeName == "float" || normalizedTypeName == "f32") {
    return "float";
  }
  if (normalizedTypeName == "f64") {
    return "double";
  }
  if (normalizedTypeName == "string") {
    return "std::string_view";
  }
  if (normalizedTypeName == "File") {
    return "ps_file_handle";
  }
  if (normalizedTypeName == "FileError") {
    return "uint32_t";
  }
  return "int";
}

std::string bindingTypeToCpp(const BindingInfo &info) {
  const std::string typeName = normalizeBindingTypeName(info.typeName);
  if (typeName == "array" || typeName == "vector" || typeName == "args") {
    std::string elemType = bindingTypeToCpp(info.typeTemplateArg);
    return "std::vector<" + elemType + ">";
  }
  if (typeName == "File") {
    return "ps_file_handle";
  }
  if (typeName == "FileError") {
    return "uint32_t";
  }
  if (auto resultType = trySourceResultCppType(typeName, info.typeTemplateArg)) {
    return *resultType;
  }
  if (typeName == "map") {
    std::vector<std::string> parts;
    if (splitTopLevelTemplateArgs(info.typeTemplateArg, parts) && parts.size() == 2) {
      std::string keyType = bindingTypeToCpp(parts[0]);
      std::string valueType = bindingTypeToCpp(parts[1]);
      return "std::unordered_map<" + keyType + ", " + valueType + ">";
    }
    return "std::unordered_map<int, int>";
  }
  if (typeName == "Pointer") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg);
    if (base.empty()) {
      base = "void";
    }
    return base + " *";
  }
  if (typeName == "Reference") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg);
    if (base.empty()) {
      base = "void";
    }
    return base + " &";
  }
  return bindingTypeToCpp(typeName);
}

ReturnKind returnKindForTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return ReturnKind::Int;
  }
  if (name == "i64") {
    return ReturnKind::Int64;
  }
  if (name == "u64") {
    return ReturnKind::UInt64;
  }
  if (name == "bool") {
    return ReturnKind::Bool;
  }
  if (name == "string") {
    return ReturnKind::String;
  }
  if (name == "float" || name == "f32") {
    return ReturnKind::Float32;
  }
  if (name == "f64") {
    return ReturnKind::Float64;
  }
  if (name == "void") {
    return ReturnKind::Void;
  }
  if (name == "FileError") {
    return ReturnKind::Int;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg)) {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return ReturnKind::Unknown;
    }
    const bool isCollectionLike =
        ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer" ||
          base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
          base == "/std/collections/experimental_vector/Vector" ||
          base.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
          base.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) &&
         args.size() == 1) ||
        ((base == "map" || base == "Map" || base == "std/collections/experimental_map/Map" ||
          base == "/std/collections/experimental_map/Map" ||
          base.rfind("std/collections/experimental_map/Map__", 0) == 0 ||
          base.rfind("/std/collections/experimental_map/Map__", 0) == 0) &&
         args.size() == 2);
    if (isCollectionLike) {
      return ReturnKind::Array;
    }
  }
  return ReturnKind::Unknown;
}

ReturnKind combineNumericKinds(ReturnKind left, ReturnKind right) {
  if (left == ReturnKind::Array || right == ReturnKind::Array) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::String ||
      right == ReturnKind::String || left == ReturnKind::Void || right == ReturnKind::Void) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
    return ReturnKind::Float64;
  }
  if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
    return ReturnKind::Float32;
  }
  if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
    return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
  }
  if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
    if ((left == ReturnKind::Int64 || left == ReturnKind::Int) && (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
      return ReturnKind::Int64;
    }
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Int && right == ReturnKind::Int) {
    return ReturnKind::Int;
  }
  return ReturnKind::Unknown;
}

ReturnKind inferPrimitiveReturnKind(const Expr &expr,
                                   const std::unordered_map<std::string, BindingInfo> &localTypes,
                                   const std::unordered_map<std::string, ReturnKind> &returnKinds,
                                   bool allowMathBare) {
  if (expr.kind == Expr::Kind::Literal) {
    if (expr.isUnsigned) {
      return ReturnKind::UInt64;
    }
    return expr.intWidth == 64 ? ReturnKind::Int64 : ReturnKind::Int;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return ReturnKind::Bool;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return ReturnKind::String;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isBuiltinMathConstantName(resolveExprPath(expr), allowMathBare)) {
      return ReturnKind::Float64;
    }
    auto it = localTypes.find(expr.name);
    if (it == localTypes.end()) {
      return ReturnKind::Unknown;
    }
    if (!isPrimitiveBindingTypeName(it->second.typeName)) {
      return ReturnKind::Unknown;
    }
    return returnKindForTypeName(normalizeBindingTypeName(it->second.typeName));
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return ReturnKind::Unknown;
  }
  if (expr.hasBodyArguments && expr.args.empty()) {
    std::unordered_map<std::string, BindingInfo> blockTypes = localTypes;
    ReturnKind lastKind = ReturnKind::Unknown;
    bool sawValue = false;
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (bodyExpr.isBinding && bodyExpr.args.size() == 1) {
        BindingInfo binding = getBindingInfo(bodyExpr);
        if (!hasExplicitBindingTypeTransform(bodyExpr)) {
          ReturnKind initKind = inferPrimitiveReturnKind(bodyExpr.args.front(), blockTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        blockTypes[bodyExpr.name] = binding;
        continue;
      }
      if (bodyExpr.isBinding) {
        continue;
      }
      sawValue = true;
      lastKind = inferPrimitiveReturnKind(bodyExpr, blockTypes, returnKinds, allowMathBare);
    }
    return sawValue ? lastKind : ReturnKind::Unknown;
  }
  std::string resolved = resolveExprPath(expr);
  auto kindIt = returnKinds.find(resolved);
  if (kindIt != returnKinds.end()) {
    ReturnKind kind = kindIt->second;
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Bool || kind == ReturnKind::String || kind == ReturnKind::Float32 ||
        kind == ReturnKind::Float64) {
      return kind;
    }
  }
  std::string builtinName;
  if (getBuiltinConvertName(expr, builtinName) && expr.templateArgs.size() == 1) {
    return returnKindForTypeName(normalizeBindingTypeName(expr.templateArgs.front()));
  }
  if (matchesScopedBuiltinSimpleCall(expr, "negate") && expr.args.size() == 1) {
    ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front(), localTypes, returnKinds, allowMathBare);
    if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
      return ReturnKind::Unknown;
    }
    return argKind;
  }
  if (matchesScopedBuiltinSimpleCall(expr, "assign") && expr.args.size() == 2) {
    return inferPrimitiveReturnKind(expr.args[1], localTypes, returnKinds, allowMathBare);
  }
  if (matchesScopedBuiltinSimpleCall(expr, "move") && expr.args.size() == 1) {
    return inferPrimitiveReturnKind(expr.args.front(), localTypes, returnKinds, allowMathBare);
  }
  std::string mutateName;
  if (getBuiltinMutationName(expr, mutateName) && expr.args.size() == 1) {
    return inferPrimitiveReturnKind(expr.args.front(), localTypes, returnKinds, allowMathBare);
  }
  if (matchesScopedBuiltinSimpleCall(expr, "if") && expr.args.size() == 3) {
    ReturnKind thenKind = inferPrimitiveReturnKind(expr.args[1], localTypes, returnKinds, allowMathBare);
    ReturnKind elseKind = inferPrimitiveReturnKind(expr.args[2], localTypes, returnKinds, allowMathBare);
    if (thenKind == elseKind) {
      return thenKind;
    }
    return combineNumericKinds(thenKind, elseKind);
  }
  if (matchesScopedBuiltinSimpleCall(expr, "and") || matchesScopedBuiltinSimpleCall(expr, "or") ||
      matchesScopedBuiltinSimpleCall(expr, "not")) {
    return ReturnKind::Bool;
  }
  if (isBuiltinClamp(expr, allowMathBare) && expr.args.size() == 3) {
    ReturnKind result = inferPrimitiveReturnKind(expr.args[0], localTypes, returnKinds, allowMathBare);
    result = combineNumericKinds(result, inferPrimitiveReturnKind(expr.args[1], localTypes, returnKinds, allowMathBare));
    result = combineNumericKinds(result, inferPrimitiveReturnKind(expr.args[2], localTypes, returnKinds, allowMathBare));
    return result;
  }
  std::string minMaxName;
  if (getBuiltinMinMaxName(expr, minMaxName, allowMathBare) && expr.args.size() == 2) {
    ReturnKind result = inferPrimitiveReturnKind(expr.args[0], localTypes, returnKinds, allowMathBare);
    result = combineNumericKinds(result, inferPrimitiveReturnKind(expr.args[1], localTypes, returnKinds, allowMathBare));
    return result;
  }
  std::string absSignName;
  if (getBuiltinAbsSignName(expr, absSignName, allowMathBare) && expr.args.size() == 1) {
    ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front(), localTypes, returnKinds, allowMathBare);
    if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
      return ReturnKind::Unknown;
    }
    return argKind;
  }
  std::string saturateName;
  if (getBuiltinSaturateName(expr, saturateName, allowMathBare) && expr.args.size() == 1) {
    ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front(), localTypes, returnKinds, allowMathBare);
    if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
      return ReturnKind::Unknown;
    }
    return argKind;
  }
  std::string mathName;
  if (getBuiltinMathName(expr, mathName, allowMathBare)) {
    if (mathName == "is_nan" || mathName == "is_inf" || mathName == "is_finite") {
      return ReturnKind::Bool;
    }
    if (mathName == "lerp" && expr.args.size() == 3) {
      ReturnKind result = inferPrimitiveReturnKind(expr.args[0], localTypes, returnKinds, allowMathBare);
      result = combineNumericKinds(result, inferPrimitiveReturnKind(expr.args[1], localTypes, returnKinds, allowMathBare));
      result = combineNumericKinds(result, inferPrimitiveReturnKind(expr.args[2], localTypes, returnKinds, allowMathBare));
      return result;
    }
    if (mathName == "pow" && expr.args.size() == 2) {
      ReturnKind result = inferPrimitiveReturnKind(expr.args[0], localTypes, returnKinds, allowMathBare);
      result = combineNumericKinds(result, inferPrimitiveReturnKind(expr.args[1], localTypes, returnKinds, allowMathBare));
      return result;
    }
    if (expr.args.empty()) {
      return ReturnKind::Unknown;
    }
    ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front(), localTypes, returnKinds, allowMathBare);
    if (argKind == ReturnKind::Float64) {
      return ReturnKind::Float64;
    }
    if (argKind == ReturnKind::Float32) {
      return ReturnKind::Float32;
    }
    return ReturnKind::Unknown;
  }
  const char *cmp = nullptr;
  if (getBuiltinComparison(expr, cmp)) {
    return ReturnKind::Bool;
  }
  char op = '\0';
  if (getBuiltinOperator(expr, op) && expr.args.size() == 2) {
    ReturnKind left = inferPrimitiveReturnKind(expr.args[0], localTypes, returnKinds, allowMathBare);
    ReturnKind right = inferPrimitiveReturnKind(expr.args[1], localTypes, returnKinds, allowMathBare);
    return combineNumericKinds(left, right);
  }
  return ReturnKind::Unknown;
}

} // namespace primec::emitter
