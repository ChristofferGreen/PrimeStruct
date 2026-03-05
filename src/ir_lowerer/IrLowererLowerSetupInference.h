
  std::unordered_map<std::string, ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferArrayElementKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferBufferElementKind;

  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  resolveMethodCallDefinition = [&](const Expr &callExpr, const LocalMap &localsIn) -> const Definition * {
    return resolveMethodCallDefinitionFromExpr(callExpr,
                                               localsIn,
                                               isArrayCountCall,
                                               isVectorCapacityCall,
                                               isEntryArgsName,
                                               importAliases,
                                               structNames,
                                               inferExprKind,
                                               resolveExprPath,
                                               defMap,
                                               error);
  };

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
  inferPointerTargetKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferPointerTargetValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, std::string &builtinName) {
          return getBuiltinOperatorName(candidate, builtinName);
        });
  };

  inferBufferElementKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferBufferElementValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, const LocalMap &candidateLocals) {
          return inferArrayElementKind(candidate, candidateLocals);
        });
  };

  inferArrayElementKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferArrayElementValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, const LocalMap &candidateLocals) {
          return inferBufferElementKind(candidate, candidateLocals);
        },
        [&](const Expr &candidate) { return resolveExprPath(candidate); },
        [&](const std::string &structPath, LocalInfo::ValueKind &kindOut) {
          StructArrayInfo structInfo;
          if (!resolveStructArrayInfoFromPath(structPath, structInfo)) {
            return false;
          }
          kindOut = structInfo.elementKind;
          return true;
        },
        [&](const Expr &candidate, const LocalMap &, LocalInfo::ValueKind &kindOut) {
          return resolveDefinitionCallReturnKind(candidate, defMap, resolveExprPath, getReturnInfo, true, kindOut);
        },
        [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
          return resolveCountMethodCallReturnKind(candidate,
                                                  candidateLocals,
                                                  isArrayCountCall,
                                                  isStringCountCall,
                                                  resolveMethodCallDefinition,
                                                  getReturnInfo,
                                                  true,
                                                  kindOut);
        },
        [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
          return resolveMethodCallReturnKind(
              candidate, candidateLocals, resolveMethodCallDefinition, getReturnInfo, true, kindOut);
        });
  };

  inferExprKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    switch (expr.kind) {
      case Expr::Kind::Literal:
        if (expr.isUnsigned) {
          return LocalInfo::ValueKind::UInt64;
        }
        if (expr.intWidth == 64) {
          return LocalInfo::ValueKind::Int64;
        }
        return LocalInfo::ValueKind::Int32;
      case Expr::Kind::FloatLiteral:
        return (expr.floatWidth == 64) ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      case Expr::Kind::BoolLiteral:
        return LocalInfo::ValueKind::Bool;
      case Expr::Kind::StringLiteral:
        return LocalInfo::ValueKind::String;
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it == localsIn.end()) {
          std::string mathConst;
          if (getMathConstantName(expr.name, mathConst)) {
            return LocalInfo::ValueKind::Float64;
          }
          return LocalInfo::ValueKind::Unknown;
        }
        if (it->second.kind == LocalInfo::Kind::Value) {
          return it->second.valueKind;
        }
        if (it->second.kind == LocalInfo::Kind::Reference) {
          if (it->second.referenceToArray) {
            return LocalInfo::ValueKind::Unknown;
          }
          return it->second.valueKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      case Expr::Kind::Call: {
        if (expr.isFieldAccess) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &receiver = expr.args.front();
          std::string structPath = inferStructExprPath(receiver, localsIn);
          if (structPath.empty()) {
            return LocalInfo::ValueKind::Unknown;
          }
          StructSlotFieldInfo fieldInfo;
          if (!resolveStructFieldSlot(structPath, expr.name, fieldInfo)) {
            return LocalInfo::ValueKind::Unknown;
          }
          return fieldInfo.structPath.empty() ? fieldInfo.valueKind : LocalInfo::ValueKind::Unknown;
        }
        if (!expr.isMethodCall &&
            (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
            expr.args.size() == 1) {
          UninitializedStorageAccess access;
          bool resolved = false;
          if (!resolveUninitializedStorage(expr.args.front(), localsIn, access, resolved)) {
            return LocalInfo::ValueKind::Unknown;
          }
          if (resolved) {
            if (access.typeInfo.kind == LocalInfo::Kind::Value &&
                access.typeInfo.structPath.empty() &&
                access.typeInfo.valueKind != LocalInfo::ValueKind::Unknown) {
              return access.typeInfo.valueKind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
        }
        if (expr.isMethodCall) {
          if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
              expr.args.front().name == "Result") {
            if (expr.name == "ok") {
              return expr.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
            }
            if (expr.name == "why") {
              return LocalInfo::ValueKind::String;
            }
          }
          if (!expr.args.empty() && expr.name == "why") {
            const Expr &receiver = expr.args.front();
            if (receiver.kind == Expr::Kind::Name) {
              if (receiver.name == "FileError") {
                return LocalInfo::ValueKind::String;
              }
              auto it = localsIn.find(receiver.name);
              if (it != localsIn.end() && it->second.isFileError) {
                return LocalInfo::ValueKind::String;
              }
            }
          }
          if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
            auto it = localsIn.find(expr.args.front().name);
            if (it != localsIn.end() && it->second.isFileHandle) {
              if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" ||
                  expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
                return LocalInfo::ValueKind::Int32;
              }
            }
          }
        } else {
          if (isSimpleCallName(expr, "File")) {
            return LocalInfo::ValueKind::Int64;
          }
          if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
            const Expr &arg = expr.args.front();
            if (arg.kind == Expr::Kind::Name) {
              auto it = localsIn.find(arg.name);
              if (it != localsIn.end() && it->second.isResult) {
                return it->second.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
              }
            }
            if (arg.kind == Expr::Kind::Call) {
              if (!arg.isMethodCall && isSimpleCallName(arg, "File")) {
                return LocalInfo::ValueKind::Int64;
              }
              if (arg.isMethodCall && !arg.args.empty() && arg.args.front().kind == Expr::Kind::Name) {
                auto it = localsIn.find(arg.args.front().name);
                if (it != localsIn.end() && it->second.isFileHandle) {
                  if (arg.name == "write" || arg.name == "write_line" || arg.name == "write_byte" ||
                      arg.name == "write_bytes" || arg.name == "flush" || arg.name == "close") {
                    return LocalInfo::ValueKind::Int32;
                  }
                }
                if (arg.args.front().name == "Result" && arg.name == "ok") {
                  return arg.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
                }
              }
            }
          }
        }
        LocalInfo::ValueKind callReturnKind = LocalInfo::ValueKind::Unknown;
        const auto callReturnResolution = resolveCallExpressionReturnKind(
            expr,
            localsIn,
            [&](const Expr &candidate,
                const LocalMap &,
                LocalInfo::ValueKind &kindOut,
                bool &matchedOut) {
              bool definitionMatched = false;
              const bool resolved = resolveDefinitionCallReturnKind(
                  candidate, defMap, resolveExprPath, getReturnInfo, false, kindOut, &definitionMatched);
              matchedOut = definitionMatched;
              return resolved;
            },
            [&](const Expr &candidate,
                const LocalMap &candidateLocals,
                LocalInfo::ValueKind &kindOut,
                bool &matchedOut) {
              bool countMethodResolved = false;
              const bool resolved = resolveCountMethodCallReturnKind(candidate,
                                                                    candidateLocals,
                                                                    isArrayCountCall,
                                                                    isStringCountCall,
                                                                    resolveMethodCallDefinition,
                                                                    getReturnInfo,
                                                                    false,
                                                                    kindOut,
                                                                    &countMethodResolved);
              matchedOut = countMethodResolved;
              return resolved;
            },
            [&](const Expr &candidate,
                const LocalMap &candidateLocals,
                LocalInfo::ValueKind &kindOut,
                bool &matchedOut) {
              bool methodResolved = false;
              const bool resolved = resolveMethodCallReturnKind(
                  candidate, candidateLocals, resolveMethodCallDefinition, getReturnInfo, false, kindOut, &methodResolved);
              matchedOut = methodResolved;
              return resolved;
            },
            callReturnKind);
        if (callReturnResolution == CallExpressionReturnKindResolution::Resolved) {
          return callReturnKind;
        }
        if (callReturnResolution == CallExpressionReturnKindResolution::MatchedButUnsupported) {
          return LocalInfo::ValueKind::Unknown;
        }
        if (isArrayCountCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        if (isStringCountCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        if (isVectorCapacityCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        LocalInfo::ValueKind accessElementKind = LocalInfo::ValueKind::Unknown;
        if (resolveArrayMapAccessElementKind(
                expr,
                localsIn,
                [&](const Expr &candidate, const LocalMap &candidateLocals) {
                  return isEntryArgsName(candidate, candidateLocals);
                },
                accessElementKind) == ArrayMapAccessElementKindResolution::Resolved) {
          return accessElementKind;
        }
        std::string gpuBuiltin;
        if (getBuiltinGpuName(expr, gpuBuiltin)) {
          return LocalInfo::ValueKind::Int32;
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "buffer_load") && expr.args.size() == 2) {
          LocalInfo::ValueKind elemKind = inferBufferElementKind(expr.args.front(), localsIn);
          if (elemKind != LocalInfo::ValueKind::Unknown && elemKind != LocalInfo::ValueKind::String) {
            return elemKind;
          }
          return LocalInfo::ValueKind::Unknown;
        }
        std::string builtin;
        if (getBuiltinComparisonName(expr, builtin)) {
          return LocalInfo::ValueKind::Bool;
        }
        if (getBuiltinOperatorName(expr, builtin)) {
          if (builtin == "negate") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferExprKind(expr.args.front(), localsIn);
          }
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto left = inferExprKind(expr.args[0], localsIn);
          auto right = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(left, right);
        }
        LocalInfo::ValueKind mathBuiltinKind = LocalInfo::ValueKind::Unknown;
        if (inferMathBuiltinReturnKind(
                expr,
                localsIn,
                hasMathImport,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferExprKind(candidateExpr, candidateLocals);
                },
                [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
                  return combineNumericKinds(left, right);
                },
                mathBuiltinKind) == MathBuiltinReturnKindResolution::Resolved) {
          return mathBuiltinKind;
        }
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          const std::string &typeName = expr.templateArgs.front();
          return valueKindFromTypeName(typeName);
        }
        if (isSimpleCallName(expr, "increment") || isSimpleCallName(expr, "decrement")) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (isSimpleCallName(expr, "move")) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end()) {
              if (it->second.kind != LocalInfo::Kind::Value && it->second.kind != LocalInfo::Kind::Reference) {
                return LocalInfo::ValueKind::Unknown;
              }
              return it->second.valueKind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") && target.args.size() == 1) {
            return inferPointerTargetKind(target.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        if (isMatchCall(expr)) {
          Expr expanded;
          if (!lowerMatchToIf(expr, expanded, error)) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expanded, localsIn);
        }
        if (isIfCall(expr) && expr.args.size() == 3) {
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
          auto inferBranchValueKind = [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
            if (!isIfBlockEnvelope(candidate)) {
              return inferExprKind(candidate, localsBase);
            }
            return inferBodyValueKindWithLocalsScaffolding(
                candidate.bodyArguments,
                localsBase,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferExprKind(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr) { return isBindingMutable(candidateExpr); },
                [&](const Expr &candidateExpr) { return bindingKind(candidateExpr); },
                [&](const Expr &candidateExpr) { return hasExplicitBindingTypeTransform(candidateExpr); },
                [&](const Expr &candidateExpr, LocalInfo::Kind kind) {
                  return bindingValueKind(candidateExpr, kind);
                },
                [&](const Expr &candidateExpr, LocalInfo &info) { applyStructArrayInfo(candidateExpr, info); },
                [&](const Expr &candidateExpr, LocalInfo &info) { applyStructValueInfo(candidateExpr, info); },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferStructExprPath(candidateExpr, candidateLocals);
                });
          };

          LocalInfo::ValueKind thenKind = inferBranchValueKind(expr.args[1], localsIn);
          LocalInfo::ValueKind elseKind = inferBranchValueKind(expr.args[2], localsIn);
          if (thenKind == elseKind) {
            return thenKind;
          }
          return combineNumericKinds(thenKind, elseKind);
        }
        if (isBlockCall(expr) && expr.hasBodyArguments) {
          const std::string resolved = resolveExprPath(expr);
          if (defMap.find(resolved) == defMap.end() && expr.args.empty() && expr.templateArgs.empty() &&
              !hasNamedArguments(expr.argNames)) {
            return inferBodyValueKindWithLocalsScaffolding(
                expr.bodyArguments,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferExprKind(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr) { return isBindingMutable(candidateExpr); },
                [&](const Expr &candidateExpr) { return bindingKind(candidateExpr); },
                [&](const Expr &candidateExpr) { return hasExplicitBindingTypeTransform(candidateExpr); },
                [&](const Expr &candidateExpr, LocalInfo::Kind kind) {
                  return bindingValueKind(candidateExpr, kind);
                },
                [&](const Expr &candidateExpr, LocalInfo &info) { applyStructArrayInfo(candidateExpr, info); },
                [&](const Expr &candidateExpr, LocalInfo &info) { applyStructValueInfo(candidateExpr, info); },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferStructExprPath(candidateExpr, candidateLocals);
                });
          }
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (builtin == "dereference") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferPointerTargetKind(expr.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };

  getReturnInfo = [&](const std::string &path, ReturnInfo &outInfo) -> bool {
    auto cached = returnInfoCache.find(path);
    if (cached != returnInfoCache.end()) {
      outInfo = cached->second;
      return true;
    }
    auto defIt = defMap.find(path);
    if (defIt == defMap.end() || !defIt->second) {
      error = "native backend cannot resolve definition: " + path;
      return false;
    }
    if (!returnInferenceStack.insert(path).second) {
      error = "native backend return type inference requires explicit annotation on " + path;
      return false;
    }

    const Definition &def = *defIt->second;
    ReturnInfo info;
