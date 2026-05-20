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

bool isTupleDestructuringIdentifier(std::string_view name) {
  if (name.empty()) {
    return false;
  }
  const auto isStart = [](char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) || c == '_';
  };
  const auto isBody = [](char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_';
  };
  if (!isStart(name.front())) {
    return false;
  }
  return std::all_of(name.begin() + 1, name.end(), isBody);
}

bool isPrimitiveTupleDestructuringTypeName(const std::string &name) {
  const std::string normalized = normalizeBindingTypeName(name);
  return normalized == "auto" || normalized == "int" || normalized == "i32" ||
         normalized == "i64" || normalized == "u64" || normalized == "bool" ||
         normalized == "f32" || normalized == "f64" || normalized == "string";
}

bool isKnownTupleDestructuringBracketEntry(const Transform &transform,
                                           const std::string &namespacePrefix,
                                           const Context &ctx) {
  if (!transform.arguments.empty() || !transform.templateArgs.empty() ||
      isNonTypeTransformName(transform.name) ||
      isBindingAuxTransformName(transform.name) ||
      isPrimitiveTupleDestructuringTypeName(transform.name) ||
      isBuiltinTemplateContainer(transform.name) ||
      isRootBuiltinName(transform.name)) {
    return true;
  }
  const std::string resolvedPath =
      resolveNameToPath(transform.name,
                        namespacePrefix,
                        scopedImportAliasesForNamespace(namespacePrefix, ctx),
                        ctx.sourceDefs);
  return ctx.sourceDefs.count(resolvedPath) > 0;
}

bool isStdlibTupleMonomorphPath(std::string_view path) {
  return path == "/std/tuple/tuple" ||
         path.rfind("/std/tuple/tuple__t", 0) == 0;
}

std::string resolveTupleMonomorphBasePath(const std::string &base,
                                          const std::string &namespacePrefix,
                                          const Context &ctx) {
  std::string normalizedBase = normalizeBindingTypeName(base);
  if (normalizedBase.empty()) {
    return {};
  }
  if (isStdlibTupleMonomorphPath(normalizedBase)) {
    return normalizedBase;
  }
  if (!normalizedBase.empty() && normalizedBase.front() == '/') {
    return {};
  }
  return resolveNameToPath(normalizedBase,
                           namespacePrefix,
                           scopedImportAliasesForNamespace(namespacePrefix, ctx),
                           ctx.sourceDefs);
}

bool extractTupleDestructuringArgsFromTypeText(std::string typeText,
                                               const std::string &namespacePrefix,
                                               const Context &ctx,
                                               bool &borrowedOut,
                                               std::vector<std::string> &tupleArgsOut) {
  borrowedOut = false;
  tupleArgsOut.clear();
  typeText = normalizeBindingTypeName(std::move(typeText));

  while (true) {
    std::string wrapperBase;
    std::string wrapperArgText;
    if (!splitTemplateTypeName(typeText, wrapperBase, wrapperArgText)) {
      break;
    }
    wrapperBase = normalizeBindingTypeName(wrapperBase);
    if (wrapperBase != "Reference" && wrapperBase != "Pointer") {
      break;
    }
    std::vector<std::string> wrapperArgs;
    if (!splitTopLevelTemplateArgs(wrapperArgText, wrapperArgs) ||
        wrapperArgs.size() != 1) {
      return false;
    }
    borrowedOut = true;
    typeText = normalizeBindingTypeName(wrapperArgs.front());
  }

  std::string tupleBase;
  std::string tupleArgText;
  if (splitTemplateTypeName(typeText, tupleBase, tupleArgText)) {
    if (!isStdlibTupleMonomorphPath(
            resolveTupleMonomorphBasePath(tupleBase, namespacePrefix, ctx))) {
      return false;
    }
    if (tupleArgText.empty()) {
      return true;
    }
    return splitTopLevelTemplateArgs(tupleArgText, tupleArgsOut);
  }

  const std::string tuplePath =
      resolveTupleMonomorphBasePath(typeText, namespacePrefix, ctx);
  if (!isStdlibTupleMonomorphPath(tuplePath)) {
    return false;
  }
  auto defIt = ctx.sourceDefs.find(tuplePath);
  if (defIt == ctx.sourceDefs.end()) {
    return false;
  }
  for (const TemplatePackBinding &packBinding : defIt->second.templatePackBindings) {
    if (packBinding.parameterName == "Ts") {
      tupleArgsOut = packBinding.arguments;
      return true;
    }
  }
  return false;
}

