        auto resolveDefinitionReturnBinding = [&](const Definition &def, BindingInfo &bindingOut) -> bool {
          for (const auto &transform : def.transforms) {
            if (transform.name != "return" || transform.templateArgs.size() != 1) {
              continue;
            }
            bindingOut = BindingInfo{};
            std::string base;
            std::string arg;
            const std::string &returnType = transform.templateArgs.front();
            if (splitTemplateTypeName(returnType, base, arg)) {
              bindingOut.typeName = normalizeBindingTypeName(base);
              bindingOut.typeTemplateArg = arg;
            } else {
              bindingOut.typeName = normalizeBindingTypeName(returnType);
            }
            return !bindingOut.typeName.empty();
          }
          return false;
        };

        auto resolveStructTypePath =
            [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
          if (typeName.empty()) {
            return "";
          }
          if (!typeName.empty() && typeName[0] == '/') {
            return structTypeMap.count(typeName) > 0 ? typeName : "";
          }
          std::string current = namespacePrefix;
          while (true) {
            if (!current.empty()) {
              std::string scoped = current + "/" + typeName;
              if (structTypeMap.count(scoped) > 0) {
                return scoped;
              }
              if (current.size() > typeName.size()) {
                const size_t start = current.size() - typeName.size();
                if (start > 0 && current[start - 1] == '/' &&
                    current.compare(start, typeName.size(), typeName) == 0 &&
                    structTypeMap.count(current) > 0) {
                  return current;
                }
              }
            } else {
              std::string root = "/" + typeName;
              if (structTypeMap.count(root) > 0) {
                return root;
              }
            }
            if (current.empty()) {
              break;
            }
            const size_t slash = current.find_last_of('/');
            if (slash == std::string::npos || slash == 0) {
              current.clear();
            } else {
              current.erase(slash);
            }
          }
          auto importIt = importAliases.find(typeName);
          if (importIt != importAliases.end() && structTypeMap.count(importIt->second) > 0) {
            return importIt->second;
          }
          return "";
        };

        std::function<bool(const Expr &, BindingInfo &)> resolveExprBinding;
        resolveExprBinding = [&](const Expr &candidate, BindingInfo &bindingOut) -> bool {
          if (candidate.kind == Expr::Kind::Name) {
            auto localIt = localTypes.find(candidate.name);
            if (localIt != localTypes.end()) {
              bindingOut = localIt->second;
              return true;
            }
          }

          if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "dereference") &&
              candidate.args.size() == 1) {
            BindingInfo derefSourceBinding;
            if (resolveExprBinding(candidate.args.front(), derefSourceBinding)) {
              const std::string normalizedSourceType =
                  normalizeBindingTypeName(derefSourceBinding.typeName);
              if ((normalizedSourceType == "Reference" || normalizedSourceType == "Pointer") &&
                  !derefSourceBinding.typeTemplateArg.empty()) {
                bindingOut = BindingInfo{};
                std::string base;
                std::string arg;
                if (splitTemplateTypeName(derefSourceBinding.typeTemplateArg, base, arg)) {
                  bindingOut.typeName = normalizeBindingTypeName(base);
                  bindingOut.typeTemplateArg = arg;
                } else {
                  bindingOut.typeName = normalizeBindingTypeName(derefSourceBinding.typeTemplateArg);
                }
                return !bindingOut.typeName.empty();
              }
            }
          }

          auto resolveAccessBinding = [&](const Expr &accessExpr, BindingInfo &accessBindingOut) -> bool {
            if (accessExpr.kind != Expr::Kind::Call || accessExpr.args.size() != 2 || accessExpr.name.empty()) {
              return false;
            }

            std::string normalizedName = resolveExprPath(accessExpr);
            if (!normalizedName.empty() && normalizedName.front() == '/') {
              normalizedName.erase(normalizedName.begin());
            }
            const bool isVectorAccessName =
                normalizedName == "at" || normalizedName == "at_unsafe" ||
                normalizedName == "std/collections/vector/at" ||
                normalizedName == "std/collections/vector/at_unsafe";
            const bool isMapAccessName =
                isCanonicalMapAccessHelperName(normalizedName) ||
                isCanonicalMapAccessHelperPath(normalizedName);
            if (!isVectorAccessName && !isMapAccessName) {
              return false;
            }

            size_t receiverIndex = 0;
            if (hasNamedArguments(accessExpr.argNames)) {
              for (size_t i = 0; i < accessExpr.argNames.size() && i < accessExpr.args.size(); ++i) {
                if (accessExpr.argNames[i].has_value() && *accessExpr.argNames[i] == "values") {
                  receiverIndex = i;
                  break;
                }
              }
            }

            const Expr &receiver = accessExpr.args[receiverIndex];
            std::string elementType;
            if (!inferCollectionElementTypeNameFromExpr(receiver, localTypes, {}, elementType)) {
              return false;
            }

            accessBindingOut = BindingInfo{};
            std::string base;
            std::string arg;
            if (splitTemplateTypeName(elementType, base, arg)) {
              accessBindingOut.typeName = normalizeBindingTypeName(base);
              accessBindingOut.typeTemplateArg = arg;
            } else {
              accessBindingOut.typeName = normalizeBindingTypeName(elementType);
            }
            return !accessBindingOut.typeName.empty();
          };

          auto resolveFieldAccessBinding = [&](const Expr &fieldExpr, BindingInfo &fieldBindingOut) -> bool {
            if (fieldExpr.kind != Expr::Kind::Call || !fieldExpr.isFieldAccess ||
                fieldExpr.args.size() != 1) {
              return false;
            }

            BindingInfo receiverBinding;
            if (!resolveExprBinding(fieldExpr.args.front(), receiverBinding)) {
              return false;
            }

            std::string receiverTypeName = receiverBinding.typeName;
            if ((normalizeBindingTypeName(receiverBinding.typeName) == "Reference" ||
                 normalizeBindingTypeName(receiverBinding.typeName) == "Pointer") &&
                !receiverBinding.typeTemplateArg.empty()) {
              receiverTypeName = receiverBinding.typeTemplateArg;
            }

            std::string base;
            std::string arg;
            if (splitTemplateTypeName(receiverTypeName, base, arg) &&
                normalizeBindingTypeName(base) == "uninitialized") {
              receiverTypeName = arg;
            }

            const std::string receiverStructPath =
                resolveStructTypePath(receiverTypeName, fieldExpr.args.front().namespacePrefix);
            if (receiverStructPath.empty()) {
              return false;
            }

            auto defIt = defMap.find(receiverStructPath);
            if (defIt == defMap.end() || defIt->second == nullptr) {
              return false;
            }

            for (const auto &fieldStmt : defIt->second->statements) {
              bool isStaticField = false;
              for (const auto &transform : fieldStmt.transforms) {
                if (transform.name == "static") {
                  isStaticField = true;
                  break;
                }
              }
              if (!fieldStmt.isBinding || isStaticField || fieldStmt.name != fieldExpr.name) {
                continue;
              }
              fieldBindingOut = getBindingInfo(fieldStmt);
              return true;
            }

            return false;
          };

          if (resolveFieldAccessBinding(candidate, bindingOut)) {
            return true;
          }

          if (resolveAccessBinding(candidate, bindingOut)) {
            return true;
          }

          std::string resolvedPath;
          if (candidate.kind == Expr::Kind::Call) {
            if (candidate.isMethodCall) {
              if (!resolveMethodCallPath(candidate,
                                         defMap,
                                         localTypes,
                                         importAliases,
                                         structTypeMap,
                                         returnKinds,
                                         returnStructs,
                                         resolvedPath)) {
                resolvedPath.clear();
              }
            } else {
              resolvedPath = resolveExprPath(candidate);
            }
          } else if (candidate.kind == Expr::Kind::Name) {
            resolvedPath = resolveExprPath(candidate);
          }

          auto tryResolveDefinitionBinding = [&](const std::string &candidatePath) -> bool {
            if (candidatePath.empty()) {
              return false;
            }
            auto defIt = defMap.find(candidatePath);
            if (defIt == defMap.end() || defIt->second == nullptr) {
              return false;
            }
            return resolveDefinitionReturnBinding(*defIt->second, bindingOut);
          };

          if (tryResolveDefinitionBinding(resolvedPath)) {
            return true;
          }

          if (!candidate.name.empty()) {
            auto importIt = importAliases.find(candidate.name);
            if (importIt != importAliases.end() && importIt->second != resolvedPath) {
              return tryResolveDefinitionBinding(importIt->second);
            }
          }

          return false;
        };

        auto emitPackedArgExpr = [&](const Expr &packedArgExpr) -> std::string {
          std::function<std::string(const Expr &)> emitPackedLocationLValue =
              [&](const Expr &locationTarget) -> std::string {
            if (locationTarget.kind == Expr::Kind::Call && locationTarget.isFieldAccess &&
                locationTarget.args.size() == 1) {
              const Expr &receiver = locationTarget.args.front();
              std::string receiverText;
              if (receiver.kind == Expr::Kind::Call && receiver.isFieldAccess &&
                  receiver.args.size() == 1) {
                receiverText = emitPackedLocationLValue(receiver);
              } else {
                receiverText =
                    emitExpr(receiver,
                             nameMap,
                             paramMap,
                             defMap,
                             structTypeMap,
                             importAliases,
                             localTypes,
                             returnKinds,
                             resultInfos,
                             returnStructs,
                             allowMathBare);
              }
              BindingInfo receiverBinding;
              if (resolveExprBinding(receiver, receiverBinding)) {
                const std::string normalizedReceiverType =
                    normalizeBindingTypeName(receiverBinding.typeName);
                if (normalizedReceiverType == "Reference" || normalizedReceiverType == "Pointer") {
                  receiverText = "ps_deref(" + receiverText + ")";
                }
              }
              return receiverText + "." + locationTarget.name;
            }

            return emitExpr(locationTarget,
                            nameMap,
                            paramMap,
                            defMap,
                            structTypeMap,
                            importAliases,
                            localTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            allowMathBare);
          };

          std::string packBase;
          std::string packArgText;
          if (splitTemplateTypeName(packedParamInfo.typeTemplateArg, packBase, packArgText) &&
              isSimpleCallName(packedArgExpr, "location") && packedArgExpr.args.size() == 1) {
            const std::string normalizedPackBase = normalizeBindingTypeName(packBase);
            const Expr &locationTarget = packedArgExpr.args.front();
            const std::string targetExpr = emitPackedLocationLValue(locationTarget);
            if (normalizedPackBase == "Reference") {
              BindingInfo locationBinding;
              if (resolveExprBinding(locationTarget, locationBinding) &&
                  normalizeBindingTypeName(locationBinding.typeName) == "Reference") {
                return "std::ref(ps_deref(" + targetExpr + "))";
              }
              return "std::ref(" + targetExpr + ")";
            }
            if (normalizedPackBase == "Pointer" && locationTarget.kind == Expr::Kind::Call &&
                locationTarget.isFieldAccess && locationTarget.args.size() == 1) {
              BindingInfo locationBinding;
              if (resolveExprBinding(locationTarget, locationBinding) &&
                  normalizeBindingTypeName(locationBinding.typeName) == "Reference") {
                return "(&ps_deref(" + targetExpr + "))";
              }
              return "(&(" + targetExpr + "))";
            }
            if (normalizedPackBase == "Pointer") {
              BindingInfo locationBinding;
              if (resolveExprBinding(locationTarget, locationBinding) &&
                  normalizeBindingTypeName(locationBinding.typeName) == "Reference") {
                return "(&ps_deref(" + targetExpr + "))";
              }
            }
          }
          return emitExpr(packedArgExpr,
                          nameMap,
                          paramMap,
                          defMap,
                          structTypeMap,
                          importAliases,
                          localTypes,
                          returnKinds,
                          resultInfos,
                          returnStructs,
                          allowMathBare);
        };

        std::ostringstream packOut;
        if (packedArgs.empty()) {
          packOut << "std::vector<" << packElementType << ">{}";
        } else {
          const Expr *spreadArg = packedArgs.back()->isSpread ? packedArgs.back() : nullptr;
          const size_t explicitCount = spreadArg ? packedArgs.size() - 1 : packedArgs.size();
          if (spreadArg != nullptr && explicitCount == 0) {
            packOut << emitExpr(*spreadArg,
                                nameMap,
                                paramMap,
                                defMap,
                                structTypeMap,
                                importAliases,
                                localTypes,
                                returnKinds,
                                resultInfos,
                                returnStructs,
                                allowMathBare);
          } else {
            if (spreadArg != nullptr) {
              packOut << "ps_args_concat(";
            }
            packOut << "std::vector<" << packElementType << ">{";
            for (size_t i = 0; i < explicitCount; ++i) {
              if (i > 0) {
                packOut << ", ";
              }
              packOut << emitPackedArgExpr(*packedArgs[i]);
            }
            packOut << "}";
            if (spreadArg != nullptr) {
              packOut << ", "
                      << emitExpr(*spreadArg,
                                  nameMap,
                                  paramMap,
                                  defMap,
                                  structTypeMap,
                                  importAliases,
                                  localTypes,
                                  returnKinds,
                                  resultInfos,
                                  returnStructs,
                                  allowMathBare)
                      << ")";
            }
          }
        }
        emitRenderedArg(packOut.str(), needComma);
