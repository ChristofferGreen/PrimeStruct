// soa-surface-audit: exempt
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
    const std::unordered_set<std::string> &overloadedCanonicalHelpers);

void rewriteExperimentalSoaSamePathHelperMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers,
    bool publicSoaSurfaceVisible,
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
          overloadedCanonicalHelpers);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
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
        overloadedCanonicalHelpers);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }

  std::string helperName = expr.name;
  if (!helperName.empty() && helperName.front() == '/') {
    helperName.erase(helperName.begin());
  }
  if (helperName.rfind("std/collections/soa/", 0) == 0) {
    helperName = helperName.substr(std::string("std/collections/soa/").size());
  } else if (helperName.rfind("soa/", 0) == 0) {
    helperName = helperName.substr(std::string("soa/").size());
  }
  if (helperName != "count" && helperName != "count_ref" &&
      helperName != "get" && helperName != "get_ref" &&
      helperName != "ref" && helperName != "ref_ref" &&
      helperName != "push" && helperName != "reserve" &&
      helperName != "to_aos" && helperName != "to_aos_ref") {
    return;
  }
  const std::string helperPath = "/soa/" + helperName;
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
  expr.name = hasVisibleSamePathHelper ? helperPath
                                       : "/std/collections/soa/" + helperName;
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
           std::string_view("count_ref"),
           std::string_view("get"),
           std::string_view("get_ref"),
           std::string_view("ref"),
           std::string_view("ref_ref"),
           std::string_view("push"),
           std::string_view("reserve"),
           std::string_view("to_aos"),
           std::string_view("to_aos_ref")}) {
    if (hasVisibleExperimentalSoaSamePathHelper(program, helperName)) {
      visibleSoaHelpers.insert("/soa/" + std::string(helperName));
    }
  }
  // Public soa<T> name receivers are rewritten to the canonical
  // /std/collections/soa/<helper> spelling only when that surface is
  // actually reachable - either the module was merged into the program or
  // an import covers it. Without this gate the rewrite turns valid
  // retired-binding programs (no soa import at all) into dead-path errors.
  bool publicSoaSurfaceVisible = false;
  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind("/std/collections/soa/", 0) == 0) {
      publicSoaSurfaceVisible = true;
      break;
    }
  }
  if (!publicSoaSurfaceVisible) {
    const auto &importPaths =
        program.sourceImports.empty() ? program.imports : program.sourceImports;
    for (const auto &importPath : importPaths) {
      if (localImportPathCoversTarget(importPath, "/std/collections/soa/soa")) {
        publicSoaSurfaceVisible = true;
        break;
      }
    }
  }
  // Helpers whose canonical /std/collections/soa/<helper> path carries
  // more than one definition (the stdlib template plus user
  // type-differentiated shadows) - see the skip in
  // rewriteExperimentalSoaSamePathHelperMethodExpr.
  std::unordered_set<std::string> overloadedCanonicalHelpers;
  {
    std::unordered_map<std::string, int> canonicalDefCounts;
    constexpr std::string_view kCanonicalPrefix = "/std/collections/soa/";
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
    if (def.fullPath.rfind("/soa/", 0) == 0 ||
        def.fullPath.rfind("/std/collections/soa/", 0) == 0 ||
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
        overloadedCanonicalHelpers);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
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
    return canonicalPath.rfind("/std/collections/soa/", 0) == 0 &&
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

std::optional<Expr> normalizeExperimentalSoaInlineBorrowReceiver(
    const Expr &receiver,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> *soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> *structPaths) {
  auto canonicalExperimentalSoaReceiver = [&](const Expr &expr) -> std::optional<Expr> {
    if (expr.kind == Expr::Kind::Name) {
      const std::string &name = expr.name;
      auto bindingIt = bindings.find(name);
      if (bindingIt == bindings.end()) {
        return std::nullopt;
      }
      std::string ignoredElemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              bindingIt->second, ignoredElemType)) {
        return expr;
      }
      return std::nullopt;
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding || soaCollectionReturnDefinitions == nullptr) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(expr, definitionNamespace, &bindings, structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions->find(candidatePath);
      if (returnIt == soaCollectionReturnDefinitions->end()) {
        continue;
      }
      std::string ignoredElemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              returnIt->second, ignoredElemType)) {
        return canonicalizeResolvedCallPath(expr, candidatePath);
      }
    }
    return std::nullopt;
  };
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return std::nullopt;
  }
  if (semantics::isSimpleCallName(receiver, "location") && receiver.args.size() == 1) {
    if (auto canonicalReceiver = canonicalExperimentalSoaReceiver(receiver.args.front());
        canonicalReceiver.has_value()) {
      return canonicalReceiver;
    }
  }
  if (semantics::isSimpleCallName(receiver, "dereference") &&
      receiver.args.size() == 1) {
    const Expr &borrowedExpr = receiver.args.front();
    if (borrowedExpr.kind == Expr::Kind::Call &&
        !borrowedExpr.isBinding &&
        semantics::isSimpleCallName(borrowedExpr, "location") &&
        borrowedExpr.args.size() == 1) {
      if (auto canonicalReceiver = canonicalExperimentalSoaReceiver(borrowedExpr.args.front());
          canonicalReceiver.has_value()) {
        return canonicalReceiver;
      }
    }
  }
  return std::nullopt;
}

