#include "SemanticsValidateTransforms.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateTransformsInternal.h"
#include "primec/TransformRegistry.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" || name == "enum" || name == "compute" || name == "workgroup_size" ||
         name == "unsafe" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" ||
         name == "buffer" || name == "ast" || name == "reflect" || name == "generate" || name == "Additive" ||
         name == "Multiplicative" || name == "Comparable" || name == "Indexable";
}

bool isSingleTypeReturnCandidate(const Transform &transform) {
  if (!transform.arguments.empty()) {
    return false;
  }
  if (transform.isAstTransformHook) {
    return false;
  }
  if (isNonTypeTransformName(transform.name)) {
    return false;
  }
  return true;
}

bool isTextOnlyTransform(const Transform &transform) {
  if (transform.phase == TransformPhase::Text) {
    return true;
  }
  if (transform.phase == TransformPhase::Semantic) {
    return false;
  }
  const bool isText = primec::isTextTransformName(transform.name);
  const bool isSemantic = primec::isSemanticTransformName(transform.name);
  return isText && !isSemantic;
}

void stripTextTransforms(std::vector<Transform> &transforms) {
  size_t out = 0;
  for (size_t i = 0; i < transforms.size(); ++i) {
    auto &transform = transforms[i];
    if (isTextOnlyTransform(transform)) {
      continue;
    }
    if (out != i) {
      transforms[out] = std::move(transform);
    }
    ++out;
  }
  transforms.resize(out);
}

void stripTextTransforms(Expr &expr) {
  stripTextTransforms(expr.transforms);
  for (auto &arg : expr.args) {
    stripTextTransforms(arg);
  }
  for (auto &arg : expr.bodyArguments) {
    stripTextTransforms(arg);
  }
}

void stripTextTransforms(Definition &def) {
  stripTextTransforms(def.transforms);
  for (auto &param : def.parameters) {
    stripTextTransforms(param);
  }
  for (auto &stmt : def.statements) {
    stripTextTransforms(stmt);
  }
  if (def.returnExpr.has_value()) {
    stripTextTransforms(*def.returnExpr);
  }
}

void stripTextTransforms(Execution &exec) {
  stripTextTransforms(exec.transforms);
  for (auto &arg : exec.arguments) {
    stripTextTransforms(arg);
  }
  for (auto &arg : exec.bodyArguments) {
    stripTextTransforms(arg);
  }
}

std::string formatTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

std::string formatTransformType(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + formatTemplateArgs(transform.templateArgs) + ">";
}

