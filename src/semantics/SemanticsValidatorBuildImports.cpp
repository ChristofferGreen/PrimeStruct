#include "SemanticsValidator.h"

#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

namespace primec::semantics {
namespace {

int stdlibSurfaceImportAliasPriority(const StdlibSurfaceMetadata &metadata) {
  switch (metadata.shape) {
    case StdlibSurfaceShape::ConstructorFamily:
      return 30;
    case StdlibSurfaceShape::ErrorFamily:
      return 20;
    case StdlibSurfaceShape::HelperFamily:
      return 10;
  }
  return 0;
}

bool stdlibSurfaceMatchesImportAliasPath(const StdlibSurfaceMetadata &metadata,
                                         const std::string_view importPath) {
  return metadata.canonicalPath == importPath ||
         std::find(metadata.importAliasSpellings.begin(), metadata.importAliasSpellings.end(), importPath) !=
             metadata.importAliasSpellings.end();
}

const StdlibSurfaceMetadata *findStdlibSurfaceImportAliasMetadata(const std::string_view importPath) {
  const StdlibSurfaceMetadata *bestMatch = nullptr;
  int bestPriority = -1;
  for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
    if (!stdlibSurfaceMatchesImportAliasPath(metadata, importPath)) {
      continue;
    }
    const int priority = stdlibSurfaceImportAliasPriority(metadata);
    if (bestMatch == nullptr || priority > bestPriority) {
      bestMatch = &metadata;
      bestPriority = priority;
    }
  }
  return bestMatch;
}

std::string resolveStdlibSurfaceImportAliasTarget(const std::string_view importPath) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceImportAliasMetadata(importPath);
  if (metadata == nullptr) {
    return std::string(importPath);
  }
  return std::string(metadata->canonicalPath);
}

bool isCanonicalSoaVectorHelperAliasName(std::string_view aliasName) {
  return aliasName == "count" || aliasName == "get" || aliasName == "ref" ||
         aliasName == "count_ref" || aliasName == "get_ref" ||
         aliasName == "ref_ref" || aliasName == "reserve" ||
         aliasName == "push" || aliasName == "to_aos" ||
         aliasName == "to_aos_ref";
}

const StdlibSurfaceMetadata *findStdlibSurfaceWildcardAliasMetadata(const std::string_view importRoot,
                                                                    const std::string_view aliasName) {
  const StdlibSurfaceMetadata *bestMatch = nullptr;
  int bestPriority = -1;
  for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
    if (metadata.canonicalImportRoot != importRoot) {
      continue;
    }
    if (std::find(metadata.importAliasSpellings.begin(),
                  metadata.importAliasSpellings.end(),
                  aliasName) == metadata.importAliasSpellings.end()) {
      continue;
    }
    const int priority = stdlibSurfaceImportAliasPriority(metadata);
    if (bestMatch == nullptr || priority > bestPriority) {
      bestMatch = &metadata;
      bestPriority = priority;
    }
  }
  return bestMatch;
}

} // namespace

