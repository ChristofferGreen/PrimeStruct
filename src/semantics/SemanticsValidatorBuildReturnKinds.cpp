#include "SemanticsValidator.h"

#include "StdlibCollectionSurfaceHelpers.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {
namespace {

const StdlibSurfaceMetadata *
keyValueConstructorSurfaceMetadataForBuildReturnKinds() {
  return keyValueConstructorSurfaceMetadataLocal();
}

std::string keyValueCollectionMarkerPathForBuildReturnKinds() {
  const StdlibSurfaceMetadata *metadata =
      keyValueConstructorSurfaceMetadataForBuildReturnKinds();
  if (metadata != nullptr) {
    for (std::string_view alias : metadata->importAliasSpellings) {
      if (alias.empty()) {
        continue;
      }
      std::string rootedAlias(alias);
      if (rootedAlias.front() != '/') {
        rootedAlias.insert(rootedAlias.begin(), '/');
      }
      if (rootedAlias.find('/', 1) == std::string::npos) {
        return rootedAlias;
      }
    }
  }
  return std::string(1, '/') + std::string("map");
}

} // namespace

std::string SemanticsValidator::formatLocalGeneratedStructEscapeDiagnostic(
    const std::string &structPath) const {
  auto sourcePointText = [](int line, int column) {
    return std::to_string(line) + ":" + std::to_string(column);
  };

  const Definition *structDef = nullptr;
  for (const auto &def : program_.definitions) {
    if (def.fullPath == structPath) {
      structDef = &def;
      break;
    }
  }

  std::string message =
      "local generated struct cannot escape return type: " + structPath;
  if (structDef == nullptr) {
    return message;
  }

  const std::size_t branchMarker = structPath.find("__ct_if_");
  if (branchMarker != std::string::npos) {
    const std::size_t branchStart =
        branchMarker + std::string("__ct_if_").size();
    const std::size_t branchEnd = structPath.find('_', branchStart);
    if (branchEnd != std::string::npos && branchEnd > branchStart) {
      message += " (selected compile-time branch: ";
      message += structPath.substr(branchStart, branchEnd - branchStart);
    } else {
      message += " (selected compile-time branch";
    }
  } else {
    if (structDef->sourceLine > 0 && structDef->sourceColumn > 0) {
      message += " (local type defined at ";
      message += sourcePointText(structDef->sourceLine, structDef->sourceColumn);
    } else {
      message += " (local type definition source unknown";
    }
  }

  if (branchMarker != std::string::npos) {
    if (structDef->sourceLine > 0 && structDef->sourceColumn > 0) {
      message += "; local type defined at ";
      message += sourcePointText(structDef->sourceLine, structDef->sourceColumn);
    } else {
      message += "; local type definition source unknown";
    }
  }

  const Definition *parentDef = nullptr;
  for (const auto &def : program_.definitions) {
    if (def.fullPath == structDef->namespacePrefix) {
      parentDef = &def;
      break;
    }
  }
  if (parentDef == nullptr) {
    message += ")";
    return message;
  }

  struct TypeLocalFact {
    std::string name;
    int line = 0;
    int column = 0;
  };
  std::vector<TypeLocalFact> typeLocals;
  for (const auto &stmt : parentDef->statements) {
    if (stmt.sourceLine == structDef->sourceLine &&
        stmt.sourceColumn == structDef->sourceColumn) {
      break;
    }
    if (!isCompileTimeTypeBinding(stmt)) {
      continue;
    }
    typeLocals.push_back({stmt.name, stmt.sourceLine, stmt.sourceColumn});
  }

  auto referencesTypeLocal = [](const Transform &transform,
                                const TypeLocalFact &fact) {
    auto nameMatches = [](const std::string &candidate,
                          const std::string &name) {
      if (candidate == name) {
        return true;
      }
      const std::string suffix = "/" + name;
      return candidate.size() >= suffix.size() &&
             candidate.compare(candidate.size() - suffix.size(),
                               suffix.size(),
                               suffix) == 0;
    };
    if (nameMatches(transform.name, fact.name)) {
      return true;
    }
    for (const auto &arg : transform.templateArgs) {
      if (nameMatches(arg, fact.name)) {
        return true;
      }
    }
    return false;
  };

  std::vector<TypeLocalFact> referencedFacts;
  for (const auto &field : structDef->statements) {
    for (const auto &transform : field.transforms) {
      if (isBindingAuxTransformName(transform.name)) {
        continue;
      }
      for (const auto &fact : typeLocals) {
        if (!referencesTypeLocal(transform, fact)) {
          continue;
        }
        const bool alreadyRecorded =
            std::any_of(referencedFacts.begin(),
                        referencedFacts.end(),
                        [&](const TypeLocalFact &existing) {
                          return existing.name == fact.name;
                        });
        if (!alreadyRecorded) {
          referencedFacts.push_back(fact);
        }
      }
    }
  }

  if (!referencedFacts.empty()) {
    message += "; type facts: ";
    for (std::size_t i = 0; i < referencedFacts.size(); ++i) {
      if (i > 0) {
        message += ", ";
      }
      message += referencedFacts[i].name;
      if (referencedFacts[i].line > 0 && referencedFacts[i].column > 0) {
        message += " at ";
        message += sourcePointText(referencedFacts[i].line,
                                   referencedFacts[i].column);
      }
    }
  }
  message += ")";
  return message;
}

