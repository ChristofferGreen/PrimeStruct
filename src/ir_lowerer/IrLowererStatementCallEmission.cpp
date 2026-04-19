#include "IrLowererStatementCallHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

#include <optional>
#include <utility>

namespace primec::ir_lowerer {

namespace {

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

bool matchesRegistrySpellingSet(std::span<const std::string_view> spellings,
                                std::string_view spelling) {
  for (const std::string_view candidate : spellings) {
    if (candidate == spelling) {
      return true;
    }
  }
  return false;
}

std::string canonicalStatementVectorHelperName(std::string helperName) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "vectorCount") {
    return "count";
  }
  if (helperName == "vectorCapacity") {
    return "capacity";
  }
  if (helperName == "vectorAt") {
    return "at";
  }
  if (helperName == "vectorAtUnsafe") {
    return "at_unsafe";
  }
  if (helperName == "vectorPush") {
    return "push";
  }
  if (helperName == "vectorPop") {
    return "pop";
  }
  if (helperName == "vectorReserve") {
    return "reserve";
  }
  if (helperName == "vectorClear") {
    return "clear";
  }
  if (helperName == "vectorRemoveAt") {
    return "remove_at";
  }
  if (helperName == "vectorRemoveSwap") {
    return "remove_swap";
  }
  return helperName;
}

bool resolvePublishedStatementVectorHelperName(std::string_view resolvedPath,
                                               std::string &helperNameOut) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  if (metadata == nullptr || metadata->id != StdlibSurfaceId::CollectionsVectorHelpers) {
    return false;
  }
  const size_t slash = resolvedPath.find_last_of('/');
  if (slash == std::string_view::npos || slash + 1 >= resolvedPath.size()) {
    return false;
  }
  helperNameOut =
      canonicalStatementVectorHelperName(std::string(resolvedPath.substr(slash + 1)));
  return !helperNameOut.empty();
}

bool isPublishedCanonicalStatementVectorHelperPath(std::string_view resolvedPath) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  return metadata != nullptr &&
         metadata->id == StdlibSurfaceId::CollectionsVectorHelpers &&
         matchesRegistrySpellingSet(metadata->loweringSpellings, resolvedPath);
}

std::string canonicalStatementMapHelperName(std::string helperName) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "mapCount") {
    return "count";
  }
  if (helperName == "mapCountRef") {
    return "count_ref";
  }
  if (helperName == "mapContains") {
    return "contains";
  }
  if (helperName == "mapContainsRef") {
    return "contains_ref";
  }
  if (helperName == "mapTryAt") {
    return "tryAt";
  }
  if (helperName == "mapTryAtRef") {
    return "tryAt_ref";
  }
  if (helperName == "mapAt") {
    return "at";
  }
  if (helperName == "mapAtRef") {
    return "at_ref";
  }
  if (helperName == "mapAtUnsafe") {
    return "at_unsafe";
  }
  if (helperName == "mapAtUnsafeRef") {
    return "at_unsafe_ref";
  }
  if (helperName == "mapInsert") {
    return "insert";
  }
  if (helperName == "mapInsertRef") {
    return "insert_ref";
  }
  return helperName;
}

bool resolvePublishedStatementMapHelperName(std::string_view resolvedPath,
                                            std::string &helperNameOut) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  if (metadata == nullptr || metadata->id != StdlibSurfaceId::CollectionsMapHelpers) {
    return false;
  }
  const size_t slash = resolvedPath.find_last_of('/');
  if (slash == std::string_view::npos || slash + 1 >= resolvedPath.size()) {
    return false;
  }
  helperNameOut =
      canonicalStatementMapHelperName(std::string(resolvedPath.substr(slash + 1)));
  return !helperNameOut.empty();
}

bool isFreeMemoryIntrinsicCall(const Expr &expr) {
  std::string builtinName;
  return expr.kind == Expr::Kind::Call && getBuiltinMemoryName(expr, builtinName) && builtinName == "free";
}

static bool resolveStatementVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  return resolvePublishedStatementVectorHelperName(expr.name, helperNameOut);
}

static bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveStatementVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

static bool isExplicitVectorMutatorHelperCall(const Expr &expr) {
  std::string aliasName;
  if (!resolveStatementVectorHelperAliasName(expr, aliasName)) {
    return false;
  }
  return aliasName == "push" || aliasName == "pop" || aliasName == "reserve" || aliasName == "clear" ||
         aliasName == "remove_at" || aliasName == "remove_swap";
}

static size_t explicitVectorHelperReceiverIndex(const Expr &expr) {
  for (size_t i = 0; i < expr.argNames.size() && i < expr.args.size(); ++i) {
    if (expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
      return i;
    }
  }
  return 0;
}

static bool isSoaVectorTargetExpr(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(expr, collection) && collection == "soa_vector";
  }
  return false;
}

