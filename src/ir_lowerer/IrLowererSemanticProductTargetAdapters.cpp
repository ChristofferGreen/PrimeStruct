#include "IrLowererSemanticProductTargetAdapters.h"

#include <string_view>

namespace primec::ir_lowerer {
namespace {

std::string makeTargetLookupKey(int sourceLine, int sourceColumn, std::string_view name) {
  if (sourceLine <= 0 || sourceColumn <= 0 || name.empty()) {
    return {};
  }
  return std::to_string(sourceLine) + ":" + std::to_string(sourceColumn) + ":" + std::string(name);
}

std::string makeTargetLookupKey(const Expr &expr, std::string_view name) {
  return makeTargetLookupKey(expr.sourceLine, expr.sourceColumn, name);
}

std::string normalizedSemanticProductBridgeHelperName(const Expr &expr) {
  auto stripTemplateSuffix = [](std::string &helperName) {
    const size_t specializationSuffix = helperName.find("__t");
    if (specializationSuffix != std::string::npos) {
      helperName.erase(specializationSuffix);
    }
  };

  auto consumePrefixedHelper = [&](std::string_view prefix) -> std::string {
    if (expr.name.rfind(prefix, 0) != 0) {
      return {};
    }
    std::string helperName = expr.name.substr(prefix.size());
    stripTemplateSuffix(helperName);
    return helperName;
  };

  if (std::string helperName = consumePrefixedHelper("/vector/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/std/collections/vector/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/map/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/std/collections/map/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/soa_vector/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/std/collections/soa_vector/"); !helperName.empty()) {
    return helperName;
  }
  return {};
}

} // namespace

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }

  adapter.directCallTargetsByExpr.reserve(semanticProgram->directCallTargets.size());
  for (const auto &entry : semanticProgram->directCallTargets) {
    if (std::string key = makeTargetLookupKey(entry.sourceLine, entry.sourceColumn, entry.callName);
        !key.empty()) {
      adapter.directCallTargetsByExpr[key] = entry.resolvedPath;
    }
  }

  adapter.methodCallTargetsByExpr.reserve(semanticProgram->methodCallTargets.size());
  for (const auto &entry : semanticProgram->methodCallTargets) {
    if (std::string key = makeTargetLookupKey(entry.sourceLine, entry.sourceColumn, entry.methodName);
        !key.empty()) {
      adapter.methodCallTargetsByExpr[key] = entry.resolvedPath;
    }
  }

  adapter.bridgePathChoicesByExpr.reserve(semanticProgram->bridgePathChoices.size());
  for (const auto &entry : semanticProgram->bridgePathChoices) {
    if (std::string key = makeTargetLookupKey(entry.sourceLine, entry.sourceColumn, entry.helperName);
        !key.empty()) {
      adapter.bridgePathChoicesByExpr[key] = entry.chosenPath;
    }
  }

  return adapter;
}

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  const std::string key = makeTargetLookupKey(expr, expr.name);
  if (key.empty()) {
    return {};
  }
  if (const auto it = adapter.directCallTargetsByExpr.find(key); it != adapter.directCallTargetsByExpr.end()) {
    return it->second;
  }
  return {};
}

std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  const std::string key = makeTargetLookupKey(expr, expr.name);
  if (key.empty()) {
    return {};
  }
  if (const auto it = adapter.methodCallTargetsByExpr.find(key); it != adapter.methodCallTargetsByExpr.end()) {
    return it->second;
  }
  return {};
}

std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  const std::string helperName = normalizedSemanticProductBridgeHelperName(expr);
  if (helperName.empty()) {
    return {};
  }
  const std::string key = makeTargetLookupKey(expr, helperName);
  if (key.empty()) {
    return {};
  }
  if (const auto it = adapter.bridgePathChoicesByExpr.find(key); it != adapter.bridgePathChoicesByExpr.end()) {
    return it->second;
  }
  return {};
}

} // namespace primec::ir_lowerer