bool validateTransformPhaseList(const std::vector<Transform> &transforms,
                                const std::string &context,
                                std::string &error) {
  for (const auto &transform : transforms) {
    const bool isText = primec::isTextTransformName(transform.name);
    const bool isSemantic = primec::isSemanticTransformName(transform.name);
    if (transform.phase == TransformPhase::Text) {
      if (!isText) {
        error = "text(...) group requires text transforms on " + context + ": " + transform.name;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Semantic) {
      if (!isSemantic && isText) {
        error = "text transform cannot appear in semantic(...) group on " + context + ": " + transform.name;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Auto && isText && isSemantic) {
      error = "ambiguous transform name on " + context + ": " + transform.name;
      return false;
    }
  }
  return true;
}

bool validateTransformListContext(const std::vector<Transform> &transforms,
                                  const std::string &context,
                                  bool allowSingleTypeToReturn,
                                  std::string &error) {
  if (!validateTransformPhaseList(transforms, context, error)) {
    return false;
  }
  for (const auto &transform : transforms) {
    if (transform.name == "package") {
      error = "package visibility has been removed; use [private] or omit for public: " + context;
      return false;
    }
  }
  if (!allowSingleTypeToReturn) {
    for (const auto &transform : transforms) {
      if (transform.name == "single_type_to_return") {
        error = "single_type_to_return is only valid on definitions: " + context;
        return false;
      }
      if (transform.name == "ast") {
        error = "ast transform is only valid on definitions: " + context;
        return false;
      }
    }
  }
  return true;
}

bool hasTransformNamed(const std::vector<Transform> &transforms, std::string_view name) {
  for (const auto &transform : transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

bool hasPublicVisibility(const Definition &def) {
  return hasTransformNamed(def.transforms, "public");
}

const Transform *findTransformNamed(const std::vector<Transform> &transforms, std::string_view name) {
  for (const auto &transform : transforms) {
    if (transform.name == name) {
      return &transform;
    }
  }
  return nullptr;
}

bool astTransformHookHasSupportedSignature(const Definition &def, std::string &error) {
  const Transform *astMarker = findTransformNamed(def.transforms, "ast");
  if (astMarker == nullptr) {
    return false;
  }
  if (!astMarker->templateArgs.empty() || !astMarker->arguments.empty()) {
    error = "ast transform marker does not accept arguments on " + def.fullPath;
    return false;
  }
  if (!def.templateArgs.empty()) {
    error = "ast transform hook must not be templated: " + def.fullPath;
    return false;
  }
  const Transform *returnTransform = findTransformNamed(def.transforms, "return");
  if (returnTransform == nullptr || returnTransform->templateArgs.size() != 1 ||
      !returnTransform->arguments.empty()) {
    error = "ast transform hook must use return<void> or return<FunctionAst>: " + def.fullPath;
    return false;
  }
  const std::string &returnType = returnTransform->templateArgs.front();
  if (returnType == "void") {
    if (!def.parameters.empty()) {
      error = "ast transform hook with return<void> must not declare parameters: " + def.fullPath;
      return false;
    }
    return true;
  }
  if (returnType != "FunctionAst") {
    error = "ast transform hook must use return<void> or return<FunctionAst>: " + def.fullPath;
    return false;
  }
  if (def.parameters.size() != 1) {
    error = "ast transform hook with return<FunctionAst> must declare one FunctionAst parameter: " +
            def.fullPath;
    return false;
  }
  const Expr &param = def.parameters.front();
  if (!param.isBinding || param.name.empty() || param.transforms.size() != 1) {
    error = "ast transform hook with return<FunctionAst> must declare one FunctionAst parameter: " +
            def.fullPath;
    return false;
  }
  const Transform &paramType = param.transforms.front();
  if (paramType.name != "FunctionAst" || !paramType.templateArgs.empty() ||
      !paramType.arguments.empty()) {
    error = "ast transform hook with return<FunctionAst> must declare one FunctionAst parameter: " +
            def.fullPath;
    return false;
  }
  return true;
}

bool isExecutableAstTransformHook(const Definition &def) {
  const Transform *returnTransform = findTransformNamed(def.transforms, "return");
  return returnTransform != nullptr &&
         returnTransform->templateArgs.size() == 1 &&
         returnTransform->templateArgs.front() == "FunctionAst" &&
         def.parameters.size() == 1;
}

Expr makeReturnCallForExpr(const Definition &def, Expr returnValue) {
  Expr returnCall;
  returnCall.kind = Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.namespacePrefix = def.namespacePrefix;
  returnCall.sourceLine = def.sourceLine;
  returnCall.sourceColumn = def.sourceColumn;
  returnCall.args.push_back(std::move(returnValue));
  returnCall.argNames.push_back(std::nullopt);
  return returnCall;
}

bool applyExecutableAstTransformHook(const Definition &hookDef,
                                     Definition &targetDef,
                                     std::string &error) {
  if (!hookDef.returnExpr.has_value()) {
    error = "ast transform hook did not return FunctionAst on " + targetDef.fullPath +
            " via " + hookDef.fullPath;
    return false;
  }

  const Expr &result = *hookDef.returnExpr;
  const std::string &paramName = hookDef.parameters.front().name;
  if (result.kind != Expr::Kind::Call ||
      result.name != "replace_body_with_return_i32" ||
      result.args.size() != 2 ||
      !result.templateArgs.empty() ||
      result.hasBodyArguments ||
      !result.bodyArguments.empty() ||
      result.args[0].kind != Expr::Kind::Name ||
      result.args[0].name != paramName ||
      result.args[1].kind != Expr::Kind::Literal ||
      result.args[1].intWidth != 32 ||
      result.args[1].isUnsigned) {
    error = "ast transform hook returned unsupported FunctionAst shape on " +
            targetDef.fullPath + " via " + hookDef.fullPath;
    return false;
  }

  Expr returnValue = result.args[1];
  returnValue.namespacePrefix = targetDef.namespacePrefix;
  targetDef.returnExpr = returnValue;
  targetDef.statements.clear();
  targetDef.statements.push_back(makeReturnCallForExpr(targetDef, std::move(returnValue)));
  targetDef.hasReturnStatement = true;
  return true;
}

bool importTargetsOnlyPublicAstHooks(const Program &program, const std::string &importPath) {
  if (importPath.empty() || importPath.front() != '/') {
    return false;
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

  if (!isWildcard) {
    const auto defIt = std::find_if(program.definitions.begin(),
                                    program.definitions.end(),
                                    [&](const Definition &def) {
                                      return def.fullPath == importPath;
                                    });
    return defIt != program.definitions.end() &&
           hasPublicVisibility(*defIt) &&
           hasTransformNamed(defIt->transforms, "ast");
  }

  const std::string scopedPrefix = prefix + "/";
  bool sawPublicAstHook = false;
  bool sawPublicNonAstDefinition = false;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind(scopedPrefix, 0) != 0 || !hasPublicVisibility(def)) {
      continue;
    }
    const std::string remainder = def.fullPath.substr(scopedPrefix.size());
    if (remainder.empty() || remainder.find('/') != std::string::npos) {
      continue;
    }
    if (hasTransformNamed(def.transforms, "ast")) {
      sawPublicAstHook = true;
    } else {
      sawPublicNonAstDefinition = true;
    }
  }
  return sawPublicAstHook && !sawPublicNonAstDefinition;
}

void pruneAstHookOnlyImports(Program &program) {
  auto prune = [&](std::vector<std::string> &imports) {
    imports.erase(std::remove_if(imports.begin(),
                                 imports.end(),
                                 [&](const std::string &importPath) {
                                   return importTargetsOnlyPublicAstHooks(program, importPath);
                                 }),
                  imports.end());
  };
  prune(program.imports);
  prune(program.sourceImports);
}

bool executeDefinitionAstTransformHooks(Program &program, std::string &error) {
  std::unordered_map<std::string, const Definition *> definitionsByPath;
  for (const auto &def : program.definitions) {
    definitionsByPath.emplace(def.fullPath, &def);
  }

  for (auto &def : program.definitions) {
    if (hasTransformNamed(def.transforms, "ast")) {
      continue;
    }
    for (const auto &transform : def.transforms) {
      if (!transform.isAstTransformHook || transform.resolvedPath.empty()) {
        continue;
      }
      const auto hookIt = definitionsByPath.find(transform.resolvedPath);
      if (hookIt == definitionsByPath.end() || hookIt->second == nullptr) {
        error = "resolved ast transform hook is missing on " + def.fullPath + ": " +
                transform.resolvedPath;
        return false;
      }
      const Definition &hookDef = *hookIt->second;
      if (!isExecutableAstTransformHook(hookDef)) {
        continue;
      }
      if (!applyExecutableAstTransformHook(hookDef, def, error)) {
        return false;
      }
    }
  }

  pruneAstHookOnlyImports(program);
  program.definitions.erase(
      std::remove_if(program.definitions.begin(),
                     program.definitions.end(),
                     [](const Definition &def) {
                       return hasTransformNamed(def.transforms, "ast");
                     }),
      program.definitions.end());
  return true;
}

std::string importAliasNameForPath(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return path;
  }
  return path.substr(slash + 1);
}

struct AstTransformResolutionIndex {
  std::unordered_map<std::string, const Definition *> definitionsByPath;
  std::unordered_map<std::string, std::vector<std::string>> visibleHookAliases;
  std::unordered_map<std::string, std::vector<std::string>> hiddenHookAliases;
};

void addAstTransformAlias(std::unordered_map<std::string, std::vector<std::string>> &aliases,
                          const std::string &alias,
                          const std::string &path) {
  auto &paths = aliases[alias];
  if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
    paths.push_back(path);
    std::sort(paths.begin(), paths.end());
  }
}

AstTransformResolutionIndex buildAstTransformResolutionIndex(const Program &program) {
  AstTransformResolutionIndex index;
  for (const auto &def : program.definitions) {
    index.definitionsByPath.emplace(def.fullPath, &def);
  }

  auto addImportedAliases = [&](const std::string &importPath) {
    if (importPath.empty() || importPath.front() != '/') {
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
      for (const auto &def : program.definitions) {
        if (!hasTransformNamed(def.transforms, "ast") ||
            def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        auto &targetAliases =
            hasPublicVisibility(def) ? index.visibleHookAliases : index.hiddenHookAliases;
        addAstTransformAlias(targetAliases, remainder, def.fullPath);
      }
      return;
    }
    const auto defIt = index.definitionsByPath.find(importPath);
    if (defIt == index.definitionsByPath.end() || defIt->second == nullptr ||
        !hasTransformNamed(defIt->second->transforms, "ast")) {
      return;
    }
    auto &targetAliases =
        hasPublicVisibility(*defIt->second) ? index.visibleHookAliases : index.hiddenHookAliases;
    addAstTransformAlias(targetAliases, importAliasNameForPath(importPath), importPath);
  };

  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    addImportedAliases(importPath);
  }
  return index;
}

void addLocalAstTransformAlias(const AstTransformResolutionIndex &index,
                               std::unordered_map<std::string, std::vector<std::string>> &aliases,
                               const std::string &alias,
                               const std::string &path) {
  const auto defIt = index.definitionsByPath.find(path);
  if (defIt == index.definitionsByPath.end() || defIt->second == nullptr ||
      !hasTransformNamed(defIt->second->transforms, "ast")) {
    return;
  }
  addAstTransformAlias(aliases, alias, path);
}

std::unordered_map<std::string, std::vector<std::string>> collectVisibleAstTransformCandidates(
    const AstTransformResolutionIndex &index,
    const Definition &def) {
  std::unordered_map<std::string, std::vector<std::string>> aliases = index.visibleHookAliases;
  for (const auto &[path, candidateDef] : index.definitionsByPath) {
    if (candidateDef == nullptr || !hasTransformNamed(candidateDef->transforms, "ast")) {
      continue;
    }
    if (candidateDef->namespacePrefix == def.namespacePrefix) {
      addAstTransformAlias(aliases, candidateDef->name, path);
    }
  }
  if (!def.namespacePrefix.empty()) {
    for (const auto &[path, candidateDef] : index.definitionsByPath) {
      if (candidateDef == nullptr || !hasTransformNamed(candidateDef->transforms, "ast")) {
        continue;
      }
      if (candidateDef->fullPath.rfind(def.namespacePrefix + "/", 0) == 0) {
        addAstTransformAlias(aliases, candidateDef->name, path);
      }
    }
  }
  for (const auto &[path, candidateDef] : index.definitionsByPath) {
    if (candidateDef == nullptr || !hasTransformNamed(candidateDef->transforms, "ast")) {
      continue;
    }
    if (candidateDef->namespacePrefix.empty()) {
      addAstTransformAlias(aliases, candidateDef->name, path);
    }
  }
  return aliases;
}

bool resolveDefinitionAstTransformHooks(Program &program, std::string &error) {
  const AstTransformResolutionIndex index = buildAstTransformResolutionIndex(program);

  for (const auto &def : program.definitions) {
    if (!hasTransformNamed(def.transforms, "ast")) {
      continue;
    }
    std::string signatureError;
    if (!astTransformHookHasSupportedSignature(def, signatureError)) {
      error = signatureError;
      return false;
    }
  }

  for (auto &def : program.definitions) {
    std::unordered_map<std::string, std::vector<std::string>> visibleAliases =
        collectVisibleAstTransformCandidates(index, def);
    for (auto &transform : def.transforms) {
      transform.isAstTransformHook = false;
      transform.resolvedPath.clear();
      if (transform.name == "ast") {
        continue;
      }

      std::vector<std::string> candidates;
      if (!transform.name.empty() && transform.name.front() == '/') {
        addLocalAstTransformAlias(index, visibleAliases, transform.name, transform.name);
      }
      const auto visibleIt = visibleAliases.find(transform.name);
      if (visibleIt != visibleAliases.end()) {
        candidates = visibleIt->second;
      }
      if (candidates.empty()) {
        const auto hiddenIt = index.hiddenHookAliases.find(transform.name);
        if (hiddenIt != index.hiddenHookAliases.end() && !hiddenIt->second.empty()) {
          error = "ast transform hook is not visible on " + def.fullPath + ": " + transform.name;
          return false;
        }
        continue;
      }
      if (candidates.size() > 1) {
        error = "ambiguous ast transform hook on " + def.fullPath + ": " + transform.name;
        for (const auto &candidate : candidates) {
          error += " " + candidate;
        }
        return false;
      }
      if (transform.phase == TransformPhase::Text) {
        error = "ast transform hook cannot appear in text(...) group on " + def.fullPath + ": " +
                transform.name;
        return false;
      }
      const auto candidateDefIt = index.definitionsByPath.find(candidates.front());
      if (candidateDefIt == index.definitionsByPath.end() || candidateDefIt->second == nullptr) {
        continue;
      }
      std::string signatureError;
      if (!astTransformHookHasSupportedSignature(*candidateDefIt->second, signatureError)) {
        error = signatureError;
        return false;
      }
      transform.resolvedPath = candidates.front();
      transform.isAstTransformHook = true;
      transform.phase = TransformPhase::Semantic;
    }
  }
  return true;
}

bool isLoopBodyEnvelope(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
    return false;
  }
  if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
    return false;
  }
  return true;
}

