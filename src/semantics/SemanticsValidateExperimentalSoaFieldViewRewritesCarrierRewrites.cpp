// soa-surface-audit: exempt
#include "SemanticsValidateExperimentalSoaFieldViewRewrites.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateExperimentalSoaMethodRewrites.h"
#include "SemanticsValidateSoaBindingExtraction.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CompileArena.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec {

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewCarrierIndexStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, std::string> fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        stmt, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = fieldViewBindings;
      rewriteExperimentalSoaFieldViewCarrierIndexStatements(
          stmt.bodyArguments, bodyBindings, structFieldNames, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto parsed = extractParsedBindingInfo(stmt, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[stmt.name] = elemType;
        }
      }
      if (stmt.args.size() == 1 && fieldViewBindings.count(stmt.name) == 0) {
        const Expr &initializer = stmt.args.front();
        if (initializer.kind == Expr::Kind::Call && !initializer.isBinding) {
          std::string initPath = initializer.name;
          if (!initPath.empty() && initPath.front() != '/') {
            initPath.insert(initPath.begin(), '/');
          }
          if (semantics::isExperimentalSoaFieldViewHelperPath(initPath)) {
            if (initializer.templateArgs.size() >= 2) {
              fieldViewBindings[stmt.name] =
                  qualifySoaFieldViewTypeText(initializer.templateArgs[1],
                                              definitionNamespace,
                                              structPaths);
            }
          }
        }
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call ||
      expr.templateArgs.size() != 0 || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || semantics::hasNamedArguments(expr.argNames) ||
      expr.args.size() != 2) {
    return;
  }

  std::string builtinAccessName;
  if (!semantics::getBuiltinArrayAccessName(expr, builtinAccessName) ||
      builtinAccessName != "at") {
    return;
  }

  const Expr &fieldViewExpr = expr.args.front();
  std::string elemType;
  if (fieldViewExpr.kind == Expr::Kind::Name) {
    auto bindingIt = fieldViewBindings.find(fieldViewExpr.name);
    if (bindingIt != fieldViewBindings.end()) {
      elemType = bindingIt->second;
    }
  } else if (fieldViewExpr.kind == Expr::Kind::Call && !fieldViewExpr.isBinding) {
    std::string callPath = fieldViewExpr.name;
    if (!callPath.empty() && callPath.front() != '/') {
      callPath.insert(callPath.begin(), '/');
    }
    if (callPath == collection_helpers::kCanonicalSoaFieldView &&
        fieldViewExpr.templateArgs.size() >= 2 &&
        fieldViewExpr.args.size() == 2 &&
        !semantics::hasNamedArguments(fieldViewExpr.argNames)) {
      const std::string structPath = semantics::resolveStructTypePath(
          fieldViewExpr.templateArgs.front(), definitionNamespace, structPaths);
      auto fieldsIt = structFieldNames.find(structPath);
      const auto fieldIndex =
          extractNonNegativeI32LiteralIndex(fieldViewExpr.args[1]);
      if (fieldsIt != structFieldNames.end() && fieldIndex.has_value() &&
          *fieldIndex < fieldsIt->second.size()) {
        Expr getCall;
        getCall.kind = Expr::Kind::Call;
        getCall.name = collection_helpers::kCanonicalSoaGet;
        getCall.templateArgs = {fieldViewExpr.templateArgs.front()};
        getCall.args.push_back(fieldViewExpr.args.front());
        getCall.args.push_back(expr.args[1]);
        getCall.argNames.resize(getCall.args.size());
        getCall.sourceLine = fieldViewExpr.sourceLine;
        getCall.sourceColumn = fieldViewExpr.sourceColumn;

        Expr fieldAccess;
        fieldAccess.kind = Expr::Kind::Call;
        fieldAccess.name = fieldsIt->second[*fieldIndex];
        fieldAccess.isMethodCall = true;
        fieldAccess.isFieldAccess = true;
        fieldAccess.args.push_back(std::move(getCall));
        fieldAccess.argNames.resize(fieldAccess.args.size());
        fieldAccess.sourceLine = fieldViewExpr.sourceLine;
        fieldAccess.sourceColumn = fieldViewExpr.sourceColumn;
        expr = std::move(fieldAccess);
        return;
      }
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(callPath)) {
      if (fieldViewExpr.templateArgs.size() >= 2) {
        elemType = qualifySoaFieldViewTypeText(fieldViewExpr.templateArgs[1],
                                               definitionNamespace,
                                               structPaths);
      }
    }
  }
  if (elemType.empty()) {
    return;
  }

  Expr readCall;
  readCall.kind = Expr::Kind::Call;
  readCall.name = collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRead");
  readCall.templateArgs = {elemType};
  readCall.args.push_back(fieldViewExpr);
  readCall.args.push_back(expr.args[1]);
  readCall.argNames.resize(readCall.args.size());
  readCall.sourceLine = expr.sourceLine;
  readCall.sourceColumn = expr.sourceColumn;
  expr = std::move(readCall);
}

