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
  if (expr.isLambda) {
    struct LambdaCapture {
      std::string name;
      bool byRef = false;
    };
    auto splitCaptureTokens = [](const std::string &capture) {
      std::vector<std::string> tokens;
      std::istringstream stream(capture);
      std::string token;
      while (stream >> token) {
        tokens.push_back(token);
      }
      return tokens;
    };

    std::string captureAllToken;
    std::vector<LambdaCapture> explicitCaptures;
    explicitCaptures.reserve(expr.lambdaCaptures.size());
    for (const auto &capture : expr.lambdaCaptures) {
      std::vector<std::string> tokens = splitCaptureTokens(capture);
      if (tokens.empty()) {
        continue;
      }
      const std::string &last = tokens.back();
      if (last == "=" || last == "&") {
        if (captureAllToken.empty()) {
          captureAllToken = last;
        }
        continue;
      }
      bool byRef = false;
      for (const auto &token : tokens) {
        if (token == "ref") {
          byRef = true;
          break;
        }
      }
      explicitCaptures.push_back({last, byRef});
    }

    std::unordered_map<std::string, BindingInfo> lambdaTypes;
    lambdaTypes.reserve(localTypes.size() + expr.args.size());
    if (!captureAllToken.empty()) {
      for (const auto &entry : localTypes) {
        lambdaTypes.emplace(entry.first, entry.second);
      }
    } else {
      for (const auto &capture : explicitCaptures) {
        auto it = localTypes.find(capture.name);
        if (it != localTypes.end()) {
          lambdaTypes.emplace(it->first, it->second);
        }
      }
    }

    std::ostringstream out;
    out << "[";
    bool firstCapture = true;
    if (!captureAllToken.empty()) {
      out << captureAllToken;
      firstCapture = false;
    }
    if (captureAllToken.empty()) {
      for (const auto &capture : explicitCaptures) {
        if (!firstCapture) {
          out << ", ";
        }
        if (capture.byRef) {
          out << "&";
        }
        out << capture.name;
        firstCapture = false;
      }
    }
    out << "](";
    for (size_t i = 0; i < expr.args.size(); ++i) {
      const Expr &param = expr.args[i];
      BindingInfo paramInfo = getBindingInfo(param);
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        ReturnKind initKind = inferPrimitiveReturnKind(param.args.front(), lambdaTypes, returnKinds, allowMathBare);
        std::string inferred = typeNameForReturnKind(initKind);
        if (!inferred.empty()) {
          paramInfo.typeName = inferred;
          paramInfo.typeTemplateArg.clear();
        }
      }
      const std::string typeNamespace = param.namespacePrefix.empty() ? expr.namespacePrefix : param.namespacePrefix;
      std::string paramType = bindingTypeToCpp(paramInfo, typeNamespace, importAliases, structTypeMap);
      const bool refCandidate = isReferenceCandidate(paramInfo);
      const bool passByRef = refCandidate && !paramInfo.isCopy;
      if (passByRef) {
        if (!paramInfo.isMutable && paramType.rfind("const ", 0) != 0) {
          out << "const ";
        }
        out << paramType << " & " << param.name;
      } else {
        bool needsConst = !paramInfo.isMutable;
        if (needsConst && paramType.rfind("const ", 0) == 0) {
          needsConst = false;
        }
        out << (needsConst ? "const " : "") << paramType << " " << param.name;
      }
      if (param.args.size() == 1) {
        out << " = "
            << emitExpr(param.args.front(),
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        lambdaTypes,
                        returnKinds,
                        allowMathBare);
      }
      if (i + 1 < expr.args.size()) {
        out << ", ";
      }
      lambdaTypes[param.name] = paramInfo;
    }
    out << ") { ";

    std::unordered_map<std::string, BindingInfo> bodyTypes = lambdaTypes;
    int repeatCounter = 0;
    std::function<void(const Expr &, std::unordered_map<std::string, BindingInfo> &)> emitLambdaStatement;
    emitLambdaStatement = [&](const Expr &stmt, std::unordered_map<std::string, BindingInfo> &activeTypes) {
      if (isReturnCall(stmt)) {
        out << "return";
        if (!stmt.args.empty()) {
          out << " "
              << emitExpr(stmt.args.front(),
                          nameMap,
                          paramMap,
                          structTypeMap,
                          importAliases,
                          activeTypes,
                          returnKinds,
                          allowMathBare);
        }
        out << "; ";
        return;
      }
      if (!stmt.isBinding && stmt.kind == Expr::Kind::Call) {
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(stmt, printBuiltin)) {
          const char *stream = (printBuiltin.target == PrintTarget::Err) ? "stderr" : "stdout";
          std::string argText = "\"\"";
          if (!stmt.args.empty()) {
            argText = emitExpr(stmt.args.front(),
                               nameMap,
                               paramMap,
                               structTypeMap,
                               importAliases,
                               activeTypes,
                               returnKinds,
                               allowMathBare);
          }
          out << "ps_print_value(" << stream << ", " << argText << ", "
              << (printBuiltin.newline ? "true" : "false") << "); ";
          return;
        }
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
        const bool lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
        if (!hasExplicitType && stmt.args.size() == 1 && !lambdaInit) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        const bool useAuto = !hasExplicitType && lambdaInit;
        activeTypes[stmt.name] = binding;
        if (useAuto) {
          out << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
          if (!stmt.args.empty()) {
            out << " = "
                << emitExpr(stmt.args.front(),
                            nameMap,
                            paramMap,
                            structTypeMap,
                            importAliases,
                            activeTypes,
                            returnKinds,
                            allowMathBare);
          }
          out << "; ";
          return;
        }
        bool needsConst = !binding.isMutable;
        const bool useRef = !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
        if (hasExplicitType) {
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
                                         activeTypes,
                                         returnKinds,
                                         allowMathBare)
                  << ")";
            } else {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      structTypeMap,
                                      importAliases,
                                      activeTypes,
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
                                    activeTypes,
                                    returnKinds,
                                    allowMathBare);
          }
          out << "; ";
        }
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
          if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
            return false;
          }
          if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
            return false;
          }
          return true;
        };
        out << "if ("
            << emitExpr(cond,
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        activeTypes,
                        returnKinds,
                        allowMathBare)
            << ") { ";
        {
          auto blockTypes = activeTypes;
          if (isIfBlockEnvelope(thenArg)) {
            for (const auto &bodyStmt : thenArg.bodyArguments) {
              emitLambdaStatement(bodyStmt, blockTypes);
            }
          } else {
            emitLambdaStatement(thenArg, blockTypes);
          }
        }
        out << "} else { ";
        {
          auto blockTypes = activeTypes;
          if (isIfBlockEnvelope(elseArg)) {
            for (const auto &bodyStmt : elseArg.bodyArguments) {
              emitLambdaStatement(bodyStmt, blockTypes);
            }
          } else {
            emitLambdaStatement(elseArg, blockTypes);
          }
        }
        out << "} ";
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
        out << "{ ";
        std::string countExpr =
            emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare);
        ReturnKind countKind = inferPrimitiveReturnKind(stmt.args[0], activeTypes, returnKinds, allowMathBare);
        out << "auto " << endVar << " = " << countExpr << "; ";
        const std::string indexType = (countKind == ReturnKind::Bool) ? "int" : ("decltype(" + endVar + ")");
        out << "for (" << indexType << " " << indexVar << " = 0; " << indexVar << " < " << endVar << "; ++"
            << indexVar << ") { ";
        if (isLoopBlockEnvelope(stmt.args[1])) {
          auto blockTypes = activeTypes;
          for (const auto &bodyStmt : stmt.args[1].bodyArguments) {
            emitLambdaStatement(bodyStmt, blockTypes);
          }
        }
        out << "} ";
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isWhileCall(stmt) && stmt.args.size() == 2 && isLoopBlockEnvelope(stmt.args[1])) {
        out << "while ("
            << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
            << ") { ";
        auto blockTypes = activeTypes;
        for (const auto &bodyStmt : stmt.args[1].bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isForCall(stmt) && stmt.args.size() == 4 && isLoopBlockEnvelope(stmt.args[3])) {
        out << "{ ";
        auto loopTypes = activeTypes;
        emitLambdaStatement(stmt.args[0], loopTypes);
        out << "while ("
            << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, loopTypes, returnKinds, allowMathBare)
            << ") { ";
        auto blockTypes = loopTypes;
        for (const auto &bodyStmt : stmt.args[3].bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        emitLambdaStatement(stmt.args[2], loopTypes);
        out << "} ";
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isRepeatCall(stmt)) {
        const int repeatIndex = repeatCounter++;
        const std::string endVar = "ps_repeat_end_" + std::to_string(repeatIndex);
        const std::string indexVar = "ps_repeat_i_" + std::to_string(repeatIndex);
        out << "{ ";
        std::string countExpr = "\"\"";
        ReturnKind countKind = ReturnKind::Unknown;
        if (!stmt.args.empty()) {
          countExpr =
              emitExpr(stmt.args.front(), nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare);
          countKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
        }
        out << "auto " << endVar << " = " << countExpr << "; ";
        const std::string indexType = (countKind == ReturnKind::Bool) ? "int" : ("decltype(" + endVar + ")");
        out << "for (" << indexType << " " << indexVar << " = 0; " << indexVar << " < " << endVar << "; ++"
            << indexVar << ") { ";
        auto blockTypes = activeTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        out << "} ";
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinBlock(stmt, nameMap) && (stmt.hasBodyArguments || !stmt.bodyArguments.empty())) {
        out << "{ ";
        auto blockTypes = activeTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && (stmt.hasBodyArguments || !stmt.bodyArguments.empty())) {
        std::string full = resolveExprPath(stmt);
        if (stmt.isMethodCall && !isArrayCountCall(stmt, activeTypes) && !isMapCountCall(stmt, activeTypes)) {
          std::string methodPath;
          if (resolveMethodCallPath(stmt, activeTypes, importAliases, returnKinds, methodPath)) {
            full = methodPath;
          }
        }
        auto it = nameMap.find(full);
        if (it == nameMap.end()) {
          out << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
              << "; ";
          return;
        }
        out << it->second << "(";
        std::vector<const Expr *> orderedArgs;
        auto paramIt = paramMap.find(full);
        if (paramIt != paramMap.end()) {
          orderedArgs = orderCallArguments(stmt, paramIt->second);
        } else {
          for (const auto &arg : stmt.args) {
            orderedArgs.push_back(&arg);
          }
        }
        for (size_t i = 0; i < orderedArgs.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(*orderedArgs[i],
                          nameMap,
                          paramMap,
                          structTypeMap,
                          importAliases,
                          activeTypes,
                          returnKinds,
                          allowMathBare);
        }
        if (!orderedArgs.empty()) {
          out << ", ";
        }
        out << "[&]() { ";
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitLambdaStatement(bodyStmt, activeTypes);
        }
        out << "}";
        out << "); ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinAssign(stmt, nameMap) && stmt.args.size() == 2 &&
          stmt.args.front().kind == Expr::Kind::Name) {
        out << stmt.args.front().name << " = "
            << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
            << "; ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isPathSpaceBuiltinName(stmt) && nameMap.find(resolveExprPath(stmt)) == nameMap.end()) {
        for (const auto &arg : stmt.args) {
          out << "(void)("
              << emitExpr(arg, nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
              << "); ";
        }
        return;
      }
      if (stmt.kind == Expr::Kind::Call) {
        std::string vectorHelper;
        if (getVectorMutatorName(stmt, nameMap, vectorHelper)) {
          if (vectorHelper == "push") {
            out << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ".push_back("
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "pop") {
            out << "ps_vector_pop("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "reserve") {
            out << "ps_vector_reserve("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "clear") {
            out << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ".clear(); ";
            return;
          }
          if (vectorHelper == "remove_at") {
            out << "ps_vector_remove_at("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "remove_swap") {
            out << "ps_vector_remove_swap("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
        }
      }
      out << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
          << "; ";
    };
    for (const auto &stmt : expr.bodyArguments) {
      emitLambdaStatement(stmt, bodyTypes);
    }
    out << "}";
    return out.str();
  }
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
        const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
        const bool lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
        if (!hasExplicitType && stmt.args.size() == 1 && !lambdaInit) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), blockTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        blockTypes[stmt.name] = binding;
        const bool useAuto = !hasExplicitType && lambdaInit;
        if (useAuto) {
          out << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
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
          continue;
        }
        bool needsConst = !binding.isMutable;
        const bool useRef =
            !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
        if (hasExplicitType) {
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
      return true;
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
          const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
          const bool lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
          if (!hasExplicitType && stmt.args.size() == 1 && !lambdaInit) {
            ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), branchTypes, returnKinds, allowMathBare);
            std::string inferred = typeNameForReturnKind(initKind);
            if (!inferred.empty()) {
              binding.typeName = inferred;
              binding.typeTemplateArg.clear();
            }
          }
          branchTypes[stmt.name] = binding;
          const bool useAuto = !hasExplicitType && lambdaInit;
          if (useAuto) {
            out << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
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
            continue;
          }
          bool needsConst = !binding.isMutable;
          const bool useRef =
              !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
          if (hasExplicitType) {
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
