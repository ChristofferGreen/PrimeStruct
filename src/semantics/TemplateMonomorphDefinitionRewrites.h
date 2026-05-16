#pragma once

namespace {

bool isSumDefinitionForMonomorphRefresh(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return false;
}

std::string sumPayloadTypeTextForMonomorphRefresh(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + joinTemplateArgs(transform.templateArgs) + ">";
}

std::string stripGeneratedSumUnitVariantSuffix(std::string name) {
  const size_t overloadSuffix = name.find("__ov");
  if (overloadSuffix != std::string::npos) {
    name.erase(overloadSuffix);
  }
  const size_t specializationSuffix = name.find("__t");
  if (specializationSuffix != std::string::npos) {
    name.erase(specializationSuffix);
  }
  return name;
}

std::string generatedSumUnitVariantName(const Expr &stmt, const Definition &def) {
  if (def.fullPath.find("__t") == std::string::npos ||
      stmt.kind != Expr::Kind::Call || stmt.isBinding || stmt.name.empty() ||
      !stmt.args.empty() || !stmt.argNames.empty() ||
      !stmt.bodyArguments.empty() || stmt.hasBodyArguments ||
      !stmt.transforms.empty() || !stmt.templateArgs.empty()) {
    return {};
  }
  std::string name = stmt.name;
  if (!name.empty() && name.front() == '/') {
    name.erase(name.begin());
  }
  if (name.find('/') == std::string::npos &&
      name.find("__ov") == std::string::npos &&
      name.find("__t") == std::string::npos) {
    return {};
  }
  const size_t slash = name.find_last_of('/');
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  return stripGeneratedSumUnitVariantSuffix(std::move(name));
}

void refreshMonomorphizedSumVariants(Definition &def) {
  if (!isSumDefinitionForMonomorphRefresh(def)) {
    return;
  }

  def.sumVariants.clear();
  def.sumVariants.reserve(def.statements.size());
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding && stmt.transforms.size() == 1) {
      const Transform &payload = stmt.transforms.front();
      def.sumVariants.push_back(SumVariant{
          stmt.name,
          true,
          payload.name,
          payload.templateArgs,
          sumPayloadTypeTextForMonomorphRefresh(payload),
          def.sumVariants.size(),
          stmt.sourceLine,
          stmt.sourceColumn,
          stmt.semanticNodeId,
      });
      continue;
    }
    const std::string generatedUnitName = generatedSumUnitVariantName(stmt, def);
    if (!generatedUnitName.empty()) {
      def.sumVariants.push_back(SumVariant{
          generatedUnitName,
          false,
          {},
          {},
          {},
          def.sumVariants.size(),
          stmt.sourceLine,
          stmt.sourceColumn,
          stmt.semanticNodeId,
      });
      continue;
    }
    if (stmt.kind == Expr::Kind::Name && !stmt.name.empty() &&
        stmt.args.empty() && stmt.argNames.empty() &&
        stmt.bodyArguments.empty() && !stmt.hasBodyArguments &&
        stmt.transforms.empty() && stmt.templateArgs.empty()) {
      def.sumVariants.push_back(SumVariant{
          stmt.name,
          false,
          {},
          {},
          {},
          def.sumVariants.size(),
          stmt.sourceLine,
          stmt.sourceColumn,
          stmt.semanticNodeId,
      });
    }
  }
}

bool hasTypePackExpansionTransform(const Expr &expr) {
  for (const Transform &transform : expr.transforms) {
    if (transform.isPackExpansion) {
      return true;
    }
  }
  return false;
}

std::string generatedPackFieldName(const std::string &sourceName,
                                   size_t index) {
  return "__pack_" + sourceName + "_" + std::to_string(index);
}

std::string templateSpecializationBasePath(std::string path) {
  const size_t marker = path.rfind("__t");
  if (marker != std::string::npos) {
    bool allHex = marker + 3 < path.size();
    for (size_t i = marker + 3; i < path.size(); ++i) {
      if (!std::isxdigit(static_cast<unsigned char>(path[i]))) {
        allHex = false;
        break;
      }
    }
    if (allHex) {
      path.erase(marker);
    }
  }
  return path;
}