bool rewriteExperimentalSoaFieldViewCarrierIndexes(Program &program, std::string &error) {
  error.clear();
  std::unordered_set<std::string> structPaths;
  std::unordered_map<std::string, std::vector<std::string>> structFieldNames;
  for (const Definition &def : program.definitions) {
    if (semantics::isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
      auto isStaticField = [](const Expr &stmt) {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      std::vector<std::string> fields;
      for (const Expr &stmt : def.statements) {
        if (stmt.isBinding && !isStaticField(stmt)) {
          fields.push_back(stmt.name);
        }
      }
      if (!fields.empty()) {
        structFieldNames.emplace(def.fullPath, std::move(fields));
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    for (const Expr &param : def.parameters) {
      if (auto parsed = extractParsedBindingInfo(param, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[param.name] = elemType;
        }
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewCarrierIndexStatements(
        def.statements, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          *def.returnExpr, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
  }
  for (auto &exec : program.executions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    std::string definitionNamespace;
    const size_t slash = exec.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = exec.fullPath.substr(0, slash);
    }
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewAssignTargetsExpr(Expr &expr) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(bodyArg);
  }

  if (!semantics::isAssignCall(expr) || expr.args.size() != 2) {
    return;
  }

  Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::Call && !target.isBinding) {
    // TODO-5235: built via systemHeapValue() so this magic static's backing
    // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
    static const std::string fieldRefPrefix = primec::systemHeapValue([] {
      return collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRef");
    });
    if (semantics::isExperimentalSoaFieldViewReadHelperPath(target.name) &&
        target.args.size() == 2 && !target.templateArgs.empty()) {
      Expr refCall;
      refCall.kind = Expr::Kind::Call;
      refCall.name = std::string(fieldRefPrefix);
      refCall.templateArgs = {target.templateArgs.front()};
      refCall.args = target.args;
      refCall.argNames.resize(refCall.args.size());
      refCall.sourceLine = target.sourceLine;
      refCall.sourceColumn = target.sourceColumn;

      Expr dereferenceCall;
      dereferenceCall.kind = Expr::Kind::Call;
      dereferenceCall.name = "dereference";
      dereferenceCall.args.push_back(std::move(refCall));
      dereferenceCall.argNames.resize(dereferenceCall.args.size());
      dereferenceCall.sourceLine = target.sourceLine;
      dereferenceCall.sourceColumn = target.sourceColumn;

      target = std::move(dereferenceCall);
      return;
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(target.name)) {
      return;
    }
  }
  if (target.kind != Expr::Kind::Call || !target.isFieldAccess ||
      target.args.size() != 1) {
    return;
  }

  Expr &receiver = target.args.front();
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return;
  }

  // TODO-5235: built via systemHeapValue() so these magic statics' backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::string getPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorGet");
  });
  static const std::string refPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorRef");
  });
  static const std::string fieldReadPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRead");
  });
  static const std::string fieldRefPrefix = primec::systemHeapValue([] {
    return collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "soaFieldViewRef");
  });
  auto rewriteSamePathSoaGetCarrierToRef = [](std::string &path) -> bool {
    const size_t specializationSuffix = path.find("__t");
    const std::string specializationText =
        specializationSuffix == std::string::npos
            ? std::string{}
            : path.substr(specializationSuffix);
    std::string basePath =
        specializationSuffix == std::string::npos
            ? path
            : path.substr(0, specializationSuffix);
    const std::string canonicalGetPath =
        semantics::canonicalizeLegacySoaGetHelperPath(basePath);
    if (canonicalGetPath == collection_helpers::kCanonicalSoaGet) {
      path = (collection_helpers::isRootedSoaPath(basePath) ? collection_helpers::kRootedSoaRef
                                                     : collection_helpers::kCanonicalSoaRef) +
             specializationText;
      return true;
    }
    if (canonicalGetPath == collection_helpers::kCanonicalSoaGet) {
      path = collection_helpers::kCanonicalSoaRef + specializationText;
      return true;
    }
    if (canonicalGetPath == collection_helpers::kCanonicalSoaGetRef) {
      path =
          (collection_helpers::isRootedSoaPath(basePath) ? collection_helpers::kRootedSoaRefRef
                                                  : collection_helpers::kCanonicalSoaRefRef) +
          specializationText;
      return true;
    }
    if (canonicalGetPath == collection_helpers::kCanonicalSoaGetRef) {
      path = collection_helpers::kCanonicalSoaRefRef + specializationText;
      return true;
    }
    return false;
  };
  if (!semantics::isExperimentalSoaGetLikeHelperPath(receiver.name)) {
    if (!semantics::isExperimentalSoaFieldViewReadHelperPath(receiver.name)) {
      if (!rewriteSamePathSoaGetCarrierToRef(receiver.name)) {
        return;
      }
      return;
    }
    receiver.name.replace(0, fieldReadPrefix.size(), fieldRefPrefix);
    return;
  }
  receiver.name.replace(0, getPrefix.size(), refPrefix);
}

bool rewriteExperimentalSoaFieldViewAssignTargets(Program &program,
                                                  std::string &error) {
  error.clear();
  for (Definition &def : program.definitions) {
    for (Expr &stmt : def.statements) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(stmt);
    }
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(*def.returnExpr);
    }
  }
  for (auto &exec : program.executions) {
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
  }
  return true;
}

} // namespace primec
