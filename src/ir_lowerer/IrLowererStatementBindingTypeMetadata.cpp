#include "IrLowererStatementBindingInternal.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isExplicitPackedResultLiteral(const Expr &expr) {
  return expr.kind == Expr::Kind::Literal && !expr.isUnsigned && expr.intWidth == 64 &&
         expr.literalValue == 4294967296ull;
}

bool isPointerMemoryIntrinsicCallImpl(const Expr &expr) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return false;
  }
  return builtinName == "alloc" || builtinName == "realloc" || builtinName == "at" ||
         builtinName == "at_unsafe" || builtinName == "reinterpret";
}

bool inferPointerMemoryIntrinsicTargetsUninitializedStorageImpl(const Expr &expr, const LocalMap &localsIn) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return false;
  }
  if (builtinName == "alloc" && expr.templateArgs.size() == 1) {
    std::string targetType;
    return extractTopLevelUninitializedTypeText(expr.templateArgs.front(), targetType);
  }
  if (builtinName == "reinterpret" && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
    std::string targetType;
    return extractTopLevelUninitializedTypeText(expr.templateArgs.front(), targetType);
  }
  if ((builtinName == "realloc" && expr.args.size() == 2) || (builtinName == "at" && expr.args.size() == 3) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2)) {
    if (expr.args.front().kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(expr.args.front().name);
    return it != localsIn.end() && it->second.targetsUninitializedStorage;
  }
  return false;
}

bool resolveSpecializedExperimentalMapStructPath(const std::string &typeText, std::string &structPathOut) {
  structPathOut.clear();
  std::string normalizedType = trimTemplateTypeText(typeText);
  if (!normalizedType.empty() && normalizedType.front() != '/') {
    normalizedType.insert(normalizedType.begin(), '/');
  }
  if (normalizedType.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    structPathOut = normalizedType;
    return true;
  }

  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(typeText, base, argList) ||
      normalizeCollectionBindingTypeName(base) != "map" ||
      argList.empty()) {
    return false;
  }

  std::vector<std::string> templateArgs;
  if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 2) {
    return false;
  }

  std::string canonicalArgs = joinTemplateArgsText(templateArgs);
  canonicalArgs.erase(
      std::remove_if(canonicalArgs.begin(),
                     canonicalArgs.end(),
                     [](unsigned char ch) { return std::isspace(ch) != 0; }),
      canonicalArgs.end());

  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : canonicalArgs) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }

  std::ostringstream specializedPath;
  specializedPath << "/std/collections/experimental_map/Map__t" << std::hex << hash;
  structPathOut = specializedPath.str();
  return true;
}

bool namesExperimentalMapType(const std::string &typeText) {
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(typeText, base, arg)) {
    base = trimTemplateTypeText(base);
  } else {
    base = trimTemplateTypeText(typeText);
  }
  if (!base.empty() && base.front() == '/') {
    base.erase(base.begin());
  }
  return base == "Map" ||
         base == "std/collections/experimental_map/Map" ||
         base.rfind("std/collections/experimental_map/Map__", 0) == 0;
}

} // namespace

bool applyErrorTypeMetadata(const std::string &typeText, LocalInfo &infoOut) {
  const std::string normalized = trimTemplateTypeText(typeText);
  if (normalized != "FileError" && normalized != "ImageError" &&
      normalized != "ContainerError" && normalized != "GfxError") {
    return false;
  }

  infoOut.errorTypeName = normalized;
  infoOut.valueKind = LocalInfo::ValueKind::Int32;
  if (normalized == "FileError") {
    infoOut.isFileError = true;
    infoOut.errorHelperNamespacePath = "/std/file/FileError";
  } else if (normalized == "ImageError") {
    infoOut.errorHelperNamespacePath = "/std/image/ImageError";
  } else if (normalized == "ContainerError") {
    infoOut.errorHelperNamespacePath = "/std/collections/ContainerError";
  }
  return true;
}

bool isFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string arg;
  return splitTemplateTypeName(trimTemplateTypeText(typeText), base, arg) &&
         normalizeCollectionBindingTypeName(base) == "File";
}

