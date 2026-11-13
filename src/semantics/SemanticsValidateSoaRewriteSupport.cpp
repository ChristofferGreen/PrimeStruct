#include "SemanticsValidateSoaRewriteSupport.h"

#include "SemanticsHelpers.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec {

std::string soaDefinitionNamespace(const std::string &definitionPath) {
  const size_t slash = definitionPath.find_last_of('/');
  if (slash != std::string::npos && slash > 0) {
    return definitionPath.substr(0, slash);
  }
  return {};
}

SoaBindingMap collectSoaReturnDefinitionBindings(const Program &program,
                                                 SoaReturnBindingExtractor extractReturnBinding) {
  SoaBindingMap definitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractReturnBinding(def); binding.has_value()) {
      definitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        definitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  return definitions;
}

void trackBuiltinSoaBinding(const Expr &expr, SoaBindingMap &bindings) {
  if (auto vectorBinding = extractBuiltinVectorBinding(expr); vectorBinding.has_value()) {
    bindings[expr.name] = *vectorBinding;
  } else if (auto soaBinding = extractBuiltinSoaVectorBinding(expr); soaBinding.has_value()) {
    bindings[expr.name] = *soaBinding;
  }
}

std::optional<semantics::BindingInfo> findBuiltinVectorValueBinding(
    const Expr &candidate, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope) {
  const SoaBindingMap &vectorReturnDefinitions = scope.vectorReturnDefinitions;
  const std::string &definitionNamespace = scope.definitionNamespace;
  if (candidate.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(candidate.name);
    if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
      return bindingIt->second;
    }
    return std::nullopt;
  }
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
    return std::nullopt;
  }
  std::string collectionName;
  if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
      collectionName == "vector" && candidate.templateArgs.size() == 1) {
    semantics::BindingInfo binding;
    binding.typeName = "vector";
    binding.typeTemplateArg = candidate.templateArgs.front();
    return binding;
  }
  for (const std::string &candidatePath :
       candidateDefinitionPaths(candidate, definitionNamespace)) {
    auto returnIt = vectorReturnDefinitions.find(candidatePath);
    if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
      return returnIt->second;
    }
  }
  if (!candidate.isMethodCall && semantics::isSimpleCallName(candidate, "dereference") &&
      candidate.args.size() == 1) {
    const Expr &derefTarget = candidate.args.front();
    if (derefTarget.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(derefTarget.name);
      if (bindingIt != bindings.end()) {
        return extractBuiltinCollectionBindingFromWrappedTypeText(
            bindingTypeText(bindingIt->second), "vector");
      }
    }
    std::string accessName;
    if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
        derefTarget.args.size() == 2 && derefTarget.args.front().kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(derefTarget.args.front().name);
      if (bindingIt != bindings.end()) {
        return extractBuiltinCollectionBindingFromWrappedTypeText(
            bindingTypeText(bindingIt->second), "vector");
      }
    }
  }
  return std::nullopt;
}

std::optional<BuiltinSoaReceiverBindingInfo> findBuiltinSoaReceiverValueBinding(
    const Expr &candidate, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope) {
  const SoaBindingMap &soaCollectionReturnDefinitions = scope.soaCollectionReturnDefinitions;
  const std::string &definitionNamespace = scope.definitionNamespace;
  if (candidate.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(candidate.name);
    if (bindingIt != bindings.end()) {
      return extractBuiltinSoaReceiverBinding(bindingIt->second);
    }
    return std::nullopt;
  }
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
    return std::nullopt;
  }
  std::string collectionName;
  if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
      collectionName == semantics::internalSoaCollectionTypeName() &&
      candidate.templateArgs.size() == 1) {
    semantics::BindingInfo binding;
    binding.typeName = semantics::internalSoaCollectionTypeName();
    binding.typeTemplateArg = candidate.templateArgs.front();
    return BuiltinSoaReceiverBindingInfo{binding, false};
  }
  for (const std::string &candidatePath :
       candidateDefinitionPaths(candidate, definitionNamespace)) {
    auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
    if (returnIt != soaCollectionReturnDefinitions.end()) {
      return extractBuiltinSoaReceiverBinding(returnIt->second);
    }
  }
  if (!candidate.isMethodCall && semantics::isSimpleCallName(candidate, "dereference") &&
      candidate.args.size() == 1) {
    const Expr &derefTarget = candidate.args.front();
    if (derefTarget.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(derefTarget.name);
      if (bindingIt != bindings.end()) {
        if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                bindingTypeText(bindingIt->second), semantics::internalSoaCollectionTypeName());
            binding.has_value()) {
          return BuiltinSoaReceiverBindingInfo{*binding, false};
        }
      }
    }
    std::string accessName;
    if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
        derefTarget.args.size() == 2 && derefTarget.args.front().kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(derefTarget.args.front().name);
      if (bindingIt != bindings.end()) {
        if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                bindingTypeText(bindingIt->second), semantics::internalSoaCollectionTypeName());
            binding.has_value()) {
          return BuiltinSoaReceiverBindingInfo{*binding, false};
        }
      }
    }
  }
  return std::nullopt;
}

ExperimentalSoaProgramData collectExperimentalSoaProgramData(const Program &program) {
  ExperimentalSoaProgramData data;
  data.specializedSoaVectorElementTypes =
      buildSpecializedExperimentalSoaVectorElementTypes(program);
  for (const Definition &def : program.definitions) {
    if (collection_helpers::isRootedSoaPath(def.fullPath)) {
      data.visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind(collection_helpers::kCanonicalSoaPrefix, 0) == 0) {
      data.visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string(collection_helpers::kCanonicalSoaPrefix).size());
      data.visibleSoaFieldHelpers.insert(collection_helpers::kRootedSoaPrefix + helperSuffix);
    }
    if (semantics::isStructLikeDefinition(def)) {
      data.structPaths.insert(def.fullPath);
    }
  }
  data.soaCollectionReturnDefinitions = collectSoaReturnDefinitionBindings(
      program, extractExperimentalSoaVectorOrBorrowedReturnBinding);
  return data;
}

void trackExperimentalSoaBinding(const Expr &expr,
                                 const std::unordered_set<std::string> &structPaths,
                                 SoaBindingMap &bindings) {
  if (auto binding = extractParsedOrExperimentalSoaBindingInfo(expr, &structPaths);
      binding.has_value()) {
    bindings[expr.name] = *binding;
  }
}

} // namespace primec
