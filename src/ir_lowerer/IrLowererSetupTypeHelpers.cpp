#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

SetupTypeAdapters makeSetupTypeAdapters() {
  SetupTypeAdapters adapters;
  adapters.valueKindFromTypeName = makeValueKindFromTypeName();
  adapters.combineNumericKinds = makeCombineNumericKinds();
  return adapters;
}

ValueKindFromTypeNameFn makeValueKindFromTypeName() {
  return [](const std::string &name) {
    return valueKindFromTypeName(name);
  };
}

CombineNumericKindsFn makeCombineNumericKinds() {
  return [](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
    return combineNumericKinds(left, right);
  };
}

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return LocalInfo::ValueKind::Int32;
  }
  if (name == "i64") {
    return LocalInfo::ValueKind::Int64;
  }
  if (name == "u64") {
    return LocalInfo::ValueKind::UInt64;
  }
  if (name == "float" || name == "f32") {
    return LocalInfo::ValueKind::Float32;
  }
  if (name == "f64") {
    return LocalInfo::ValueKind::Float64;
  }
  if (name == "bool") {
    return LocalInfo::ValueKind::Bool;
  }
  if (name == "string") {
    return LocalInfo::ValueKind::String;
  }
  if (name == "FileError" || name == "ImageError" || name == "ContainerError" ||
      name == "GfxError") {
    return LocalInfo::ValueKind::Int32;
  }

  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg) && base == "File") {
    return LocalInfo::ValueKind::Int64;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Unknown || right == LocalInfo::ValueKind::Unknown) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::String || right == LocalInfo::ValueKind::String) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Bool || right == LocalInfo::ValueKind::Bool) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Float32 || right == LocalInfo::ValueKind::Float32 ||
      left == LocalInfo::ValueKind::Float64 || right == LocalInfo::ValueKind::Float64) {
    if (left == LocalInfo::ValueKind::Float32 && right == LocalInfo::ValueKind::Float32) {
      return LocalInfo::ValueKind::Float32;
    }
    if (left == LocalInfo::ValueKind::Float64 && right == LocalInfo::ValueKind::Float64) {
      return LocalInfo::ValueKind::Float64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::UInt64 || right == LocalInfo::ValueKind::UInt64) {
    return (left == LocalInfo::ValueKind::UInt64 && right == LocalInfo::ValueKind::UInt64)
               ? LocalInfo::ValueKind::UInt64
               : LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int64) {
    if ((left == LocalInfo::ValueKind::Int64 || left == LocalInfo::ValueKind::Int32) &&
        (right == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int32)) {
      return LocalInfo::ValueKind::Int64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int32 && right == LocalInfo::ValueKind::Int32) {
    return LocalInfo::ValueKind::Int32;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Bool) {
    left = LocalInfo::ValueKind::Int32;
  }
  if (right == LocalInfo::ValueKind::Bool) {
    right = LocalInfo::ValueKind::Int32;
  }
  return combineNumericKinds(left, right);
}

std::string typeNameForValueKind(LocalInfo::ValueKind kind) {
  switch (kind) {
    case LocalInfo::ValueKind::Int32:
      return "i32";
    case LocalInfo::ValueKind::Int64:
      return "i64";
    case LocalInfo::ValueKind::UInt64:
      return "u64";
    case LocalInfo::ValueKind::Float32:
      return "f32";
    case LocalInfo::ValueKind::Float64:
      return "f64";
    case LocalInfo::ValueKind::Bool:
      return "bool";
    case LocalInfo::ValueKind::String:
      return "string";
    default:
      return "";
  }
}

std::string normalizeDeclaredCollectionTypeBase(const std::string &base) {
  if (base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
      base == "/std/collections/experimental_vector/Vector" ||
      base.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
      base.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
    return "vector";
  }
  if (base == "/map" || base == "std/collections/map" || base == "/std/collections/map") {
    return "map";
  }
  if (base == "Buffer" || base == "std/gfx/Buffer" || base == "/std/gfx/Buffer" ||
      base == "std/gfx/experimental/Buffer" || base == "/std/gfx/experimental/Buffer") {
    return "Buffer";
  }
  return base;
}

bool inferDeclaredReturnCollection(const Definition &definition,
                                   std::string &collectionNameOut,
                                   std::vector<std::string> &collectionArgsOut) {
  collectionNameOut.clear();
  collectionArgsOut.clear();
  auto inferCollectionFromType = [&](const std::string &typeName,
                                     auto &&inferCollectionFromTypeRef) -> bool {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeName, base, argText)) {
      return false;
    }
    base = normalizeDeclaredCollectionTypeBase(base);
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args)) {
      return false;
    }
    if ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer") && args.size() == 1) {
      collectionNameOut = base;
      collectionArgsOut = std::move(args);
      return true;
    }
    if (base == "map" && args.size() == 2) {
      collectionNameOut = base;
      collectionArgsOut = std::move(args);
      return true;
    }
    if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
      return inferCollectionFromTypeRef(trimTemplateTypeText(args.front()), inferCollectionFromTypeRef);
    }
    return false;
  };
  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string declaredType = trimTemplateTypeText(transform.templateArgs.front());
    if (declaredType == "auto") {
      break;
    }
    return inferCollectionFromType(declaredType, inferCollectionFromType);
  }
  auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return allowAnyName || isBlockCall(candidate);
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };
  auto inferDirectCollectionCall = [&](const Expr &candidate,
                                       std::string &nameOut,
                                       std::vector<std::string> &argsOut) -> bool {
    nameOut.clear();
    argsOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    std::string collection;
    if (getBuiltinCollectionName(candidate, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          candidate.templateArgs.size() == 1) {
        nameOut = collection;
        argsOut = candidate.templateArgs;
        return true;
      }
      if (collection == "map" && candidate.templateArgs.size() == 2) {
        nameOut = "map";
        argsOut = candidate.templateArgs;
        return true;
      }
    }
    auto normalizedName = candidate.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    auto matchesPath = [&](std::string_view basePath) {
      return normalizedName == basePath || normalizedName.rfind(std::string(basePath) + "__", 0) == 0;
    };
    auto isDirectMapConstructor = [&]() {
      return matchesPath("std/collections/map/map") ||
             matchesPath("std/collections/mapNew") ||
             matchesPath("std/collections/mapSingle") ||
             matchesPath("std/collections/mapDouble") ||
             matchesPath("std/collections/mapPair") ||
             matchesPath("std/collections/mapTriple") ||
             matchesPath("std/collections/mapQuad") ||
             matchesPath("std/collections/mapQuint") ||
             matchesPath("std/collections/mapSext") ||
             matchesPath("std/collections/mapSept") ||
             matchesPath("std/collections/mapOct") ||
             matchesPath("std/collections/experimental_map/mapNew") ||
             matchesPath("std/collections/experimental_map/mapSingle") ||
             matchesPath("std/collections/experimental_map/mapDouble") ||
             matchesPath("std/collections/experimental_map/mapPair") ||
             matchesPath("std/collections/experimental_map/mapTriple") ||
             matchesPath("std/collections/experimental_map/mapQuad") ||
             matchesPath("std/collections/experimental_map/mapQuint") ||
             matchesPath("std/collections/experimental_map/mapSext") ||
             matchesPath("std/collections/experimental_map/mapSept") ||
             matchesPath("std/collections/experimental_map/mapOct") ||
             isSimpleCallName(candidate, "mapNew") ||
             isSimpleCallName(candidate, "mapSingle") ||
             isSimpleCallName(candidate, "mapDouble") ||
             isSimpleCallName(candidate, "mapPair") ||
             isSimpleCallName(candidate, "mapTriple") ||
             isSimpleCallName(candidate, "mapQuad") ||
             isSimpleCallName(candidate, "mapQuint") ||
             isSimpleCallName(candidate, "mapSext") ||
             isSimpleCallName(candidate, "mapSept") ||
             isSimpleCallName(candidate, "mapOct");
    };
    auto isDirectVectorConstructor = [&]() {
      return matchesPath("std/collections/vector/vector") ||
             matchesPath("std/collections/vectorNew") ||
             matchesPath("std/collections/vectorSingle") ||
             isSimpleCallName(candidate, "vectorNew") ||
             isSimpleCallName(candidate, "vectorSingle");
    };
    if (isDirectMapConstructor() && candidate.templateArgs.size() == 2) {
      nameOut = "map";
      argsOut = candidate.templateArgs;
      return true;
    }
    if (isDirectMapConstructor() && candidate.args.size() % 2 == 0 && !candidate.args.empty()) {
      auto inferLiteralType = [&](const Expr &value, std::string &typeOut) -> bool {
        typeOut.clear();
        if (value.kind == Expr::Kind::Literal) {
          typeOut = value.isUnsigned ? "u64" : (value.intWidth == 64 ? "i64" : "i32");
          return true;
        }
        if (value.kind == Expr::Kind::BoolLiteral) {
          typeOut = "bool";
          return true;
        }
        if (value.kind == Expr::Kind::FloatLiteral) {
          typeOut = value.floatWidth == 64 ? "f64" : "f32";
          return true;
        }
        if (value.kind == Expr::Kind::StringLiteral) {
          typeOut = "string";
          return true;
        }
        return false;
      };
      std::string keyType;
      std::string valueType;
      for (size_t i = 0; i < candidate.args.size(); i += 2) {
        std::string currentKeyType;
        std::string currentValueType;
        if (!inferLiteralType(candidate.args[i], currentKeyType) ||
            !inferLiteralType(candidate.args[i + 1], currentValueType)) {
          return false;
        }
        if (keyType.empty()) {
          keyType = currentKeyType;
        } else if (keyType != currentKeyType) {
          return false;
        }
        if (valueType.empty()) {
          valueType = currentValueType;
        } else if (valueType != currentValueType) {
          return false;
        }
      }
      nameOut = "map";
      argsOut = {keyType, valueType};
      return true;
    }
    if (isDirectVectorConstructor() && candidate.templateArgs.size() == 1) {
      nameOut = "vector";
      argsOut = candidate.templateArgs;
      return true;
    }
    return false;
  };
  std::function<bool(const Expr &)> inferCollectionFromExpr;
  inferCollectionFromExpr = [&](const Expr &candidate) -> bool {
    if (isIfCall(candidate) && candidate.args.size() == 3) {
      const Expr &thenArg = candidate.args[1];
      const Expr &elseArg = candidate.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      std::string thenName;
      std::vector<std::string> thenArgs;
      std::string elseName;
      std::vector<std::string> elseArgs;
      if (!inferCollectionFromExpr(thenValue ? *thenValue : thenArg)) {
        return false;
      }
      thenName = collectionNameOut;
      thenArgs = collectionArgsOut;
      if (!inferCollectionFromExpr(elseValue ? *elseValue : elseArg)) {
        return false;
      }
      elseName = collectionNameOut;
      elseArgs = collectionArgsOut;
      if (thenName != elseName || thenArgs != elseArgs) {
        return false;
      }
      collectionNameOut = std::move(thenName);
      collectionArgsOut = std::move(thenArgs);
      return true;
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferCollectionFromExpr(valueExpr->args.front());
      }
      return inferCollectionFromExpr(*valueExpr);
    }
    return inferDirectCollectionCall(candidate, collectionNameOut, collectionArgsOut);
  };
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : definition.statements) {
    if (stmt.isBinding) {
      continue;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (definition.returnExpr.has_value()) {
    valueExpr = &*definition.returnExpr;
  }
  if (valueExpr != nullptr && inferCollectionFromExpr(*valueExpr)) {
    return true;
  }
  return false;
}
bool inferReceiverTypeFromDeclaredReturn(const Definition &definition, std::string &typeNameOut) {
  std::vector<std::string> collectionArgs;
  if (inferDeclaredReturnCollection(definition, typeNameOut, collectionArgs)) {
    return true;
  }

  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &declaredType = transform.templateArgs.front();
    if (declaredType.empty() || declaredType == "void" || declaredType == "auto") {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(declaredType, base, argText)) {
      // Non-collection templated returns are not method receiver targets.
      return false;
    }
    typeNameOut = declaredType;
    if (!typeNameOut.empty() && typeNameOut.front() == '/') {
      typeNameOut.erase(0, 1);
    }
    return !typeNameOut.empty();
  }
  return false;
}

} // namespace primec::ir_lowerer