bool extractResultValueTypeText(const std::string &typeText, std::string &valueTypeOut) {
  valueTypeOut.clear();
  std::string base;
  std::string argList;
  std::vector<std::string> args;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argList) ||
      normalizeCollectionBindingTypeName(base) != "Result" ||
      !splitTemplateArgs(argList, args) || args.size() != 2) {
    return false;
  }
  valueTypeOut = trimTemplateTypeText(args.front());
  return true;
}

bool hasSoaVectorTypeTransform(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "soa_vector") {
      return true;
    }
  }
  return false;
}

bool extractArgsPackElementTypeText(const Expr &expr, std::string &typeTextOut) {
  typeTextOut.clear();
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (transform.name != "args" || transform.templateArgs.size() != 1) {
      return false;
    }
    typeTextOut = trimTemplateTypeText(transform.templateArgs.front());
    return !typeTextOut.empty();
  }
  return false;
}

bool isExplicitPackedResultReturnExpr(const Expr &expr) {
  if (!(expr.kind == Expr::Kind::Call && isSimpleCallName(expr, "multiply") && expr.args.size() == 2)) {
    return false;
  }
  return isExplicitPackedResultLiteral(expr.args[0]) || isExplicitPackedResultLiteral(expr.args[1]);
}

bool inferCallParameterDefaultResultInfo(
    const Expr &expr,
    const LocalMap &localsForKindInference,
    const InferBindingExprKindFn &inferExprKind,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    ResultExprInfo &infoOut) {
  const auto resolveMethodCall = resolveMethodCallDefinition
                                     ? resolveMethodCallDefinition
                                     : [](const Expr &, const LocalMap &) -> const Definition * { return nullptr; };
  const auto resolveDefinition = resolveDefinitionCall
                                     ? resolveDefinitionCall
                                     : [](const Expr &) -> const Definition * { return nullptr; };
  const auto lookupReturnInfo = getReturnInfo ? getReturnInfo : [](const std::string &, ReturnInfo &) { return false; };
  return resolveResultExprInfoFromLocals(
      expr,
      localsForKindInference,
      resolveMethodCall,
      resolveDefinition,
      lookupReturnInfo,
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
      infoOut);
}

