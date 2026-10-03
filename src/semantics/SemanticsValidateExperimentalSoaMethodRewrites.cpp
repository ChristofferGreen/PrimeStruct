#include "SemanticsValidateExperimentalSoaMethodRewrites.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateSoaBindingExtraction.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec {

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper);

void rewriteExperimentalSoaSamePathHelperMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers,
    bool publicSoaSurfaceVisible,
    bool shortSoaConstructorVisible,
    const std::unordered_set<std::string> &overloadedCanonicalHelpers);

namespace {

// TODO-5317: `[auto] values{soa<T>(...)}` spells the public soa constructor
// by its short imported name, which extractParsedOrExperimentalSoaBindingInfo
// (keyed on the fully-qualified /std/collections/soa/soa path) leaves as a
// plain `auto` binding. Treat it exactly like the explicit `[soa<T>]` local
// it constructs, so `values.push(...)`/`values.reserve(...)` desugar to the
// public soa helpers the same way the fully-qualified constructor spelling
// does. Only applies when the public soa surface is visible and no
// user-declared `soa` definition shadows the short constructor name.
std::optional<semantics::BindingInfo> extractSamePathSoaMethodReceiverBinding(
    const Expr &expr,
    const std::unordered_set<std::string> &structPaths,
    bool shortSoaConstructorVisible) {
  auto binding = extractParsedOrExperimentalSoaBindingInfo(expr, &structPaths);
  if (!binding.has_value() || !shortSoaConstructorVisible || !expr.isBinding ||
      expr.args.size() != 1 ||
      semantics::normalizeBindingTypeName(binding->typeName) != "auto") {
    return binding;
  }
  const Expr &initializer = expr.args.front();
  if (initializer.kind != Expr::Kind::Call || initializer.isBinding ||
      initializer.isMethodCall || initializer.isFieldAccess ||
      initializer.templateArgs.size() != 1 || initializer.name != "soa") {
    return binding;
  }
  semantics::BindingInfo inferred = *binding;
  inferred.typeName = "soa";
  inferred.typeTemplateArg = initializer.templateArgs.front();
  return inferred;
}

bool hasUserSoaConstructorShadow(const Program &program) {
  for (const Definition &def : program.definitions) {
    std::string path = def.fullPath;
    if (path.rfind("/std/", 0) == 0) {
      continue;
    }
    const size_t templateStart = path.find('<');
    if (templateStart != std::string::npos) {
      path.erase(templateStart);
    }
    const size_t leafStart = path.find_last_of('/');
    std::string leaf =
        path.substr(leafStart == std::string::npos ? 0 : leafStart + 1);
    const size_t generatedSuffix = leaf.find("__");
    if (generatedSuffix != std::string::npos) {
      leaf.erase(generatedSuffix);
    }
    if (leaf == "soa") {
      return true;
    }
  }
  return false;
}

} // namespace

void rewriteExperimentalSoaSamePathHelperMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers,
    bool publicSoaSurfaceVisible,
    bool shortSoaConstructorVisible,
    const std::unordered_set<std::string> &overloadedCanonicalHelpers) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaSamePathHelperMethodExpr(
        stmt,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers,
        publicSoaSurfaceVisible,
        shortSoaConstructorVisible,
        overloadedCanonicalHelpers);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaSamePathHelperMethodStatements(
          stmt.bodyArguments,
          bodyBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          visibleSoaHelpers,
          publicSoaSurfaceVisible,
          shortSoaConstructorVisible,
          overloadedCanonicalHelpers);
    }
    if (stmt.isBinding) {
      if (auto binding = extractSamePathSoaMethodReceiverBinding(
              stmt, structPaths, shortSoaConstructorVisible);
          binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaSamePathHelperMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers,
    bool publicSoaSurfaceVisible,
    bool shortSoaConstructorVisible,
    const std::unordered_set<std::string> &overloadedCanonicalHelpers) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaSamePathHelperMethodExpr(
        arg,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers,
        publicSoaSurfaceVisible,
        shortSoaConstructorVisible,
        overloadedCanonicalHelpers);
    // TODO-5321: call-argument body envelopes (`if(c, then(){...},
    // else(){...})`, `while(c, do(){...})`) carry their statements in the
    // argument's own bodyArguments; walk them with the enclosing bindings
    // so soa method sugar and rooted /soa/* calls in nested bodies are
    // rewritten like top-level statements.
    if (!arg.bodyArguments.empty()) {
      rewriteExperimentalSoaSamePathHelperMethodStatements(
          arg.bodyArguments,
          bindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          visibleSoaHelpers,
          publicSoaSurfaceVisible,
          shortSoaConstructorVisible,
          overloadedCanonicalHelpers);
    }
  }
  if (expr.kind != Expr::Kind::Call || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  // TODO-5319: a rooted direct call (`/soa/<helper>(values, ...)`) is the
  // non-method twin of the slash-method form (`values./soa/<helper>(...)`)
  // and routes the same way: to a user `/soa/<helper>` shadow when one is
  // visible, otherwise to the canonical /std/collections/soa/<helper>
  // wrapper. Without the soa surface in scope the no-import helper rule
  // rejects it instead, so leave it untouched here.
  if (!expr.isMethodCall) {
    const std::string rootedPath =
        expr.namespacePrefix.empty() || expr.namespacePrefix == "/"
            ? expr.name
            : expr.namespacePrefix + "/" + expr.name;
    if (!publicSoaSurfaceVisible || !collection_helpers::isRootedSoaPath(rootedPath) ||
        !expr.templateArgs.empty() ||
        semantics::hasNamedArguments(expr.argNames)) {
      return;
    }
  }

  std::string helperName = expr.isMethodCall
                               ? expr.name
                               : (expr.namespacePrefix.empty() ||
                                          expr.namespacePrefix == "/"
                                      ? expr.name
                                      : expr.namespacePrefix + "/" + expr.name);
  if (!helperName.empty() && helperName.front() == '/') {
    helperName.erase(helperName.begin());
  }
  if (helperName.rfind("std/collections/soa/", 0) == 0) {
    helperName = helperName.substr(std::string("std/collections/soa/").size());
  } else if (helperName.rfind("soa/", 0) == 0) {
    helperName = helperName.substr(std::string("soa/").size());
  }
  if (!collection_helpers::isCountHelperName(helperName) &&
      !collection_helpers::isGetHelperName(helperName) &&
      !collection_helpers::isRefHelperName(helperName) &&
      helperName != "push" && helperName != "reserve" &&
      !collection_helpers::isToAosHelperName(helperName)) {
    return;
  }
  const std::string helperPath = collection_helpers::kRootedSoaPrefix + helperName;
  const bool hasVisibleSamePathHelper =
      visibleSoaHelpers.count(helperPath) > 0;
  // When a user program shadows the canonical
  // /std/collections/soa/<helper> path with additional concrete
  // overloads, the desugared direct call cannot type the rewritten
  // call receiver for overload selection ("arg0 type=unknown"), so the
  // family reports a spurious ambiguity. Unless a same-path /soa
  // shadow takes precedence anyway, leave those method calls to the
  // method-target resolution machinery, which types the receiver.
  if (!hasVisibleSamePathHelper &&
      overloadedCanonicalHelpers.count(helperName) > 0) {
    return;
  }

  std::optional<semantics::BindingInfo> receiverBinding;
  std::optional<Expr> canonicalReceiverExpr;
  const Expr &receiver = expr.args.front();
  auto isPublicSoaSurfaceBinding = [](const semantics::BindingInfo &binding) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(binding.typeName);
    return (normalizedType == "soa" || normalizedType.rfind("soa<", 0) == 0) &&
           !binding.typeTemplateArg.empty();
  };
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() &&
        (isExperimentalSoaVectorBinding(bindingIt->second) ||
         (publicSoaSurfaceVisible &&
          isPublicSoaSurfaceBinding(bindingIt->second)))) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    for (const std::string &candidatePath :
         candidatePathsForExprCall(receiver, definitionNamespace, &bindings, &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() &&
          isExperimentalSoaVectorBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  // A visible root `/to_aos` shadow wins over the canonical helper, like the
  // /soa/<helper> same-path shadows do for their siblings.
  const bool hasVisibleRootToAosShadow =
      helperName == "to_aos" && !hasVisibleSamePathHelper &&
      visibleSoaHelpers.count("/to_aos") > 0;
  expr.name = hasVisibleSamePathHelper ? helperPath
              : hasVisibleRootToAosShadow ? std::string("/to_aos")
                                          : collection_helpers::kCanonicalSoaPrefix + helperName;
  expr.namespacePrefix.clear();
  if (canonicalReceiverExpr.has_value()) {
    expr.args.front() = *canonicalReceiverExpr;
  }
}

bool rewriteExperimentalSoaSamePathHelperMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaHelpers;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorReturnBindingImpl(def, false);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (semantics::isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (std::string_view helperName : {
           std::string_view("count"),
           std::string_view(collection_helpers::kCountRef),
           std::string_view("get"),
           std::string_view(collection_helpers::kGetRef),
           std::string_view("ref"),
           std::string_view(collection_helpers::kRefRef),
           std::string_view("push"),
           std::string_view("reserve"),
           std::string_view("to_aos"),
           std::string_view(collection_helpers::kToAosRef)}) {
    if (hasVisibleExperimentalSoaSamePathHelper(program, helperName)) {
      visibleSoaHelpers.insert(collection_helpers::kRootedSoaPrefix + std::string(helperName));
    }
  }
  if (hasVisibleRootExperimentalSoaHelper(program, "to_aos")) {
    visibleSoaHelpers.insert("/to_aos");
  }
  // Public soa<T> name receivers are rewritten to the canonical
  // /std/collections/soa/<helper> spelling only when that surface is
  // actually reachable - either the module was merged into the program or
  // an import covers it. Without this gate the rewrite turns valid
  // retired-binding programs (no soa import at all) into dead-path errors.
  bool publicSoaSurfaceVisible = false;
  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind(collection_helpers::kCanonicalSoaPrefix, 0) == 0) {
      publicSoaSurfaceVisible = true;
      break;
    }
  }
  if (!publicSoaSurfaceVisible) {
    const auto &importPaths =
        program.sourceImports.empty() ? program.imports : program.sourceImports;
    for (const auto &importPath : importPaths) {
      if (localImportPathCoversTarget(importPath, collection_helpers::kCanonicalSoaSoa)) {
        publicSoaSurfaceVisible = true;
        break;
      }
    }
  }
  const bool shortSoaConstructorVisible =
      publicSoaSurfaceVisible && !hasUserSoaConstructorShadow(program);
  // Helpers whose canonical /std/collections/soa/<helper> path carries
  // more than one definition (the stdlib template plus user
  // type-differentiated shadows) - see the skip in
  // rewriteExperimentalSoaSamePathHelperMethodExpr.
  std::unordered_set<std::string> overloadedCanonicalHelpers;
  {
    std::unordered_map<std::string, int> canonicalDefCounts;
    constexpr std::string_view kCanonicalPrefix = collection_helpers::kCanonicalSoaPrefix;
    for (const Definition &def : program.definitions) {
      if (def.fullPath.rfind(kCanonicalPrefix, 0) != 0) {
        continue;
      }
      std::string helper = def.fullPath.substr(kCanonicalPrefix.size());
      if (const size_t generatedSuffix = helper.find("__");
          generatedSuffix != std::string::npos) {
        continue;
      }
      if (helper.find('/') != std::string::npos) {
        continue;
      }
      ++canonicalDefCounts[helper];
    }
    for (const auto &[helper, defCount] : canonicalDefCounts) {
      if (defCount > 1) {
        overloadedCanonicalHelpers.insert(helper);
      }
    }
  }
  for (Definition &def : program.definitions) {
    if (collection_helpers::isRootedSoaPath(def.fullPath) ||
        def.fullPath.rfind(collection_helpers::kCanonicalSoaPrefix, 0) == 0 ||
        def.fullPath.rfind(collection_paths::modulePrefix(collection_paths::kExperimentalSoaVectorFolder), 0) == 0) {
      continue;
    }
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths);
          binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaSamePathHelperMethodStatements(
        def.statements,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers,
        publicSoaSurfaceVisible,
        shortSoaConstructorVisible,
        overloadedCanonicalHelpers);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractSamePathSoaMethodReceiverBinding(
                stmt, structPaths, shortSoaConstructorVisible);
            binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaSamePathHelperMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          visibleSoaHelpers,
          publicSoaSurfaceVisible,
          shortSoaConstructorVisible,
          overloadedCanonicalHelpers);
    }
  }
  return true;
}

void rewriteExperimentalSoaToAosMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaToAosMethodExpr(
        stmt,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaToAosMethodStatements(
          stmt.bodyArguments,
          bodyBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          hasVisibleRootToAosHelper,
          hasVisibleCanonicalToAosHelper);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaToAosMethodExpr(
        arg,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
    // TODO-5321: walk call-argument body envelopes (then/else/do bodies).
    if (!arg.bodyArguments.empty()) {
      rewriteExperimentalSoaToAosMethodStatements(
          arg.bodyArguments,
          bindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          hasVisibleRootToAosHelper,
          hasVisibleCanonicalToAosHelper);
    }
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  if (builtinSoaConversionMethodName(expr.name) != "to_aos") {
    return;
  }

  std::optional<semantics::BindingInfo> receiverBinding;
  std::optional<Expr> canonicalReceiverExpr;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForToAosRewrite(binding, ignoredElemType);
  };
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (semantics::isSimpleCallName(receiver, "dereference") && receiver.args.size() == 1 &&
        receiver.args.front().kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.args.front().name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        receiverBinding = bindingIt->second;
      }
    }
    const std::vector<std::string> candidatePaths =
        candidatePathsForExprCall(receiver, definitionNamespace, &bindings, &structPaths);
    if (!receiverBinding.has_value()) {
      for (const std::string &candidatePath : candidatePaths) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          receiverBinding = returnIt->second;
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          break;
        }
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  const bool isExplicitRootToAosSurface =
      !expr.name.empty() && expr.name.front() == '/';
  if (isExplicitRootToAosSurface) {
    if (!hasVisibleRootToAosHelper) {
      return;
    }
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to_aos";
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }
  if (hasVisibleRootToAosHelper) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to_aos";
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }
  if (hasVisibleCanonicalToAosHelper) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
  expr.name = findCompatibilitySpelling(StdlibSurfaceId::CollectionsColumnarHelpers, "to_aos");
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = semantics::compatibilitySoaHelperTargetPath("to_aos");
  expr.namespacePrefix.clear();
  if (canonicalReceiverExpr.has_value()) {
    expr.args.front() = *canonicalReceiverExpr;
  }
}

bool rewriteExperimentalSoaToAosMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  const bool hasVisibleRootToAosHelper =
      hasVisibleRootExperimentalSoaHelper(program, "to_aos");
  bool hasVisibleCanonicalToAosHelper = false;
  auto canonicalizeSoaToAosDefinitionPath = [](std::string path) {
    const size_t specializationSuffix = path.find("__");
    if (specializationSuffix != std::string::npos) {
      path.erase(specializationSuffix);
    }
    return path;
  };
  auto isCanonicalSoaToAosDefinitionPath = [&](std::string_view path) {
    const std::string canonicalPath =
        canonicalizeSoaToAosDefinitionPath(std::string(path));
    return canonicalPath.rfind(collection_helpers::kCanonicalSoaPrefix, 0) == 0 &&
           semantics::isLegacyOrCanonicalSoaHelperPath(canonicalPath, "to_aos");
  };
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (semantics::isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
    if (isCanonicalSoaToAosDefinitionPath(def.fullPath)) {
      hasVisibleCanonicalToAosHelper = true;
    }
  }
  const auto &importPaths =
      program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    const std::string canonicalToAosImportTarget =
        semantics::compatibilitySoaHelperTargetPath("to_aos");
    if (isCanonicalSoaToAosDefinitionPath(canonicalToAosImportTarget) &&
        localImportPathCoversTarget(importPath, canonicalToAosImportTarget)) {
      hasVisibleCanonicalToAosHelper = true;
    }
    if (hasVisibleCanonicalToAosHelper) {
      break;
    }
  }
  for (Definition &def : program.definitions) {
    if (def.fullPath == "/to_aos" ||
        def.fullPath.rfind("/to_aos__", 0) == 0 ||
        isCanonicalSoaToAosDefinitionPath(def.fullPath)) {
      continue;
    }
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaToAosMethodStatements(
        def.statements,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaToAosMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          hasVisibleRootToAosHelper,
          hasVisibleCanonicalToAosHelper);
    }
  }
  return true;
}

} // namespace primec
