
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
      return ir_lowerer::computeStructLayoutFromFieldInfo(
          def, structFieldInfoByName, resolveStructTypePath, defMap, computeStructLayout, layout, layoutError);
    };
    return ir_lowerer::computeStructLayoutWithCache(
        def.fullPath, layoutCache, layoutStack, computeUncachedLayout, layoutOut, error);
  };

  if (!ir_lowerer::appendProgramStructLayouts(program, computeStructLayout, out.structLayouts, error)) {
    return false;
  }
