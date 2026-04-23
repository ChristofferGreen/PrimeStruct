#include "IrLowererSetupTypeHelpers.h"

#include <functional>

#include "IrLowererHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

std::string normalizeDeclaredCollectionTypeBase(const std::string &base) {
  if (base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
      base == "/std/collections/experimental_vector/Vector" ||
      base.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
      base.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
    return "vector";
  }
  if (base == "SoaVector" ||
      base == "std/collections/experimental_soa_vector/SoaVector" ||
      base == "/std/collections/experimental_soa_vector/SoaVector" ||
      base.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) == 0 ||
      base.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0) {
    return "soa_vector";
  }
  if (base == "/map" || base == "std/collections/map" || base == "/std/collections/map" ||
      base == "Map" || base == "std/collections/experimental_map/Map" ||
      base == "/std/collections/experimental_map/Map" ||
      base.rfind("std/collections/experimental_map/Map__", 0) == 0 ||
      base.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    return "map";
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
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
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
  auto extractDeclaredExprTypeName = [&](const Expr &expr) {
    std::string typeName;
    std::vector<std::string> templateArgs;
    if (extractFirstBindingTypeTransform(expr, typeName, templateArgs)) {
      if (!templateArgs.empty()) {
        typeName += "<";
        for (size_t index = 0; index < templateArgs.size(); ++index) {
          if (index != 0) {
            typeName += ", ";
          }
          typeName += trimTemplateTypeText(templateArgs[index]);
        }
        typeName += ">";
      }
      return typeName;
    }
    return std::string{};
  };
  auto inferLiteralTypeName = [&](const Expr &value, std::string &typeOut) {
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
    auto isDirectMapConstructor = [&]() {
      return normalizedName == "std/collections/map/map" ||
             normalizedName.rfind("std/collections/map/map__", 0) == 0 ||
             isPublishedStdlibSurfaceConstructorExpr(
                 candidate,
                 primec::StdlibSurfaceId::CollectionsMapConstructors);
    };
    auto isDirectVectorConstructor = [&]() {
      return normalizedName == "std/collections/vector/vector" ||
             normalizedName.rfind("std/collections/vector/vector__", 0) == 0 ||
             isPublishedStdlibSurfaceConstructorExpr(
                 candidate,
                 primec::StdlibSurfaceId::CollectionsVectorConstructors);
    };
    if (isDirectMapConstructor() && candidate.templateArgs.size() == 2) {
      nameOut = "map";
      argsOut = candidate.templateArgs;
      return true;
    }
    if (isDirectMapConstructor() && candidate.args.size() % 2 == 0 && !candidate.args.empty()) {
      std::string keyType;
      std::string valueType;
      for (size_t i = 0; i < candidate.args.size(); i += 2) {
        std::string currentKeyType;
        std::string currentValueType;
        if (!inferLiteralTypeName(candidate.args[i], currentKeyType) ||
            !inferLiteralTypeName(candidate.args[i + 1], currentValueType)) {
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
    if (isDirectVectorConstructor() && !candidate.args.empty()) {
      std::string elementType;
      for (const auto &arg : candidate.args) {
        std::string currentElementType;
        if (!inferLiteralTypeName(arg, currentElementType)) {
          return false;
        }
        if (elementType.empty()) {
          elementType = currentElementType;
          continue;
        }
        if (elementType != currentElementType) {
          return false;
        }
      }
      nameOut = "vector";
      argsOut = {elementType};
      return true;
    }
    return false;
  };
  std::function<bool(const Expr &)> inferCollectionFromExpr;
  std::function<bool(const std::string &)> inferCollectionFromNamedValue;
  inferCollectionFromNamedValue = [&](const std::string &name) {
    for (const auto &parameter : definition.parameters) {
      if (parameter.name != name) {
        continue;
      }
      const std::string declaredType = extractDeclaredExprTypeName(parameter);
      if (declaredType.empty()) {
        return false;
      }
      return inferCollectionFromType(trimTemplateTypeText(declaredType),
                                     inferCollectionFromType);
    }
    for (const auto &stmt : definition.statements) {
      if (!stmt.isBinding || stmt.name != name) {
        continue;
      }
      const std::string declaredType = extractDeclaredExprTypeName(stmt);
      if (!declaredType.empty()) {
        return inferCollectionFromType(trimTemplateTypeText(declaredType),
                                       inferCollectionFromType);
      }
      if (stmt.args.size() == 1) {
        return inferCollectionFromExpr(stmt.args.front());
      }
      return false;
    }
    return false;
  };
  inferCollectionFromExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::Name &&
        inferCollectionFromNamedValue(candidate.name)) {
      return true;
    }
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

} // namespace primec::ir_lowerer
