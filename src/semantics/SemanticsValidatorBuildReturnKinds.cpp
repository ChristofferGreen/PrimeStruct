#include "SemanticsValidator.h"

namespace primec::semantics {

std::string SemanticsValidator::resolveStructReturnPathForBuild(const std::string &typeName,
                                                               const std::string &namespacePrefix) const {
  if (typeName.empty()) {
    return "";
  }

  auto normalizeCollectionPath = [](const std::string &baseName) -> std::string {
    if (baseName == "array" || baseName == "vector" || baseName == "string") {
      return "/" + baseName;
    }
    if (isMapCollectionTypeName(baseName)) {
      return "/map";
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
      if (isMapCollectionTypeName(collectionBase) && args.size() == 2) {
        return "/map";
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
      for (const auto &transform : def.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        hasExplicitSumReturn =
            resolveSumDefinitionForTypeText(transform.templateArgs.front(),
                                            def.namespacePrefix) != nullptr;
        break;
      }
      std::string returnKindError;
      if (!hasExplicitSumReturn) {
        for (const auto &transform : def.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "soa_vector") {
            break;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            break;
          }
          if (!isSoaVectorStructElementType(args.front(), def.namespacePrefix, structNames_, importAliases_)) {
            returnKindError = "soa_vector return type requires struct element type on " + def.fullPath;
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
