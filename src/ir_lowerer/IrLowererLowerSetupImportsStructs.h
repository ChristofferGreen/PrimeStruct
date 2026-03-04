
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_set<std::string> structNames;
  ir_lowerer::buildDefinitionMapAndStructNames(program.definitions, defMap, structNames);

  std::unordered_map<std::string, std::string> importAliases =
      buildImportAliasesFromProgram(program.imports, program.definitions, defMap);

  auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    return resolveStructTypePathCandidateFromScope(typeName, namespacePrefix, structNames, importAliases);
  };
  std::unordered_map<std::string, std::vector<ir_lowerer::LayoutFieldBinding>> structFieldInfoByName;
  if (!ir_lowerer::collectStructLayoutFieldBindingsFromProgramContext(
          program, structNames, resolveStructTypePath, defMap, importAliases, structFieldInfoByName, error)) {
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