bool stripSharedScopeTransform(std::vector<Transform> &transforms,
                               bool &found,
                               const std::string &context,
                               std::string &error) {
  found = false;
  for (auto it = transforms.begin(); it != transforms.end(); ++it) {
    if (it->name != "shared_scope") {
      continue;
    }
    if (found) {
      error = "duplicate shared_scope transform on " + context;
      return false;
    }
    if (!it->templateArgs.empty()) {
      error = "shared_scope does not accept template arguments on " + context;
      return false;
    }
    if (!it->arguments.empty()) {
      error = "shared_scope does not accept arguments on " + context;
      return false;
    }
    found = true;
    transforms.erase(it);
    break;
  }
  return true;
}

bool rewriteSharedScopeStatements(std::vector<Expr> &statements, std::string &error);

bool shouldRewriteIndexedAccessSugar(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.isMethodCall) {
    return false;
  }
  std::string ignoredName;
  return isAssignCall(expr) || getBuiltinOperatorName(expr, ignoredName) ||
         getBuiltinComparisonName(expr, ignoredName) || getBuiltinClampName(expr, ignoredName, true) ||
         getBuiltinMinMaxName(expr, ignoredName, true) || getBuiltinAbsSignName(expr, ignoredName, true) ||
         getBuiltinSaturateName(expr, ignoredName, true) || getBuiltinMathName(expr, ignoredName, true);
}