std::string SemanticsValidator::resolveStructReturnPathForBuild(const std::string &typeName,
                                                               const std::string &namespacePrefix) const {
  if (typeName.empty()) {
    return "";
  }

  auto normalizeCollectionPath = [](const std::string &baseName) -> std::string {
    if (baseName == "array" || baseName == "vector" || baseName == "string") {
      return "/" + baseName;
    }
    if (isKeyValueCollectionTypeName(baseName)) {
      return keyValueCollectionMarkerPathForBuildReturnKinds();
    }
    return "";
  };

  std::string normalizedType = normalizeBindingTypeName(typeName);
  if (std::string collectionPath = normalizeCollectionPath(normalizedType); !collectionPath.empty()) {
    return collectionPath;
  }

  std::string collectionBase;
  std::string collectionArgs;
  if (splitTemplateTypeName(normalizedType, collectionBase, collectionArgs)) {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(collectionArgs, args)) {
      if (collectionBase == "array" && args.size() == 1) {
        return "/array";
      }
      if (collectionBase == "vector" && args.size() == 1) {
        return "/vector";
      }
      if (isKeyValueCollectionTypeName(collectionBase) && args.size() == 2) {
        return specializedExperimentalKeyValueStructReturnPath(args);
      }
    }
  }

  if (typeName.front() == '/') {
    return structNames_.count(typeName) > 0 ? typeName : "";
  }

  std::string current = namespacePrefix;
  while (true) {
    if (!current.empty()) {
      std::string direct = current + "/" + typeName;
      if (structNames_.count(direct) > 0) {
        return direct;
      }
      if (current.size() > typeName.size()) {
        const size_t start = current.size() - typeName.size();
        if (start > 0 && current[start - 1] == '/' &&
            current.compare(start, typeName.size(), typeName) == 0 &&
            structNames_.count(current) > 0) {
          return current;
        }
      }
    } else {
      std::string root = "/" + typeName;
      if (structNames_.count(root) > 0) {
        return root;
      }
    }

    if (current.empty()) {
      break;
    }
    const size_t slash = current.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      current.clear();
    } else {
      current.erase(slash);
    }
  }

  auto importIt = importAliases_.find(typeName);
  if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
    return importIt->second;
  }

  const std::string suffix = "/" + typeName;
  std::string uniqueMatch;
  for (const auto &path : structNames_) {
    if (path.size() < suffix.size() ||
        path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
      continue;
    }
    if (!uniqueMatch.empty() && uniqueMatch != path) {
      return "";
    }
    uniqueMatch = path;
  }
  return uniqueMatch;
}