std::optional<Expr> normalizeExperimentalSoaBorrowedHelperReceiver(
    const Expr &receiver,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &structPaths) {
  auto makeDereferenceCall = [](Expr borrowedExpr) {
    Expr dereferenceCall;
    dereferenceCall.kind = Expr::Kind::Call;
    dereferenceCall.name = "dereference";
    dereferenceCall.sourceLine = borrowedExpr.sourceLine;
    dereferenceCall.sourceColumn = borrowedExpr.sourceColumn;
    dereferenceCall.args.push_back(std::move(borrowedExpr));
    dereferenceCall.argNames.resize(dereferenceCall.args.size());
    return dereferenceCall;
  };
  auto isBorrowedBinding = [&](const semantics::BindingInfo &binding) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(binding.typeName);
    if (normalizedType != "Reference" && normalizedType != "Pointer") {
      return false;
    }
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, ignoredElemType);
  };
  auto canonicalBorrowedExperimentalSoaCall = [&](const Expr &expr) -> std::optional<Expr> {
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(expr, definitionNamespace, &bindings, &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() &&
          isBorrowedBinding(returnIt->second)) {
        return canonicalizeResolvedCallPath(expr, candidatePath);
      }
    }
    return std::nullopt;
  };
  if (auto normalizedInline = normalizeExperimentalSoaInlineBorrowReceiver(
          receiver, bindings, &soaCollectionReturnDefinitions, definitionNamespace, &structPaths);
      normalizedInline.has_value()) {
    return normalizedInline;
  }
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
      return makeDereferenceCall(receiver);
    }
  }
  if (auto canonicalReceiver = canonicalBorrowedExperimentalSoaCall(receiver);
      canonicalReceiver.has_value()) {
    return makeDereferenceCall(*canonicalReceiver);
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      semantics::isSimpleCallName(receiver, "dereference") &&
      receiver.args.size() == 1) {
    const Expr &borrowedSource = receiver.args.front();
    if (borrowedSource.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(borrowedSource.name);
      if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
        return receiver;
      }
    }
    if (auto canonicalBorrowedSource = canonicalBorrowedExperimentalSoaCall(borrowedSource);
        canonicalBorrowedSource.has_value()) {
      Expr normalizedReceiver = receiver;
      normalizedReceiver.args.front() = *canonicalBorrowedSource;
      return normalizedReceiver;
    }
  }
  return std::nullopt;
}