void rewriteIndexedAccessSugar(Expr &expr) {
  for (auto &arg : expr.args) {
    rewriteIndexedAccessSugar(arg);
  }
  for (auto &arg : expr.bodyArguments) {
    rewriteIndexedAccessSugar(arg);
  }
  if (!shouldRewriteIndexedAccessSugar(expr) || expr.args.size() < 2) {
    return;
  }
  if (!expr.templateArgs.empty() || expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return;
  }
  if (expr.argNames.size() < expr.args.size()) {
    expr.argNames.resize(expr.args.size());
  }
  for (size_t argIndex = 1; argIndex < expr.args.size(); ++argIndex) {
    if (expr.argNames[argIndex - 1].has_value() || !expr.argNames[argIndex].has_value()) {
      continue;
    }
    const std::string &indexName = *expr.argNames[argIndex];
    if (indexName.empty()) {
      continue;
    }

    Expr accessExpr;
    accessExpr.kind = Expr::Kind::Call;
    accessExpr.name = "at";
    accessExpr.namespacePrefix = expr.namespacePrefix;
    accessExpr.args.push_back(std::move(expr.args[argIndex - 1]));

    Expr indexExpr;
    indexExpr.kind = Expr::Kind::Name;
    indexExpr.name = indexName;
    indexExpr.namespacePrefix = expr.namespacePrefix;
    accessExpr.args.push_back(std::move(indexExpr));
    accessExpr.argNames.resize(2);

    expr.args[argIndex - 1] = std::move(accessExpr);
    expr.argNames[argIndex] = std::nullopt;
  }
}

