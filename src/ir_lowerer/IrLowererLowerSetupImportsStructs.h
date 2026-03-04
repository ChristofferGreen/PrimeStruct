
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
    auto computeUncachedLayout = [&](IrStructLayout &layout, std::string &layoutError) -> bool {
      auto fieldInfoIt = structFieldInfoByName.find(def.fullPath);
      if (fieldInfoIt == structFieldInfoByName.end()) {
        layoutError = "internal error: missing struct field info for " + def.fullPath;
        return false;
      }
      auto computeNestedStructLayout = [&](const Definition &nestedDef,
                                           IrStructLayout &nestedLayout,
                                           std::string &nestedError) -> bool {
        (void)nestedError;
        return computeStructLayout(nestedDef, nestedLayout);
      };
      auto resolveFieldTypeLayout = [&](const FieldBinding &fieldBinding,
                                        ir_lowerer::BindingTypeLayout &fieldTypeLayout,
                                        std::string &fieldError) -> bool {
        return ir_lowerer::resolveBindingTypeLayout(fieldBinding,
                                                    def.namespacePrefix,
                                                    resolveStructTypePath,
                                                    defMap,
                                                    computeNestedStructLayout,
                                                    fieldTypeLayout,
                                                    fieldError);
      };
      return ir_lowerer::computeStructLayoutUncached(
          def, fieldInfoIt->second, resolveFieldTypeLayout, layout, layoutError);
    };
    return ir_lowerer::computeStructLayoutWithCache(
        def.fullPath, layoutCache, layoutStack, computeUncachedLayout, layoutOut, error);
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