const BindingInfo *findTupleDestructuringOperandBinding(
    std::string_view name,
    const std::vector<ParameterInfo> &params,
    const LocalTypeMap &locals) {
  if (auto localIt = locals.find(std::string(name)); localIt != locals.end()) {
    return &localIt->second;
  }
  for (const ParameterInfo &param : params) {
    if (param.name == name) {
      return &param.binding;
    }
  }
  return nullptr;
}

bool isTupleDestructuringStatementCandidate(const Expr &stmt,
                                            const std::vector<ParameterInfo> &params,
                                            const LocalTypeMap &locals) {
  return stmt.isBinding && stmt.args.empty() && !stmt.transforms.empty() &&
         !stmt.name.empty() &&
         findTupleDestructuringOperandBinding(stmt.name, params, locals) != nullptr;
}

Expr makeTupleDestructuringNameExpr(const std::string &name,
                                    const Expr &source) {
  Expr expr;
  expr.kind = Expr::Kind::Name;
  expr.name = name;
  expr.sourceName = name;
  expr.sourceLine = source.sourceLine;
  expr.sourceColumn = source.sourceColumn;
  return expr;
}

Expr makeTupleDestructuringGetExpr(const std::string &receiverName,
                                   size_t index,
                                   const std::vector<std::string> &tupleArgs,
                                   const Expr &source) {
  Expr get;
  get.kind = Expr::Kind::Call;
  get.name = "/std/tuple/get";
  get.sourceName = get.name;
  get.sourceLine = source.sourceLine;
  get.sourceColumn = source.sourceColumn;
  get.args.push_back(makeTupleDestructuringNameExpr(receiverName, source));
  get.argNames.push_back(std::nullopt);
  get.templateArgs.reserve(tupleArgs.size() + 1);
  get.templateArgDetails.reserve(tupleArgs.size() + 1);
  get.templateArgs.push_back(std::to_string(index));
  get.templateArgDetails.push_back(
      TemplateArgument::integer(get.templateArgs.front(), index));
  for (const std::string &tupleArg : tupleArgs) {
    get.templateArgs.push_back(tupleArg);
    get.templateArgDetails.push_back(TemplateArgument::type(tupleArg));
  }
  return get;
}

Expr makeTupleDestructuringBindingExpr(const std::string &bindingName,
                                       const std::string &receiverName,
                                       size_t index,
                                       const std::vector<std::string> &tupleArgs,
                                       const Expr &source) {
  Expr binding;
  binding.kind = Expr::Kind::Call;
  binding.name = bindingName;
  binding.sourceName = bindingName;
  binding.sourceLine = source.sourceLine;
  binding.sourceColumn = source.sourceColumn;
  binding.isBinding = true;
  Transform autoTransform;
  autoTransform.name = "auto";
  binding.transforms.push_back(std::move(autoTransform));
  binding.args.push_back(
      makeTupleDestructuringGetExpr(receiverName, index, tupleArgs, source));
  binding.argNames.push_back(std::nullopt);
  return binding;
}

