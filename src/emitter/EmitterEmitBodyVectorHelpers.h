        std::string vectorHelper;
        if (getVectorMutatorName(stmt, nameMap, vectorHelper)) {
          const std::string explicitRequestedVectorHelperPath =
              (!stmt.isMethodCall &&
               (resolveExprPath(stmt).rfind("/std/collections/vector/", 0) == 0))
                  ? resolveExprPath(stmt)
                  : std::string{};
          std::string helperPath;
          bool hasUserVectorHelper = false;
          Expr helperCall = stmt;
          if (stmt.isMethodCall) {
            if (resolveMethodCallPath(
                    stmt, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, helperPath)) {
              if (!stmt.name.empty() && stmt.name.front() == '/' && !explicitRequestedVectorHelperPath.empty() &&
                  helperPath != explicitRequestedVectorHelperPath) {
                helperPath.clear();
              } else {
                hasUserVectorHelper = nameMap.find(helperPath) != nameMap.end();
              }
            }
            if (hasUserVectorHelper) {
              helperCall.isMethodCall = true;
            }
          } else {
            std::vector<size_t> receiverIndices;
            auto appendReceiverIndex = [&](size_t index) {
              if (index >= stmt.args.size()) {
                return;
              }
              for (size_t existing : receiverIndices) {
                if (existing == index) {
                  return;
                }
              }
              receiverIndices.push_back(index);
            };
            const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
            if (hasNamedArgs) {
              bool hasValuesNamedReceiver = false;
              for (size_t i = 0; i < stmt.args.size(); ++i) {
                if (i < stmt.argNames.size() && stmt.argNames[i].has_value() &&
                    *stmt.argNames[i] == "values") {
                  appendReceiverIndex(i);
                  hasValuesNamedReceiver = true;
                }
              }
              if (!hasValuesNamedReceiver) {
                appendReceiverIndex(0);
                for (size_t i = 1; i < stmt.args.size(); ++i) {
                  appendReceiverIndex(i);
                }
              }
            } else {
              appendReceiverIndex(0);
            }
            const bool probePositionalReorderedReceiver =
                !hasNamedArgs && stmt.args.size() > 1 &&
                (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
                 stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
                 (stmt.args.front().kind == Expr::Kind::Name && !isVectorValue(stmt.args.front(), localTypes)));
            if (probePositionalReorderedReceiver) {
              for (size_t i = 1; i < stmt.args.size(); ++i) {
                appendReceiverIndex(i);
              }
            }
            for (size_t receiverIndex : receiverIndices) {
              Expr methodCandidate = stmt;
              methodCandidate.isMethodCall = true;
              methodCandidate.name = vectorHelper;
              if (receiverIndex != 0 && receiverIndex < methodCandidate.args.size()) {
                std::swap(methodCandidate.args[0], methodCandidate.args[receiverIndex]);
                if (methodCandidate.argNames.size() < methodCandidate.args.size()) {
                  methodCandidate.argNames.resize(methodCandidate.args.size());
                }
                std::swap(methodCandidate.argNames[0], methodCandidate.argNames[receiverIndex]);
              }
              const bool resolvedMethodPath = resolveMethodCallPath(
                  methodCandidate,
                  defMap,
                  localTypes,
                  importAliases,
                  structTypeMap,
                  returnKinds,
                  returnStructs,
                  helperPath);
              if (resolvedMethodPath) {
                if (!explicitRequestedVectorHelperPath.empty() &&
                    helperPath != explicitRequestedVectorHelperPath) {
                  helperPath.clear();
                }
              }
              if (resolvedMethodPath && !helperPath.empty() && nameMap.find(helperPath) != nameMap.end()) {
                hasUserVectorHelper = true;
                helperCall = std::move(methodCandidate);
                break;
              }
            }
          }
          if (hasUserVectorHelper) {
            out << pad
                << emitExpr(helperCall,
                            nameMap,
                            paramMap,
                            defMap,
                            structTypeMap,
                            importAliases,
                            localTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            hasMathImport)
                << ";\n";
            return;
          }
          if (vectorHelper == "push") {
            out << pad << "ps_vector_push("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "pop") {
            out << pad << "ps_vector_pop("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "reserve") {
            out << pad << "ps_vector_reserve("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "clear") {
            out << pad << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ".clear();\n";
            return;
          }
          if (vectorHelper == "remove_at") {
            out << pad << "ps_vector_remove_at("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ");\n";
            return;
          }
          if (vectorHelper == "remove_swap") {
            out << pad << "ps_vector_remove_swap("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, hasMathImport)
                << ");\n";
            return;
          }
        }
