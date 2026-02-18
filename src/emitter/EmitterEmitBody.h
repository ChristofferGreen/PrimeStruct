  auto pickCreateHelper = [&](const std::string &structPath) -> const Definition * {
    auto it = helpersByStruct.find(structPath);
    if (it == helpersByStruct.end()) {
      return nullptr;
    }
    if (it->second.createStack) {
      return it->second.createStack;
    }
    return it->second.create;
  };
  auto pickDestroyHelper = [&](const std::string &structPath) -> const Definition * {
    auto it = helpersByStruct.find(structPath);
    if (it == helpersByStruct.end()) {
      return nullptr;
    }
    if (it->second.destroyStack) {
      return it->second.destroyStack;
    }
    return it->second.destroy;
  };
  for (const auto &def : program.definitions) {
    if (!isStructDefinition(def)) {
      continue;
    }
    out << "struct " << structTypeMap.at(def.fullPath) << ";\n";
  }
  for (const auto &def : program.definitions) {
    std::string parentPath;
    if (!isLifecycleHelper(def, parentPath)) {
      continue;
    }
    out << "static " << returnTypeFor(def) << " " << nameMap[def.fullPath] << "(";
    const auto &params = paramMap[def.fullPath];
    for (size_t i = 0; i < params.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      appendParam(out, params[i], def.namespacePrefix);
    }
    out << ");\n";
  }
  for (const auto &def : program.definitions) {
    if (!isStructDefinition(def)) {
      continue;
    }
    const std::string &structType = structTypeMap.at(def.fullPath);
    out << "struct " << structType << " {\n";
    std::unordered_map<std::string, BindingInfo> emptyLocals;
    for (const auto &field : def.statements) {
      if (!field.isBinding) {
        continue;
      }
      BindingInfo binding = getBindingInfo(field);
      std::string fieldType =
          bindingTypeToCpp(binding, def.namespacePrefix, importAliases, structTypeMap);
      if (binding.isStatic) {
        out << "  static inline " << fieldType << " " << field.name;
        if (!field.args.empty()) {
          out << " = "
              << emitExpr(field.args.front(),
                          nameMap,
                          paramMap,
                          structTypeMap,
                          importAliases,
                          emptyLocals,
                          returnKinds,
                          hasMathImport);
        }
        out << ";\n";
      } else {
        out << "  " << fieldType << " " << field.name << ";\n";
      }
    }
    const Definition *destroyHelper = pickDestroyHelper(def.fullPath);
    if (destroyHelper) {
      out << "  ~" << structType << "() {\n";
      out << "    " << nameMap[destroyHelper->fullPath] << "(*this);\n";
      out << "  }\n";
    }
    out << "};\n";

    out << "static inline " << structType << " " << nameMap[def.fullPath] << "(";
    const auto &fields = paramMap[def.fullPath];
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      appendParam(out, fields[i], def.namespacePrefix);
    }
    out << ") {\n";
    out << "  " << structType << " __instance{";
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << fields[i].name;
    }
    out << "};\n";
    const Definition *createHelper = pickCreateHelper(def.fullPath);
    if (createHelper) {
      out << "  " << nameMap[createHelper->fullPath] << "(__instance);\n";
    }
    out << "  return __instance;\n";
    out << "}\n";
  }
  for (const auto &def : program.definitions) {
    if (isStructDefinition(def)) {
      continue;
    }
    ReturnKind returnKind = returnKinds[def.fullPath];
    std::string returnType = returnTypeFor(def);
    const auto &params = paramMap[def.fullPath];
    const bool structDef = isStructDefinition(def);
    out << "static " << returnType << " " << nameMap[def.fullPath] << "(";
    for (size_t i = 0; i < params.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      appendParam(out, params[i], def.namespacePrefix);
    }
    out << ") {\n";
    std::function<void(const Expr &, int, std::unordered_map<std::string, BindingInfo> &)> emitStatement;
    int repeatCounter = 0;
    emitStatement = [&](const Expr &stmt, int indent, std::unordered_map<std::string, BindingInfo> &localTypes) {
      std::string pad(static_cast<size_t>(indent) * 2, ' ');
      if (isReturnCall(stmt)) {
        out << pad << "return";
        if (!stmt.args.empty()) {
          out << " " << emitExpr(stmt.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport);
        }
        out << ";\n";
        return;
      }
      if (!stmt.isBinding && stmt.kind == Expr::Kind::Call) {
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(stmt, printBuiltin)) {
          const char *stream = (printBuiltin.target == PrintTarget::Err) ? "stderr" : "stdout";
          std::string argText = "\"\"";
          if (!stmt.args.empty()) {
            argText = emitExpr(stmt.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport);
          }
          out << pad << "ps_print_value(" << stream << ", " << argText << ", "
              << (printBuiltin.newline ? "true" : "false") << ");\n";
          return;
        }
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
        const bool lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
        if (!hasExplicitType && stmt.args.size() == 1 && !lambdaInit) {
          std::unordered_map<std::string, ReturnKind> locals;
          locals.reserve(localTypes.size());
          for (const auto &entry : localTypes) {
            locals.emplace(entry.first, returnKindForTypeName(entry.second.typeName));
          }
          ReturnKind initKind = inferExprReturnKind(stmt.args.front(), params, locals);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        const std::string typeNamespace = stmt.namespacePrefix.empty() ? def.namespacePrefix : stmt.namespacePrefix;
        localTypes[stmt.name] = binding;
        const bool useAuto = !hasExplicitType && lambdaInit;
        bool isReference = binding.typeName == "Reference";
        if (useAuto) {
          out << pad << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
        } else {
          std::string type = bindingTypeToCpp(binding, typeNamespace, importAliases, structTypeMap);
          const bool useRef =
              !binding.isMutable && !binding.isCopy && isReferenceCandidate(binding) && !stmt.args.empty();
          if (useRef) {
            if (type.rfind("const ", 0) != 0) {
              out << pad << "const " << type << " & " << stmt.name;
            } else {
              out << pad << type << " & " << stmt.name;
            }
          } else {
            bool needsConst = !binding.isMutable;
            if (needsConst && type.rfind("const ", 0) == 0) {
              needsConst = false;
            }
            out << pad << (needsConst ? "const " : "") << type << " " << stmt.name;
          }
        }
        if (!stmt.args.empty()) {
          if (!useAuto && isReference) {
            out << " = *(" << emitExpr(stmt.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport) << ")";
          } else {
            out << " = " << emitExpr(stmt.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport);
          }
        }
        out << ";\n";
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &cond = stmt.args[0];
        const Expr &thenArg = stmt.args[1];
        const Expr &elseArg = stmt.args[2];
        auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
          if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
            return false;
          }
          if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
            return false;
          }
          if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
            return false;
          }
          return true;
        };
        out << pad << "if (" << emitExpr(cond, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport) << ") {\n";
        {
          auto blockTypes = localTypes;
          if (isIfBlockEnvelope(thenArg)) {
            for (const auto &bodyStmt : thenArg.bodyArguments) {
              emitStatement(bodyStmt, indent + 1, blockTypes);
            }
          } else {
            emitStatement(thenArg, indent + 1, blockTypes);
          }
        }
        out << pad << "} else {\n";
        {
          auto blockTypes = localTypes;
          if (isIfBlockEnvelope(elseArg)) {
            for (const auto &bodyStmt : elseArg.bodyArguments) {
              emitStatement(bodyStmt, indent + 1, blockTypes);
            }
          } else {
            emitStatement(elseArg, indent + 1, blockTypes);
          }
        }
        out << pad << "}\n";
        return;
      }
      auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
          return false;
        }
        if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
          return false;
        }
        return true;
      };
      if (stmt.kind == Expr::Kind::Call && isLoopCall(stmt) && stmt.args.size() == 2) {
        const int repeatIndex = repeatCounter++;
        const std::string endVar = "ps_loop_end_" + std::to_string(repeatIndex);
        const std::string indexVar = "ps_loop_i_" + std::to_string(repeatIndex);
        out << pad << "{\n";
        std::string innerPad(static_cast<size_t>(indent + 1) * 2, ' ');
        std::string countExpr =
            emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport);
        std::unordered_map<std::string, ReturnKind> locals;
        locals.reserve(localTypes.size());
        for (const auto &entry : localTypes) {
          locals.emplace(entry.first, returnKindForTypeName(entry.second.typeName));
        }
        ReturnKind countKind = inferExprReturnKind(stmt.args[0], params, locals);
        out << innerPad << "auto " << endVar << " = " << countExpr << ";\n";
        const std::string indexType = (countKind == ReturnKind::Bool) ? "int" : ("decltype(" + endVar + ")");
        out << innerPad << "for (" << indexType << " " << indexVar << " = 0; " << indexVar << " < " << endVar
            << "; ++" << indexVar << ") {\n";
        if (isLoopBlockEnvelope(stmt.args[1])) {
          auto blockTypes = localTypes;
          for (const auto &bodyStmt : stmt.args[1].bodyArguments) {
            emitStatement(bodyStmt, indent + 2, blockTypes);
          }
        }
        out << innerPad << "}\n";
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isWhileCall(stmt) && stmt.args.size() == 2 && isLoopBlockEnvelope(stmt.args[1])) {
        out << pad << "while ("
            << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport) << ") {\n";
        auto blockTypes = localTypes;
        for (const auto &bodyStmt : stmt.args[1].bodyArguments) {
          emitStatement(bodyStmt, indent + 1, blockTypes);
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isForCall(stmt) && stmt.args.size() == 4 && isLoopBlockEnvelope(stmt.args[3])) {
        out << pad << "{\n";
        auto loopTypes = localTypes;
        emitStatement(stmt.args[0], indent + 1, loopTypes);
        std::string innerPad(static_cast<size_t>(indent + 1) * 2, ' ');
        const Expr &cond = stmt.args[1];
        const Expr &step = stmt.args[2];
        const Expr &body = stmt.args[3];
        if (cond.isBinding) {
          out << innerPad << "while (true) {\n";
          auto blockTypes = loopTypes;
          emitStatement(cond, indent + 2, blockTypes);
          out << innerPad << "  if (!" << cond.name << ") { break; }\n";
          for (const auto &bodyStmt : body.bodyArguments) {
            emitStatement(bodyStmt, indent + 2, blockTypes);
          }
          emitStatement(step, indent + 2, blockTypes);
          out << innerPad << "}\n";
        } else {
          out << innerPad << "while ("
              << emitExpr(cond, nameMap, paramMap, structTypeMap, importAliases, loopTypes, returnKinds, hasMathImport) << ") {\n";
          auto blockTypes = loopTypes;
          for (const auto &bodyStmt : body.bodyArguments) {
            emitStatement(bodyStmt, indent + 2, blockTypes);
          }
          emitStatement(step, indent + 2, loopTypes);
          out << innerPad << "}\n";
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isRepeatCall(stmt)) {
        const int repeatIndex = repeatCounter++;
        const std::string endVar = "ps_repeat_end_" + std::to_string(repeatIndex);
        const std::string indexVar = "ps_repeat_i_" + std::to_string(repeatIndex);
        out << pad << "{\n";
        std::string innerPad(static_cast<size_t>(indent + 1) * 2, ' ');
        std::string countExpr = "\"\"";
        ReturnKind countKind = ReturnKind::Unknown;
        if (!stmt.args.empty()) {
          countExpr = emitExpr(stmt.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport);
          std::unordered_map<std::string, ReturnKind> locals;
          locals.reserve(localTypes.size());
          for (const auto &entry : localTypes) {
            locals.emplace(entry.first, returnKindForTypeName(entry.second.typeName));
          }
          countKind = inferExprReturnKind(stmt.args.front(), params, locals);
        }
        out << innerPad << "auto " << endVar << " = " << countExpr << ";\n";
        const std::string indexType = (countKind == ReturnKind::Bool) ? "int" : ("decltype(" + endVar + ")");
        out << innerPad << "for (" << indexType << " " << indexVar << " = 0; " << indexVar << " < " << endVar
            << "; ++" << indexVar << ") {\n";
        auto blockTypes = localTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 2, blockTypes);
        }
        out << innerPad << "}\n";
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinBlock(stmt, nameMap) && (stmt.hasBodyArguments || !stmt.bodyArguments.empty())) {
        out << pad << "{\n";
        auto blockTypes = localTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 1, blockTypes);
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && (stmt.hasBodyArguments || !stmt.bodyArguments.empty())) {
        Expr callExpr = stmt;
        callExpr.bodyArguments.clear();
        callExpr.hasBodyArguments = false;
        out << pad
            << emitExpr(callExpr, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
            << ";\n";
        out << pad << "{\n";
        auto blockTypes = localTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 1, blockTypes);
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinAssign(stmt, nameMap) && stmt.args.size() == 2 &&
          stmt.args.front().kind == Expr::Kind::Name) {
        out << pad << stmt.args.front().name << " = "
            << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport) << ";\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isPathSpaceBuiltinName(stmt) && nameMap.find(resolveExprPath(stmt)) == nameMap.end()) {
        for (const auto &arg : stmt.args) {
          out << pad << "(void)(" << emitExpr(arg, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport) << ");\n";
        }
        return;
      }
      if (stmt.kind == Expr::Kind::Call) {
        std::string vectorHelper;
        if (getVectorMutatorName(stmt, nameMap, vectorHelper)) {
          if (vectorHelper == "push") {
            out << pad << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ".push_back("
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "pop") {
            out << pad << "ps_vector_pop("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "reserve") {
            out << pad << "ps_vector_reserve("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "clear") {
            out << pad << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ".clear();\n";
            return;
          }
          if (vectorHelper == "remove_at") {
            out << pad << "ps_vector_remove_at("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "remove_swap") {
            out << pad << "ps_vector_remove_swap("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport)
                << ");\n";
            return;
          }
        }
      }
      out << pad << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport) << ";\n";
    };
    std::unordered_map<std::string, BindingInfo> localTypes;
    for (const auto &param : params) {
      localTypes[param.name] = getBindingInfo(param);
    }
    if (!structDef) {
      for (const auto &stmt : def.statements) {
        emitStatement(stmt, 1, localTypes);
      }
    }
    if (returnKind == ReturnKind::Void) {
      out << "  return;\n";
    }
    out << "}\n";
  }
  ReturnKind entryReturn = ReturnKind::Int;
  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryReturn = returnKinds[def.fullPath];
      entryDef = &def;
      break;
    }
  }
  bool entryHasArgs = false;
  if (entryDef && entryDef->parameters.size() == 1) {
    BindingInfo paramInfo = getBindingInfo(entryDef->parameters.front());
    if (paramInfo.typeName == "array" && paramInfo.typeTemplateArg == "string") {
      entryHasArgs = true;
    }
  }
  if (entryHasArgs) {
    out << "int main(int argc, char **argv) {\n";
    out << "  std::vector<std::string_view> ps_args;\n";
    out << "  ps_args.reserve(static_cast<size_t>(argc));\n";
    out << "  for (int i = 0; i < argc; ++i) {\n";
    out << "    ps_args.emplace_back(argv[i]);\n";
    out << "  }\n";
    if (entryReturn == ReturnKind::Void) {
      out << "  " << nameMap.at(entryPath) << "(ps_args);\n";
      out << "  return 0;\n";
    } else if (entryReturn == ReturnKind::Int) {
      out << "  return " << nameMap.at(entryPath) << "(ps_args);\n";
    } else {
      out << "  return static_cast<int>(" << nameMap.at(entryPath) << "(ps_args));\n";
    }
    out << "}\n";
  } else {
    if (entryReturn == ReturnKind::Void) {
      out << "int main() { " << nameMap.at(entryPath) << "(); return 0; }\n";
    } else if (entryReturn == ReturnKind::Int) {
      out << "int main() { return " << nameMap.at(entryPath) << "(); }\n";
    } else if (entryReturn == ReturnKind::Int64 || entryReturn == ReturnKind::UInt64) {
      out << "int main() { return static_cast<int>(" << nameMap.at(entryPath) << "()); }\n";
    } else {
      out << "int main() { return static_cast<int>(" << nameMap.at(entryPath) << "()); }\n";
    }
  }
  return out.str();
}
