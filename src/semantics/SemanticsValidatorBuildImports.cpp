#include "SemanticsValidator.h"

#include <cctype>

namespace primec::semantics {

bool SemanticsValidator::buildImportAliases() {
  importAliases_.clear();
  std::vector<SemanticDiagnosticRecord> importDiagnosticRecords;
  const bool collectImportDiagnostics = collectDiagnostics_ && diagnosticInfo_ != nullptr;

  auto addImportDiagnostic = [&](const std::string &message, const Definition *relatedDef = nullptr) {
    if (!collectImportDiagnostics) {
      error_ = message;
      return true;
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
    if (error_.empty()) {
      error_ = message;
    }
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

  for (const auto &importPath : program_.imports) {
    if (importPath == "/std/math") {
      if (addImportDiagnostic("import /std/math is not supported; use import /std/math/* or /std/math/<name>")) {
        return false;
      }
      continue;
    }
    if (importPath.empty() || importPath[0] != '/') {
      if (addImportDiagnostic("import path must be a slash path")) {
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
      const std::string scopedPrefix = prefix + "/";
      bool sawImmediateDefinition = false;
      bool importError = false;
      for (const auto &def : program_.definitions) {
        DefinitionContextScope definitionScope(*this, def);
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions_.count(def.fullPath) == 0) {
          continue;
        }
        sawImmediateDefinition = true;
        if (isGeneratedTemplateSpecializationName(remainder)) {
          continue;
        }
        if (isRootBuiltinName(remainder)) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, &def)) {
            return false;
          }
          importError = true;
          break;
        }
        if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, &def)) {
            return false;
          }
          importError = true;
          break;
        }
        const std::string rootPath = "/" + remainder;
        auto rootIt = defMap_.find(rootPath);
        if (rootIt != defMap_.end()) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, rootIt->second)) {
            return false;
          }
          importError = true;
          break;
        }
        auto [it, inserted] = importAliases_.emplace(remainder, def.fullPath);
        if (!inserted && it->second != def.fullPath) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, &def)) {
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
        if (addImportDiagnostic("unknown import path: " + importPath)) {
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
        if (addImportDiagnostic("unknown import path: " + importPath)) {
          return false;
        }
      }
      continue;
    }
    if (publicDefinitions_.count(importPath) == 0) {
      if (addImportDiagnostic("import path refers to private definition: " + importPath, defIt->second)) {
        return false;
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (isRootBuiltinName(remainder)) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
    if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
    const std::string rootPath = "/" + remainder;
    auto rootIt = defMap_.find(rootPath);
    if (rootIt != defMap_.end()) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, rootIt->second)) {
        return false;
      }
      continue;
    }
    auto [it, inserted] = importAliases_.emplace(remainder, importPath);
    if (!inserted && it->second != importPath) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
  }

  if (collectImportDiagnostics && !importDiagnosticRecords.empty()) {
    diagnosticSink_.setRecords(std::move(importDiagnosticRecords));
    return false;
  }
  return true;
}

} // namespace primec::semantics
