#include "primec/Semantics.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include "SemanticsValidateConvertConstructors.h"
#include "SemanticsValidateExperimentalGfxConstructors.h"
#include "SemanticsValidateReflectionGeneratedHelpers.h"
#include "SemanticsValidateReflectionMetadata.h"
#include "SemanticsValidateTransforms.h"
#include "SemanticsValidator.h"
#include "TypeResolutionGraphPreparation.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec {

namespace semantics {
bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error);

namespace {

std::string returnKindSnapshotName(ReturnKind kind) {
  switch (kind) {
    case ReturnKind::Unknown:
      return "unknown";
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Void:
      return "void";
    case ReturnKind::Array:
      return "array";
  }
  return "unknown";
}

std::string bindingTypeTextForSnapshot(const BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

} // namespace
}

namespace {

bool rewriteOmittedStructInitializers(Program &program, std::string &error) {
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
      fieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          fieldOnlyStruct = false;
          break;
        }
      }
    }
    if (hasStructTransform || fieldOnlyStruct) {
      structNames.insert(def.fullPath);
    }
  }

  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool sawPublic = false;
    bool sawPrivate = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "public") {
        sawPublic = true;
      } else if (transform.name == "private") {
        sawPrivate = true;
      }
    }
    if (sawPublic && !sawPrivate) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
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
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      std::string scopedPrefix = prefix;
      if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
        scopedPrefix += "/";
      }
      for (const auto &def : program.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions.count(def.fullPath) == 0) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (publicDefinitions.count(importPath) == 0) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }

  auto rewriteExpr = [&](auto &self, Expr &expr) -> bool {
    for (auto &arg : expr.args) {
      if (!self(self, arg)) {
        return false;
      }
    }
    for (auto &arg : expr.bodyArguments) {
      if (!self(self, arg)) {
        return false;
      }
    }
    auto isEmptyBuiltinBlockInitializer = [&](const Expr &initializer) -> bool {
      if (!initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
        return false;
      }
      if (!initializer.args.empty() || !initializer.templateArgs.empty() ||
          semantics::hasNamedArguments(initializer.argNames)) {
        return false;
      }
      return initializer.kind == Expr::Kind::Call && !initializer.isBinding &&
             initializer.name == "block" && initializer.namespacePrefix.empty();
    };
    const bool omittedInitializer = expr.args.empty() ||
                                    (expr.args.size() == 1 && isEmptyBuiltinBlockInitializer(expr.args.front()));
    if (!expr.isBinding || !omittedInitializer) {
      return true;
    }
    semantics::BindingInfo info;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!semantics::parseBindingInfo(expr, expr.namespacePrefix, structNames, importAliases, info, restrictType, parseError)) {
      error = parseError;
      return false;
    }
    const std::string normalizedType = semantics::normalizeBindingTypeName(info.typeName);
    if (!info.typeTemplateArg.empty()) {
      if (normalizedType != "vector") {
        error = "omitted initializer requires struct type: " + info.typeName;
        return false;
      }
      std::vector<std::string> templateArgs;
      if (!semantics::splitTopLevelTemplateArgs(info.typeTemplateArg, templateArgs) || templateArgs.size() != 1) {
        error = "vector requires exactly one template argument";
        return false;
      }
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = info.typeName;
      call.namespacePrefix = expr.namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      expr.args.clear();
      expr.argNames.clear();
      expr.args.push_back(std::move(call));
      expr.argNames.push_back(std::nullopt);
      return true;
    }
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = info.typeName;
    call.namespacePrefix = expr.namespacePrefix;
    expr.args.clear();
    expr.argNames.clear();
    expr.args.push_back(std::move(call));
    expr.argNames.push_back(std::nullopt);
    return true;
  };

  for (auto &def : program.definitions) {
    for (auto &stmt : def.statements) {
      if (!rewriteExpr(rewriteExpr, stmt)) {
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      if (!rewriteExpr(rewriteExpr, *def.returnExpr)) {
        return false;
      }
    }
  }
  for (auto &exec : program.executions) {
    for (auto &arg : exec.arguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
    for (auto &arg : exec.bodyArguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
  }
  return true;
}
} // namespace

bool Semantics::validate(Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects,
                         const std::vector<std::string> &entryDefaultEffects,
                         const std::vector<std::string> &semanticTransforms,
                         SemanticDiagnosticInfo *diagnosticInfo,
                         bool collectDiagnostics) const {
  error.clear();
  DiagnosticSink diagnosticSink(diagnosticInfo);
  diagnosticSink.reset();
  bool validationSucceeded = false;
  struct ValidationDiagnosticScope {
    DiagnosticSink &diagnosticSink;
    std::string &error;
    bool &validationSucceeded;

    ~ValidationDiagnosticScope() {
      if (!validationSucceeded && !error.empty()) {
        diagnosticSink.setSummary(error);
      }
    }
  } validationDiagnosticScope{diagnosticSink, error, validationSucceeded};
  if (!semantics::applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  if (!semantics::rewriteExperimentalGfxConstructors(program, error)) {
    return false;
  }
  if (!semantics::rewriteReflectionGeneratedHelpers(program, error)) {
    return false;
  }
  if (!semantics::monomorphizeTemplates(program, entryPath, error)) {
    return false;
  }
  if (!semantics::rewriteReflectionMetadataQueries(program, error)) {
    return false;
  }
  if (!semantics::rewriteConvertConstructors(program, error)) {
    return false;
  }
  semantics::SemanticsValidator validator(
      program, entryPath, error, defaultEffects, entryDefaultEffects, diagnosticInfo, collectDiagnostics);
  if (!validator.run()) {
    return false;
  }
  if (!rewriteOmittedStructInitializers(program, error)) {
    return false;
  }
  error.clear();
  validationSucceeded = true;
  return true;
}

bool semantics::computeTypeResolutionReturnSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionReturnSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  error.clear();
  if (!semantics::prepareProgramForTypeResolutionAnalysis(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  semantics::SemanticsValidator validator(program, entryPath, error, defaults, defaults, nullptr, false);
  if (!validator.run()) {
    return false;
  }

  const auto entries = validator.returnResolutionSnapshotForTesting();
  out.entries.reserve(entries.size());
  for (const auto &entry : entries) {
    out.entries.push_back(TypeResolutionReturnSnapshotEntry{
        entry.definitionPath,
        returnKindSnapshotName(entry.kind),
        entry.structPath,
        bindingTypeTextForSnapshot(entry.binding),
    });
  }
  return true;
}

bool semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionLocalBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  error.clear();
  if (!semantics::prepareProgramForTypeResolutionAnalysis(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  semantics::SemanticsValidator validator(program, entryPath, error, defaults, defaults, nullptr, false);
  if (!validator.run()) {
    return false;
  }

  const auto entries = validator.localAutoBindingSnapshotForTesting();
  out.entries.reserve(entries.size());
  for (const auto &entry : entries) {
    out.entries.push_back(TypeResolutionLocalBindingSnapshotEntry{
        entry.scopePath,
        entry.bindingName,
        entry.sourceLine,
        entry.sourceColumn,
        bindingTypeTextForSnapshot(entry.binding),
        entry.initializerQueryTypeText,
    });
  }
  return true;
}

bool semantics::computeTypeResolutionQueryCallSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryCallSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  error.clear();
  if (!semantics::prepareProgramForTypeResolutionAnalysis(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  semantics::SemanticsValidator validator(program, entryPath, error, defaults, defaults, nullptr, false);
  if (!validator.run()) {
    return false;
  }

  const auto entries = validator.queryCallTypeSnapshotForTesting();
  out.entries.reserve(entries.size());
  for (const auto &entry : entries) {
    out.entries.push_back(TypeResolutionQueryCallSnapshotEntry{
        entry.scopePath,
        entry.callName,
        entry.resolvedPath,
        entry.sourceLine,
        entry.sourceColumn,
        entry.typeText,
    });
  }
  return true;
}

bool semantics::computeTypeResolutionCallBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionCallBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  error.clear();
  if (!semantics::prepareProgramForTypeResolutionAnalysis(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  semantics::SemanticsValidator validator(program, entryPath, error, defaults, defaults, nullptr, false);
  if (!validator.run()) {
    return false;
  }

  const auto entries = validator.callBindingSnapshotForTesting();
  out.entries.reserve(entries.size());
  for (const auto &entry : entries) {
    out.entries.push_back(TypeResolutionCallBindingSnapshotEntry{
        entry.scopePath,
        entry.callName,
        entry.resolvedPath,
        entry.sourceLine,
        entry.sourceColumn,
        bindingTypeTextForSnapshot(entry.binding),
    });
  }
  return true;
}

} // namespace primec