bool SemanticsValidator::buildDefinitionReturnKinds(const std::unordered_set<std::string> &explicitStructs) {
  auto isLocalGeneratedStructReturn = [&](const Definition &def,
                                          const std::string &structPath) {
    if (structPath.empty() || structNames_.count(def.fullPath) > 0) {
      return false;
    }
    const std::string localPrefix = def.fullPath + "/";
    return structPath.rfind(localPrefix, 0) == 0;
  };
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    if (structNames_.count(def.fullPath) > 0) {
      returnStructs_[def.fullPath] = def.fullPath;
      continue;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string structPath =
          resolveStructReturnPathForBuild(transform.templateArgs.front(), def.namespacePrefix);
      if (!structPath.empty()) {
        returnStructs_[def.fullPath] = structPath;
      } else if (const Definition *sumDef =
                     resolveSumDefinitionForTypeText(transform.templateArgs.front(),
                                                     def.namespacePrefix)) {
        returnStructs_[def.fullPath] = sumDef->fullPath;
      }
      break;
    }
  }

  std::vector<SemanticDiagnosticRecord> returnKindDiagnosticRecords;
  const bool collectReturnKindDiagnostics = shouldCollectStructuredDiagnostics();
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    auto failReturnKindDiagnostic = [&](std::string message) -> bool {
      return failDefinitionDiagnostic(def, std::move(message));
    };
    auto addReturnKindDiagnostic = [&](const std::string &message) -> bool {
      if (!collectReturnKindDiagnostics) {
        return failReturnKindDiagnostic(message);
      }
      SemanticDiagnosticRecord record;
      record.message = message;
      if (def.sourceLine > 0 && def.sourceColumn > 0) {
        record.primarySpan.line = def.sourceLine;
        record.primarySpan.column = def.sourceColumn;
        record.primarySpan.endLine = def.sourceLine;
        record.primarySpan.endColumn = def.sourceColumn;
        record.hasPrimarySpan = true;
      }
      SemanticDiagnosticRelatedSpan related;
      related.span.line = def.sourceLine;
      related.span.column = def.sourceColumn;
      related.span.endLine = def.sourceLine;
      related.span.endColumn = def.sourceColumn;
      related.label = "definition: " + def.fullPath;
      record.relatedSpans.push_back(std::move(related));
      rememberFirstCollectedDiagnosticMessage(message);
      returnKindDiagnosticRecords.push_back(std::move(record));
      return true;
    };
    ReturnKind kind = ReturnKind::Void;
    bool isSumDefinition = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "sum") {
        isSumDefinition = true;
        break;
      }
    }
    if (isSumDefinition) {
      kind = ReturnKind::Void;
    } else if (explicitStructs.count(def.fullPath) > 0) {
      kind = ReturnKind::Array;
    } else {
      bool hasExplicitSumReturn = false;
      std::string returnKindError;
      for (const auto &transform : def.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        if (const std::string localStructPath =
                resolveStructReturnPathForBuild(transform.templateArgs.front(),
                                                def.namespacePrefix);
            isLocalGeneratedStructReturn(def, localStructPath)) {
          returnKindError =
              formatLocalGeneratedStructEscapeDiagnostic(localStructPath);
          break;
        }
        hasExplicitSumReturn =
            resolveSumDefinitionForTypeText(transform.templateArgs.front(),
                                            def.namespacePrefix) != nullptr;
        break;
      }
      if (!hasExplicitSumReturn) {
        for (const auto &transform : def.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "soa" "_vector") {
            break;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            break;
          }
          if (!isSoaVectorStructElementType(args.front(), def.namespacePrefix, structNames_, importAliases_)) {
            returnKindError = "soa return type requires struct element type on " + def.fullPath;
            break;
          }
          if (!validateSoaVectorElementFieldEnvelopes(args.front(), def.namespacePrefix)) {
            returnKindError = error_;
          }
          break;
        }
      }
      if (returnKindError.empty()) {
        kind = hasExplicitSumReturn
                   ? ReturnKind::Array
                   : getReturnKind(def, structNames_, importAliases_, returnKindError);
        if (kind == ReturnKind::Unknown && isLifecycleHelperName(def.fullPath) &&
            !def.returnExpr.has_value()) {
          kind = ReturnKind::Void;
        }
      }
      if (!returnKindError.empty()) {
        if (!addReturnKindDiagnostic(returnKindError)) {
          return false;
        }
        continue;
      }
    }
    returnKinds_[def.fullPath] = kind;
  }

  if (!finalizeCollectedStructuredDiagnostics(returnKindDiagnosticRecords)) {
    return false;
  }
  return true;
}

}  // namespace primec::semantics
