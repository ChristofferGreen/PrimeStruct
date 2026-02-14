#include "primec/Emitter.h"

#include "EmitterHelpers.h"

#include <functional>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace primec {
using namespace emitter;

std::string Emitter::toCppName(const std::string &fullPath) const {
  std::string name = "ps";
  for (char c : fullPath) {
    if (c == '/') {
      name += "_";
    } else {
      name += c;
    }
  }
  return name;
}

std::string Emitter::emitExpr(const Expr &expr,
                              const std::unordered_map<std::string, std::string> &nameMap,
                              const std::unordered_map<std::string, std::vector<Expr>> &paramMap,
                              const std::unordered_map<std::string, std::string> &structTypeMap,
                              const std::unordered_map<std::string, std::string> &importAliases,
                              const std::unordered_map<std::string, BindingInfo> &localTypes,
                              const std::unordered_map<std::string, ReturnKind> &returnKinds,
                              bool allowMathBare) const {
  std::function<bool(const Expr &)> isPointerExpr;
  isPointerExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localTypes.find(candidate.name);
      return it != localTypes.end() && it->second.typeName == "Pointer";
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "location")) {
      return true;
    }
    char op = '\0';
    if (getBuiltinOperator(candidate, op) && candidate.args.size() == 2 && (op == '+' || op == '-')) {
      return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
    }
    return false;
  };
  if (expr.kind == Expr::Kind::Literal) {
    if (!expr.isUnsigned && expr.intWidth == 32) {
      return std::to_string(static_cast<int64_t>(expr.literalValue));
    }
    if (expr.isUnsigned) {
      std::ostringstream out;
      out << "static_cast<uint64_t>(" << static_cast<uint64_t>(expr.literalValue) << ")";
      return out.str();
    }
    std::ostringstream out;
    out << "static_cast<int64_t>(" << static_cast<int64_t>(expr.literalValue) << ")";
    return out.str();
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return expr.boolValue ? "true" : "false";
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    if (expr.floatWidth == 64) {
      return expr.floatValue;
    }
    std::string literal = expr.floatValue;
    if (literal.find('.') == std::string::npos && literal.find('e') == std::string::npos &&
        literal.find('E') == std::string::npos) {
      literal += ".0";
    }
    return literal + "f";
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return "std::string_view(" + stripStringLiteralSuffix(expr.stringValue) + ")";
  }
  if (expr.kind == Expr::Kind::Name) {
    if (localTypes.count(expr.name) == 0 && isBuiltinMathConstantName(expr.name, allowMathBare)) {
      std::string constantName = expr.name;
      if (!constantName.empty() && constantName[0] == '/') {
        constantName.erase(0, 1);
      }
      if (constantName.rfind("math/", 0) == 0) {
        constantName.erase(0, 5);
      }
      return "ps_const_" + constantName;
    }
    return expr.name;
  }
  std::string full = resolveExprPath(expr);
  if (expr.isMethodCall && !isArrayCountCall(expr, localTypes) && !isMapCountCall(expr, localTypes) &&
      !isStringCountCall(expr, localTypes)) {
    std::string methodPath;
    if (resolveMethodCallPath(expr, localTypes, importAliases, returnKinds, methodPath)) {
      full = methodPath;
    }
  }
  if (!expr.isMethodCall && !expr.name.empty() && expr.name[0] != '/' && expr.name.find('/') == std::string::npos) {
    if (nameMap.count(full) == 0) {
      if (!expr.namespacePrefix.empty()) {
        auto importIt = importAliases.find(expr.name);
        if (importIt != importAliases.end()) {
          full = importIt->second;
        }
      } else {
        const std::string root = "/" + expr.name;
        if (nameMap.count(root) > 0) {
          full = root;
        } else {
          auto importIt = importAliases.find(expr.name);
          if (importIt != importAliases.end()) {
            full = importIt->second;
          }
        }
      }
    }
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "count") && expr.args.size() == 1 && nameMap.count(full) == 0 &&
      !isArrayCountCall(expr, localTypes) && !isMapCountCall(expr, localTypes) && !isStringCountCall(expr, localTypes)) {
    Expr methodExpr = expr;
    methodExpr.isMethodCall = true;
    std::string methodPath;
    if (resolveMethodCallPath(methodExpr, localTypes, importAliases, returnKinds, methodPath)) {
      full = methodPath;
    }
  }
  if (isBuiltinBlock(expr, nameMap) && expr.hasBodyArguments) {
    if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
      return "0";
    }
    if (expr.bodyArguments.empty()) {
      return "0";
    }
    std::unordered_map<std::string, BindingInfo> blockTypes = localTypes;
    std::ostringstream out;
    out << "([&]() { ";
    for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
      const Expr &stmt = expr.bodyArguments[i];
      const bool isLast = (i + 1 == expr.bodyArguments.size());
      if (isLast) {
        if (stmt.isBinding) {
          out << "return 0; ";
          break;
        }
        out << "return "
            << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, blockTypes, returnKinds, allowMathBare)
            << "; ";
        break;
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), blockTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        blockTypes[stmt.name] = binding;
        bool needsConst = !binding.isMutable;
        const bool useRef =
            !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
        if (hasExplicitBindingTypeTransform(stmt)) {
          std::string type = bindingTypeToCpp(binding, stmt.namespacePrefix, importAliases, structTypeMap);
          bool isReference = binding.typeName == "Reference";
          if (useRef) {
            if (type.rfind("const ", 0) != 0) {
              out << "const " << type << " & " << stmt.name;
            } else {
              out << type << " & " << stmt.name;
            }
          } else {
            out << (needsConst ? "const " : "") << type << " " << stmt.name;
          }
          if (!stmt.args.empty()) {
            if (isReference) {
              out << " = *(" << emitExpr(stmt.args.front(),
                                         nameMap,
                                         paramMap,
                                         structTypeMap,
                                         importAliases,
                                         blockTypes,
                                         returnKinds,
                                         allowMathBare)
                  << ")";
            } else {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      structTypeMap,
                                      importAliases,
                                      blockTypes,
                                      returnKinds,
                                      allowMathBare);
            }
          }
          out << "; ";
        } else {
          if (useRef) {
            out << "const auto & " << stmt.name;
          } else {
            out << (needsConst ? "const " : "") << "auto " << stmt.name;
          }
          if (!stmt.args.empty()) {
            out << " = " << emitExpr(stmt.args.front(),
                                    nameMap,
                                    paramMap,
                                    structTypeMap,
                                    importAliases,
                                    blockTypes,
                                    returnKinds,
                                    allowMathBare);
          }
          out << "; ";
        }
        continue;
      }
      out << "(void)"
          << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, blockTypes, returnKinds, allowMathBare)
          << "; ";
    }
    out << "}())";
    return out.str();
  }
  if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
    auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      const std::string resolved = resolveExprPath(candidate);
      return nameMap.count(resolved) == 0;
    };
    auto emitBranchValueExpr =
        [&](const Expr &candidate, std::unordered_map<std::string, BindingInfo> branchTypes) -> std::string {
      if (!isIfBlockEnvelope(candidate)) {
        return emitExpr(candidate,
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        branchTypes,
                        returnKinds,
                        allowMathBare);
      }
      if (candidate.bodyArguments.empty()) {
        return "0";
      }
      std::ostringstream out;
      out << "([&]() { ";
      for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
        const Expr &stmt = candidate.bodyArguments[i];
        const bool isLast = (i + 1 == candidate.bodyArguments.size());
        if (isLast) {
          if (stmt.isBinding) {
            out << "return 0; ";
            break;
          }
          out << "return "
              << emitExpr(stmt,
                          nameMap,
                          paramMap,
                          structTypeMap,
                          importAliases,
                          branchTypes,
                          returnKinds,
                          allowMathBare)
              << "; ";
          break;
        }
        if (stmt.isBinding) {
          BindingInfo binding = getBindingInfo(stmt);
          if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
            ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), branchTypes, returnKinds, allowMathBare);
            std::string inferred = typeNameForReturnKind(initKind);
            if (!inferred.empty()) {
              binding.typeName = inferred;
              binding.typeTemplateArg.clear();
            }
          }
          branchTypes[stmt.name] = binding;
          bool needsConst = !binding.isMutable;
          const bool useRef =
              !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
          if (hasExplicitBindingTypeTransform(stmt)) {
            std::string type = bindingTypeToCpp(binding, stmt.namespacePrefix, importAliases, structTypeMap);
            bool isReference = binding.typeName == "Reference";
            if (useRef) {
              if (type.rfind("const ", 0) != 0) {
                out << "const " << type << " & " << stmt.name;
              } else {
                out << type << " & " << stmt.name;
              }
            } else {
              out << (needsConst ? "const " : "") << type << " " << stmt.name;
            }
            if (!stmt.args.empty()) {
            if (isReference) {
              out << " = *(" << emitExpr(stmt.args.front(),
                                         nameMap,
                                         paramMap,
                                         structTypeMap,
                                         importAliases,
                                         branchTypes,
                                         returnKinds,
                                         allowMathBare)
                  << ")";
            } else {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      structTypeMap,
                                      importAliases,
                                      branchTypes,
                                      returnKinds,
                                      allowMathBare);
            }
          }
          out << "; ";
        } else {
          if (useRef) {
            out << "const auto & " << stmt.name;
          } else {
            out << (needsConst ? "const " : "") << "auto " << stmt.name;
          }
          if (!stmt.args.empty()) {
            out << " = " << emitExpr(stmt.args.front(),
                                    nameMap,
                                    paramMap,
                                    structTypeMap,
                                    importAliases,
                                    branchTypes,
                                    returnKinds,
                                    allowMathBare);
          }
          out << "; ";
        }
        continue;
      }
      out << "(void)"
          << emitExpr(stmt,
                      nameMap,
                      paramMap,
                      structTypeMap,
                      importAliases,
                      branchTypes,
                      returnKinds,
                      allowMathBare)
          << "; ";
    }
    out << "}())";
    return out.str();
  };
  std::ostringstream out;
    out << "("
        << emitExpr(expr.args[0],
                    nameMap,
                    paramMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    allowMathBare)
        << " ? "
        << emitBranchValueExpr(expr.args[1], localTypes) << " : " << emitBranchValueExpr(expr.args[2], localTypes)
        << ")";
    return out.str();
  }
  auto it = nameMap.find(full);
  if (it == nameMap.end()) {
    if (isMapCountCall(expr, localTypes)) {
      std::ostringstream out;
      out << "ps_map_count("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (isArrayCountCall(expr, localTypes)) {
      std::ostringstream out;
      out << "ps_array_count("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (isStringCountCall(expr, localTypes)) {
      std::ostringstream out;
      out << "ps_string_count("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (isVectorCapacityCall(expr, localTypes)) {
      std::ostringstream out;
      out << "ps_vector_capacity("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.name == "at" && expr.args.size() == 2) {
      std::ostringstream out;
      const Expr &target = expr.args[0];
      if (isStringValue(target, localTypes)) {
        out << "ps_string_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else if (isMapValue(target, localTypes)) {
        out << "ps_map_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else {
        out << "ps_array_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      }
      return out.str();
    }
    if (expr.name == "at_unsafe" && expr.args.size() == 2) {
      std::ostringstream out;
      const Expr &target = expr.args[0];
      if (isStringValue(target, localTypes)) {
        out << "ps_string_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else if (isMapValue(target, localTypes)) {
        out << "ps_map_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else {
        out << "ps_array_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      }
      return out.str();
    }
    char op = '\0';
    if (getBuiltinOperator(expr, op) && expr.args.size() == 2) {
      std::ostringstream out;
      if ((op == '+' || op == '-') && isPointerExpr(expr.args[0])) {
        out << "ps_pointer_add("
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", ";
        if (op == '-') {
          out << "-("
              << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
              << ")";
        } else {
          out << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
        }
        out << ")";
        return out.str();
      }
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << " "
          << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-"
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && expr.args.size() == 1) {
      std::ostringstream out;
      const char *op = mutateName == "increment" ? "++" : "--";
      out << "(" << op
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "("
            << cmp
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "("
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << " "
            << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr, allowMathBare) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", "
          << emitExpr(expr.args[2], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
          << ")";
      return out.str();
    }
    std::string minMaxName;
    if (getBuiltinMinMaxName(expr, minMaxName, allowMathBare) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "ps_builtin_" << minMaxName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    std::string absSignName;
    if (getBuiltinAbsSignName(expr, absSignName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << absSignName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    std::string saturateName;
    if (getBuiltinSaturateName(expr, saturateName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << saturateName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
          << ")";
      return out.str();
    }
    std::string mathName;
    if (getBuiltinMathName(expr, mathName, allowMathBare)) {
      std::ostringstream out;
      out << "ps_builtin_" << mathName << "(";
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        out << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
      }
      out << ")";
      return out.str();
    }
    std::string convertName;
    if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      std::string targetType =
          bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
      std::ostringstream out;
      out << "static_cast<" << targetType << ">("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(" << pointerOp
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector") && expr.templateArgs.size() == 1) {
        std::ostringstream out;
        const std::string elemType =
            bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
        out << "std::vector<" << elemType << ">{";
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
        }
        out << "}";
        return out.str();
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        std::ostringstream out;
        const std::string keyType =
            bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
        const std::string valueType =
            bindingTypeToCpp(expr.templateArgs[1], expr.namespacePrefix, importAliases, structTypeMap);
        out << "std::unordered_map<" << keyType << ", " << valueType << ">{";
        for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
          if (i > 0) {
            out << ", ";
          }
          out << "{"
              << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
              << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
              << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    return "0";
  }
  std::ostringstream out;
  out << it->second << "(";
  std::vector<const Expr *> orderedArgs;
  auto paramIt = paramMap.find(full);
  if (paramIt != paramMap.end()) {
    orderedArgs = orderCallArguments(expr, paramIt->second);
  } else {
    for (const auto &arg : expr.args) {
      orderedArgs.push_back(&arg);
    }
  }
  for (size_t i = 0; i < orderedArgs.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << emitExpr(*orderedArgs[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
  }
  out << ")";
  return out.str();
}


} // namespace primec
