  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<ir_lowerer::LayoutFieldBinding>> structFieldInfoByName;
  if (!ir_lowerer::runLowerImportsStructsSetup(
          program, out, defMap, structNames, importAliases, structFieldInfoByName, error)) {
    return false;
  }