void applyArgsPackElementMetadata(const std::string &typeText, LocalInfo &infoOut) {
  if (applyErrorTypeMetadata(typeText, infoOut)) {
    return;
  }

  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
  if (parseResultTypeName(typeText, resultHasValue, resultValueKind, resultErrorType)) {
    infoOut.isResult = true;
    infoOut.resultHasValue = resultHasValue;
    std::string resultValueType;
    infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
    infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
    infoOut.resultValueIsFileHandle =
        resultHasValue && extractResultValueTypeText(typeText, resultValueType) &&
        isFileHandleTypeText(resultValueType);
    if (resultHasValue && !resultValueType.empty()) {
      resolveSupportedResultCollectionType(
          resultValueType,
          infoOut.resultValueCollectionKind,
          infoOut.resultValueKind,
          &infoOut.resultValueMapKeyKind);
    }
    if (infoOut.resultValueIsFileHandle) {
      infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
    } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
      infoOut.resultValueKind = resultValueKind;
    }
    infoOut.resultErrorType = resultErrorType;
    infoOut.valueKind = resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    return;
  }

  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeText, base, arg)) {
    return;
  }
  base = normalizeCollectionBindingTypeName(base);
  if (base == "File") {
    infoOut.isFileHandle = true;
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
    return;
  }
  if (base == "Buffer") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Buffer;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "array") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Array;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "Pointer") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Pointer;
    std::string pointerTargetType = trimTemplateTypeText(arg);
    if (extractTopLevelUninitializedTypeText(pointerTargetType, pointerTargetType)) {
      infoOut.targetsUninitializedStorage = true;
    }
    std::string pointerBase;
    std::string pointerArg;
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "File") {
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "array") {
      infoOut.pointerToArray = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "vector") {
      infoOut.pointerToVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "soa_vector") {
      infoOut.pointerToVector = true;
      infoOut.isSoaVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(pointerArg, args) || args.size() != 2) {
        return;
      }
      infoOut.pointerToMap = true;
      infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
      infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
      infoOut.valueKind = infoOut.mapValueKind;
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "Buffer") {
      infoOut.pointerToBuffer = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (applyErrorTypeMetadata(pointerTargetType, infoOut)) {
      return;
    }
    bool pointerResultHasValue = false;
    LocalInfo::ValueKind pointerResultValueKind = LocalInfo::ValueKind::Unknown;
    std::string pointerResultErrorType;
    if (parseResultTypeName(pointerTargetType,
                            pointerResultHasValue,
                            pointerResultValueKind,
                            pointerResultErrorType)) {
      infoOut.isResult = true;
      infoOut.resultHasValue = pointerResultHasValue;
      std::string resultValueType;
      infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
      infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
      infoOut.resultValueIsFileHandle =
          pointerResultHasValue && extractResultValueTypeText(pointerTargetType, resultValueType) &&
          isFileHandleTypeText(resultValueType);
      if (pointerResultHasValue && !resultValueType.empty()) {
        resolveSupportedResultCollectionType(
            resultValueType,
            infoOut.resultValueCollectionKind,
            infoOut.resultValueKind,
            &infoOut.resultValueMapKeyKind);
      }
      if (infoOut.resultValueIsFileHandle) {
        infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
      } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
        infoOut.resultValueKind = pointerResultValueKind;
      }
      infoOut.resultErrorType = pointerResultErrorType;
      infoOut.valueKind = pointerResultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      return;
    }
    const LocalInfo::ValueKind pointerValueKind = valueKindFromTypeName(pointerTargetType);
    if (pointerValueKind != LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = pointerValueKind;
    }
    return;
  }
  if (base == "Reference") {
    std::string refBase;
    std::string refArg;
    infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
    std::string refTargetType = trimTemplateTypeText(arg);
    if (extractTopLevelUninitializedTypeText(refTargetType, refTargetType)) {
      infoOut.targetsUninitializedStorage = true;
    }
    bool refResultHasValue = false;
    LocalInfo::ValueKind refResultValueKind = LocalInfo::ValueKind::Unknown;
    std::string refResultErrorType;
    if (parseResultTypeName(refTargetType, refResultHasValue, refResultValueKind, refResultErrorType)) {
      infoOut.isResult = true;
      infoOut.resultHasValue = refResultHasValue;
      std::string resultValueType;
      infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
      infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
      infoOut.resultValueIsFileHandle =
          refResultHasValue && extractResultValueTypeText(refTargetType, resultValueType) &&
          isFileHandleTypeText(resultValueType);
      if (refResultHasValue && !resultValueType.empty()) {
        resolveSupportedResultCollectionType(
            resultValueType,
            infoOut.resultValueCollectionKind,
            infoOut.resultValueKind,
            &infoOut.resultValueMapKeyKind);
      }
      if (infoOut.resultValueIsFileHandle) {
        infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
      } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
        infoOut.resultValueKind = refResultValueKind;
      }
      infoOut.resultErrorType = refResultErrorType;
      infoOut.valueKind = refResultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      return;
    }
    if (!splitTemplateTypeName(refTargetType, refBase, refArg)) {
      applyErrorTypeMetadata(refTargetType, infoOut);
      const LocalInfo::ValueKind refValueKind = valueKindFromTypeName(refTargetType);
      if (refValueKind != LocalInfo::ValueKind::Unknown) {
        infoOut.valueKind = refValueKind;
      }
      return;
    }
    refBase = normalizeCollectionBindingTypeName(refBase);
    if (refBase == "array") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToArray = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    if (refBase == "vector") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    if (refBase == "soa_vector") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToVector = true;
      infoOut.isSoaVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    if (refBase == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(refArg, args) || args.size() != 2) {
        return;
      }
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToMap = true;
      infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
      infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
      infoOut.valueKind = infoOut.mapValueKind;
      return;
    }
    if (refBase == "File") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      return;
    }
    if (refBase == "Buffer") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToBuffer = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    const LocalInfo::ValueKind refValueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    if (refValueKind != LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = refValueKind;
    }
    return;
  }
  if (base == "vector") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Vector;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "soa_vector") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Vector;
    infoOut.isSoaVector = true;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "map") {
    std::vector<std::string> args;
    if (!splitTemplateArgs(arg, args) || args.size() != 2) {
      return;
    }
    infoOut.argsPackElementKind = LocalInfo::Kind::Map;
    infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
    infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
    infoOut.valueKind = infoOut.mapValueKind;
  }
}

