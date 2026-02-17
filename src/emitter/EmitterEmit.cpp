#include "primec/Emitter.h"

#include "EmitterHelpers.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace primec {
using namespace emitter;

std::string Emitter::emitCpp(const Program &program, const std::string &entryPath) const {
  std::unordered_map<std::string, std::string> nameMap;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, std::vector<Expr>> paramMap;
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> importAliases;
  bool hasMathImport = false;
  auto isMathImport = [](const std::string &path) -> bool {
    if (path == "/math/*") {
      return true;
    }
    return path.rfind("/math/", 0) == 0 && path.size() > 6;
  };
  for (const auto &importPath : program.imports) {
    if (isMathImport(importPath)) {
      hasMathImport = true;
      break;
    }
  }
  auto isStructTransformName = [](const std::string &name) {
    return name == "struct" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
           name == "platform_independent_padding";
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
  enum class HelperKind { Create, Destroy };
  struct HelperSuffixInfo {
    std::string_view suffix;
    HelperKind kind;
    std::string_view placement;
  };
  auto matchLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, HelperKind &kindOut, std::string &placementOut) -> bool {
    static const std::array<HelperSuffixInfo, 8> suffixes = {{
        {"Create", HelperKind::Create, ""},
        {"Destroy", HelperKind::Destroy, ""},
        {"CreateStack", HelperKind::Create, "stack"},
        {"DestroyStack", HelperKind::Destroy, "stack"},
        {"CreateHeap", HelperKind::Create, "heap"},
        {"DestroyHeap", HelperKind::Destroy, "heap"},
        {"CreateBuffer", HelperKind::Create, "buffer"},
        {"DestroyBuffer", HelperKind::Destroy, "buffer"},
    }};
    for (const auto &info : suffixes) {
      const std::string_view suffix = info.suffix;
      if (fullPath.size() < suffix.size() + 1) {
        continue;
      }
      const size_t suffixStart = fullPath.size() - suffix.size();
      if (fullPath[suffixStart - 1] != '/') {
        continue;
      }
      if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
        continue;
      }
      parentOut = fullPath.substr(0, suffixStart - 1);
      kindOut = info.kind;
      placementOut = std::string(info.placement);
      return true;
    }
    return false;
  };
  auto isLifecycleHelper = [&](const Definition &def, std::string &parentOut) {
    HelperKind kind = HelperKind::Create;
    std::string placement;
    if (!matchLifecycleHelper(def.fullPath, parentOut, kind, placement)) {
      parentOut.clear();
      return false;
    }
    return true;
  };
  auto isHelperMutable = [](const Definition &def) {
    for (const auto &transform : def.transforms) {
      if (transform.name == "mut") {
        return true;
      }
    }
    return false;
  };
  struct LifecycleHelpers {
    const Definition *create = nullptr;
    const Definition *destroy = nullptr;
    const Definition *createStack = nullptr;
    const Definition *destroyStack = nullptr;
  };
  std::unordered_map<std::string, LifecycleHelpers> helpersByStruct;

  for (const auto &def : program.definitions) {
    if (isStructDefinition(def)) {
      structTypeMap[def.fullPath] = toCppName(def.fullPath);
    }
    std::string parentPath;
    HelperKind kind = HelperKind::Create;
    std::string placement;
    if (matchLifecycleHelper(def.fullPath, parentPath, kind, placement)) {
      auto &helpers = helpersByStruct[parentPath];
      if (placement == "stack") {
        if (kind == HelperKind::Create) {
          helpers.createStack = &def;
        } else {
          helpers.destroyStack = &def;
        }
      } else if (placement.empty()) {
        if (kind == HelperKind::Create) {
          helpers.create = &def;
        } else {
          helpers.destroy = &def;
        }
      }
    }
  }

  auto makeThisParam = [&](const std::string &structPath, bool isMutable) {
    Expr param;
    param.kind = Expr::Kind::Name;
    param.isBinding = true;
    param.name = "__self";
    Transform typeTransform;
    typeTransform.name = "Reference";
    typeTransform.templateArgs.push_back(structPath);
    param.transforms.push_back(std::move(typeTransform));
    if (isMutable) {
      Transform mutTransform;
      mutTransform.name = "mut";
      param.transforms.push_back(std::move(mutTransform));
    }
    return param;
  };

  auto returnTypeFor = [&](const Definition &def) {
    ReturnKind returnKind = returnKinds[def.fullPath];
    if (returnKind == ReturnKind::Void) {
      return std::string("void");
    }
    if (returnKind == ReturnKind::Int64) {
      return std::string("int64_t");
    }
    if (returnKind == ReturnKind::UInt64) {
      return std::string("uint64_t");
    }
    if (returnKind == ReturnKind::Float32) {
      return std::string("float");
    }
    if (returnKind == ReturnKind::Float64) {
      return std::string("double");
    }
    if (returnKind == ReturnKind::Bool) {
      return std::string("bool");
    }
    return std::string("int");
  };
  auto appendParam = [&](std::ostringstream &out,
                         const Expr &param,
                         const std::string &typeNamespace) {
    BindingInfo paramInfo = getBindingInfo(param);
    std::string paramType =
        bindingTypeToCpp(paramInfo, typeNamespace, importAliases, structTypeMap);
    const bool refCandidate = isReferenceCandidate(paramInfo);
    const bool passByRef = refCandidate && !paramInfo.isCopy;
    if (passByRef) {
      if (!paramInfo.isMutable && paramType.rfind("const ", 0) != 0) {
        out << "const ";
      }
      out << paramType << " & " << param.name;
      return;
    }
    bool needsConst = !paramInfo.isMutable;
    if (needsConst && paramType.rfind("const ", 0) == 0) {
      needsConst = false;
    }
    out << (needsConst ? "const " : "") << paramType << " " << param.name;
  };

  for (const auto &def : program.definitions) {
    if (isStructDefinition(def)) {
      nameMap[def.fullPath] = toCppName(def.fullPath) + "_ctor";
      std::vector<Expr> instanceFields;
      instanceFields.reserve(def.statements.size());
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          continue;
        }
        if (getBindingInfo(stmt).isStatic) {
          continue;
        }
        instanceFields.push_back(stmt);
      }
      paramMap[def.fullPath] = std::move(instanceFields);
    } else {
      nameMap[def.fullPath] = toCppName(def.fullPath);
      std::string parentPath;
      if (isLifecycleHelper(def, parentPath)) {
        std::vector<Expr> params;
        params.reserve(def.parameters.size() + 1);
        params.push_back(makeThisParam(parentPath, isHelperMutable(def)));
        for (const auto &param : def.parameters) {
          params.push_back(param);
        }
        paramMap[def.fullPath] = std::move(params);
      } else {
        paramMap[def.fullPath] = def.parameters;
      }
    }
    defMap[def.fullPath] = &def;
    returnKinds[def.fullPath] = getReturnKind(def);
  }

  auto isWildcardImport = [](const std::string &path, std::string &prefixOut) -> bool {
    if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
      prefixOut = path.substr(0, path.size() - 2);
      return true;
    }
    if (path.find('/', 1) == std::string::npos) {
      prefixOut = path;
      return true;
    }
    return false;
  };
  for (const auto &importPath : program.imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      const std::string scopedPrefix = prefix + "/";
      for (const auto &def : program.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        const std::string rootPath = "/" + remainder;
        if (nameMap.count(rootPath) > 0) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    auto defIt = defMap.find(importPath);
    if (defIt == defMap.end()) {
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    const std::string rootPath = "/" + remainder;
    if (nameMap.count(rootPath) > 0) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
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
      if (isBuiltinMathConstantName(expr.name, hasMathImport)) {
        return ReturnKind::Float64;
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
          if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
            return false;
          }
          if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
            return false;
          }
          return true;
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
      if (isBuiltinClamp(expr, hasMathImport)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      std::string mathName;
      if (getBuiltinMathName(expr, mathName, hasMathImport)) {
        if (mathName == "is_nan" || mathName == "is_inf" || mathName == "is_finite") {
          return ReturnKind::Bool;
        }
        if (mathName == "lerp" && expr.args.size() == 3) {
          ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
          result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
          result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
          return result;
        }
        if (mathName == "pow" && expr.args.size() == 2) {
          ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
          result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
          return result;
        }
        if (expr.args.empty()) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Float64) {
          return ReturnKind::Float64;
        }
        if (argKind == ReturnKind::Float32) {
          return ReturnKind::Float32;
        }
        return ReturnKind::Unknown;
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
  out << "#include <cmath>\n";
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
  out << "static constexpr double ps_const_pi = 3.14159265358979323846;\n";
  out << "static constexpr double ps_const_tau = 6.28318530717958647692;\n";
  out << "static constexpr double ps_const_e = 2.71828182845904523536;\n";
  out << "template <typename T, typename U, typename V>\n";
  out << "static inline std::common_type_t<T, U, V> ps_builtin_lerp(T start, U end, V t) {\n";
  out << "  using R = std::common_type_t<T, U, V>;\n";
  out << "  R a = static_cast<R>(start);\n";
  out << "  R b = static_cast<R>(end);\n";
  out << "  R tt = static_cast<R>(t);\n";
  out << "  return a + (b - a) * tt;\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_floor(T value) {\n";
  out << "  return static_cast<T>(std::floor(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_ceil(T value) {\n";
  out << "  return static_cast<T>(std::ceil(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_round(T value) {\n";
  out << "  return static_cast<T>(std::round(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_trunc(T value) {\n";
  out << "  return static_cast<T>(std::trunc(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_fract(T value) {\n";
  out << "  long double v = static_cast<long double>(value);\n";
  out << "  return static_cast<T>(v - std::floor(v));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_sqrt(T value) {\n";
  out << "  return static_cast<T>(std::sqrt(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_cbrt(T value) {\n";
  out << "  return static_cast<T>(std::cbrt(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T, typename U>\n";
  out << "static inline std::common_type_t<T, U> ps_builtin_pow(T base, U exponent) {\n";
  out << "  using R = std::common_type_t<T, U>;\n";
  out << "  if constexpr (std::is_integral_v<R>) {\n";
  out << "    if constexpr (std::is_signed_v<U>) {\n";
  out << "      if (exponent < 0) {\n";
  out << "        std::fprintf(stderr, \"pow exponent must be non-negative\\n\");\n";
  out << "        std::exit(3);\n";
  out << "      }\n";
  out << "    }\n";
  out << "    using Unsigned = std::make_unsigned_t<R>;\n";
  out << "    Unsigned exp = static_cast<Unsigned>(exponent);\n";
  out << "    R result = static_cast<R>(1);\n";
  out << "    R factor = static_cast<R>(base);\n";
  out << "    while (exp > 0) {\n";
  out << "      if (exp & static_cast<Unsigned>(1)) {\n";
  out << "        result = static_cast<R>(result * factor);\n";
  out << "      }\n";
  out << "      exp >>= static_cast<Unsigned>(1);\n";
  out << "      if (exp == 0) {\n";
  out << "        break;\n";
  out << "      }\n";
  out << "      factor = static_cast<R>(factor * factor);\n";
  out << "    }\n";
  out << "    return result;\n";
  out << "  }\n";
  out << "  return static_cast<R>(std::pow(static_cast<long double>(base), static_cast<long double>(exponent)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_exp(T value) {\n";
  out << "  return static_cast<T>(std::exp(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_exp2(T value) {\n";
  out << "  return static_cast<T>(std::exp2(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_log(T value) {\n";
  out << "  return static_cast<T>(std::log(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_log2(T value) {\n";
  out << "  return static_cast<T>(std::log2(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_log10(T value) {\n";
  out << "  return static_cast<T>(std::log10(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_sin(T value) {\n";
  out << "  return static_cast<T>(std::sin(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_cos(T value) {\n";
  out << "  return static_cast<T>(std::cos(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_tan(T value) {\n";
  out << "  return static_cast<T>(std::tan(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_asin(T value) {\n";
  out << "  return static_cast<T>(std::asin(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_acos(T value) {\n";
  out << "  return static_cast<T>(std::acos(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_atan(T value) {\n";
  out << "  return static_cast<T>(std::atan(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T, typename U>\n";
  out << "static inline std::common_type_t<T, U> ps_builtin_atan2(T y, U x) {\n";
  out << "  using R = std::common_type_t<T, U>;\n";
  out << "  return static_cast<R>(std::atan2(static_cast<long double>(y), static_cast<long double>(x)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_radians(T value) {\n";
  out << "  return static_cast<T>(static_cast<long double>(value) * (ps_const_pi / 180.0));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_degrees(T value) {\n";
  out << "  return static_cast<T>(static_cast<long double>(value) * (180.0 / ps_const_pi));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_sinh(T value) {\n";
  out << "  return static_cast<T>(std::sinh(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_cosh(T value) {\n";
  out << "  return static_cast<T>(std::cosh(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_tanh(T value) {\n";
  out << "  return static_cast<T>(std::tanh(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_asinh(T value) {\n";
  out << "  return static_cast<T>(std::asinh(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_acosh(T value) {\n";
  out << "  return static_cast<T>(std::acosh(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline T ps_builtin_atanh(T value) {\n";
  out << "  return static_cast<T>(std::atanh(static_cast<long double>(value)));\n";
  out << "}\n";
  out << "template <typename T, typename U, typename V>\n";
  out << "static inline std::common_type_t<T, U, V> ps_builtin_fma(T a, U b, V c) {\n";
  out << "  using R = std::common_type_t<T, U, V>;\n";
  out << "  return static_cast<R>(std::fma(static_cast<long double>(a), static_cast<long double>(b), static_cast<long double>(c)));\n";
  out << "}\n";
  out << "template <typename T, typename U>\n";
  out << "static inline std::common_type_t<T, U> ps_builtin_hypot(T a, U b) {\n";
  out << "  using R = std::common_type_t<T, U>;\n";
  out << "  return static_cast<R>(std::hypot(static_cast<long double>(a), static_cast<long double>(b)));\n";
  out << "}\n";
  out << "template <typename T, typename U>\n";
  out << "static inline std::common_type_t<T, U> ps_builtin_copysign(T mag, U sign) {\n";
  out << "  using R = std::common_type_t<T, U>;\n";
  out << "  return static_cast<R>(std::copysign(static_cast<long double>(mag), static_cast<long double>(sign)));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline bool ps_builtin_is_nan(T value) {\n";
  out << "  return std::isnan(static_cast<long double>(value));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline bool ps_builtin_is_inf(T value) {\n";
  out << "  return std::isinf(static_cast<long double>(value));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline bool ps_builtin_is_finite(T value) {\n";
  out << "  return std::isfinite(static_cast<long double>(value));\n";
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
  out << "template <typename T>\n";
  out << "static inline int ps_vector_capacity(const std::vector<T> &value) {\n";
  out << "  return static_cast<int>(value.capacity());\n";
  out << "}\n";
  out << "template <typename T, typename Index>\n";
  out << "static inline void ps_vector_reserve(std::vector<T> &value, Index capacity) {\n";
  out << "  int64_t cap = static_cast<int64_t>(capacity);\n";
  out << "  if (cap < 0) {\n";
  out << "    std::fprintf(stderr, \"vector reserve expects non-negative capacity\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  value.reserve(static_cast<size_t>(cap));\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline void ps_vector_pop(std::vector<T> &value) {\n";
  out << "  if (value.empty()) {\n";
  out << "    std::fprintf(stderr, \"vector pop on empty\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  value.pop_back();\n";
  out << "}\n";
  out << "template <typename T, typename Index>\n";
  out << "static inline void ps_vector_remove_at(std::vector<T> &value, Index index) {\n";
  out << "  int64_t i = static_cast<int64_t>(index);\n";
  out << "  if (i < 0 || static_cast<size_t>(i) >= value.size()) {\n";
  out << "    std::fprintf(stderr, \"vector index out of bounds\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  value.erase(value.begin() + static_cast<size_t>(i));\n";
  out << "}\n";
  out << "template <typename T, typename Index>\n";
  out << "static inline void ps_vector_remove_swap(std::vector<T> &value, Index index) {\n";
  out << "  int64_t i = static_cast<int64_t>(index);\n";
  out << "  if (i < 0 || static_cast<size_t>(i) >= value.size()) {\n";
  out << "    std::fprintf(stderr, \"vector index out of bounds\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  size_t idx = static_cast<size_t>(i);\n";
  out << "  if (idx + 1 < value.size()) {\n";
  out << "    value[idx] = std::move(value.back());\n";
  out << "  }\n";
  out << "  value.pop_back();\n";
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
        std::string full = resolveExprPath(stmt);
        if (stmt.isMethodCall && !isArrayCountCall(stmt, localTypes) && !isMapCountCall(stmt, localTypes)) {
          std::string methodPath;
          if (resolveMethodCallPath(stmt, localTypes, importAliases, returnKinds, methodPath)) {
            full = methodPath;
          }
        }
        auto it = nameMap.find(full);
        if (it == nameMap.end()) {
          out << pad << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport) << ";\n";
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
          out << emitExpr(*orderedArgs[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, hasMathImport);
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

} // namespace primec
