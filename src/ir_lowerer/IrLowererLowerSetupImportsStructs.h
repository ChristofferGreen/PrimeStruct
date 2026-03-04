
  std::unordered_map<std::string, const Definition *> defMap;
  defMap.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  std::unordered_map<std::string, std::string> importAliases =
      buildImportAliasesFromProgram(program.imports, program.definitions, defMap);

  std::unordered_set<std::string> structNames;
  for (const auto &def : program.definitions) {
    if (ir_lowerer::isStructDefinition(def)) {
      structNames.insert(def.fullPath);
    }
  }
  auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    return resolveStructTypePathCandidateFromScope(typeName, namespacePrefix, structNames, importAliases);
  };
  using FieldBinding = ir_lowerer::LayoutFieldBinding;
  auto extractExplicitBindingTypeForLayout = [&](const Expr &expr, FieldBinding &bindingOut) -> bool {
    bindingOut = {};
    for (const auto &transform : expr.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (isLayoutQualifierName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      if (!transform.templateArgs.empty()) {
        bindingOut.typeName = transform.name;
        bindingOut.typeTemplateArg = joinTemplateArgsText(transform.templateArgs);
        return true;
      }
      bindingOut.typeName = transform.name;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    return false;
  };
  auto resolveStructLayoutExprPath = [&](const Expr &expr) -> std::string {
    return resolveStructLayoutExprPathFromScope(expr, defMap, importAliases);
  };
  std::function<std::string(const std::string &, std::unordered_set<std::string> &)> inferStructReturnPathFromDefinition;
  std::function<std::string(const Expr &,
                            const std::unordered_map<std::string, FieldBinding> &,
                            std::unordered_set<std::string> &)>
      inferStructReturnPathFromExpr;
  inferStructReturnPathFromExpr =
      [&](const Expr &expr,
          const std::unordered_map<std::string, FieldBinding> &knownFields,
          std::unordered_set<std::string> &visitedDefs) -> std::string {
    if (expr.kind == Expr::Kind::Name) {
      auto fieldIt = knownFields.find(expr.name);
      if (fieldIt == knownFields.end()) {
        return "";
      }
      std::string fieldType = fieldIt->second.typeName;
      if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldIt->second.typeTemplateArg.empty()) {
        fieldType = fieldIt->second.typeTemplateArg;
      }
      std::string resolved = resolveStructTypePath(fieldType, expr.namespacePrefix);
      return structNames.count(resolved) > 0 ? resolved : "";
    }
    if (expr.kind != Expr::Kind::Call) {
      return "";
    }
    if (isMatchCall(expr)) {
      Expr lowered;
      std::string loweredError;
      if (!lowerMatchToIf(expr, lowered, loweredError)) {
        return "";
      }
      return inferStructReturnPathFromExpr(lowered, knownFields, visitedDefs);
    }
    if (isIfCall(expr) && expr.args.size() == 3) {
      const Expr *thenValue = ir_lowerer::getEnvelopeValueExpr(expr.args[1], true);
      const Expr *elseValue = ir_lowerer::getEnvelopeValueExpr(expr.args[2], true);
      const Expr &thenExpr = thenValue ? *thenValue : expr.args[1];
      const Expr &elseExpr = elseValue ? *elseValue : expr.args[2];
      std::string thenPath = inferStructReturnPathFromExpr(thenExpr, knownFields, visitedDefs);
      if (thenPath.empty()) {
        return "";
      }
      std::string elsePath = inferStructReturnPathFromExpr(elseExpr, knownFields, visitedDefs);
      return thenPath == elsePath ? thenPath : "";
    }
    if (const Expr *valueExpr = ir_lowerer::getEnvelopeValueExpr(expr, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferStructReturnPathFromExpr(valueExpr->args.front(), knownFields, visitedDefs);
      }
      return inferStructReturnPathFromExpr(*valueExpr, knownFields, visitedDefs);
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        return "";
      }
      std::string receiverStruct = inferStructReturnPathFromExpr(expr.args.front(), knownFields, visitedDefs);
      if (receiverStruct.empty()) {
        return "";
      }
      return inferStructReturnPathFromDefinition(receiverStruct + "/" + expr.name, visitedDefs);
    }
    std::string resolved = resolveStructLayoutExprPath(expr);
    if (structNames.count(resolved) > 0) {
      return resolved;
    }
    return inferStructReturnPathFromDefinition(resolved, visitedDefs);
  };
  inferStructReturnPathFromDefinition = [&](const std::string &defPath,
                                            std::unordered_set<std::string> &visitedDefs) -> std::string {
    if (defPath.empty()) {
      return "";
    }
    if (!visitedDefs.insert(defPath).second) {
      return "";
    }
    auto defIt = defMap.find(defPath);
    if (defIt == defMap.end() || !defIt->second) {
      return "";
    }
    const Definition &def = *defIt->second;
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string resolved = resolveStructTypePath(transform.templateArgs.front(), def.namespacePrefix);
      if (structNames.count(resolved) > 0) {
        return resolved;
      }
      break;
    }
    auto inferFromReturnValue = [&](const Expr &valueExpr) -> std::string {
      std::unordered_map<std::string, FieldBinding> noFields;
      std::string inferred = inferStructReturnPathFromExpr(valueExpr, noFields, visitedDefs);
      return inferred;
    };
    if (def.returnExpr.has_value()) {
      std::string inferred = inferFromReturnValue(*def.returnExpr);
      if (!inferred.empty()) {
        return inferred;
      }
    }
    std::string inferred;
    for (const auto &stmt : def.statements) {
      if (!isReturnCall(stmt) || stmt.args.size() != 1) {
        continue;
      }
      std::string candidate = inferFromReturnValue(stmt.args.front());
      if (candidate.empty()) {
        continue;
      }
      if (inferred.empty()) {
        inferred = std::move(candidate);
        continue;
      }
      if (candidate != inferred) {
        return "";
      }
    }
    return inferred;
  };
  auto resolveFieldBindingForLayout = [&](const Definition &def,
                                          const Expr &expr,
                                          const std::unordered_map<std::string, FieldBinding> &knownFields,
                                          FieldBinding &bindingOut) -> bool {
    if (extractExplicitBindingTypeForLayout(expr, bindingOut)) {
      return true;
    }
    const std::string fieldPath = def.fullPath + "/" + expr.name;
    if (expr.args.size() != 1) {
      error = "omitted struct field envelope requires exactly one initializer: " + fieldPath;
      return false;
    }
    if (ir_lowerer::inferPrimitiveFieldBinding(expr.args.front(), knownFields, bindingOut)) {
      return true;
    }
    std::unordered_set<std::string> visitedDefs;
    std::string inferredStruct = inferStructReturnPathFromExpr(expr.args.front(), knownFields, visitedDefs);
    if (!inferredStruct.empty()) {
      bindingOut.typeName = std::move(inferredStruct);
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    error = "unresolved or ambiguous omitted struct field envelope: " + fieldPath;
    return false;
  };
  auto formatEnvelope = [&](const FieldBinding &binding) -> std::string {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  struct StructFieldInfo {
    std::string name;
    FieldBinding binding;
    bool isStatic = false;
  };
  std::unordered_map<std::string, std::vector<StructFieldInfo>> structFieldInfoByName;
  for (const auto &def : program.definitions) {
    if (!ir_lowerer::isStructDefinition(def)) {
      continue;
    }
    std::vector<StructFieldInfo> fields;
    std::unordered_map<std::string, FieldBinding> knownFields;
    fields.reserve(def.statements.size());
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      FieldBinding binding;
      if (!resolveFieldBindingForLayout(def, stmt, knownFields, binding)) {
        return false;
      }
      StructFieldInfo field;
      field.name = stmt.name;
      field.binding = std::move(binding);
      field.isStatic = isStaticField(stmt);
      knownFields[field.name] = field.binding;
      fields.push_back(std::move(field));
    }
    structFieldInfoByName.emplace(def.fullPath, std::move(fields));
  }
  struct TypeLayout {
    uint32_t sizeBytes = 0;
    uint32_t alignmentBytes = 1;
  };
  std::unordered_map<std::string, IrStructLayout> layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::function<bool(const Definition &, IrStructLayout &)> computeStructLayout;
  std::function<bool(const FieldBinding &, const std::string &, TypeLayout &)> typeLayoutForBinding;

  computeStructLayout = [&](const Definition &def, IrStructLayout &layoutOut) -> bool {
    auto cached = layoutCache.find(def.fullPath);
    if (cached != layoutCache.end()) {
      layoutOut = cached->second;
      return true;
    }
    if (!layoutStack.insert(def.fullPath).second) {
      error = "recursive struct layout not supported: " + def.fullPath;
      return false;
    }
    IrStructLayout layout;
    layout.name = def.fullPath;
    uint32_t structAlign = 1;
    uint32_t explicitStructAlign = 1;
    bool hasStructAlign = false;
    if (!extractAlignment(
            def.transforms, "struct " + def.fullPath, explicitStructAlign, hasStructAlign, error)) {
      return false;
    }
    uint32_t offset = 0;
    auto fieldInfoIt = structFieldInfoByName.find(def.fullPath);
    if (fieldInfoIt == structFieldInfoByName.end()) {
      error = "internal error: missing struct field info for " + def.fullPath;
      return false;
    }
    size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      if (fieldIndex >= fieldInfoIt->second.size()) {
        error = "internal error: mismatched struct field info for " + def.fullPath;
        return false;
      }
      const FieldBinding &binding = fieldInfoIt->second[fieldIndex].binding;
      ++fieldIndex;
      const bool fieldIsStatic = isStaticField(stmt);
      if (fieldIsStatic) {
        IrStructField field;
        field.name = stmt.name;
        field.envelope = formatEnvelope(binding);
        field.offsetBytes = 0;
        field.sizeBytes = 0;
        field.alignmentBytes = 1;
        field.paddingKind = IrStructPaddingKind::None;
        field.category = fieldCategory(stmt);
        field.visibility = fieldVisibility(stmt);
        field.isStatic = true;
        layout.fields.push_back(std::move(field));
        continue;
      }
      TypeLayout typeLayout;
      if (!typeLayoutForBinding(binding, def.namespacePrefix, typeLayout)) {
        return false;
      }
      uint32_t explicitFieldAlign = 1;
      bool hasFieldAlign = false;
      const std::string fieldContext = "field " + def.fullPath + "/" + stmt.name;
      if (!extractAlignment(stmt.transforms, fieldContext, explicitFieldAlign, hasFieldAlign, error)) {
        return false;
      }
      if (hasFieldAlign && explicitFieldAlign < typeLayout.alignmentBytes) {
        error = "alignment requirement on " + fieldContext + " is smaller than required alignment of " +
                std::to_string(typeLayout.alignmentBytes);
        return false;
      }
      uint32_t fieldAlign = hasFieldAlign ? std::max(explicitFieldAlign, typeLayout.alignmentBytes)
                                          : typeLayout.alignmentBytes;
      uint32_t &activeOffset = offset;
      uint32_t alignedOffset = alignTo(activeOffset, fieldAlign);
      IrStructField field;
      field.name = stmt.name;
      field.envelope = formatEnvelope(binding);
      field.offsetBytes = alignedOffset;
      field.sizeBytes = typeLayout.sizeBytes;
      field.alignmentBytes = fieldAlign;
      field.paddingKind = (alignedOffset != activeOffset) ? IrStructPaddingKind::Align : IrStructPaddingKind::None;
      field.category = fieldCategory(stmt);
      field.visibility = fieldVisibility(stmt);
      field.isStatic = false;
      layout.fields.push_back(std::move(field));
      activeOffset = alignedOffset + typeLayout.sizeBytes;
      structAlign = std::max(structAlign, fieldAlign);
    }
    if (hasStructAlign && explicitStructAlign < structAlign) {
      error = "alignment requirement on struct " + def.fullPath + " is smaller than required alignment of " +
              std::to_string(structAlign);
      return false;
    }
    structAlign = hasStructAlign ? std::max(structAlign, explicitStructAlign) : structAlign;
    layout.alignmentBytes = structAlign;
    layout.totalSizeBytes = alignTo(offset, structAlign);
    layoutCache.emplace(def.fullPath, layout);
    layoutStack.erase(def.fullPath);
    layoutOut = layout;
    return true;
  };

  typeLayoutForBinding = [&](const FieldBinding &binding,
                             const std::string &namespacePrefix,
                             TypeLayout &layoutOut) -> bool {
    auto normalizeTypeName = [](const std::string &name) -> std::string {
      if (name == "int") {
        return "i32";
      }
      if (name == "float") {
        return "f32";
      }
      return name;
    };
    const std::string normalized = normalizeTypeName(binding.typeName);
    if (normalized == "i32" || normalized == "f32") {
      layoutOut = {4u, 4u};
      return true;
    }
    if (normalized == "i64" || normalized == "u64" || normalized == "f64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "bool") {
      layoutOut = {1u, 1u};
      return true;
    }
    if (normalized == "string") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "uninitialized") {
      if (binding.typeTemplateArg.empty()) {
        error = "uninitialized requires a template argument for layout";
        return false;
      }
      std::string base;
      std::string arg;
      FieldBinding unwrapped = binding;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
        unwrapped.typeName = base;
        unwrapped.typeTemplateArg = arg;
      } else {
        unwrapped.typeName = binding.typeTemplateArg;
        unwrapped.typeTemplateArg.clear();
      }
      return typeLayoutForBinding(unwrapped, namespacePrefix, layoutOut);
    }
    if (binding.typeName == "Pointer" || binding.typeName == "Reference") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "map") {
      layoutOut = {8u, 8u};
      return true;
    }
    std::string structPath = resolveStructTypePath(binding.typeName, namespacePrefix);
    auto defIt = defMap.find(structPath);
    if (defIt == defMap.end()) {
      error = "unknown struct type for layout: " + binding.typeName;
      return false;
    }
    IrStructLayout nested;
    if (!computeStructLayout(*defIt->second, nested)) {
      return false;
    }
    layoutOut.sizeBytes = nested.totalSizeBytes;
    layoutOut.alignmentBytes = nested.alignmentBytes;
    return true;
  };

  for (const auto &def : program.definitions) {
    if (!ir_lowerer::isStructDefinition(def)) {
      continue;
    }
    IrStructLayout layout;
    if (!computeStructLayout(def, layout)) {
      return false;
    }
    out.structLayouts.push_back(std::move(layout));
  }
