#include "SemanticsValidator.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace primec::semantics {

bool SemanticsValidator::buildImportAliases() {
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

  auto registerCanonicalSoaVectorWildcardAliases = [&](const std::string &prefix) {
    if (prefix != "/std/collections/soa_vector") {
      return false;
    }
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 10> aliases = {{
        {"count", "/std/collections/soa_vector/count"},
        {"count_ref", "/std/collections/soa_vector/count_ref"},
        {"get", "/std/collections/soa_vector/get"},
        {"get_ref", "/std/collections/soa_vector/get_ref"},
        {"ref", "/std/collections/soa_vector/ref"},
        {"ref_ref", "/std/collections/soa_vector/ref_ref"},
        {"reserve", "/std/collections/soa_vector/reserve"},
        {"push", "/std/collections/soa_vector/push"},
        {"to_aos", "/std/collections/soa_vector/to_aos"},
        {"to_aos_ref", "/std/collections/soa_vector/to_aos_ref"},
    }};
    for (const auto &[name, path] : aliases) {
      importAliases_[std::string(name)] = std::string(path);
    }
    return true;
  };
  auto registerExperimentalSoaVectorConversionWildcardAliases = [&](const std::string &prefix) {
    if (prefix != "/std/collections/experimental_soa_vector_conversions") {
      return false;
    }
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 2> aliases = {{
        {"soaVectorToAos", "/std/collections/experimental_soa_vector_conversions/soaVectorToAos"},
        {"soaVectorToAosRef", "/std/collections/experimental_soa_vector_conversions/soaVectorToAosRef"},
    }};
    for (const auto &[name, path] : aliases) {
      importAliases_[std::string(name)] = std::string(path);
    }
    return true;
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
      if (registerCanonicalSoaVectorWildcardAliases(prefix)) {
        continue;
      }
      if (registerExperimentalSoaVectorConversionWildcardAliases(prefix)) {
        continue;
      }
      const std::string scopedPrefix = prefix + "/";
      bool sawImmediateDefinition = false;
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
        auto [it, inserted] = importAliases_.emplace(remainder, path);
        if (!inserted && it->second != path) {
          if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
            return false;
          }
          importError = true;
          break;
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
    auto defIt = defMap_.find(importPath);
    if (defIt == defMap_.end()) {
      if (!isMathBuiltinImport) {
        if (!addImportDiagnostic("unknown import path: " + importPath)) {
          return false;
        }
      }
      continue;
    }
    if (publicDefinitions_.count(importPath) == 0) {
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
    auto [it, inserted] = importAliases_.emplace(remainder, importPath);
    if (!inserted && it->second != importPath) {
      if (!addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
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
          importAliases_.emplace(remainder, path);
        }
        return;
      }
      auto defIt = defMap_.find(importPath);
      if (defIt == defMap_.end() || publicDefinitions_.count(importPath) == 0) {
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
      importAliases_.emplace(remainder, importPath);
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