void rewriteIndexedAccessSugar(Definition &def) {
  for (auto &param : def.parameters) {
    rewriteIndexedAccessSugar(param);
  }
  for (auto &stmt : def.statements) {
    rewriteIndexedAccessSugar(stmt);
  }
  if (def.returnExpr.has_value()) {
    rewriteIndexedAccessSugar(*def.returnExpr);
  }
}

void rewriteIndexedAccessSugar(Execution &exec) {
  for (auto &arg : exec.arguments) {
    rewriteIndexedAccessSugar(arg);
  }
  for (auto &arg : exec.bodyArguments) {
    rewriteIndexedAccessSugar(arg);
  }
}

bool rewriteSharedScopeStatement(Expr &stmt, std::string &error) {
  if (stmt.kind == Expr::Kind::Call) {
    bool hasSharedScope = false;
    if (!stripSharedScopeTransform(stmt.transforms, hasSharedScope, "statement", error)) {
      return false;
    }
    if (hasSharedScope) {
      const bool isLoop = isLoopCall(stmt);
      const bool isWhile = isWhileCall(stmt);
      const bool isFor = isForCall(stmt);
      if (!isLoop && !isWhile && !isFor) {
        error = "shared_scope is only valid on loop/while/for statements";
        return false;
      }
      const size_t bodyIndex = isFor ? 3 : 1;
      if (stmt.args.size() <= bodyIndex) {
        error = "shared_scope requires loop body";
        return false;
      }
      Expr &body = stmt.args[bodyIndex];
      if (!isLoopBodyEnvelope(body)) {
        error = "shared_scope requires loop body in do() { ... }";
        return false;
      }
      std::vector<Expr> hoisted;
      std::vector<Expr> remaining;
      for (auto &bodyStmt : body.bodyArguments) {
        if (bodyStmt.isBinding) {
          hoisted.push_back(std::move(bodyStmt));
        } else {
          remaining.push_back(std::move(bodyStmt));
        }
      }
      body.bodyArguments = std::move(remaining);
      if (!rewriteSharedScopeStatements(body.bodyArguments, error)) {
        return false;
      }
      if (hoisted.empty()) {
        return true;
      }

      Expr blockCall;
      blockCall.kind = Expr::Kind::Call;
      blockCall.name = "block";
      blockCall.namespacePrefix = stmt.namespacePrefix;
      blockCall.hasBodyArguments = true;
      blockCall.bodyArguments.reserve(hoisted.size() + 2);

      if (isFor) {
        Expr initStmt = std::move(stmt.args[0]);
        Expr cond = std::move(stmt.args[1]);
        Expr step = std::move(stmt.args[2]);
        Expr forBody = std::move(body);

        Expr noOpInit;
        noOpInit.kind = Expr::Kind::Call;
        noOpInit.name = "block";
        noOpInit.namespacePrefix = stmt.namespacePrefix;
        noOpInit.hasBodyArguments = true;

        Expr forCall;
        forCall.kind = Expr::Kind::Call;
        forCall.name = "for";
        forCall.namespacePrefix = stmt.namespacePrefix;
        forCall.transforms = std::move(stmt.transforms);
        forCall.args.push_back(std::move(noOpInit));
        forCall.argNames.push_back(std::nullopt);
        forCall.args.push_back(std::move(cond));
        forCall.argNames.push_back(std::nullopt);
        forCall.args.push_back(std::move(step));
        forCall.argNames.push_back(std::nullopt);
        forCall.args.push_back(std::move(forBody));
        forCall.argNames.push_back(std::nullopt);

        blockCall.bodyArguments.push_back(std::move(initStmt));
        for (auto &binding : hoisted) {
          blockCall.bodyArguments.push_back(std::move(binding));
        }
        blockCall.bodyArguments.push_back(std::move(forCall));
        stmt = std::move(blockCall);
        return true;
      }

      for (auto &binding : hoisted) {
        blockCall.bodyArguments.push_back(std::move(binding));
      }
      blockCall.bodyArguments.push_back(std::move(stmt));
      stmt = std::move(blockCall);
      return true;
    }

    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return rewriteSharedScopeStatements(stmt.bodyArguments, error);
    }
  }
  return true;
}

