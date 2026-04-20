#include "SemanticsValidateExperimentalGfxConstructorsInternal.h"
#include "primec/StdlibSurfaceRegistry.h"

#include "SemanticsHelpers.h"

namespace primec::semantics {

namespace {

bool isWildcardImport(const std::string &path, std::string &prefixOut) {
  if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
    prefixOut = path.substr(0, path.size() - 2);
    return true;
  }
  if (path.find('/', 1) == std::string::npos) {
    prefixOut = path;
    return true;
  }
  return false;
}

std::unordered_set<std::string> collectExperimentalGfxStructNames(const Program &program) {
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
      fieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          fieldOnlyStruct = false;
          break;
        }
      }
    }
    if (hasStructTransform || fieldOnlyStruct) {
      structNames.insert(def.fullPath);
    }
  }
  return structNames;
}

std::unordered_set<std::string> collectExperimentalGfxPublicDefinitions(const Program &program) {
  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool sawPublic = false;
    bool sawPrivate = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "public") {
        sawPublic = true;
      } else if (transform.name == "private") {
        sawPrivate = true;
      }
    }
    if (sawPublic && !sawPrivate) {
      publicDefinitions.insert(def.fullPath);
    }
  }
  return publicDefinitions;
}

std::unordered_map<std::string, std::string> collectExperimentalGfxImportAliases(
    const Program &program,
    const std::unordered_set<std::string> &publicDefinitions) {
  std::unordered_map<std::string, std::string> importAliases;
  for (const auto &importPath : program.imports) {
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      std::string scopedPrefix = prefix;
      if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
        scopedPrefix += "/";
      }
      for (const auto &def : program.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions.count(def.fullPath) == 0) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (publicDefinitions.count(importPath) == 0) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }
  return importAliases;
}

} // namespace

ExperimentalGfxRewriteContext buildExperimentalGfxRewriteContext(const Program &program) {
  ExperimentalGfxRewriteContext context;
  context.structNames = collectExperimentalGfxStructNames(program);
  const auto publicDefinitions = collectExperimentalGfxPublicDefinitions(program);
  context.importAliases = collectExperimentalGfxImportAliases(program, publicDefinitions);
  return context;
}

std::string ExperimentalGfxRewriteContext::resolveImportedStructPath(const std::string &name,
                                                                     const std::string &namespacePrefix) const {
  std::string resolved = resolveStructTypePath(name, namespacePrefix, structNames);
  if (resolved.empty()) {
    auto it = importAliases.find(name);
    if (it != importAliases.end() && structNames.count(it->second) > 0) {
      resolved = it->second;
    }
  }
  return resolved;
}

bool hasExperimentalGfxImportedDefinitionPath(const Program &program, const std::string &path) {
  std::string canonicalPath = path;
  const size_t suffix = canonicalPath.find("__t");
  if (suffix != std::string::npos) {
    canonicalPath.erase(suffix);
  }
  if (canonicalPath.rfind("/File/", 0) == 0 || canonicalPath.rfind("/FileError/", 0) == 0) {
    canonicalPath.insert(0, "/std/file");
  }
  for (const auto &importPath : program.imports) {
    if (importPath == canonicalPath) {
      return true;
    }
    if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(importPath);
        metadata != nullptr &&
        metadata->shape != StdlibSurfaceShape::ConstructorFamily &&
        canonicalPath.rfind(std::string(metadata->canonicalPath) + "/", 0) == 0) {
      return true;
    }
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      const std::string prefix = importPath.substr(0, importPath.size() - 2);
      if (canonicalPath == prefix || canonicalPath.rfind(prefix + "/", 0) == 0) {
        return true;
      }
    }
  }
  return false;
}

} // namespace primec::semantics