void applyArgsPackElementStructMetadata(const Expr &param,
                                        const std::string &elementTypeText,
                                        const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                        const ApplyStructBindingInfoFn &applyStructValueInfo,
                                        LocalInfo &infoOut) {
  if (elementTypeText.empty() || !infoOut.structTypeName.empty()) {
    return;
  }

  Expr syntheticBinding;
  syntheticBinding.namespacePrefix = param.namespacePrefix;

  Transform syntheticTransform;
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(elementTypeText, base, arg)) {
    syntheticTransform.name = trimTemplateTypeText(base);
    if (!arg.empty()) {
      std::vector<std::string> templateArgs;
      if (splitTemplateArgs(arg, templateArgs)) {
        syntheticTransform.templateArgs = std::move(templateArgs);
      } else {
        syntheticTransform.templateArgs.push_back(trimTemplateTypeText(arg));
      }
    }
  } else {
    syntheticTransform.name = trimTemplateTypeText(elementTypeText);
  }
  syntheticBinding.transforms.push_back(std::move(syntheticTransform));

  LocalInfo elementInfo = infoOut;
  elementInfo.kind = (infoOut.argsPackElementKind == LocalInfo::Kind::Value) ? LocalInfo::Kind::Value
                                                                              : infoOut.argsPackElementKind;
  elementInfo.isArgsPack = false;
  applyStructArrayInfo(syntheticBinding, elementInfo);
  applyStructValueInfo(syntheticBinding, elementInfo);
  if (!elementInfo.structTypeName.empty()) {
    infoOut.structTypeName = elementInfo.structTypeName;
    infoOut.structFieldCount = elementInfo.structFieldCount;
    infoOut.structSlotCount = elementInfo.structSlotCount;
    return;
  }

  std::string specializedExperimentalMapStruct;
  if (namesExperimentalMapType(elementTypeText) &&
      resolveSpecializedExperimentalMapStructPath(elementTypeText, specializedExperimentalMapStruct)) {
    infoOut.structTypeName = specializedExperimentalMapStruct;
  }
}

LocalInfo::ValueKind inferPointerMemoryIntrinsicValueKind(const Expr &expr,
                                                          const LocalMap &localsIn,
                                                          const InferBindingExprKindFn &inferExprKind) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (builtinName == "alloc" && expr.templateArgs.size() == 1) {
    return valueKindFromTypeName(unwrapTopLevelUninitializedTypeText(expr.templateArgs.front()));
  }
  if ((builtinName == "realloc" && expr.args.size() == 2) || (builtinName == "at" && expr.args.size() == 3) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2)) {
    if (expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        return it->second.valueKind;
      }
    }
    return inferExprKind(expr.args.front(), localsIn);
  }
  return LocalInfo::ValueKind::Unknown;
}

std::string inferPointerMemoryIntrinsicStructType(const Expr &expr, const LocalMap &localsIn) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return "";
  }
  if (builtinName == "realloc" && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
      return it->second.structTypeName;
    }
  }
  if ((builtinName == "at" && expr.args.size() == 3 && expr.args.front().kind == Expr::Kind::Name) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name)) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
      return it->second.structTypeName;
    }
  }
  return "";
}

LocalInfo::ValueKind inferSpecialCallValueKind(const Expr &expr) {
  if (!(expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result")) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (expr.name == "ok") {
    return expr.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
  }
  if (expr.name == "error") {
    return LocalInfo::ValueKind::Bool;
  }
  if (expr.name == "why") {
    return LocalInfo::ValueKind::String;
  }
  return LocalInfo::ValueKind::Unknown;
}

bool inferPointerMemoryIntrinsicTargetsUninitializedStorage(const Expr &expr, const LocalMap &localsIn) {
  return inferPointerMemoryIntrinsicTargetsUninitializedStorageImpl(expr, localsIn);
}

bool isPointerMemoryIntrinsicCall(const Expr &expr) {
  return isPointerMemoryIntrinsicCallImpl(expr);
}

} // namespace primec::ir_lowerer
