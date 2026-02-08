#include "primec/Emitter.h"

#include "EmitterHelpers.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace primec {
using namespace emitter;

std::string Emitter::emitCpp(const Program &program, const std::string &entryPath) const {
  std::unordered_map<std::string, std::string> nameMap;
  std::unordered_map<std::string, std::vector<Expr>> paramMap;
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  auto isStructTransformName = [](const std::string &name) {
    return name == "struct" || name == "pod" || name == "handle" || name == "gpu_lane";
  };
  auto isStructDefinition = [&](const Definition &def) {
    bool hasStruct = false;
    bool hasReturn = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturn = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStruct = true;
      }
    }
    if (hasStruct) {
      return true;
    }
    if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return false;
      }
    }
    return true;
  };
  for (const auto &def : program.definitions) {
    nameMap[def.fullPath] = toCppName(def.fullPath);
    if (isStructDefinition(def)) {
      paramMap[def.fullPath] = def.statements;
    } else {
      paramMap[def.fullPath] = def.parameters;
    }
    defMap[def.fullPath] = &def;
    returnKinds[def.fullPath] = getReturnKind(def);
  }

  auto isParam = [](const std::vector<Expr> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param.name == name) {
        return true;
      }
    }
    return false;
  };

  std::unordered_set<std::string> inferenceStack;
  std::function<ReturnKind(const Definition &)> inferDefinitionReturnKind;
  std::function<ReturnKind(const Expr &,
                           const std::vector<Expr> &,
                           const std::unordered_map<std::string, ReturnKind> &)>
      inferExprReturnKind;

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<Expr> &params,
                            const std::unordered_map<std::string, ReturnKind> &locals) -> ReturnKind {
    auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
      if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::Void || right == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
        return ReturnKind::Float64;
      }
      if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
        return ReturnKind::Float32;
      }
      if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
        return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
        if (left == ReturnKind::Int64 || left == ReturnKind::Int) {
          if (right == ReturnKind::Int64 || right == ReturnKind::Int) {
            return ReturnKind::Int64;
          }
        }
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int && right == ReturnKind::Int) {
        return ReturnKind::Int;
      }
      return ReturnKind::Unknown;
    };

    if (expr.kind == Expr::Kind::Literal) {
      if (expr.isUnsigned) {
        return ReturnKind::UInt64;
      }
      if (expr.intWidth == 64) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Int;
    }
    if (expr.kind == Expr::Kind::BoolLiteral) {
      return ReturnKind::Bool;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      return ReturnKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name)) {
        return ReturnKind::Unknown;
      }
      auto it = locals.find(expr.name);
      if (it == locals.end()) {
        return ReturnKind::Unknown;
      }
      return it->second;
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string full = resolveExprPath(expr);
      auto defIt = defMap.find(full);
      if (defIt != defMap.end()) {
        ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
        if (calleeKind == ReturnKind::Void || calleeKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        return calleeKind;
      }
      if (isBuiltinBlock(expr, nameMap) && expr.hasBodyArguments) {
        if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
          return ReturnKind::Unknown;
        }
        std::unordered_map<std::string, ReturnKind> blockLocals = locals;
        bool sawValue = false;
        ReturnKind last = ReturnKind::Unknown;
        for (const auto &bodyExpr : expr.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo info = getBindingInfo(bodyExpr);
            ReturnKind bindingKind = returnKindForTypeName(info.typeName);
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
              if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
                bindingKind = initKind;
              }
            }
            blockLocals.emplace(bodyExpr.name, bindingKind);
            continue;
          }
          sawValue = true;
          last = inferExprReturnKind(bodyExpr, params, blockLocals);
        }
        return sawValue ? last : ReturnKind::Unknown;
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
          return defMap.find(resolved) == defMap.end();
        };
        auto inferBranchValueKind = [&](const Expr &candidate,
                                        const std::unordered_map<std::string, ReturnKind> &localsBase) -> ReturnKind {
          if (!isIfBlockEnvelope(candidate)) {
            return inferExprReturnKind(candidate, params, localsBase);
          }
          std::unordered_map<std::string, ReturnKind> branchLocals = localsBase;
          bool sawValue = false;
          ReturnKind lastKind = ReturnKind::Unknown;
          for (const auto &bodyExpr : candidate.bodyArguments) {
            if (bodyExpr.isBinding) {
              BindingInfo info = getBindingInfo(bodyExpr);
              ReturnKind bindingKind = returnKindForTypeName(info.typeName);
              if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
                ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, branchLocals);
                if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
                  bindingKind = initKind;
                }
              }
              branchLocals.emplace(bodyExpr.name, bindingKind);
              continue;
            }
            sawValue = true;
            lastKind = inferExprReturnKind(bodyExpr, params, branchLocals);
          }
          return sawValue ? lastKind : ReturnKind::Unknown;
        };

        ReturnKind thenKind = inferBranchValueKind(expr.args[1], locals);
        ReturnKind elseKind = inferBranchValueKind(expr.args[2], locals);
        if (thenKind == ReturnKind::Unknown || elseKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        if (thenKind == elseKind) {
          return thenKind;
        }
        return combineNumeric(thenKind, elseKind);
      }
      const char *cmp = nullptr;
      if (getBuiltinComparison(expr, cmp)) {
        return ReturnKind::Bool;
      }
      char op = '\0';
      if (getBuiltinOperator(expr, op)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
        ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
        return combineNumeric(left, right);
      }
      if (isBuiltinNegate(expr)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (isBuiltinClamp(expr)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      std::string convertName;
      if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1) {
        return returnKindForTypeName(expr.templateArgs.front());
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  inferDefinitionReturnKind = [&](const Definition &def) -> ReturnKind {
    ReturnKind &kind = returnKinds[def.fullPath];
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (!inferenceStack.insert(def.fullPath).second) {
      return ReturnKind::Unknown;
    }
    const auto &params = paramMap[def.fullPath];
    ReturnKind inferred = ReturnKind::Unknown;
    bool sawReturn = false;
    std::unordered_map<std::string, ReturnKind> locals;
    std::function<void(const Expr &, std::unordered_map<std::string, ReturnKind> &)> visitStmt;
    visitStmt = [&](const Expr &stmt, std::unordered_map<std::string, ReturnKind> &activeLocals) {
      if (stmt.isBinding) {
        BindingInfo info = getBindingInfo(stmt);
        ReturnKind bindingKind = returnKindForTypeName(info.typeName);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferExprReturnKind(stmt.args.front(), params, activeLocals);
          if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
            bindingKind = initKind;
          }
        }
        activeLocals.emplace(stmt.name, bindingKind);
        return;
      }
      if (isReturnCall(stmt)) {
        sawReturn = true;
        ReturnKind exprKind = ReturnKind::Void;
        if (!stmt.args.empty()) {
          exprKind = inferExprReturnKind(stmt.args.front(), params, activeLocals);
        }
        if (exprKind == ReturnKind::Unknown) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Unknown) {
          inferred = exprKind;
          return;
        }
        if (inferred != exprKind) {
          inferred = ReturnKind::Unknown;
          return;
        }
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        auto walkBlock = [&](const Expr &block) {
          std::unordered_map<std::string, ReturnKind> blockLocals = activeLocals;
          for (const auto &bodyStmt : block.bodyArguments) {
            visitStmt(bodyStmt, blockLocals);
          }
        };
        walkBlock(thenBlock);
        walkBlock(elseBlock);
        return;
      }
    };
    for (const auto &stmt : def.statements) {
      visitStmt(stmt, locals);
    }
    if (!sawReturn) {
      kind = ReturnKind::Void;
    } else if (inferred == ReturnKind::Unknown) {
      kind = ReturnKind::Int;
    } else {
      kind = inferred;
    }
    inferenceStack.erase(def.fullPath);
    return kind;
  };

  for (const auto &def : program.definitions) {
    if (returnKinds[def.fullPath] == ReturnKind::Unknown) {
      inferDefinitionReturnKind(def);
    }
  }

  std::ostringstream out;
  out << "// Generated by primec (minimal)\n";
  out << "#include <cstdint>\n";
  out << "#include <cstdio>\n";
  out << "#include <cstdlib>\n";
  out << "#include <cstring>\n";
  out << "#include <string>\n";
  out << "#include <string_view>\n";
  out << "#include <type_traits>\n";
  out << "#include <unordered_map>\n";
  out << "#include <vector>\n";
  out << "template <typename T, typename Offset>\n";
  out << "static inline T *ps_pointer_add(T *ptr, Offset offset) {\n";
  out << "  if constexpr (std::is_signed_v<Offset>) {\n";
  out << "    return reinterpret_cast<T *>(reinterpret_cast<std::intptr_t>(ptr) + "
         "static_cast<std::intptr_t>(offset));\n";
  out << "  }\n";
  out << "  return reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(ptr) + "
         "static_cast<std::uintptr_t>(offset));\n";
  out << "}\n";
  out << "template <typename T, typename Offset>\n";
  out << "static inline T *ps_pointer_add(T &ref, Offset offset) {\n";
  out << "  return ps_pointer_add(&ref, offset);\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_abs(T value) {\n";
  out << "  if constexpr (std::is_unsigned_v<T>) {\n";
  out << "    return value;\n";
  out << "  }\n";
  out << "  return value < static_cast<T>(0) ? -value : value;\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_sign(T value) {\n";
  out << "  if constexpr (std::is_unsigned_v<T>) {\n";
  out << "    return value == static_cast<T>(0) ? static_cast<T>(0) : static_cast<T>(1);\n";
  out << "  }\n";
  out << "  if (value < static_cast<T>(0)) {\n";
  out << "    return static_cast<T>(-1);\n";
  out << "  }\n";
  out << "  if (value > static_cast<T>(0)) {\n";
  out << "    return static_cast<T>(1);\n";
  out << "  }\n";
  out << "  return static_cast<T>(0);\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_saturate(T value) {\n";
  out << "  if constexpr (std::is_unsigned_v<T>) {\n";
  out << "    return value > static_cast<T>(1) ? static_cast<T>(1) : value;\n";
  out << "  }\n";
  out << "  if (value < static_cast<T>(0)) {\n";
  out << "    return static_cast<T>(0);\n";
  out << "  }\n";
  out << "  if (value > static_cast<T>(1)) {\n";
  out << "    return static_cast<T>(1);\n";
  out << "  }\n";
  out << "  return value;\n";
  out << "}\n";
  out << "template <typename T, typename U>\n";
  out << "static inline std::common_type_t<T, U> ps_builtin_min(T left, U right) {\n";
  out << "  using R = std::common_type_t<T, U>;\n";
  out << "  R lhs = static_cast<R>(left);\n";
  out << "  R rhs = static_cast<R>(right);\n";
  out << "  return lhs < rhs ? lhs : rhs;\n";
  out << "}\n";
  out << "template <typename T, typename U>\n";
  out << "static inline std::common_type_t<T, U> ps_builtin_max(T left, U right) {\n";
  out << "  using R = std::common_type_t<T, U>;\n";
  out << "  R lhs = static_cast<R>(left);\n";
  out << "  R rhs = static_cast<R>(right);\n";
  out << "  return lhs > rhs ? lhs : rhs;\n";
  out << "}\n";
  out << "template <typename T, typename U, typename V>\n";
  out << "static inline std::common_type_t<T, U, V> ps_builtin_clamp(T value, U minValue, V maxValue) {\n";
  out << "  using R = std::common_type_t<T, U, V>;\n";
  out << "  R v = static_cast<R>(value);\n";
  out << "  R minV = static_cast<R>(minValue);\n";
  out << "  R maxV = static_cast<R>(maxValue);\n";
  out << "  if (v < minV) {\n";
  out << "    return minV;\n";
  out << "  }\n";
  out << "  if (v > maxV) {\n";
  out << "    return maxV;\n";
  out << "  }\n";
  out << "  return v;\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline int ps_array_count(const std::vector<T> &value) {\n";
  out << "  return static_cast<int>(value.size());\n";
  out << "}\n";
  out << "template <typename Key, typename Value>\n";
  out << "static inline int ps_map_count(const std::unordered_map<Key, Value> &value) {\n";
  out << "  return static_cast<int>(value.size());\n";
  out << "}\n";
  out << "template <typename Key, typename Value, typename K>\n";
  out << "static inline const Value &ps_map_at(const std::unordered_map<Key, Value> &value, const K &key) {\n";
  out << "  auto it = value.find(key);\n";
  out << "  if (it == value.end()) {\n";
  out << "    std::fprintf(stderr, \"map key not found\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  return it->second;\n";
  out << "}\n";
  out << "template <typename Key, typename Value, typename K>\n";
  out << "static inline const Value &ps_map_at_unsafe(const std::unordered_map<Key, Value> &value, const K &key) {\n";
  out << "  return value.find(key)->second;\n";
  out << "}\n";
  out << "template <typename T, typename Index>\n";
  out << "static inline const T &ps_array_at(const std::vector<T> &value, Index index) {\n";
  out << "  int64_t i = static_cast<int64_t>(index);\n";
  out << "  if (i < 0 || static_cast<size_t>(i) >= value.size()) {\n";
  out << "    std::fprintf(stderr, \"array index out of bounds\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  return value[static_cast<size_t>(i)];\n";
  out << "}\n";
  out << "template <typename T, typename Index>\n";
  out << "static inline const T &ps_array_at_unsafe(const std::vector<T> &value, Index index) {\n";
  out << "  return value[static_cast<size_t>(index)];\n";
  out << "}\n";
  out << "static inline int ps_string_count(std::string_view value) {\n";
  out << "  return static_cast<int>(value.size());\n";
  out << "}\n";
  out << "template <typename Index>\n";
  out << "static inline int ps_string_at(std::string_view value, Index index) {\n";
  out << "  int64_t i = static_cast<int64_t>(index);\n";
  out << "  if (i < 0 || static_cast<size_t>(i) >= value.size()) {\n";
  out << "    std::fprintf(stderr, \"string index out of bounds\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  return static_cast<unsigned char>(value[static_cast<size_t>(i)]);\n";
  out << "}\n";
  out << "template <typename Index>\n";
  out << "static inline int ps_string_at_unsafe(std::string_view value, Index index) {\n";
  out << "  return static_cast<unsigned char>(value[static_cast<size_t>(index)]);\n";
  out << "}\n";
  out << "static inline void ps_write(FILE *stream, const char *data, size_t len, bool newline) {\n";
  out << "  if (len > 0) {\n";
  out << "    std::fwrite(data, 1, len, stream);\n";
  out << "  }\n";
  out << "  if (newline) {\n";
  out << "    std::fputc('\\n', stream);\n";
  out << "  }\n";
  out << "}\n";
  out << "static inline void ps_print_value(FILE *stream, const char *text, bool newline) {\n";
  out << "  if (!text) {\n";
  out << "    if (newline) {\n";
  out << "      std::fputc('\\n', stream);\n";
  out << "    }\n";
  out << "    return;\n";
  out << "  }\n";
  out << "  ps_write(stream, text, std::strlen(text), newline);\n";
  out << "}\n";
  out << "static inline void ps_print_value(FILE *stream, std::string_view text, bool newline) {\n";
  out << "  ps_write(stream, text.data(), text.size(), newline);\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline void ps_print_value(FILE *stream, T value, bool newline) {\n";
  out << "  std::string text = std::to_string(value);\n";
  out << "  ps_write(stream, text.c_str(), text.size(), newline);\n";
  out << "}\n";
  for (const auto &def : program.definitions) {
    ReturnKind returnKind = returnKinds[def.fullPath];
    std::string returnType = "int";
    if (returnKind == ReturnKind::Void) {
      returnType = "void";
    } else if (returnKind == ReturnKind::Int64) {
      returnType = "int64_t";
    } else if (returnKind == ReturnKind::UInt64) {
      returnType = "uint64_t";
    } else if (returnKind == ReturnKind::Float32) {
      returnType = "float";
    } else if (returnKind == ReturnKind::Float64) {
      returnType = "double";
    } else if (returnKind == ReturnKind::Bool) {
      returnType = "bool";
    }
    const auto &params = paramMap[def.fullPath];
    const bool structDef = isStructDefinition(def);
    out << "static " << returnType << " " << nameMap[def.fullPath] << "(";
    for (size_t i = 0; i < params.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      BindingInfo paramInfo = getBindingInfo(params[i]);
      std::string paramType = bindingTypeToCpp(paramInfo);
      bool needsConst = !paramInfo.isMutable;
      if (needsConst && paramType.rfind("const ", 0) == 0) {
        needsConst = false;
      }
      out << (needsConst ? "const " : "") << paramType << " " << params[i].name;
    }
    out << ") {\n";
    std::function<void(const Expr &, int, std::unordered_map<std::string, BindingInfo> &)> emitStatement;
    int repeatCounter = 0;
    emitStatement = [&](const Expr &stmt, int indent, std::unordered_map<std::string, BindingInfo> &localTypes) {
      std::string pad(static_cast<size_t>(indent) * 2, ' ');
      if (isReturnCall(stmt)) {
        out << pad << "return";
        if (!stmt.args.empty()) {
          out << " " << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
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
            argText = emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
          }
          out << pad << "ps_print_value(" << stream << ", " << argText << ", "
              << (printBuiltin.newline ? "true" : "false") << ");\n";
          return;
        }
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
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
        std::string type = bindingTypeToCpp(binding);
        bool isReference = binding.typeName == "Reference";
        localTypes[stmt.name] = binding;
        bool needsConst = !binding.isMutable;
        if (needsConst && type.rfind("const ", 0) == 0) {
          needsConst = false;
        }
        out << pad << (needsConst ? "const " : "") << type << " " << stmt.name;
        if (!stmt.args.empty()) {
          if (isReference) {
            out << " = *(" << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds) << ")";
          } else {
            out << " = " << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
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
          if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
            return false;
          }
          if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
            return false;
          }
          const std::string resolved = resolveExprPath(candidate);
          return nameMap.count(resolved) == 0;
        };
        out << pad << "if (" << emitExpr(cond, nameMap, paramMap, localTypes, returnKinds) << ") {\n";
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
      if (stmt.kind == Expr::Kind::Call && isRepeatCall(stmt)) {
        const int repeatIndex = repeatCounter++;
        const std::string endVar = "ps_repeat_end_" + std::to_string(repeatIndex);
        const std::string indexVar = "ps_repeat_i_" + std::to_string(repeatIndex);
        out << pad << "{\n";
        std::string innerPad(static_cast<size_t>(indent + 1) * 2, ' ');
        std::string countExpr = "\"\"";
        if (!stmt.args.empty()) {
          countExpr = emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
        }
        out << innerPad << "auto " << endVar << " = " << countExpr << ";\n";
        out << innerPad << "for (decltype(" << endVar << ") " << indexVar << " = 0; " << indexVar << " < " << endVar
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
        std::string full = resolveExprPath(stmt);
        if (stmt.isMethodCall && !isArrayCountCall(stmt, localTypes) && !isMapCountCall(stmt, localTypes)) {
          std::string methodPath;
          if (resolveMethodCallPath(stmt, localTypes, returnKinds, methodPath)) {
            full = methodPath;
          }
        }
        auto it = nameMap.find(full);
        if (it == nameMap.end()) {
          out << pad << emitExpr(stmt, nameMap, paramMap, localTypes, returnKinds) << ";\n";
          return;
        }
        out << pad << it->second << "(";
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
          out << emitExpr(*orderedArgs[i], nameMap, paramMap, localTypes, returnKinds);
        }
        if (!orderedArgs.empty()) {
          out << ", ";
        }
        out << "[&]() {\n";
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 1, localTypes);
        }
        out << pad << "}";
        out << ");\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinAssign(stmt, nameMap) && stmt.args.size() == 2 &&
          stmt.args.front().kind == Expr::Kind::Name) {
        out << pad << stmt.args.front().name << " = "
            << emitExpr(stmt.args[1], nameMap, paramMap, localTypes, returnKinds) << ";\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isPathSpaceBuiltinName(stmt) && nameMap.find(resolveExprPath(stmt)) == nameMap.end()) {
        for (const auto &arg : stmt.args) {
          out << pad << "(void)(" << emitExpr(arg, nameMap, paramMap, localTypes, returnKinds) << ");\n";
        }
        return;
      }
      out << pad << emitExpr(stmt, nameMap, paramMap, localTypes, returnKinds) << ";\n";
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

} // namespace primec