bool tryExpandTupleDestructuringStatement(const Expr &stmt,
                                          const std::vector<ParameterInfo> &params,
                                          const LocalTypeMap &locals,
                                          const std::string &namespacePrefix,
                                          Context &ctx,
                                          std::string &error,
                                          std::vector<Expr> &expandedOut) {
  expandedOut.clear();
  if (!isTupleDestructuringStatementCandidate(stmt, params, locals)) {
    return true;
  }

  const BindingInfo *operandBinding =
      findTupleDestructuringOperandBinding(stmt.name, params, locals);
  if (operandBinding == nullptr) {
    return true;
  }

  bool borrowedOperand = false;
  std::vector<std::string> tupleArgs;
  const bool isTupleOperand = extractTupleDestructuringArgsFromTypeText(
      bindingTypeToString(*operandBinding),
      namespacePrefix,
      ctx,
      borrowedOperand,
      tupleArgs);
  bool hasFreshEntry = false;
  bool hasKnownEntry = false;
  for (const Transform &transform : stmt.transforms) {
    const bool knownEntry =
        isKnownTupleDestructuringBracketEntry(transform, namespacePrefix, ctx);
    hasKnownEntry = hasKnownEntry || knownEntry;
    hasFreshEntry = hasFreshEntry || !knownEntry;
  }

  if (!hasFreshEntry) {
    return true;
  }
  if (hasKnownEntry) {
    error = "tuple destructuring pattern cannot mix binding names with type or modifier transforms";
    return false;
  }
  if (!isTupleOperand) {
    error = "tuple destructuring operand must have tuple type: " + stmt.name;
    return false;
  }
  if (borrowedOperand) {
    error = "tuple destructuring does not support borrowed tuple operands yet";
    return false;
  }
  if (stmt.transforms.size() != tupleArgs.size()) {
    error = "tuple destructuring arity mismatch: pattern has " +
            std::to_string(stmt.transforms.size()) + " names but tuple has " +
            std::to_string(tupleArgs.size()) + " elements";
    return false;
  }

  std::unordered_set<std::string> names;
  for (const Transform &transform : stmt.transforms) {
    if (!isTupleDestructuringIdentifier(transform.name)) {
      error = "tuple destructuring binding name must be an identifier: " +
              transform.name;
      return false;
    }
    if (!names.insert(transform.name).second) {
      error = "duplicate tuple destructuring binding name: " + transform.name;
      return false;
    }
    if (findTupleDestructuringOperandBinding(transform.name, params, locals) !=
        nullptr) {
      error = "duplicate binding name: " + transform.name;
      return false;
    }
  }

  expandedOut.reserve(stmt.transforms.size());
  for (size_t index = 0; index < stmt.transforms.size(); ++index) {
    expandedOut.push_back(makeTupleDestructuringBindingExpr(
        stmt.transforms[index].name, stmt.name, index, tupleArgs, stmt));
  }
  return true;
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
    std::vector<Expr> tupleDestructuringExpansion;
    if (!isStructDefinition(def) &&
        !tryExpandTupleDestructuringStatement(stmt,
                                              params,
                                              locals,
                                              def.namespacePrefix,
                                              ctx,
                                              error,
                                              tupleDestructuringExpansion)) {
      restoreDefinitionPath();
      return false;
    }
    if (!tupleDestructuringExpansion.empty()) {
      def.statements.erase(def.statements.begin() + static_cast<std::ptrdiff_t>(stmtIndex));
      def.statements.insert(def.statements.begin() + static_cast<std::ptrdiff_t>(stmtIndex),
                            std::make_move_iterator(tupleDestructuringExpansion.begin()),
                            std::make_move_iterator(tupleDestructuringExpansion.end()));
      --stmtIndex;
      continue;
    }
    auto &rewrittenStmt = def.statements[stmtIndex];
    const bool isReturnPathStmt = isDefinitionReturnPathStatement(stmt, stmtIndex, returnStatementSelection);
    if (isReturnPathStmt &&
        !rewriteDefinitionExperimentalReturnConstructors(rewrittenStmt,
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
    if (!rewriteExpr(rewrittenStmt, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      restoreDefinitionPath();
      return false;
    }
    if (isReturnPathStmt &&
        !rewriteDefinitionExperimentalReturnConstructors(rewrittenStmt,
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
    recordDefinitionStatementBindingLocal(rewrittenStmt, params, locals, allowMathBare, ctx, locals);
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