bool rewriteSharedScopeStatements(std::vector<Expr> &statements, std::string &error) {
  for (auto &stmt : statements) {
    if (!rewriteSharedScopeStatement(stmt, error)) {
      return false;
    }
  }
  return true;
}

bool applySingleTypeToReturn(std::vector<Transform> &transforms,
                             bool force,
                             const std::string &context,
                             std::string &error) {
  size_t markerIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (transforms[i].name != "single_type_to_return") {
      continue;
    }
    if (markerIndex != transforms.size()) {
      error = "duplicate single_type_to_return transform on " + context;
      return false;
    }
    if (!transforms[i].templateArgs.empty()) {
      error = "single_type_to_return does not accept template arguments on " + context;
      return false;
    }
    if (!transforms[i].arguments.empty()) {
      error = "single_type_to_return does not accept arguments on " + context;
      return false;
    }
    markerIndex = i;
  }
  const bool hasExplicitMarker = markerIndex != transforms.size();
  if (!hasExplicitMarker && !force) {
    return true;
  }
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      if (hasExplicitMarker) {
        error = "single_type_to_return cannot be combined with return transform on " + context;
        return false;
      }
      return true;
    }
  }
  size_t typeIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (!isSingleTypeReturnCandidate(transforms[i])) {
      continue;
    }
    if (typeIndex != transforms.size()) {
      if (hasExplicitMarker) {
        error = "single_type_to_return requires a single type transform on " + context;
        return false;
      }
      return true;
    }
    typeIndex = i;
  }
  if (typeIndex == transforms.size()) {
    if (hasExplicitMarker) {
      error = "single_type_to_return requires a type transform on " + context;
      return false;
    }
    return true;
  }
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.phase = TransformPhase::Semantic;
  returnTransform.templateArgs.push_back(formatTransformType(transforms[typeIndex]));
  if (!hasExplicitMarker) {
    transforms[typeIndex] = std::move(returnTransform);
    return true;
  }
  std::vector<Transform> rewritten;
  rewritten.reserve(transforms.size() - 1);
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (i == typeIndex) {
      rewritten.push_back(std::move(returnTransform));
      continue;
    }
    rewritten.push_back(std::move(transforms[i]));
  }
  transforms = std::move(rewritten);
  return true;
}

