
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
  auto resolveStructLayoutExprPath = [&](const Expr &expr) -> std::string {
    return resolveStructLayoutExprPathFromScope(expr, defMap, importAliases);
  };
  std::unordered_map<std::string, std::vector<FieldBinding>> structFieldInfoByName;
  if (!ir_lowerer::collectStructLayoutFieldBindings(program,
                                                    structNames,
                                                    resolveStructTypePath,
                                                    resolveStructLayoutExprPath,
                                                    defMap,
                                                    structFieldInfoByName,
                                                    error)) {
    return false;
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
      const FieldBinding &binding = fieldInfoIt->second[fieldIndex];
      ++fieldIndex;
      const bool fieldIsStatic = isStaticField(stmt);
      if (fieldIsStatic) {
        IrStructField field;
        field.name = stmt.name;
        field.envelope = ir_lowerer::formatLayoutFieldEnvelope(binding);
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
      field.envelope = ir_lowerer::formatLayoutFieldEnvelope(binding);
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
    ir_lowerer::BindingTypeLayout bindingLayout;
    std::string structTypeName;
    if (!ir_lowerer::classifyBindingTypeLayout(binding, bindingLayout, structTypeName, error)) {
      return false;
    }
    if (structTypeName.empty()) {
      layoutOut.sizeBytes = bindingLayout.sizeBytes;
      layoutOut.alignmentBytes = bindingLayout.alignmentBytes;
      return true;
    }
    std::string structPath = resolveStructTypePath(structTypeName, namespacePrefix);
    auto defIt = defMap.find(structPath);
    if (defIt == defMap.end()) {
      error = "unknown struct type for layout: " + structTypeName;
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