static std::optional<LocalInfo::ValueKind> resolveBufferTargetElementKind(
    const Expr &bufferExpr,
    const LocalMap &localsIn) {
  if (bufferExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(bufferExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Buffer) {
      return it->second.valueKind;
    }
  }

  if (bufferExpr.kind == Expr::Kind::Call && isSimpleCallName(bufferExpr, "buffer") &&
      bufferExpr.templateArgs.size() == 1) {
    return valueKindFromTypeName(bufferExpr.templateArgs.front());
  }

  if (bufferExpr.kind == Expr::Kind::Call && isSimpleCallName(bufferExpr, "dereference") &&
      bufferExpr.args.size() == 1) {
    const Expr &targetExpr = bufferExpr.args.front();
    if (targetExpr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(targetExpr.name);
      if (it != localsIn.end() &&
          ((it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
           (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
        return it->second.valueKind;
      }
    }

    std::string derefAccessName;
    if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, derefAccessName) &&
        targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(targetExpr.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack &&
          ((it->second.argsPackElementKind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
           (it->second.argsPackElementKind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
        return it->second.valueKind;
      }
    }
  }

  std::string accessName;
  if (bufferExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(bufferExpr, accessName) &&
      bufferExpr.args.size() == 2 && bufferExpr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(bufferExpr.args.front().name);
    if (it != localsIn.end() && it->second.isArgsPack && it->second.argsPackElementKind == LocalInfo::Kind::Buffer) {
      return it->second.valueKind;
    }
  }

  return std::nullopt;
}

static bool rewriteMapInsertHelperStatementToBuiltin(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    Expr &rewrittenStmt) {
  if (stmt.kind != Expr::Kind::Call || stmt.args.size() != 3) {
    return false;
  }

  auto isDirectBareMapInsertHelperStem = [&](const Expr &callExpr) {
    if (callExpr.isMethodCall || callExpr.name.empty()) {
      return false;
    }
    std::string helperName = callExpr.name;
    if (!helperName.empty() && helperName.front() == '/') {
      helperName.erase(helperName.begin());
    }
    if (helperName.find('/') != std::string::npos) {
      return false;
    }
    helperName = stripGeneratedHelperSuffix(std::move(helperName));
    return helperName == "insert" || helperName == "insert_ref" ||
           helperName == "Insert" || helperName == "InsertRef" ||
           helperName == "mapInsert" || helperName == "mapInsertRef" ||
           helperName == "MapInsert" || helperName == "MapInsertRef";
  };

  size_t receiverIndex = 0;
  if (stmt.isMethodCall) {
    std::string normalizedName = stmt.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    const size_t lastSlash = normalizedName.find_last_of('/');
    if (lastSlash != std::string::npos) {
      normalizedName = normalizedName.substr(lastSlash + 1);
    }
    normalizedName = stripGeneratedHelperSuffix(std::move(normalizedName));
    const bool isMethodInsertStem =
        normalizedName == "insert" || normalizedName == "insert_ref" ||
        normalizedName == "Insert" || normalizedName == "InsertRef" ||
        normalizedName == "mapInsert" || normalizedName == "mapInsertRef" ||
        normalizedName == "MapInsert" || normalizedName == "MapInsertRef";
    if (!isMethodInsertStem) {
      return false;
    }
  } else {
    std::string helperName;
    if ((!resolveMapHelperAliasName(stmt, helperName) || helperName != "insert") &&
        !isDirectBareMapInsertHelperStem(stmt)) {
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() &&
            *stmt.argNames[i] == "values") {
          receiverIndex = i;
          break;
        }
      }
    }
  }

  if (receiverIndex >= stmt.args.size()) {
    return false;
  }
  const auto inferCallMapTargetInfo = [&](const Expr &targetExpr, MapAccessTargetInfo &targetInfoOut) {
    auto normalizeInsertHelperStem = [&](const std::string &path) {
      std::string helperName = path;
      if (!helperName.empty() && helperName.front() == '/') {
        helperName.erase(helperName.begin());
      }
      const size_t lastSlash = helperName.find_last_of('/');
      if (lastSlash != std::string::npos) {
        helperName = helperName.substr(lastSlash + 1);
      }
      return stripGeneratedHelperSuffix(std::move(helperName));
    };
    auto isMapInsertLikeCallee = [&](const Definition &callee) {
      std::string publishedHelperName;
      if (resolvePublishedStatementMapHelperName(callee.fullPath, publishedHelperName) &&
          (publishedHelperName == "insert" || publishedHelperName == "insert_ref")) {
        return true;
      }
      const std::string helperStem = normalizeInsertHelperStem(callee.fullPath);
      if (callee.fullPath.rfind("/std/collections/map/", 0) == 0 &&
          helperStem == "insert_builtin") {
        return true;
      }
      if (helperStem == "insert" || helperStem == "insert_ref" ||
          helperStem == "Insert" || helperStem == "InsertRef" ||
          helperStem == "mapInsert" || helperStem == "mapInsertRef" ||
          helperStem == "MapInsert" || helperStem == "MapInsertRef") {
        return true;
      }
      Expr calleeExpr;
      calleeExpr.kind = Expr::Kind::Call;
      calleeExpr.name = callee.fullPath;
      std::string helperName;
      return resolveMapHelperAliasName(calleeExpr, helperName) && helperName == "insert";
    };
    std::function<bool(const std::string &, LocalInfo::ValueKind &, LocalInfo::ValueKind &)>
        inferMapKindsFromTypeText;
    inferMapKindsFromTypeText = [&](const std::string &typeText,
                                    LocalInfo::ValueKind &keyKindOut,
                                    LocalInfo::ValueKind &valueKindOut) {
      keyKindOut = LocalInfo::ValueKind::Unknown;
      valueKindOut = LocalInfo::ValueKind::Unknown;

      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
        return false;
      }

      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        return inferMapKindsFromTypeText(argText, keyKindOut, valueKindOut);
      }

      const bool isMapBase =
          normalizedBase == "map" || normalizedBase == "/map" ||
          normalizedBase == "std/collections/map" || normalizedBase == "/std/collections/map" ||
          normalizedBase == "Map" || normalizedBase == "/Map" ||
          normalizedBase == "std/collections/experimental_map/Map" ||
          normalizedBase == "/std/collections/experimental_map/Map";
      if (!isMapBase) {
        return false;
      }

      std::vector<std::string> mapArgs;
      if (!splitTemplateArgs(argText, mapArgs) || mapArgs.size() != 2) {
        return false;
      }
      keyKindOut = valueKindFromTypeName(trimTemplateTypeText(mapArgs.front()));
      valueKindOut = valueKindFromTypeName(trimTemplateTypeText(mapArgs.back()));
      return keyKindOut != LocalInfo::ValueKind::Unknown &&
             valueKindOut != LocalInfo::ValueKind::Unknown;
    };
    auto inferMapKindsFromArgsPackTypeText = [&](const std::string &typeText,
                                                 LocalInfo::ValueKind &keyKindOut,
                                                 LocalInfo::ValueKind &valueKindOut) {
      keyKindOut = LocalInfo::ValueKind::Unknown;
      valueKindOut = LocalInfo::ValueKind::Unknown;

      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
        return false;
      }

      const std::string normalizedBase = trimTemplateTypeText(base);
      const bool isArgsPackBase =
          normalizedBase == "args" || normalizedBase == "/args" ||
          normalizedBase == "Args" || normalizedBase == "/Args";
      if (!isArgsPackBase) {
        return false;
      }
      return inferMapKindsFromTypeText(argText, keyKindOut, valueKindOut);
    };
    auto extractParameterTypeName = [&](const Expr &paramExpr) {
      for (const auto &transform : paramExpr.transforms) {
        if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
            transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
            transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
            transform.name == "effects" || transform.name == "capabilities") {
          continue;
        }
        if (!transform.arguments.empty()) {
          continue;
        }
        std::string typeName = transform.name;
        if (!transform.templateArgs.empty()) {
          typeName += "<";
          for (size_t index = 0; index < transform.templateArgs.size(); ++index) {
            if (index != 0) {
              typeName += ", ";
            }
            typeName += trimTemplateTypeText(transform.templateArgs[index]);
          }
          typeName += ">";
        }
        return typeName;
      }
      return std::string{};
    };

    auto peelLocationWrappers = [&](const Expr &expr) {
      const Expr *current = &expr;
      while (current->kind == Expr::Kind::Call &&
             isSimpleCallName(*current, "location") &&
             current->args.size() == 1) {
        current = &current->args.front();
      }
      return current;
    };

    auto peelReceiverWrappers = [&](const Expr &expr) {
      const Expr *current = peelLocationWrappers(expr);
      while (current->kind == Expr::Kind::Call &&
             isSimpleCallName(*current, "dereference") &&
             current->args.size() == 1) {
        current = peelLocationWrappers(current->args.front());
      }
      return current;
    };
    auto isMapArgsPackAccessCall = [&](const Expr &expr) {
      if (expr.kind != Expr::Kind::Call || expr.args.size() != 2) {
        return false;
      }
      auto isDirectMapArgsPackStem = [&](const char *stem) {
        if (stem == nullptr || expr.isMethodCall || expr.name.empty()) {
          return false;
        }
        std::string directName = expr.name;
        if (!directName.empty() && directName.front() == '/') {
          directName.erase(directName.begin());
        }
        if (directName.find('/') != std::string::npos) {
          return false;
        }
        return stripGeneratedHelperSuffix(std::move(directName)) == stem;
      };
      auto isMapArgsPackMethodStem = [&](const char *stem) {
        if (isSimpleCallName(expr, stem)) {
          return true;
        }
        if (!expr.isMethodCall || expr.name.empty()) {
          return false;
        }
        std::string methodName = expr.name;
        if (!methodName.empty() && methodName.front() == '/') {
          methodName.erase(methodName.begin());
        }
        if (methodName.find('/') != std::string::npos) {
          return false;
        }
        return stripGeneratedHelperSuffix(std::move(methodName)) == stem;
      };
      if (expr.isMethodCall) {
        if (isMapArgsPackMethodStem("at") || isMapArgsPackMethodStem("at_unsafe") ||
            isMapArgsPackMethodStem("mapAt") || isMapArgsPackMethodStem("mapAtUnsafe") ||
            isMapArgsPackMethodStem("at_ref") || isMapArgsPackMethodStem("at_unsafe_ref") ||
            isMapArgsPackMethodStem("mapAtRef") || isMapArgsPackMethodStem("mapAtUnsafeRef") ||
            isMapArgsPackMethodStem("At") || isMapArgsPackMethodStem("AtUnsafe") ||
            isMapArgsPackMethodStem("AtRef") || isMapArgsPackMethodStem("AtUnsafeRef")) {
          return true;
        }
      }
      if (isDirectMapArgsPackStem("at") || isDirectMapArgsPackStem("at_unsafe") ||
          isDirectMapArgsPackStem("at_ref") || isDirectMapArgsPackStem("at_unsafe_ref") ||
          isDirectMapArgsPackStem("mapAt") || isDirectMapArgsPackStem("mapAtUnsafe") ||
          isDirectMapArgsPackStem("mapAtRef") || isDirectMapArgsPackStem("mapAtUnsafeRef") ||
          isDirectMapArgsPackStem("At") || isDirectMapArgsPackStem("AtUnsafe") ||
          isDirectMapArgsPackStem("AtRef") || isDirectMapArgsPackStem("AtUnsafeRef")) {
        return true;
      }
      std::string publishedHelperName;
      if (resolvePublishedStatementMapHelperName(expr.name, publishedHelperName) &&
          (publishedHelperName == "at" || publishedHelperName == "at_ref" ||
           publishedHelperName == "at_unsafe" ||
           publishedHelperName == "at_unsafe_ref")) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(expr, accessName) && accessName == "map") {
        return true;
      }
      std::string helperName;
      return resolveMapHelperAliasName(expr, helperName) &&
             (helperName == "at" || helperName == "at_ref" ||
              helperName == "at_unsafe" || helperName == "at_unsafe_ref");
    };

    const Expr *canonicalReceiverExpr = peelReceiverWrappers(targetExpr);

    // Reuse the core access-target resolver on the peeled receiver shape so
    // wrapped args-pack map-access forms (for example stacked
    // location/dereference around /map/at(argsPack, ...)) can flow through the
    // same typed map-target inference used by direct receivers.
    const auto canonicalTargetInfo = resolveMapAccessTargetInfo(*canonicalReceiverExpr, localsIn);
    if (canonicalTargetInfo.isMapTarget &&
        canonicalTargetInfo.mapKeyKind != LocalInfo::ValueKind::Unknown &&
        canonicalTargetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
      targetInfoOut = canonicalTargetInfo;
      return true;
    }

    const Expr *nonLocalFieldReceiver = nullptr;
    if (canonicalReceiverExpr->kind == Expr::Kind::Call &&
        canonicalReceiverExpr->isFieldAccess &&
        canonicalReceiverExpr->args.size() == 1) {
      nonLocalFieldReceiver = canonicalReceiverExpr;
    }

    targetInfoOut = {};
    if (canonicalReceiverExpr->kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(canonicalReceiverExpr->name);
      if (localIt != localsIn.end()) {
        const LocalInfo &localInfo = localIt->second;
        const bool directMap = localInfo.kind == LocalInfo::Kind::Map;
        const bool wrappedMap =
            (localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToMap) ||
            (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToMap);
        const bool argsPackMap =
            localInfo.isArgsPack &&
            (localInfo.argsPackElementKind == LocalInfo::Kind::Map ||
             (localInfo.argsPackElementKind == LocalInfo::Kind::Reference && localInfo.referenceToMap) ||
             (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer && localInfo.pointerToMap));
        if (directMap || wrappedMap || argsPackMap) {
          targetInfoOut.isMapTarget = true;
          targetInfoOut.mapKeyKind = localInfo.mapKeyKind;
          targetInfoOut.mapValueKind = localInfo.mapValueKind;
          if (targetInfoOut.mapKeyKind != LocalInfo::ValueKind::Unknown &&
              targetInfoOut.mapValueKind != LocalInfo::ValueKind::Unknown) {
            return true;
          }
        }
      }
    }

    auto tryPopulateFromResolvedCallee = [&](const Definition *resolvedCallee) {
      if (resolvedCallee == nullptr) {
        return false;
      }
      std::string collectionName;
      std::vector<std::string> collectionArgs;
      if (!inferDeclaredReturnCollection(*resolvedCallee, collectionName, collectionArgs) ||
          collectionName != "map" ||
          collectionArgs.size() != 2) {
        return false;
      }
      targetInfoOut.isMapTarget = true;
      targetInfoOut.mapKeyKind = valueKindFromTypeName(collectionArgs.front());
      targetInfoOut.mapValueKind = valueKindFromTypeName(collectionArgs.back());
      return targetInfoOut.mapKeyKind != LocalInfo::ValueKind::Unknown &&
             targetInfoOut.mapValueKind != LocalInfo::ValueKind::Unknown;
    };

    if (tryPopulateFromResolvedCallee(resolveDefinitionCall(*canonicalReceiverExpr))) {
      return true;
    }

    if (isMapArgsPackAccessCall(*canonicalReceiverExpr) &&
        canonicalReceiverExpr->args.front().kind != Expr::Kind::Name) {
      const Definition *mapAccessCallee = resolveDefinitionCall(*canonicalReceiverExpr);
      if (mapAccessCallee != nullptr &&
          !mapAccessCallee->parameters.empty()) {
        const std::string receiverTypeText =
            extractParameterTypeName(mapAccessCallee->parameters.front());
        if (inferMapKindsFromArgsPackTypeText(receiverTypeText,
                                              targetInfoOut.mapKeyKind,
                                              targetInfoOut.mapValueKind)) {
          targetInfoOut.isMapTarget = true;
          return true;
        }
      }
    }

    // Direct canonical insert calls on non-local field receivers already carry
    // explicit template arguments; use those to route the receiver through the
    // builtin rewrite path even when the receiver does not resolve as a
    // standalone call target (for example `holder.values`).
    if (nonLocalFieldReceiver != nullptr &&
        stmt.templateArgs.size() == 2) {
      targetInfoOut.isMapTarget = true;
      targetInfoOut.mapKeyKind = valueKindFromTypeName(stmt.templateArgs.front());
      targetInfoOut.mapValueKind = valueKindFromTypeName(stmt.templateArgs.back());
      return targetInfoOut.mapKeyKind != LocalInfo::ValueKind::Unknown &&
             targetInfoOut.mapValueKind != LocalInfo::ValueKind::Unknown;
    }

    if (!stmt.isMethodCall &&
        nonLocalFieldReceiver != nullptr) {
      const Definition *directCallee = resolveDefinitionCall(stmt);
      if (directCallee != nullptr &&
          isMapInsertLikeCallee(*directCallee) &&
          !directCallee->parameters.empty()) {
        const std::string receiverTypeText = extractParameterTypeName(directCallee->parameters.front());
        if (inferMapKindsFromTypeText(receiverTypeText,
                                      targetInfoOut.mapKeyKind,
                                      targetInfoOut.mapValueKind)) {
          targetInfoOut.isMapTarget = true;
          return true;
        }
        if (directCallee->parameters.size() >= 3) {
          const std::string keyTypeText = extractParameterTypeName(directCallee->parameters[1]);
          const std::string valueTypeText = extractParameterTypeName(directCallee->parameters[2]);
          const LocalInfo::ValueKind keyKind = valueKindFromTypeName(trimTemplateTypeText(keyTypeText));
          const LocalInfo::ValueKind valueKind = valueKindFromTypeName(trimTemplateTypeText(valueTypeText));
          if (keyKind != LocalInfo::ValueKind::Unknown &&
              valueKind != LocalInfo::ValueKind::Unknown) {
            targetInfoOut.isMapTarget = true;
            targetInfoOut.mapKeyKind = keyKind;
            targetInfoOut.mapValueKind = valueKind;
            return true;
          }
        }
      }
    }

    if (stmt.isMethodCall &&
        nonLocalFieldReceiver != nullptr) {
      const Definition *methodCallee = resolveMethodCallDefinition(stmt, localsIn);
      if (methodCallee != nullptr &&
          isMapInsertLikeCallee(*methodCallee) &&
          !methodCallee->parameters.empty()) {
        const std::string receiverTypeText = extractParameterTypeName(methodCallee->parameters.front());
        if (inferMapKindsFromTypeText(receiverTypeText,
                                     targetInfoOut.mapKeyKind,
                                     targetInfoOut.mapValueKind)) {
          targetInfoOut.isMapTarget = true;
          return true;
        }
        if (methodCallee->parameters.size() >= 3) {
          const std::string keyTypeText = extractParameterTypeName(methodCallee->parameters[1]);
          const std::string valueTypeText = extractParameterTypeName(methodCallee->parameters[2]);
          const LocalInfo::ValueKind keyKind = valueKindFromTypeName(trimTemplateTypeText(keyTypeText));
          const LocalInfo::ValueKind valueKind = valueKindFromTypeName(trimTemplateTypeText(valueTypeText));
          if (keyKind != LocalInfo::ValueKind::Unknown &&
              valueKind != LocalInfo::ValueKind::Unknown) {
            targetInfoOut.isMapTarget = true;
            targetInfoOut.mapKeyKind = keyKind;
            targetInfoOut.mapValueKind = valueKind;
            return true;
          }
        }
      }
    }
    return false;
  };

  const auto targetInfo = resolveMapAccessTargetInfo(stmt.args[receiverIndex], localsIn, inferCallMapTargetInfo);
  if (!targetInfo.isMapTarget) {
    return false;
  }

  rewrittenStmt = stmt;
  rewrittenStmt.name = "/std/collections/map/insert_builtin";
  rewrittenStmt.namespacePrefix.clear();
  rewrittenStmt.isMethodCall = false;
  rewrittenStmt.isFieldAccess = false;
  rewrittenStmt.semanticNodeId = 0;
  if (rewrittenStmt.templateArgs.empty() &&
      targetInfo.mapKeyKind != LocalInfo::ValueKind::Unknown &&
      targetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
    rewrittenStmt.templateArgs = {
        typeNameForValueKind(targetInfo.mapKeyKind),
        typeNameForValueKind(targetInfo.mapValueKind),
    };
  }
  return true;
}