bool SemanticsValidator::buildImportAliases() {
  directImportAliases_.clear();
  transitiveImportAliases_.clear();
  importAliases_.clear();
  std::vector<SemanticDiagnosticRecord> importDiagnosticRecords;
  const bool collectImportDiagnostics = shouldCollectStructuredDiagnostics();
  auto failImportDiagnostic = [&](std::string message, const Definition *relatedDef = nullptr) -> bool {
    if (relatedDef != nullptr) {
      return failDefinitionDiagnostic(*relatedDef, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
  };

  auto addImportDiagnostic = [&](const std::string &message, const Definition *relatedDef = nullptr) {
    if (!collectImportDiagnostics) {
      return failImportDiagnostic(message, relatedDef);
    }
    SemanticDiagnosticRecord record;
    record.message = message;
    if (relatedDef != nullptr && relatedDef->sourceLine > 0 && relatedDef->sourceColumn > 0) {
      SemanticDiagnosticRelatedSpan related;
      related.span.line = relatedDef->sourceLine;
      related.span.column = relatedDef->sourceColumn;
      related.span.endLine = relatedDef->sourceLine;
      related.span.endColumn = relatedDef->sourceColumn;
      related.label = "definition: " + relatedDef->fullPath;
      record.relatedSpans.push_back(std::move(related));
    }
    rememberFirstCollectedDiagnosticMessage(message);
    importDiagnosticRecords.push_back(std::move(record));
    return false;
  };

  const auto isGeneratedTemplateSpecializationName = [](const std::string &name) {
    const size_t marker = name.rfind("__t");
    if (marker == std::string::npos || marker + 3 >= name.size()) {
      return false;
    }
    for (size_t i = marker + 3; i < name.size(); ++i) {
      if (!std::isxdigit(static_cast<unsigned char>(name[i]))) {
        return false;
      }
    }
    return true;
  };

  auto hasCoveringStdlibWildcardImport = [&](const std::string &importPath) {
    if (importPath.rfind("/std/", 0) != 0) {
      return false;
    }
    std::string targetPrefix = importPath;
    if (targetPrefix.size() >= 2 && targetPrefix.compare(targetPrefix.size() - 2, 2, "/*") == 0) {
      targetPrefix.erase(targetPrefix.size() - 2);
    }
    const auto &allImportPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
    for (const auto &otherImport : allImportPaths) {
      if (otherImport == importPath || otherImport.rfind("/std/", 0) != 0) {
        continue;
      }
      std::string otherPrefix = otherImport;
      if (otherPrefix.size() >= 2 && otherPrefix.compare(otherPrefix.size() - 2, 2, "/*") == 0) {
        otherPrefix.erase(otherPrefix.size() - 2);
      } else if (otherPrefix.find('/', 1) != std::string::npos) {
        continue;
      }
      if (targetPrefix == otherPrefix) {
        return true;
      }
      if (targetPrefix.size() > otherPrefix.size() &&
          targetPrefix.rfind(otherPrefix, 0) == 0 &&
          targetPrefix[otherPrefix.size()] == '/') {
        return true;
      }
    }
    return false;
  };

  auto registerStdlibSurfaceWildcardAliases =
      [&](const std::string &prefix,
          std::unordered_map<std::string, std::string> &targetAliases) {
    for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
      if (metadata.canonicalImportRoot != prefix) {
        continue;
      }
      for (const std::string_view spelling : metadata.importAliasSpellings) {
        if (spelling.empty() || spelling.front() == '/') {
          continue;
        }
        const StdlibSurfaceMetadata *preferred =
            findStdlibSurfaceWildcardAliasMetadata(prefix, spelling);
        if (preferred != &metadata) {
          continue;
        }
        const std::string aliasName(spelling);
        const std::string aliasPath(preferred->canonicalPath);
        targetAliases[aliasName] = aliasPath;
        importAliases_[aliasName] = aliasPath;
      }
    }
  };
  auto registerCanonicalSoaVectorWildcardAliases =
      [&](const std::string &prefix, std::unordered_map<std::string, std::string> &) {
        if (prefix != "/std/collections/soa_vector") {
          return false;
        }
        bool sawAlias = false;
        static const std::array<std::string_view, 10> kHelpers = {
            "count", "get", "ref", "count_ref", "get_ref",
            "ref_ref", "reserve", "push", "to_aos", "to_aos_ref"};
        for ([[maybe_unused]] const std::string_view helperName : kHelpers) {
          sawAlias = true;
        }
        return sawAlias;
      };
  auto shouldPublishMergedImportAlias = [&](std::string_view aliasName,
                                            std::string_view targetPath) {
    return !(targetPath.rfind("/std/collections/soa_vector/", 0) == 0 &&
             isCanonicalSoaVectorHelperAliasName(aliasName));
  };

  const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
  for (const auto &importPath : importPaths) {
    if (importPath == "/std/math") {
      if (!addImportDiagnostic("import /std/math is not supported; use import /std/math/* or /std/math/<name>")) {
        return false;
      }
      continue;
    }
    if (importPath.empty() || importPath[0] != '/') {
      if (!addImportDiagnostic("import path must be a slash path")) {
        return false;
      }
      continue;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      registerStdlibSurfaceWildcardAliases(prefix, directImportAliases_);
      bool sawCanonicalSoaVectorWildcardAliases =
          registerCanonicalSoaVectorWildcardAliases(prefix, directImportAliases_);
      const std::string scopedPrefix = prefix + "/";
      bool sawImmediateDefinition = sawCanonicalSoaVectorWildcardAliases;
      bool importError = false;
      std::vector<std::string> matchingPaths;
      matchingPaths.reserve(defMap_.size());
      for (const auto &[path, defPtr] : defMap_) {
        if (defPtr == nullptr || path.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        matchingPaths.push_back(path);
      }
      std::sort(matchingPaths.begin(), matchingPaths.end());
      for (const auto &path : matchingPaths) {
        auto defIt = defMap_.find(path);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          continue;
        }
        DefinitionContextScope definitionScope(*this, *defIt->second);
        const std::string remainder = path.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions_.count(path) == 0) {
          continue;
        }
        sawImmediateDefinition = true;
        if (isGeneratedTemplateSpecializationName(remainder)) {
          continue;
        }
        if (isRootBuiltinName(remainder)) {
          if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
            return false;
          }
          importError = true;
          break;
        }
        if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
          if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
            return false;
          }
          importError = true;
          break;
        }
        const std::string rootPath = "/" + remainder;
        auto rootIt = defMap_.find(rootPath);
        if (rootIt != defMap_.end()) {
          if (!addImportDiagnostic("import creates name conflict: " + remainder, rootIt->second)) {
            return false;
          }
          importError = true;
          break;
        }
        auto [it, inserted] = directImportAliases_.emplace(remainder, path);
        if (!inserted && it->second != path) {
          if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
            return false;
          }
          importError = true;
          break;
        }
        if (shouldPublishMergedImportAlias(remainder, path)) {
          importAliases_.emplace(remainder, path);
        }
      }
      if (importError) {
        continue;
      }
      if (!sawImmediateDefinition && prefix != "/std/math") {
        if (hasCoveringStdlibWildcardImport(importPath)) {
          continue;
        }
        if (!addImportDiagnostic("unknown import path: " + importPath)) {
          return false;
        }
        continue;
      }
      continue;
    }
    bool isMathBuiltinImport = false;
    if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
      std::string name = importPath.substr(10);
      if (name.find('/') == std::string::npos && name != "*" && isMathBuiltinName(name)) {
        isMathBuiltinImport = true;
      }
    }
    const std::string aliasTargetPath = resolveStdlibSurfaceImportAliasTarget(importPath);
    if (findStdlibSurfaceImportAliasMetadata(importPath) != nullptr &&
        defMap_.find(aliasTargetPath) == defMap_.end()) {
      const std::string aliasName = importPath.substr(importPath.find_last_of('/') + 1);
      auto [it, inserted] = directImportAliases_.emplace(aliasName, aliasTargetPath);
      if (!inserted && it->second != aliasTargetPath) {
        if (!addImportDiagnostic("import creates name conflict: " + aliasName)) {
          return false;
        }
      }
      if (shouldPublishMergedImportAlias(aliasName, aliasTargetPath)) {
        importAliases_.emplace(aliasName, aliasTargetPath);
      }
      continue;
    }
    auto defIt = defMap_.find(aliasTargetPath);
    if (defIt == defMap_.end()) {
      if (!isMathBuiltinImport) {
        if (!addImportDiagnostic("unknown import path: " + importPath)) {
          return false;
        }
      }
      continue;
    }
    if (publicDefinitions_.count(aliasTargetPath) == 0) {
      if (!addImportDiagnostic("import path refers to private definition: " + importPath, defIt->second)) {
        return false;
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (isRootBuiltinName(remainder)) {
      if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
    if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
      if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
    const std::string rootPath = "/" + remainder;
    auto rootIt = defMap_.find(rootPath);
    if (rootIt != defMap_.end()) {
      if (!addImportDiagnostic("import creates name conflict: " + remainder, rootIt->second)) {
        return false;
      }
      continue;
    }
    auto [it, inserted] = directImportAliases_.emplace(remainder, aliasTargetPath);
    if (!inserted && it->second != aliasTargetPath) {
      if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
    if (shouldPublishMergedImportAlias(remainder, aliasTargetPath)) {
      importAliases_.emplace(remainder, aliasTargetPath);
    }
  }

  if (!program_.sourceImports.empty()) {
    std::unordered_set<std::string> directImportSet(importPaths.begin(), importPaths.end());
    auto registerTransitiveImportAlias = [&](const std::string &importPath) {
      if (importPath.empty() || importPath[0] != '/') {
        return;
      }
      bool isWildcard = false;
      std::string prefix;
      if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
        isWildcard = true;
        prefix = importPath.substr(0, importPath.size() - 2);
      } else if (importPath.find('/', 1) == std::string::npos) {
        isWildcard = true;
        prefix = importPath;
      }
      if (isWildcard) {
        registerStdlibSurfaceWildcardAliases(prefix, transitiveImportAliases_);
        registerCanonicalSoaVectorWildcardAliases(prefix, transitiveImportAliases_);
        const std::string scopedPrefix = prefix + "/";
        std::vector<std::string> matchingPaths;
        matchingPaths.reserve(defMap_.size());
        for (const auto &[path, defPtr] : defMap_) {
          if (defPtr == nullptr || path.rfind(scopedPrefix, 0) != 0) {
            continue;
          }
          matchingPaths.push_back(path);
        }
        std::sort(matchingPaths.begin(), matchingPaths.end());
        for (const auto &path : matchingPaths) {
          auto defIt = defMap_.find(path);
          if (defIt == defMap_.end() || defIt->second == nullptr || publicDefinitions_.count(path) == 0) {
            continue;
          }
          const std::string remainder = path.substr(scopedPrefix.size());
          if (remainder.empty() || remainder.find('/') != std::string::npos ||
              isGeneratedTemplateSpecializationName(remainder) || isRootBuiltinName(remainder) ||
              (allowMathBareName(remainder) && isMathBuiltinName(remainder))) {
            continue;
          }
          const std::string rootPath = "/" + remainder;
          if (defMap_.find(rootPath) != defMap_.end()) {
            continue;
          }
          transitiveImportAliases_.emplace(remainder, path);
          if (shouldPublishMergedImportAlias(remainder, path)) {
            importAliases_.emplace(remainder, path);
          }
        }
        return;
      }
      const std::string aliasTargetPath = resolveStdlibSurfaceImportAliasTarget(importPath);
      if (findStdlibSurfaceImportAliasMetadata(importPath) != nullptr &&
          defMap_.find(aliasTargetPath) == defMap_.end()) {
        const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
        if (!remainder.empty()) {
          const std::string rootPath = "/" + remainder;
          if (defMap_.find(rootPath) == defMap_.end()) {
            transitiveImportAliases_.emplace(remainder, aliasTargetPath);
            if (shouldPublishMergedImportAlias(remainder, aliasTargetPath)) {
              importAliases_.emplace(remainder, aliasTargetPath);
            }
          }
        }
        return;
      }
      auto defIt = defMap_.find(aliasTargetPath);
      if (defIt == defMap_.end() || publicDefinitions_.count(aliasTargetPath) == 0) {
        return;
      }
      const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
      if (remainder.empty() || isRootBuiltinName(remainder) ||
          (allowMathBareName(remainder) && isMathBuiltinName(remainder))) {
        return;
      }
      const std::string rootPath = "/" + remainder;
      if (defMap_.find(rootPath) != defMap_.end()) {
        return;
      }
      transitiveImportAliases_.emplace(remainder, aliasTargetPath);
      if (shouldPublishMergedImportAlias(remainder, aliasTargetPath)) {
        importAliases_.emplace(remainder, aliasTargetPath);
      }
    };
    for (const auto &importPath : program_.imports) {
      if (directImportSet.count(importPath) != 0) {
        continue;
      }
      registerTransitiveImportAlias(importPath);
    }
  }

  if (!finalizeCollectedStructuredDiagnostics(importDiagnosticRecords)) {
    return false;
  }
  return true;
}

} // namespace primec::semantics
