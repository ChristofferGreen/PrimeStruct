#include "TemplateMonomorphContext.h"

#include "SemanticsHelpers.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphTemplateSpecialization.h"

#include <sstream>

namespace primec {

using semantics::isRootBuiltinName;

bool usesStdlibScopedImportAliases(const std::string &namespacePrefix, const Context &ctx) {
  auto isStdlibOwnedPath = [](const std::string &path) {
    if (path.rfind("/std/", 0) == 0) {
      return true;
    }
    if (path.size() <= 1 || path.front() != '/') {
      return false;
    }
    const size_t nextSlash = path.find('/', 1);
    const std::string rootName =
        nextSlash == std::string::npos ? path.substr(1) : path.substr(1, nextSlash - 1);
    return isRootBuiltinName(rootName) || rootName == "string" || rootName == "Result" ||
           rootName == "Maybe" || rootName == "Buffer" || rootName == "ImageError" ||
           rootName == "ContainerError" || rootName == "GfxError";
  };
  return isStdlibOwnedPath(namespacePrefix) ||
         (namespacePrefix.empty() && isStdlibOwnedPath(ctx.currentDefinitionPath));
}

const std::unordered_map<std::string, std::string> &scopedImportAliasesForNamespace(
    const std::string &namespacePrefix,
    const Context &ctx) {
  if (usesStdlibScopedImportAliases(namespacePrefix, ctx)) {
    return ctx.stdlibScopedImportAliases;
  }
  return ctx.importAliases;
}

const std::unordered_map<std::string, std::vector<std::string>> &scopedImportAliasTargetsForNamespace(
    const std::string &namespacePrefix,
    const Context &ctx) {
  if (usesStdlibScopedImportAliases(namespacePrefix, ctx)) {
    return ctx.stdlibScopedImportAliasTargets;
  }
  return ctx.importAliasTargets;
}

const std::string *lookupScopedImportAliasForNamespace(std::string_view name,
                                                        const std::string &namespacePrefix,
                                                        const Context &ctx) {
  const std::string key(name);
  if (auto aliasIt = ctx.directImportAliases.find(key);
      aliasIt != ctx.directImportAliases.end()) {
    return &aliasIt->second;
  }
  if (auto aliasIt = ctx.transitiveImportAliases.find(key);
      aliasIt != ctx.transitiveImportAliases.end()) {
    return &aliasIt->second;
  }
  if (!usesStdlibScopedImportAliases(namespacePrefix, ctx)) {
    if (auto aliasIt = ctx.importAliases.find(key);
        aliasIt != ctx.importAliases.end()) {
      return &aliasIt->second;
    }
  }
  return nullptr;
}

bool rewriteExecution(Execution &exec, Context &ctx, std::string &error) {
  return rewriteExecutionEntry(exec, ctx, error);
}

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         const std::vector<TemplateArgument> *resolvedArgDetails,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut) {
  auto defIt = ctx.sourceDefs.find(basePath);
  if (defIt == ctx.sourceDefs.end()) {
    error = "unknown template definition: " + basePath;
    return false;
  }
  const Definition &baseDef = defIt->second;
  if (baseDef.templateArgs.empty()) {
    error = "template arguments are only supported on templated definitions: " +
            helperOverloadDisplayPath(basePath, ctx);
    return false;
  }
  TemplateArgumentBinding templateBinding;
  if (!bindTemplateArguments(baseDef,
                             resolvedArgs,
                             resolvedArgDetails,
                             helperOverloadDisplayPath(basePath, ctx),
                             templateBinding,
                             error)) {
    return false;
  }
  if (!definitionHasTypePackParameter(baseDef) &&
      baseDef.templateArgs.size() != resolvedArgs.size()) {
    std::ostringstream out;
    out << "template argument count mismatch for " << helperOverloadDisplayPath(basePath, ctx) << ": expected "
        << baseDef.templateArgs.size()
        << ", got " << resolvedArgs.size();
    error = out.str();
    return false;
  }
  const std::string key = basePath + "<" + joinMangledTemplateArgs(resolvedArgs, resolvedArgDetails) + ">";
  auto cacheIt = ctx.specializationCache.find(key);
  if (cacheIt != ctx.specializationCache.end()) {
    specializedPathOut = cacheIt->second;
    return true;
  }

  const size_t lastSlash = basePath.find_last_of('/');
  const std::string baseName = lastSlash == std::string::npos ? basePath : basePath.substr(lastSlash + 1);
  const std::string suffix = mangleTemplateArgs(resolvedArgs, resolvedArgDetails);
  const std::string specializedName = baseName + suffix;
  const std::string specializedBasePath = (lastSlash == std::string::npos)
                                              ? specializedName
                                              : basePath.substr(0, lastSlash + 1) + specializedName;
  if (ctx.sourceDefs.count(specializedBasePath) > 0) {
    error = "template specialization conflicts with existing definition: " + specializedBasePath;
    return false;
  }
  ctx.specializationCache.emplace(key, specializedBasePath);
  if (!specializeTemplateDefinitionFamily(
          basePath, templateBinding, specializedBasePath, specializedName, key, ctx, error)) {
    return false;
  }

  specializedPathOut = specializedBasePath;
  return true;
}

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut) {
  return instantiateTemplate(basePath, resolvedArgs, nullptr, ctx, error, specializedPathOut);
}

const std::set<std::string> &sourceDefsFamilyPathIndex(const Context &ctx) {
  if (!ctx.sourceDefsFamilyPathIndexValid) {
    ctx.sourceDefsFamilyPathIndex.clear();
    for (const auto &[defPath, definition] : ctx.sourceDefs) {
      (void)definition;
      ctx.sourceDefsFamilyPathIndex.insert(defPath);
    }
    ctx.sourceDefsFamilyPathIndexValid = true;
  }
  return ctx.sourceDefsFamilyPathIndex;
}

// True if any key in sourceDefs starts with `prefix`. Relies on
// lexicographic ordering: every element sharing `prefix` sorts
// contiguously starting at lower_bound(prefix).
bool anySourceDefStartsWith(const Context &ctx, const std::string &prefix) {
  const std::set<std::string> &index = sourceDefsFamilyPathIndex(ctx);
  const auto it = index.lower_bound(prefix);
  return it != index.end() && it->compare(0, prefix.size(), prefix) == 0;
}

bool isArgsPackParameterExpr(const Expr &param) {
  for (const Transform &transform : param.transforms) {
    if (transform.name == "args" && transform.templateArgs.size() == 1) {
      return true;
    }
  }
  return false;
}

bool definitionHasVariadicParameter(const Definition &def) {
  return !def.parameters.empty() && isArgsPackParameterExpr(def.parameters.back());
}

bool definitionHasTypePackParameter(const Definition &def) {
  for (bool isPack : def.templateArgIsPack) {
    if (isPack) {
      return true;
    }
  }
  return false;
}

bool isStructDefinition(const Definition &def) {
  bool isStruct = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return false;
    }
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (semantics::isStructTransformName(transform.name)) {
      isStruct = true;
    }
  }
  if (isStruct) {
    return true;
  }
  if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
}

} // namespace primec
