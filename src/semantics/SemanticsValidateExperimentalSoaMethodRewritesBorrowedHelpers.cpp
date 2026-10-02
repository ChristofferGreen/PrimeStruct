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
#include "primec/support/CollectionHelperNames.h"

namespace primec {

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
      definitionNamespace == collection_helpers::kCanonicalSoa &&
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
    // Only count/get/ref/to_aos have borrowed (`_ref`) wrapper helpers; other
    // surface members (push, reserve) must not be mapped to to_aos_ref
    // (TODO-5377).
    if (!collection_helpers::isCountHelperName(normalizedMethodName) &&
        !collection_helpers::isGetHelperName(normalizedMethodName) &&
        !collection_helpers::isRefHelperName(normalizedMethodName) &&
        !collection_helpers::isToAosHelperName(normalizedMethodName)) {
      return false;
    }
    const auto borrowedElemType = borrowedReceiverElementType(expr.args.front());
    const bool usesPublicSoaPath =
        expr.namespacePrefix == collection_helpers::kCanonicalSoa ||
        expr.namespacePrefix == "std/collections/soa" ||
        expr.name == collection_helpers::kCanonicalSoaCount ||
        expr.name == collection_helpers::kCanonicalSoaCountRef ||
        expr.name == collection_helpers::kCanonicalSoaGet ||
        expr.name == collection_helpers::kCanonicalSoaGetRef ||
        expr.name == collection_helpers::kCanonicalSoaRef ||
        expr.name == collection_helpers::kCanonicalSoaRefRef ||
        expr.name == collection_helpers::kRootedSoaCount ||
        expr.name == collection_helpers::kRootedSoaCountRef ||
        expr.name == collection_helpers::kRootedSoaGet ||
        expr.name == collection_helpers::kRootedSoaGetRef ||
        expr.name == collection_helpers::kRootedSoaRef ||
        expr.name == collection_helpers::kRootedSoaRefRef;
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.namespacePrefix.clear();
    expr.args.front() = *borrowedReceiver;
    if (borrowedElemType.has_value()) {
      expr.templateArgs.clear();
      expr.templateArgs.push_back(*borrowedElemType);
    }
    const std::string borrowedHelperRoot =
        usesPublicSoaPath ? collection_helpers::kCanonicalSoaPrefix
                          : collection_helpers::kCanonicalSoaPrefix;
    if (collection_helpers::isCountHelperName(normalizedMethodName)) {
      expr.name = borrowedHelperRoot + collection_helpers::kCountRef;
      return true;
    }
    if (collection_helpers::isGetHelperName(normalizedMethodName)) {
      expr.name = borrowedHelperRoot + collection_helpers::kGetRef;
      return true;
    }
    if (collection_helpers::isRefHelperName(normalizedMethodName)) {
      expr.name = borrowedHelperRoot + collection_helpers::kRefRef;
      return true;
    }
    expr.name = borrowedHelperRoot + collection_helpers::kToAosRef;
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
    // TODO-5321: walk call-argument body envelopes (then/else/do bodies).
    if (!arg.bodyArguments.empty()) {
      rewriteExperimentalSoaInlineBorrowMethodStatements(
          arg.bodyArguments, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
    }
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