enum class TypePackBindingExpansionKind {
  StructField,
  Parameter,
  Local,
};

const char *typePackBindingExpansionLabel(TypePackBindingExpansionKind kind) {
  switch (kind) {
    case TypePackBindingExpansionKind::StructField:
      return "field";
    case TypePackBindingExpansionKind::Parameter:
      return "parameter";
    case TypePackBindingExpansionKind::Local:
      return "local";
  }
  return "binding";
}

std::string typePackBindingExpansionPath(const Definition &def,
                                         const Expr &binding,
                                         TypePackBindingExpansionKind kind) {
  if (kind == TypePackBindingExpansionKind::StructField) {
    return def.fullPath + "/" + binding.name;
  }
  return def.fullPath + "(" + binding.name + ")";
}

bool expandTypePackBindingList(Definition &def,
                               std::vector<Expr> &bindings,
                               TypePackBindingExpansionKind kind,
                               std::string &error) {
  bool hasExpansion = false;
  for (const Expr &binding : bindings) {
    if (hasTypePackExpansionTransform(binding)) {
      hasExpansion = true;
      break;
    }
  }
  if (!hasExpansion) {
    return true;
  }
  std::vector<Expr> expandedBindings;
  expandedBindings.reserve(bindings.size());
  std::unordered_set<std::string> bindingNames;
  bindingNames.reserve(bindings.size());
  for (const Expr &binding : bindings) {
    if (!binding.isBinding) {
      if (hasTypePackExpansionTransform(binding)) {
        error = "type-pack expansion is only supported in binding declarations: " +
                typePackBindingExpansionPath(def, binding, kind);
        return false;
      }
      expandedBindings.push_back(binding);
      continue;
    }
    if (!hasTypePackExpansionTransform(binding)) {
      if (!binding.name.empty()) {
        bindingNames.insert(binding.name);
      }
      expandedBindings.push_back(binding);
      continue;
    }
    const char *label = typePackBindingExpansionLabel(kind);
    if (binding.transforms.size() != 1) {
      error = std::string("type-pack ") + label +
              " expansion does not accept modifiers: " +
              typePackBindingExpansionPath(def, binding, kind);
      return false;
    }
    const Transform &packTransform = binding.transforms.front();
    if (!packTransform.isPackExpansion ||
        !packTransform.templateArgs.empty() ||
        !packTransform.templateArgDetails.empty() ||
        !packTransform.arguments.empty()) {
      error = std::string("type-pack ") + label +
              " expansion requires a bare pack parameter: " +
              typePackBindingExpansionPath(def, binding, kind);
      return false;
    }
    if (!binding.args.empty() || !binding.argNames.empty()) {
      error = std::string("type-pack ") + label +
              " expansion does not accept default initializers: " +
              typePackBindingExpansionPath(def, binding, kind);
      return false;
    }
    const TemplatePackBinding *packBinding =
        findTemplatePackBinding(def, packTransform.name);
    if (packBinding == nullptr) {
      error = std::string("unknown type-pack parameter for ") + label +
              " expansion: " +
              packTransform.name;
      return false;
    }
    const std::string defTemplateBase = templateSpecializationBasePath(def.fullPath);
    for (size_t packIndex = 0; packIndex < packBinding->arguments.size();
         ++packIndex) {
      if (kind == TypePackBindingExpansionKind::StructField &&
          templateSpecializationBasePath(packBinding->arguments[packIndex]) ==
          defTemplateBase) {
        error = "recursive pack-expanded storage not supported: " +
                typePackBindingExpansionPath(def, binding, kind);
        return false;
      }
      Expr expanded = binding;
      expanded.name = generatedPackFieldName(binding.name, packIndex);
      expanded.sourceName = expanded.name;
      expanded.transforms.clear();
      expanded.transforms.push_back(Transform{});
      expanded.transforms.back().name = packBinding->arguments[packIndex];
      if (!bindingNames.insert(expanded.name).second) {
        error = std::string("generated type-pack ") + label +
                " conflicts with existing binding: " +
                typePackBindingExpansionPath(def, expanded, kind);
        return false;
      }
      expandedBindings.push_back(std::move(expanded));
    }
  }
  bindings = std::move(expandedBindings);
  return true;
}