bool validateExprTransforms(const Expr &expr, const std::string &context, std::string &error) {
  if (!validateTransformListContext(expr.transforms, context, false, error)) {
    return false;
  }
  for (const auto &arg : expr.args) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool applySemanticTransforms(Program &program,
                             const std::vector<std::string> &semanticTransforms,
                             std::string &error) {
  bool forceSingleTypeToReturn = false;
  for (const auto &name : semanticTransforms) {
    if (name == "single_type_to_return") {
      forceSingleTypeToReturn = true;
      continue;
    }
    error = "unsupported semantic transform: " + name;
    return false;
  }

  for (auto &def : program.definitions) {
    if (!validateTransformListContext(def.transforms, def.fullPath, true, error)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!validateExprTransforms(param, def.fullPath, error)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!validateExprTransforms(stmt, def.fullPath, error)) {
        return false;
      }
    }
  }

  for (auto &exec : program.executions) {
    if (!validateTransformListContext(exec.transforms, exec.fullPath, false, error)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
  }

  for (auto &def : program.definitions) {
    stripTextTransforms(def);
    rewriteIndexedAccessSugar(def);
  }
  for (auto &exec : program.executions) {
    stripTextTransforms(exec);
    rewriteIndexedAccessSugar(exec);
  }
  if (!rewriteEnumDefinitions(program, error)) {
    return false;
  }
  for (auto &def : program.definitions) {
    if (!rewriteSharedScopeStatements(def.statements, error)) {
      return false;
    }
  }
  for (auto &exec : program.executions) {
    if (!rewriteSharedScopeStatements(exec.bodyArguments, error)) {
      return false;
    }
  }
  if (!resolveDefinitionAstTransformHooks(program, error)) {
    return false;
  }
  if (!executeDefinitionAstTransformHooks(program, error)) {
    return false;
  }
  for (auto &def : program.definitions) {
    if (!applySingleTypeToReturn(def.transforms, forceSingleTypeToReturn, def.fullPath, error)) {
      return false;
    }
  }

  return true;
}

} // namespace primec::semantics