static DirectCallStatementEmitResult tryEmitVectorHelperCallFormStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.isMethodCall) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  const bool isVectorMutatorCall =
      isVectorBuiltinName(stmt, "push") || isVectorBuiltinName(stmt, "pop") || isVectorBuiltinName(stmt, "reserve") ||
      isVectorBuiltinName(stmt, "clear") || isVectorBuiltinName(stmt, "remove_at") ||
      isVectorBuiltinName(stmt, "remove_swap");
  if (!isVectorMutatorCall || stmt.args.empty()) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  std::string explicitStdlibHelperName;
  const bool isExplicitStdlibVectorHelper =
      resolveStatementVectorHelperAliasName(stmt, explicitStdlibHelperName) &&
      isPublishedCanonicalStatementVectorHelperPath(stmt.name);

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= stmt.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };

  const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
  if (hasNamedArgs) {
    bool hasValuesNamedReceiver = false;
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
        appendReceiverIndex(i);
        hasValuesNamedReceiver = true;
      }
    }
    if (!hasValuesNamedReceiver) {
      appendReceiverIndex(0);
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    appendReceiverIndex(0);
  }

  const bool probePositionalReorderedReceiver =
      !hasNamedArgs && stmt.args.size() > 1 &&
      (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
       stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
       stmt.args.front().kind == Expr::Kind::Name);
  if (probePositionalReorderedReceiver) {
    for (size_t i = 1; i < stmt.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }

  for (size_t receiverIndex : receiverIndices) {
    Expr methodStmt = stmt;
    methodStmt.isMethodCall = true;
    std::string normalizedHelperName;
    if (resolveStatementVectorHelperAliasName(methodStmt, normalizedHelperName)) {
      methodStmt.name = normalizedHelperName;
    }
    if (receiverIndex != 0) {
      std::swap(methodStmt.args[0], methodStmt.args[receiverIndex]);
      if (methodStmt.argNames.size() < methodStmt.args.size()) {
        methodStmt.argNames.resize(methodStmt.args.size());
      }
      std::swap(methodStmt.argNames[0], methodStmt.argNames[receiverIndex]);
    }
    const std::string priorError = error;
    const Definition *callee = resolveMethodCallDefinition(methodStmt, localsIn);
    if (!callee) {
      error = priorError;
      continue;
    }
    if (methodStmt.hasBodyArguments || !methodStmt.bodyArguments.empty()) {
      error = "native backend does not support block arguments on calls";
      return DirectCallStatementEmitResult::Error;
    }
    if (!emitInlineDefinitionCall(methodStmt, *callee, localsIn, false)) {
      return DirectCallStatementEmitResult::Error;
    }
    error = priorError;
    return DirectCallStatementEmitResult::Emitted;
  }

  if (isExplicitStdlibVectorHelper &&
      (explicitStdlibHelperName == "clear" || explicitStdlibHelperName == "remove_at" ||
       explicitStdlibHelperName == "remove_swap")) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  return DirectCallStatementEmitResult::NotMatched;
}

} // namespace

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "buffer_store")) {
    return BufferStoreStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() != 3) {
    error = "buffer_store requires buffer, index, and value";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  if (const auto resolvedKind = resolveBufferTargetElementKind(stmt.args[0], localsIn); resolvedKind.has_value()) {
    elemKind = *resolvedKind;
  }
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer_store requires numeric/bool buffer";
    return BufferStoreStatementEmitResult::Error;
  }

  const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
  if (!isSupportedIndexKind(indexKind)) {
    error = "buffer_store requires integer index";
    return BufferStoreStatementEmitResult::Error;
  }

  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(stmt.args[0], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  const int32_t valueLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({pushOneForIndex(indexKind), 1});
  instructions.push_back({addForIndex(indexKind), 0});
  instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
  instructions.push_back({mulForIndex(indexKind), 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return BufferStoreStatementEmitResult::Emitted;
}

DispatchStatementEmitResult tryEmitDispatchStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &)> &resolveExprPath,
    const std::function<const Definition *(const std::string &)> &findDefinitionByPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "dispatch")) {
    return DispatchStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() < 4) {
    error = "dispatch requires kernel and three dimension arguments";
    return DispatchStatementEmitResult::Error;
  }
  if (stmt.args.front().kind != Expr::Kind::Name) {
    error = "dispatch requires kernel name as first argument";
    return DispatchStatementEmitResult::Error;
  }

  const Expr &kernelExpr = stmt.args.front();
  const std::string kernelPath = resolveExprPath(kernelExpr);
  const Definition *kernelDef = findDefinitionByPath(kernelPath);
  if (!kernelDef) {
    error = "dispatch requires known kernel: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  bool isCompute = false;
  for (const auto &transform : kernelDef->transforms) {
    if (transform.name == "compute") {
      isCompute = true;
      break;
    }
  }
  if (!isCompute) {
    error = "dispatch requires compute definition: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  for (size_t i = 1; i <= 3; ++i) {
    const LocalInfo::ValueKind dimKind = inferExprKind(stmt.args[i], localsIn);
    if (dimKind != LocalInfo::ValueKind::Int32) {
      error = "dispatch requires i32 dimensions";
      return DispatchStatementEmitResult::Error;
    }
  }

  const bool hasTrailingArgsPack =
      !kernelDef->parameters.empty() && isArgsPackBinding(kernelDef->parameters.back());
  const size_t fixedKernelParamCount =
      hasTrailingArgsPack ? kernelDef->parameters.size() - 1 : kernelDef->parameters.size();
  const size_t minDispatchArgs = fixedKernelParamCount + 4;
  if ((!hasTrailingArgsPack && stmt.args.size() != minDispatchArgs) ||
      (hasTrailingArgsPack && stmt.args.size() < minDispatchArgs)) {
    error = "dispatch argument count mismatch for " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  const int32_t gxLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gxLocal)});
  const int32_t gyLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gyLocal)});
  const int32_t gzLocal = allocTempLocal();
  if (!emitExpr(stmt.args[3], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gzLocal)});

  const int32_t xLocal = allocTempLocal();
  const int32_t yLocal = allocTempLocal();
  const int32_t zLocal = allocTempLocal();
  const int32_t gidXLocal = allocTempLocal();
  const int32_t gidYLocal = allocTempLocal();
  const int32_t gidZLocal = allocTempLocal();

  LocalMap dispatchLocals = localsIn;
  LocalInfo gidInfo;
  gidInfo.index = gidXLocal;
  gidInfo.kind = LocalInfo::Kind::Value;
  gidInfo.valueKind = LocalInfo::ValueKind::Int32;
  dispatchLocals.emplace(kGpuGlobalIdXName, gidInfo);
  gidInfo.index = gidYLocal;
  dispatchLocals.emplace(kGpuGlobalIdYName, gidInfo);
  gidInfo.index = gidZLocal;
  dispatchLocals.emplace(kGpuGlobalIdZName, gidInfo);

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  const size_t zCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gzLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t zJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  const size_t yCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gyLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t yJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  const size_t xCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gxLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t xJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidXLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidYLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidZLocal)});

  Expr kernelCall;
  kernelCall.kind = Expr::Kind::Call;
  kernelCall.name = kernelPath;
  kernelCall.namespacePrefix = kernelExpr.namespacePrefix;
  for (size_t i = 4; i < stmt.args.size(); ++i) {
    kernelCall.args.push_back(stmt.args[i]);
    kernelCall.argNames.push_back(std::nullopt);
  }
  if (!emitInlineDefinitionCall(kernelCall, *kernelDef, dispatchLocals, false)) {
    return DispatchStatementEmitResult::Error;
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(xCheck)});
  const size_t xEnd = instructions.size();
  instructions[xJumpEnd].imm = static_cast<int32_t>(xEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(yCheck)});
  const size_t yEnd = instructions.size();
  instructions[yJumpEnd].imm = static_cast<int32_t>(yEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(zCheck)});
  const size_t zEnd = instructions.size();
  instructions[zJumpEnd].imm = static_cast<int32_t>(zEnd);

  return DispatchStatementEmitResult::Emitted;
}

DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  bool explicitVectorMutatorHelperCall = isExplicitVectorMutatorHelperCall(stmt);
  auto explicitVectorHelperUsesBuiltinVectorReceiver = [&](const Expr &callExpr) {
    if (!explicitVectorMutatorHelperCall || callExpr.args.empty()) {
      return false;
    }
    const size_t receiverIndex = explicitVectorHelperReceiverIndex(callExpr);
    const auto targetInfo = resolveArrayVectorAccessTargetInfo(callExpr.args[receiverIndex], localsIn);
    return targetInfo.isVectorTarget;
  };
  auto rewriteBareVectorMethodMutatorToDirectCall = [&](const Expr &callExpr, Expr &rewrittenExpr) {
    if (callExpr.kind != Expr::Kind::Call || !callExpr.isMethodCall || callExpr.args.empty() ||
        !callExpr.namespacePrefix.empty() || callExpr.name.find('/') != std::string::npos) {
      return false;
    }
    const std::string helperName = callExpr.name;
    if (helperName != "push" && helperName != "pop" && helperName != "reserve" &&
        helperName != "clear" && helperName != "remove_at" && helperName != "remove_swap") {
      return false;
    }
    const size_t expectedArgCount =
        (helperName == "pop" || helperName == "clear") ? 1u : 2u;
    if (callExpr.args.size() != expectedArgCount) {
      return false;
    }
    const auto targetInfo = resolveArrayVectorAccessTargetInfo(callExpr.args.front(), localsIn);
    if (!targetInfo.isVectorTarget) {
      return false;
    }
    rewrittenExpr = callExpr;
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.name = helperName;
    return true;
  };
  Expr directStmt = stmt;
  Expr rewrittenMapInsertStmt;
  if (rewriteMapInsertHelperStatementToBuiltin(
          stmt, localsIn, resolveMethodCallDefinition, resolveDefinitionCall, rewrittenMapInsertStmt)) {
    directStmt = rewrittenMapInsertStmt;
  }
  bool rewrittenExplicitVectorMutatorToBuiltinCall = false;
  Expr rewrittenBareVectorMethodStmt;
  if (!explicitVectorMutatorHelperCall &&
      rewriteBareVectorMethodMutatorToDirectCall(stmt, rewrittenBareVectorMethodStmt)) {
    directStmt = rewrittenBareVectorMethodStmt;
  }
  if (explicitVectorMutatorHelperCall && explicitVectorHelperUsesBuiltinVectorReceiver(directStmt)) {
    std::string helperName;
    if (resolveStatementVectorHelperAliasName(directStmt, helperName)) {
      const size_t receiverIndex = explicitVectorHelperReceiverIndex(directStmt);
      directStmt.name = helperName;
      directStmt.namespacePrefix.clear();
      if (receiverIndex < directStmt.args.size() && receiverIndex != 0) {
        std::swap(directStmt.args[0], directStmt.args[receiverIndex]);
        if (directStmt.argNames.size() < directStmt.args.size()) {
          directStmt.argNames.resize(directStmt.args.size());
        }
        std::swap(directStmt.argNames[0], directStmt.argNames[receiverIndex]);
      }
      if (hasNamedArguments(directStmt.argNames)) {
        directStmt.argNames.clear();
      }
      explicitVectorMutatorHelperCall = false;
      rewrittenExplicitVectorMutatorToBuiltinCall = true;
    }
  }
  if (rewrittenExplicitVectorMutatorToBuiltinCall) {
    if (!emitExpr(directStmt, localsIn)) {
      return DirectCallStatementEmitResult::Error;
    }
    return DirectCallStatementEmitResult::Emitted;
  }

  if (!explicitVectorMutatorHelperCall &&
      !directStmt.isMethodCall &&
      directStmt.args.size() == 1 &&
      isSoaVectorTargetExpr(directStmt.args.front(), localsIn)) {
    Expr methodStmt = directStmt;
    methodStmt.isMethodCall = true;
    const std::string priorError = error;
    if (const Definition *callee = resolveMethodCallDefinition(methodStmt, localsIn)) {
      if (methodStmt.hasBodyArguments || !methodStmt.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return DirectCallStatementEmitResult::Error;
      }
      if (!emitInlineDefinitionCall(methodStmt, *callee, localsIn, false)) {
        return DirectCallStatementEmitResult::Error;
      }
      error = priorError;
      return DirectCallStatementEmitResult::Emitted;
    }
    error = priorError;
  }

  if (directStmt.isMethodCall) {
    const bool isBuiltinCountLikeMethod =
        isArrayCountCall(directStmt, localsIn) || isStringCountCall(directStmt, localsIn) ||
        isVectorCapacityCall(directStmt, localsIn);
    const std::string priorError = error;
    const Definition *callee = resolveMethodCallDefinition(directStmt, localsIn);
    if (!callee && !isBuiltinCountLikeMethod) {
      return DirectCallStatementEmitResult::Error;
    }
    if (callee) {
      if (directStmt.hasBodyArguments || !directStmt.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return DirectCallStatementEmitResult::Error;
      }
      if (!emitInlineDefinitionCall(directStmt, *callee, localsIn, false)) {
        return DirectCallStatementEmitResult::Error;
      }
      error = priorError;
      return DirectCallStatementEmitResult::Emitted;
    }
    error.clear();
  }

  if (!explicitVectorMutatorHelperCall) {
    const auto vectorHelperCallFormResult = tryEmitVectorHelperCallFormStatement(
        directStmt, localsIn, resolveMethodCallDefinition, emitInlineDefinitionCall, error);
    if (vectorHelperCallFormResult == DirectCallStatementEmitResult::Error) {
      return DirectCallStatementEmitResult::Error;
    }
    if (vectorHelperCallFormResult == DirectCallStatementEmitResult::Emitted) {
      return DirectCallStatementEmitResult::Emitted;
    }
  }

  const std::string priorError = error;
  const Definition *callee = resolveDefinitionCall(directStmt);
  if (!callee) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  if (directStmt.hasBodyArguments || !directStmt.bodyArguments.empty()) {
    error = "native backend does not support block arguments on calls";
    return DirectCallStatementEmitResult::Error;
  }

  ReturnInfo info;
  if (!getReturnInfo(callee->fullPath, info)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!emitInlineDefinitionCall(directStmt, *callee, localsIn, false)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!info.returnsVoid) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  error = priorError;
  return DirectCallStatementEmitResult::Emitted;
}

AssignOrExprStatementEmitResult emitAssignOrExprStatementWithPop(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    std::vector<IrInstruction> &instructions) {
  if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
    if (!emitExpr(stmt, localsIn)) {
      return AssignOrExprStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::Pop, 0});
    return AssignOrExprStatementEmitResult::Emitted;
  }
  if (!emitExpr(stmt, localsIn)) {
    return AssignOrExprStatementEmitResult::Error;
  }
  if (!isFreeMemoryIntrinsicCall(stmt)) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  return AssignOrExprStatementEmitResult::Emitted;
}

} // namespace primec::ir_lowerer
