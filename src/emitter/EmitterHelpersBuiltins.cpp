#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterHelpers.h"

namespace primec::emitter {

bool isBuiltinAssign(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "assign";
}

bool getVectorMutatorName(const Expr &expr,
                          const std::unordered_map<std::string, std::string> &nameMap,
                          std::string &out) {
  auto stripGeneratedHelperSuffix = [](std::string helperName) {
    const size_t generatedSuffix = helperName.find("__");
    if (generatedSuffix != std::string::npos) {
      helperName.erase(generatedSuffix);
    }
    return helperName;
  };
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    name = stripGeneratedHelperSuffix(name.substr(std::string("std/collections/vector/").size()));
  } else if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "push" || name == "pop" || name == "reserve" || name == "clear" || name == "remove_at" ||
      name == "remove_swap") {
    out = name;
    return true;
  }
  return false;
}

std::vector<const Expr *> orderCallArguments(const Expr &expr,
                                             const std::string &resolvedPath,
                                             const std::vector<Expr> &params,
                                             const std::unordered_map<std::string, BindingInfo> &localTypes) {
  std::vector<const Expr *> ordered;
  size_t callArgStart = 0;
  if (expr.isMethodCall && !expr.args.empty() && !resolvedPath.empty()) {
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name && localTypes.find(receiver.name) == localTypes.end()) {
      const size_t methodSlash = resolvedPath.find_last_of('/');
      if (methodSlash != std::string::npos && methodSlash > 0) {
        const std::string receiverPath = resolvedPath.substr(0, methodSlash);
        const size_t receiverSlash = receiverPath.find_last_of('/');
        const std::string receiverTypeName =
            receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
        if (receiverTypeName == receiver.name) {
          callArgStart = 1;
        }
      }
    }
  }
  ordered.reserve(expr.args.size() >= callArgStart ? expr.args.size() - callArgStart : 0);
  auto fallback = [&]() {
    ordered.clear();
    ordered.reserve(expr.args.size() >= callArgStart ? expr.args.size() - callArgStart : 0);
    for (size_t i = callArgStart; i < expr.args.size(); ++i) {
      ordered.push_back(&expr.args[i]);
    }
  };
  auto isCollectionBindingType = [](const std::string &typeName) {
    const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
    return normalizedTypeName == "array" || normalizedTypeName == "vector" ||
           isMapCollectionTypeNameLocal(normalizedTypeName) || normalizedTypeName == "string" ||
           normalizedTypeName == "soa_vector";
  };
  const size_t slicedArgCount = expr.args.size() >= callArgStart ? expr.args.size() - callArgStart : 0;
  if (!hasNamedArguments(expr.argNames) && slicedArgCount == 2 && params.size() == 2 &&
      params[0].name == "values" && isCollectionBindingType(getBindingInfo(params[0]).typeName)) {
    const Expr &first = expr.args[callArgStart];
    const bool leadingNonReceiver =
        first.kind == Expr::Kind::Literal || first.kind == Expr::Kind::BoolLiteral ||
        first.kind == Expr::Kind::FloatLiteral || first.kind == Expr::Kind::StringLiteral;
    if (leadingNonReceiver) {
      ordered.assign(2, nullptr);
      ordered[0] = &expr.args[callArgStart + 1];
      ordered[1] = &expr.args[callArgStart];
      return ordered;
    }
  }
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = callArgStart; i < expr.args.size(); ++i) {
    if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
      const std::string &name = *expr.argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        fallback();
        return ordered;
      }
      ordered[index] = &expr.args[i];
      continue;
    }
    while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= ordered.size()) {
      fallback();
      return ordered;
    }
    ordered[positionalIndex] = &expr.args[i];
    ++positionalIndex;
  }
  for (size_t i = 0; i < ordered.size(); ++i) {
    if (ordered[i] != nullptr) {
      continue;
    }
    if (!params[i].args.empty()) {
      ordered[i] = &params[i].args.front();
    }
  }
  for (const auto *arg : ordered) {
    if (arg == nullptr) {
      fallback();
      return ordered;
    }
  }
  return ordered;
}

bool isBuiltinIf(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  return isSimpleCallName(expr, "if");
}

bool isBuiltinBlock(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  return isSimpleCallName(expr, "block");
}

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, "loop");
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, "while");
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, "for");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

} // namespace primec::emitter