bool normalizeExperimentalSoaBorrowedHelperMethodCall(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  auto isBorrowedBinding = [&](const semantics::BindingInfo &binding) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(binding.typeName);
    if (normalizedType != "Reference" && normalizedType != "Pointer") {
      return false;
    }
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, ignoredElemType);
  };
  auto canonicalBorrowedExperimentalSoaCall = [&](const Expr &candidate)
      -> std::optional<Expr> {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(candidate,
                                   definitionNamespace,
                                   &bindings,
                                   &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() &&
          isBorrowedBinding(returnIt->second)) {
        return canonicalizeResolvedCallPath(candidate, candidatePath);
      }
    }
    return std::nullopt;
  };
  auto normalizedBorrowedReceiver = [&](const Expr &receiver)
      -> std::optional<Expr> {
    if (receiver.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.name);
      if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
        return receiver;
      }
      return std::nullopt;
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return std::nullopt;
    }
    if (semantics::isSimpleCallName(receiver, "location") &&
        receiver.args.size() == 1) {
      const Expr &target = receiver.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(target.name);
        if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
          return target;
        }
      }
      if (auto canonicalBorrowedCall = canonicalBorrowedExperimentalSoaCall(target);
          canonicalBorrowedCall.has_value()) {
        return canonicalBorrowedCall;
      }
      return std::nullopt;
    }
    if (auto canonicalBorrowedCall = canonicalBorrowedExperimentalSoaCall(receiver);
        canonicalBorrowedCall.has_value()) {
      return canonicalBorrowedCall;
    }
    if (semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (borrowedSource.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(borrowedSource.name);
        if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
          return borrowedSource;
        }
      }
      if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding &&
          semantics::isSimpleCallName(borrowedSource, "location") &&
          borrowedSource.args.size() == 1) {
        const Expr &target = borrowedSource.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto bindingIt = bindings.find(target.name);
          if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
            return target;
          }
        }
        if (auto canonicalBorrowedTarget =
                canonicalBorrowedExperimentalSoaCall(target);
            canonicalBorrowedTarget.has_value()) {
          return canonicalBorrowedTarget;
        }
        return std::nullopt;
      }
      if (auto canonicalBorrowedSource =
              canonicalBorrowedExperimentalSoaCall(borrowedSource);
          canonicalBorrowedSource.has_value()) {
        return canonicalBorrowedSource;
      }
    }
    return std::nullopt;
  };
  auto borrowedReceiverElementType = [&](const Expr &receiver)
      -> std::optional<std::string> {
    auto bindingElementType = [&](const semantics::BindingInfo &binding)
        -> std::optional<std::string> {
      std::string elemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              binding, elemType) &&
          !elemType.empty()) {
        return elemType;
      }
      return std::nullopt;
    };
    auto elementTypeForBorrowedSource = [&](const Expr &borrowedSource)
        -> std::optional<std::string> {
      if (borrowedSource.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(borrowedSource.name);
        return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                           : std::nullopt;
      }
      if (borrowedSource.kind != Expr::Kind::Call || borrowedSource.isBinding) {
        return std::nullopt;
      }
      for (const std::string &candidatePath :
           candidatePathsForExprCall(borrowedSource,
                                     definitionNamespace,
                                     &bindings,
                                     &structPaths)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end()) {
          if (auto elemType = bindingElementType(returnIt->second);
              elemType.has_value()) {
            return elemType;
          }
        }
      }
      return std::nullopt;
    };
    if (auto normalizedBorrowed =
            normalizeExperimentalSoaBorrowedHelperReceiver(
                receiver,
                bindings,
                soaCollectionReturnDefinitions,
                definitionNamespace,
                structPaths);
        normalizedBorrowed.has_value() &&
        normalizedBorrowed->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*normalizedBorrowed, "dereference") &&
        normalizedBorrowed->args.size() == 1) {
      if (auto elemType = elementTypeForBorrowedSource(normalizedBorrowed->args.front());
          elemType.has_value()) {
        return elemType;
      }
    }
    if (receiver.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.name);
      return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                         : std::nullopt;
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidateDefinitionPaths(receiver, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        if (auto elemType = bindingElementType(returnIt->second);
            elemType.has_value()) {
          return elemType;
        }
      }
    }
    if (semantics::isSimpleCallName(receiver, "location") &&
        receiver.args.size() == 1) {
      const Expr &target = receiver.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(target.name);
        return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                           : std::nullopt;
      }
      if (target.kind == Expr::Kind::Call && !target.isBinding) {
        for (const std::string &candidatePath :
             candidateDefinitionPaths(target, definitionNamespace)) {
          auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
          if (returnIt != soaCollectionReturnDefinitions.end()) {
            if (auto elemType = bindingElementType(returnIt->second);
                elemType.has_value()) {
              return elemType;
            }
          }
        }
      }
      for (const std::string &candidatePath :
           candidatePathsForExprCall(target,
                                     definitionNamespace,
                                     &bindings,
                                     &structPaths)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end()) {
          if (auto elemType = bindingElementType(returnIt->second);
              elemType.has_value()) {
            return elemType;
          }
        }
      }
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(receiver,
                                   definitionNamespace,
                                   &bindings,
                                   &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        if (auto elemType = bindingElementType(returnIt->second);
            elemType.has_value()) {
          return elemType;
        }
      }
    }
    if (semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (auto elemType = elementTypeForBorrowedSource(borrowedSource);
          elemType.has_value()) {
        return elemType;
      }
      if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding &&
          semantics::isSimpleCallName(borrowedSource, "location") &&
          borrowedSource.args.size() == 1) {
        const Expr &target = borrowedSource.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto bindingIt = bindings.find(target.name);
          return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                             : std::nullopt;
        }
        if (target.kind == Expr::Kind::Call && !target.isBinding) {
          for (const std::string &candidatePath :
               candidateDefinitionPaths(target, definitionNamespace)) {
            auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
            if (returnIt != soaCollectionReturnDefinitions.end()) {
              if (auto elemType = bindingElementType(returnIt->second);
                  elemType.has_value()) {
                return elemType;
              }
            }
          }
        }
      }
      if (auto canonicalBorrowedSource =
              canonicalBorrowedExperimentalSoaCall(borrowedSource);
          canonicalBorrowedSource.has_value()) {
        for (const std::string &candidatePath :
             candidatePathsForExprCall(borrowedSource,
                                       definitionNamespace,
                                       &bindings,
                                       &structPaths)) {
          auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
          if (returnIt != soaCollectionReturnDefinitions.end()) {
            if (auto elemType = bindingElementType(returnIt->second);
                elemType.has_value()) {
              return elemType;
            }
          }
        }
      }
    }
    return std::nullopt;
  };
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return false;
  }
  const std::string normalizedMethodName = [&]() {
    std::string name = expr.name;
    if (!name.empty() && name.front() == '/') {
      name.erase(name.begin());
    }
    if (name.rfind("std/collections/soa/", 0) == 0) {
      name = name.substr(std::string("std/collections/soa/").size());
    } else if (name.rfind("std/collections/soa/", 0) == 0) {
      name = name.substr(std::string("std/collections/soa/").size());
    } else if (name.rfind("soa/", 0) == 0) {
      name = name.substr(std::string("soa/").size());
    } else if (name.rfind("soa/", 0) == 0) {
      name = name.substr(std::string("soa/").size());
    }
    return name;
  }();
  if (!isStdlibSurfaceMemberName(StdlibSurfaceId::CollectionsColumnarHelpers, normalizedMethodName)) {
    return false;
  }
  const bool isCanonicalBorrowedSoaWrapperBodyCall =
      definitionNamespace == "/std/collections/soa" &&
      (normalizedMethodName == "count" || normalizedMethodName == "get" ||
       normalizedMethodName == "ref" || normalizedMethodName == "to_aos") &&
      expr.args.front().kind == Expr::Kind::Call &&
      semantics::isSimpleCallName(expr.args.front(), "dereference") &&
      expr.args.front().args.size() == 1 &&
      expr.args.front().args.front().kind == Expr::Kind::Name;
  if (isCanonicalBorrowedSoaWrapperBodyCall) {
    return false;
  }
  if (auto borrowedReceiver = normalizedBorrowedReceiver(expr.args.front());
      borrowedReceiver.has_value()) {
    const auto borrowedElemType = borrowedReceiverElementType(expr.args.front());
    const bool usesPublicSoaPath =
        expr.namespacePrefix == "/std/collections/soa" ||
        expr.namespacePrefix == "std/collections/soa" ||
        expr.name == "/std/collections/soa/count" ||
        expr.name == "/std/collections/soa/count_ref" ||
        expr.name == "/std/collections/soa/get" ||
        expr.name == "/std/collections/soa/get_ref" ||
        expr.name == "/std/collections/soa/ref" ||
        expr.name == "/std/collections/soa/ref_ref" ||
        expr.name == "/soa/count" ||
        expr.name == "/soa/count_ref" ||
        expr.name == "/soa/get" ||
        expr.name == "/soa/get_ref" ||
        expr.name == "/soa/ref" ||
        expr.name == "/soa/ref_ref";
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.namespacePrefix.clear();
    expr.args.front() = *borrowedReceiver;
    if (borrowedElemType.has_value()) {
      expr.templateArgs.clear();
      expr.templateArgs.push_back(*borrowedElemType);
    }
    const std::string borrowedHelperRoot =
        usesPublicSoaPath ? "/std/collections/soa/"
                          : "/std/collections/soa/";
    if (normalizedMethodName == "count" ||
        normalizedMethodName == "count_ref") {
      expr.name = borrowedHelperRoot + "count_ref";
      return true;
    }
    if (normalizedMethodName == "get" ||
        normalizedMethodName == "get_ref") {
      expr.name = borrowedHelperRoot + "get_ref";
      return true;
    }
    if (normalizedMethodName == "ref" ||
        normalizedMethodName == "ref_ref") {
      expr.name = borrowedHelperRoot + "ref_ref";
      return true;
    }
    expr.name = borrowedHelperRoot + "to_aos_ref";
    return true;
  }
  if (!expr.isMethodCall && expr.name.find('/') != std::string::npos) {
    return false;
  }
  const auto normalizedReceiver = normalizeExperimentalSoaBorrowedHelperReceiver(
      expr.args.front(), bindings, soaCollectionReturnDefinitions, definitionNamespace, structPaths);
  if (!normalizedReceiver.has_value()) {
    return false;
  }
  expr.args.front() = *normalizedReceiver;
  if (!expr.isMethodCall) {
    expr.isMethodCall = true;
  }
  return true;
}

void rewriteExperimentalSoaInlineBorrowMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaInlineBorrowMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaInlineBorrowMethodExpr(
        stmt, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaInlineBorrowMethodStatements(
          stmt.bodyArguments, bodyBindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaInlineBorrowMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaInlineBorrowMethodExpr(
        arg, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call) {
    return;
  }
  if (expr.isFieldAccess && !expr.args.empty()) {
    Expr &receiverExpr = expr.args.front();
    normalizeExperimentalSoaBorrowedHelperMethodCall(
        receiverExpr, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
  }
  normalizeExperimentalSoaBorrowedHelperMethodCall(
      expr, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
}

bool rewriteExperimentalSoaInlineBorrowMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  std::unordered_set<std::string> structPaths;
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
  }
  for (Definition &def : program.definitions) {
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
    rewriteExperimentalSoaInlineBorrowMethodStatements(
        def.statements, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaInlineBorrowMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace);
    }
  }
  return true;
}

} // namespace primec
