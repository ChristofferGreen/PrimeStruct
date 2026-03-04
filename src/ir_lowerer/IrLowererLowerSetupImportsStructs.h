
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
  std::unordered_map<std::string, IrStructLayout> layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::function<bool(const Definition &, IrStructLayout &)> computeStructLayout;

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
      auto computeNestedStructLayout = [&](const Definition &nestedDef,
                                           IrStructLayout &nestedLayout,
                                           std::string &layoutError) -> bool {
        (void)layoutError;
        return computeStructLayout(nestedDef, nestedLayout);
      };
      auto resolveFieldTypeLayout = [&](const FieldBinding &fieldBinding,
                                        ir_lowerer::BindingTypeLayout &fieldTypeLayout,
                                        std::string &layoutError) -> bool {
        return ir_lowerer::resolveBindingTypeLayout(fieldBinding,
                                                    def.namespacePrefix,
                                                    resolveStructTypePath,
                                                    defMap,
                                                    computeNestedStructLayout,
                                                    fieldTypeLayout,
                                                    layoutError);
      };
      if (!ir_lowerer::appendStructLayoutField(
              def.fullPath, stmt, binding, resolveFieldTypeLayout, offset, structAlign, layout, error)) {
        return false;
      }
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