bool expandTypePackHelperBindings(Definition &def, std::string &error) {
  if (!expandTypePackBindingList(def, def.parameters, TypePackBindingExpansionKind::Parameter, error)) {
    return false;
  }
  if (isStructDefinition(def)) {
    return expandTypePackBindingList(def, def.statements, TypePackBindingExpansionKind::StructField, error);
  }
  return expandTypePackBindingList(def, def.statements, TypePackBindingExpansionKind::Local, error);
}

bool rejectUnsupportedTypePackStatementExpansions(const Definition &def, std::string &error) {
  if (isStructDefinition(def)) {
    return true;
  }
  for (const Expr &stmt : def.statements) {
    if (hasTypePackExpansionTransform(stmt) && !stmt.isBinding) {
      error = "type-pack expansion is only supported in binding declarations: " +
              def.fullPath + "/" + stmt.name;
      return false;
    }
  }
  return true;
}

} // namespace

bool rewriteDefinition(Definition &def,
                       const SubstMap &mapping,
                       const std::unordered_set<std::string> &allowedParams,
                       Context &ctx,
                       std::string &error) {
  const std::string previousDefinitionPath = ctx.currentDefinitionPath;
  const Definition *previousRewriteDefinition = ctx.currentRewriteDefinition;
  ctx.currentDefinitionPath = def.fullPath;
  ctx.currentRewriteDefinition = &def;
  auto restoreDefinitionPath = [&]() {
    ctx.currentDefinitionPath = previousDefinitionPath;
    ctx.currentRewriteDefinition = previousRewriteDefinition;
  };
  if (!rejectUnsupportedTypePackStatementExpansions(def, error) ||
      !expandTypePackHelperBindings(def, error)) {
    restoreDefinitionPath();
    return false;
  }
  if (!rewriteTransforms(def.transforms, mapping, allowedParams, def.namespacePrefix, ctx, error)) {
    restoreDefinitionPath();
    return false;
  }
  const bool allowMathBare = hasMathImport(ctx);
  std::vector<ParameterInfo> params;
  LocalTypeMap locals;
  const ExperimentalCollectionReturnRewritePlan returnRewritePlan =
      inferExperimentalCollectionReturnRewritePlan(def, mapping, allowedParams, allowMathBare, ctx);
  params.reserve(def.parameters.size());
  locals.reserve(def.parameters.size() + def.statements.size());
  const DefinitionReturnStatementSelection returnStatementSelection =
      determineDefinitionReturnStatementSelection(def);
  if (!rewriteDefinitionParameters(
          def.parameters, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
    restoreDefinitionPath();
    return false;
  }
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    auto &stmt = def.statements[stmtIndex];
    const bool isReturnPathStmt = isDefinitionReturnPathStatement(stmt, stmtIndex, returnStatementSelection);
    if (isReturnPathStmt &&
        !rewriteDefinitionExperimentalReturnConstructors(stmt,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      restoreDefinitionPath();
      return false;
    }
    if (!rewriteExpr(stmt, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      restoreDefinitionPath();
      return false;
    }
    if (isReturnPathStmt &&
        !rewriteDefinitionExperimentalReturnConstructors(stmt,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      restoreDefinitionPath();
      return false;
    }
    recordDefinitionStatementBindingLocal(stmt, params, locals, allowMathBare, ctx, locals);
  }
  if (def.returnExpr.has_value()) {
    if (!rewriteDefinitionExperimentalReturnConstructors(*def.returnExpr,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      restoreDefinitionPath();
      return false;
    }
    if (!rewriteExpr(*def.returnExpr, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params,
                     allowMathBare)) {
      restoreDefinitionPath();
      return false;
    }
    if (!rewriteDefinitionExperimentalReturnConstructors(*def.returnExpr,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      return false;
    }
  }
  refreshMonomorphizedSumVariants(def);
  restoreDefinitionPath();
  return true;
}
