#include "SemanticsValidateTransforms.h"

#include "SemanticsHelpers.h"
#include "primec/TransformRegistry.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" || name == "enum" || name == "compute" || name == "workgroup_size" ||
         name == "unsafe" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" ||
         name == "buffer" || name == "reflect" || name == "generate" || name == "Additive" ||
         name == "Multiplicative" || name == "Comparable" || name == "Indexable";
}

bool isSingleTypeReturnCandidate(const Transform &transform) {
  if (!transform.arguments.empty()) {
    return false;
  }
  if (isNonTypeTransformName(transform.name)) {
    return false;
  }
  return true;
}

bool isTextOnlyTransform(const Transform &transform) {
  if (transform.phase == TransformPhase::Text) {
    return true;
  }
  if (transform.phase == TransformPhase::Semantic) {
    return false;
  }
  const bool isText = primec::isTextTransformName(transform.name);
  const bool isSemantic = primec::isSemanticTransformName(transform.name);
  return isText && !isSemantic;
}

void stripTextTransforms(std::vector<Transform> &transforms) {
  size_t out = 0;
  for (size_t i = 0; i < transforms.size(); ++i) {
    auto &transform = transforms[i];
    if (isTextOnlyTransform(transform)) {
      continue;
    }
    if (out != i) {
      transforms[out] = std::move(transform);
    }
    ++out;
  }
  transforms.resize(out);
}

void stripTextTransforms(Expr &expr) {
  stripTextTransforms(expr.transforms);
  for (auto &arg : expr.args) {
    stripTextTransforms(arg);
  }
  for (auto &arg : expr.bodyArguments) {
    stripTextTransforms(arg);
  }
}

void stripTextTransforms(Definition &def) {
  stripTextTransforms(def.transforms);
  for (auto &param : def.parameters) {
    stripTextTransforms(param);
  }
  for (auto &stmt : def.statements) {
    stripTextTransforms(stmt);
  }
  if (def.returnExpr.has_value()) {
    stripTextTransforms(*def.returnExpr);
  }
}

void stripTextTransforms(Execution &exec) {
  stripTextTransforms(exec.transforms);
  for (auto &arg : exec.arguments) {
    stripTextTransforms(arg);
  }
  for (auto &arg : exec.bodyArguments) {
    stripTextTransforms(arg);
  }
}

std::string formatTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

std::string formatTransformType(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + formatTemplateArgs(transform.templateArgs) + ">";
}

bool validateTransformPhaseList(const std::vector<Transform> &transforms,
                                const std::string &context,
                                std::string &error) {
  for (const auto &transform : transforms) {
    const bool isText = primec::isTextTransformName(transform.name);
    const bool isSemantic = primec::isSemanticTransformName(transform.name);
    if (transform.phase == TransformPhase::Text) {
      if (!isText) {
        error = "text(...) group requires text transforms on " + context + ": " + transform.name;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Semantic) {
      if (!isSemantic && isText) {
        error = "text transform cannot appear in semantic(...) group on " + context + ": " + transform.name;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Auto && isText && isSemantic) {
      error = "ambiguous transform name on " + context + ": " + transform.name;
      return false;
    }
  }
  return true;
}

bool validateTransformListContext(const std::vector<Transform> &transforms,
                                  const std::string &context,
                                  bool allowSingleTypeToReturn,
                                  std::string &error) {
  if (!validateTransformPhaseList(transforms, context, error)) {
    return false;
  }
  for (const auto &transform : transforms) {
    if (transform.name == "package") {
      error = "package visibility has been removed; use [private] or omit for public: " + context;
      return false;
    }
  }
  if (!allowSingleTypeToReturn) {
    for (const auto &transform : transforms) {
      if (transform.name == "single_type_to_return") {
        error = "single_type_to_return is only valid on definitions: " + context;
        return false;
      }
    }
  }
  return true;
}

bool isLoopBodyEnvelope(const Expr &candidate) {
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
}

bool stripSharedScopeTransform(std::vector<Transform> &transforms,
                               bool &found,
                               const std::string &context,
                               std::string &error) {
  found = false;
  for (auto it = transforms.begin(); it != transforms.end(); ++it) {
    if (it->name != "shared_scope") {
      continue;
    }
    if (found) {
      error = "duplicate shared_scope transform on " + context;
      return false;
    }
    if (!it->templateArgs.empty()) {
      error = "shared_scope does not accept template arguments on " + context;
      return false;
    }
    if (!it->arguments.empty()) {
      error = "shared_scope does not accept arguments on " + context;
      return false;
    }
    found = true;
    transforms.erase(it);
    break;
  }
  return true;
}

bool rewriteSharedScopeStatements(std::vector<Expr> &statements, std::string &error);

bool shouldRewriteIndexedAccessSugar(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.isMethodCall) {
    return false;
  }
  std::string ignoredName;
  return isAssignCall(expr) || getBuiltinOperatorName(expr, ignoredName) ||
         getBuiltinComparisonName(expr, ignoredName) || getBuiltinClampName(expr, ignoredName, true) ||
         getBuiltinMinMaxName(expr, ignoredName, true) || getBuiltinAbsSignName(expr, ignoredName, true) ||
         getBuiltinSaturateName(expr, ignoredName, true) || getBuiltinMathName(expr, ignoredName, true);
}

void rewriteIndexedAccessSugar(Expr &expr) {
  for (auto &arg : expr.args) {
    rewriteIndexedAccessSugar(arg);
  }
  for (auto &arg : expr.bodyArguments) {
    rewriteIndexedAccessSugar(arg);
  }
  if (!shouldRewriteIndexedAccessSugar(expr) || expr.args.size() < 2) {
    return;
  }
  if (!expr.templateArgs.empty() || expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return;
  }
  if (expr.argNames.size() < expr.args.size()) {
    expr.argNames.resize(expr.args.size());
  }
  for (size_t argIndex = 1; argIndex < expr.args.size(); ++argIndex) {
    if (expr.argNames[argIndex - 1].has_value() || !expr.argNames[argIndex].has_value()) {
      continue;
    }
    const std::string &indexName = *expr.argNames[argIndex];
    if (indexName.empty()) {
      continue;
    }

    Expr accessExpr;
    accessExpr.kind = Expr::Kind::Call;
    accessExpr.name = "at";
    accessExpr.namespacePrefix = expr.namespacePrefix;
    accessExpr.args.push_back(std::move(expr.args[argIndex - 1]));

    Expr indexExpr;
    indexExpr.kind = Expr::Kind::Name;
    indexExpr.name = indexName;
    indexExpr.namespacePrefix = expr.namespacePrefix;
    accessExpr.args.push_back(std::move(indexExpr));
    accessExpr.argNames.resize(2);

    expr.args[argIndex - 1] = std::move(accessExpr);
    expr.argNames[argIndex] = std::nullopt;
  }
}

void rewriteIndexedAccessSugar(Definition &def) {
  for (auto &param : def.parameters) {
    rewriteIndexedAccessSugar(param);
  }
  for (auto &stmt : def.statements) {
    rewriteIndexedAccessSugar(stmt);
  }
  if (def.returnExpr.has_value()) {
    rewriteIndexedAccessSugar(*def.returnExpr);
  }
}

void rewriteIndexedAccessSugar(Execution &exec) {
  for (auto &arg : exec.arguments) {
    rewriteIndexedAccessSugar(arg);
  }
  for (auto &arg : exec.bodyArguments) {
    rewriteIndexedAccessSugar(arg);
  }
}

bool rewriteSharedScopeStatement(Expr &stmt, std::string &error) {
  if (stmt.kind == Expr::Kind::Call) {
    bool hasSharedScope = false;
    if (!stripSharedScopeTransform(stmt.transforms, hasSharedScope, "statement", error)) {
      return false;
    }
    if (hasSharedScope) {
      const bool isLoop = isLoopCall(stmt);
      const bool isWhile = isWhileCall(stmt);
      const bool isFor = isForCall(stmt);
      if (!isLoop && !isWhile && !isFor) {
        error = "shared_scope is only valid on loop/while/for statements";
        return false;
      }
      const size_t bodyIndex = isFor ? 3 : 1;
      if (stmt.args.size() <= bodyIndex) {
        error = "shared_scope requires loop body";
        return false;
      }
      Expr &body = stmt.args[bodyIndex];
      if (!isLoopBodyEnvelope(body)) {
        error = "shared_scope requires loop body in do() { ... }";
        return false;
      }
      std::vector<Expr> hoisted;
      std::vector<Expr> remaining;
      for (auto &bodyStmt : body.bodyArguments) {
        if (bodyStmt.isBinding) {
          hoisted.push_back(std::move(bodyStmt));
        } else {
          remaining.push_back(std::move(bodyStmt));
        }
      }
      body.bodyArguments = std::move(remaining);
      if (!rewriteSharedScopeStatements(body.bodyArguments, error)) {
        return false;
      }
      if (hoisted.empty()) {
        return true;
      }

      Expr blockCall;
      blockCall.kind = Expr::Kind::Call;
      blockCall.name = "block";
      blockCall.namespacePrefix = stmt.namespacePrefix;
      blockCall.hasBodyArguments = true;
      blockCall.bodyArguments.reserve(hoisted.size() + 2);

      if (isFor) {
        Expr initStmt = std::move(stmt.args[0]);
        Expr cond = std::move(stmt.args[1]);
        Expr step = std::move(stmt.args[2]);
        Expr forBody = std::move(body);

        Expr noOpInit;
        noOpInit.kind = Expr::Kind::Call;
        noOpInit.name = "block";
        noOpInit.namespacePrefix = stmt.namespacePrefix;
        noOpInit.hasBodyArguments = true;

        Expr forCall;
        forCall.kind = Expr::Kind::Call;
        forCall.name = "for";
        forCall.namespacePrefix = stmt.namespacePrefix;
        forCall.transforms = std::move(stmt.transforms);
        forCall.args.push_back(std::move(noOpInit));
        forCall.argNames.push_back(std::nullopt);
        forCall.args.push_back(std::move(cond));
        forCall.argNames.push_back(std::nullopt);
        forCall.args.push_back(std::move(step));
        forCall.argNames.push_back(std::nullopt);
        forCall.args.push_back(std::move(forBody));
        forCall.argNames.push_back(std::nullopt);

        blockCall.bodyArguments.push_back(std::move(initStmt));
        for (auto &binding : hoisted) {
          blockCall.bodyArguments.push_back(std::move(binding));
        }
        blockCall.bodyArguments.push_back(std::move(forCall));
        stmt = std::move(blockCall);
        return true;
      }

      for (auto &binding : hoisted) {
        blockCall.bodyArguments.push_back(std::move(binding));
      }
      blockCall.bodyArguments.push_back(std::move(stmt));
      stmt = std::move(blockCall);
      return true;
    }

    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return rewriteSharedScopeStatements(stmt.bodyArguments, error);
    }
  }
  return true;
}

bool rewriteSharedScopeStatements(std::vector<Expr> &statements, std::string &error) {
  for (auto &stmt : statements) {
    if (!rewriteSharedScopeStatement(stmt, error)) {
      return false;
    }
  }
  return true;
}

bool applySingleTypeToReturn(std::vector<Transform> &transforms,
                             bool force,
                             const std::string &context,
                             std::string &error) {
  size_t markerIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (transforms[i].name != "single_type_to_return") {
      continue;
    }
    if (markerIndex != transforms.size()) {
      error = "duplicate single_type_to_return transform on " + context;
      return false;
    }
    if (!transforms[i].templateArgs.empty()) {
      error = "single_type_to_return does not accept template arguments on " + context;
      return false;
    }
    if (!transforms[i].arguments.empty()) {
      error = "single_type_to_return does not accept arguments on " + context;
      return false;
    }
    markerIndex = i;
  }
  const bool hasExplicitMarker = markerIndex != transforms.size();
  if (!hasExplicitMarker && !force) {
    return true;
  }
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      if (hasExplicitMarker) {
        error = "single_type_to_return cannot be combined with return transform on " + context;
        return false;
      }
      return true;
    }
  }
  size_t typeIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (!isSingleTypeReturnCandidate(transforms[i])) {
      continue;
    }
    if (typeIndex != transforms.size()) {
      if (hasExplicitMarker) {
        error = "single_type_to_return requires a single type transform on " + context;
        return false;
      }
      return true;
    }
    typeIndex = i;
  }
  if (typeIndex == transforms.size()) {
    if (hasExplicitMarker) {
      error = "single_type_to_return requires a type transform on " + context;
      return false;
    }
    return true;
  }
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.phase = TransformPhase::Semantic;
  returnTransform.templateArgs.push_back(formatTransformType(transforms[typeIndex]));
  if (!hasExplicitMarker) {
    transforms[typeIndex] = std::move(returnTransform);
    return true;
  }
  std::vector<Transform> rewritten;
  rewritten.reserve(transforms.size() - 1);
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (i == typeIndex) {
      rewritten.push_back(std::move(returnTransform));
      continue;
    }
    rewritten.push_back(std::move(transforms[i]));
  }
  transforms = std::move(rewritten);
  return true;
}

struct EnumTypeInfo {
  std::string typeName;
  bool isUnsigned = false;
  int width = 32;
};

struct EnumValueEntry {
  bool isUnsigned = false;
  int width = 32;
  int64_t signedValue = 0;
  uint64_t unsignedValue = 0;
};

using EnumValueMap = std::unordered_map<std::string, std::unordered_map<std::string, EnumValueEntry>>;

bool parseEnumType(const Transform &transform, EnumTypeInfo &out, const std::string &context, std::string &error) {
  if (!transform.arguments.empty()) {
    error = "enum transform does not accept arguments on " + context;
    return false;
  }
  if (transform.templateArgs.size() > 1) {
    error = "enum transform accepts at most one template argument on " + context;
    return false;
  }
  std::string typeName = "i32";
  if (!transform.templateArgs.empty()) {
    typeName = normalizeBindingTypeName(transform.templateArgs.front());
  }
  if (typeName == "i32") {
    out.typeName = "i32";
    out.isUnsigned = false;
    out.width = 32;
    return true;
  }
  if (typeName == "i64") {
    out.typeName = "i64";
    out.isUnsigned = false;
    out.width = 64;
    return true;
  }
  if (typeName == "u64") {
    out.typeName = "u64";
    out.isUnsigned = true;
    out.width = 64;
    return true;
  }
  error = "enum underlying type must be i32, i64, or u64 on " + context;
  return false;
}

Expr makeEnumLiteralSigned(int64_t value, int width, const std::string &namespacePrefix) {
  Expr literal;
  literal.kind = Expr::Kind::Literal;
  literal.intWidth = width;
  literal.isUnsigned = false;
  literal.literalValue = static_cast<uint64_t>(value);
  literal.namespacePrefix = namespacePrefix;
  return literal;
}

Expr makeEnumLiteralUnsigned(uint64_t value, int width, const std::string &namespacePrefix) {
  Expr literal;
  literal.kind = Expr::Kind::Literal;
  literal.intWidth = width;
  literal.isUnsigned = true;
  literal.literalValue = value;
  literal.namespacePrefix = namespacePrefix;
  return literal;
}

bool extractEnumLiteralValue(const Expr &expr,
                             const EnumTypeInfo &type,
                             const std::string &context,
                             int64_t &signedOut,
                             uint64_t &unsignedOut,
                             std::string &error) {
  if (expr.kind != Expr::Kind::Literal) {
    error = "enum values must be integer literals on " + context;
    return false;
  }
  if (type.isUnsigned) {
    if (expr.isUnsigned) {
      unsignedOut = expr.literalValue;
      return true;
    }
    int64_t value = static_cast<int64_t>(expr.literalValue);
    if (value < 0) {
      error = "enum values must be non-negative for u64 on " + context;
      return false;
    }
    unsignedOut = static_cast<uint64_t>(value);
    return true;
  }
  int64_t value = 0;
  if (expr.isUnsigned) {
    if (expr.literalValue > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      error = "enum value is out of range on " + context;
      return false;
    }
    value = static_cast<int64_t>(expr.literalValue);
  } else {
    value = static_cast<int64_t>(expr.literalValue);
  }
  const int64_t minValue =
      type.width == 32 ? static_cast<int64_t>(std::numeric_limits<int32_t>::min()) : std::numeric_limits<int64_t>::min();
  const int64_t maxValue =
      type.width == 32 ? static_cast<int64_t>(std::numeric_limits<int32_t>::max()) : std::numeric_limits<int64_t>::max();
  if (value < minValue || value > maxValue) {
    error = "enum value is out of range on " + context;
    return false;
  }
  signedOut = value;
  return true;
}

bool rewriteEnumDefinition(Definition &def, EnumValueMap &enumValues, std::string &error) {
  size_t enumIndex = def.transforms.size();
  for (size_t i = 0; i < def.transforms.size(); ++i) {
    if (def.transforms[i].name != "enum") {
      continue;
    }
    if (enumIndex != def.transforms.size()) {
      error = "duplicate enum transform on " + def.fullPath;
      return false;
    }
    enumIndex = i;
  }
  if (enumIndex == def.transforms.size()) {
    return true;
  }
  if (!def.parameters.empty()) {
    error = "enum definitions cannot declare parameters: " + def.fullPath;
    return false;
  }
  if (!def.templateArgs.empty()) {
    error = "enum definitions do not support template parameters: " + def.fullPath;
    return false;
  }
  if (def.hasReturnStatement || def.returnExpr.has_value()) {
    error = "enum definitions cannot return values: " + def.fullPath;
    return false;
  }

  EnumTypeInfo enumType;
  if (!parseEnumType(def.transforms[enumIndex], enumType, def.fullPath, error)) {
    return false;
  }

  struct EnumEntry {
    std::string name;
    std::optional<Expr> explicitValue;
  };
  std::vector<EnumEntry> entries;
  entries.reserve(def.statements.size());
  std::unordered_set<std::string> seen;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      error = "enum definitions may only contain value entries: " + def.fullPath;
      return false;
    }
    if (stmt.kind == Expr::Kind::Name) {
      if (!seen.insert(stmt.name).second) {
        error = "duplicate enum entry: " + stmt.name;
        return false;
      }
      EnumEntry entry;
      entry.name = stmt.name;
      entries.push_back(std::move(entry));
      continue;
    }
    if (stmt.kind == Expr::Kind::Call && isAssignCall(stmt) && stmt.args.size() == 2) {
      const Expr &lhs = stmt.args.front();
      const Expr &rhs = stmt.args.back();
      if (lhs.kind != Expr::Kind::Name) {
        error = "enum entry assignment requires a name on " + def.fullPath;
        return false;
      }
      if (!seen.insert(lhs.name).second) {
        error = "duplicate enum entry: " + lhs.name;
        return false;
      }
      EnumEntry entry;
      entry.name = lhs.name;
      entry.explicitValue = rhs;
      entries.push_back(std::move(entry));
      continue;
    }
    error = "enum definitions may only contain value entries: " + def.fullPath;
    return false;
  }

  bool sawStructTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "struct") {
      sawStructTransform = true;
      break;
    }
  }
  if (!sawStructTransform) {
    Transform structTransform;
    structTransform.name = "struct";
    structTransform.phase = TransformPhase::Auto;
    def.transforms.push_back(std::move(structTransform));
  }
  def.transforms.erase(def.transforms.begin() + enumIndex);

  std::vector<Expr> rewritten;
  rewritten.reserve(entries.size() + 1);

  Expr valueBinding;
  valueBinding.kind = Expr::Kind::Call;
  valueBinding.name = "value";
  valueBinding.namespacePrefix = def.namespacePrefix;
  valueBinding.isBinding = true;
  Transform typeTransform;
  typeTransform.name = enumType.typeName;
  typeTransform.phase = TransformPhase::Auto;
  valueBinding.transforms.push_back(std::move(typeTransform));
  if (enumType.isUnsigned) {
    valueBinding.args.push_back(makeEnumLiteralUnsigned(0u, enumType.width, def.namespacePrefix));
  } else {
    valueBinding.args.push_back(makeEnumLiteralSigned(0, enumType.width, def.namespacePrefix));
  }
  valueBinding.argNames.push_back(std::nullopt);
  rewritten.push_back(std::move(valueBinding));

  bool nextValid = true;
  int64_t nextSigned = 0;
  uint64_t nextUnsigned = 0;
  const int64_t maxSigned = enumType.width == 32 ? static_cast<int64_t>(std::numeric_limits<int32_t>::max())
                                                 : std::numeric_limits<int64_t>::max();

  auto &enumMap = enumValues[def.fullPath];
  for (const auto &entry : entries) {
    int64_t signedValue = 0;
    uint64_t unsignedValue = 0;
    if (entry.explicitValue.has_value()) {
      if (!extractEnumLiteralValue(*entry.explicitValue, enumType, def.fullPath, signedValue, unsignedValue, error)) {
        return false;
      }
      if (enumType.isUnsigned) {
        if (unsignedValue == std::numeric_limits<uint64_t>::max()) {
          nextValid = false;
        } else {
          nextUnsigned = unsignedValue + 1;
          nextValid = true;
        }
      } else {
        if (signedValue == maxSigned) {
          nextValid = false;
        } else {
          nextSigned = signedValue + 1;
          nextValid = true;
        }
      }
    } else {
      if (!nextValid) {
        error = "enum value overflow on " + def.fullPath;
        return false;
      }
      if (enumType.isUnsigned) {
        unsignedValue = nextUnsigned;
        if (unsignedValue == std::numeric_limits<uint64_t>::max()) {
          nextValid = false;
        } else {
          nextUnsigned = unsignedValue + 1;
        }
      } else {
        signedValue = nextSigned;
        if (signedValue == maxSigned) {
          nextValid = false;
        } else {
          nextSigned = signedValue + 1;
        }
      }
    }

    EnumValueEntry entryValue;
    entryValue.isUnsigned = enumType.isUnsigned;
    entryValue.width = enumType.width;
    entryValue.signedValue = signedValue;
    entryValue.unsignedValue = unsignedValue;
    enumMap.emplace(entry.name, entryValue);

    Expr enumValue = enumType.isUnsigned
                         ? makeEnumLiteralUnsigned(unsignedValue, enumType.width, def.namespacePrefix)
                         : makeEnumLiteralSigned(signedValue, enumType.width, def.namespacePrefix);

    Expr constructorCall;
    constructorCall.kind = Expr::Kind::Call;
    constructorCall.name = def.fullPath;
    constructorCall.namespacePrefix = def.namespacePrefix;
    constructorCall.args.push_back(std::move(enumValue));
    constructorCall.argNames.push_back(std::nullopt);

    Expr enumBinding;
    enumBinding.kind = Expr::Kind::Call;
    enumBinding.name = entry.name;
    enumBinding.namespacePrefix = def.namespacePrefix;
    enumBinding.isBinding = true;
    Transform publicTransform;
    publicTransform.name = "public";
    publicTransform.phase = TransformPhase::Auto;
    Transform staticTransform;
    staticTransform.name = "static";
    staticTransform.phase = TransformPhase::Auto;
    Transform valueTypeTransform;
    valueTypeTransform.name = def.fullPath;
    valueTypeTransform.phase = TransformPhase::Auto;
    enumBinding.transforms.push_back(std::move(publicTransform));
    enumBinding.transforms.push_back(std::move(staticTransform));
    enumBinding.transforms.push_back(std::move(valueTypeTransform));
    enumBinding.args.push_back(std::move(constructorCall));
    enumBinding.argNames.push_back(std::nullopt);
    rewritten.push_back(std::move(enumBinding));
  }

  def.statements = std::move(rewritten);
  return true;
}

bool rewriteEnumDefinitions(Program &program, std::string &error) {
  EnumValueMap enumValues;
  for (auto &def : program.definitions) {
    if (!rewriteEnumDefinition(def, enumValues, error)) {
      return false;
    }
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
    if (!expr.isFieldAccess || expr.args.size() != 1) {
      return true;
    }
    const Expr &receiver = expr.args.front();
    if (receiver.kind != Expr::Kind::Name) {
      return true;
    }
    const std::string structPath = resolveTypePath(receiver.name, receiver.namespacePrefix);
    auto typeIt = enumValues.find(structPath);
    if (typeIt == enumValues.end()) {
      return true;
    }
    auto entryIt = typeIt->second.find(expr.name);
    if (entryIt == typeIt->second.end()) {
      return true;
    }
    const EnumValueEntry &value = entryIt->second;
    Expr literal = value.isUnsigned ? makeEnumLiteralUnsigned(value.unsignedValue, value.width, expr.namespacePrefix)
                                    : makeEnumLiteralSigned(value.signedValue, value.width, expr.namespacePrefix);
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = structPath;
    call.namespacePrefix = expr.namespacePrefix;
    call.args.push_back(std::move(literal));
    call.argNames.push_back(std::nullopt);
    expr = std::move(call);
    return true;
  };
  for (auto &def : program.definitions) {
    for (auto &param : def.parameters) {
      if (!rewriteExpr(rewriteExpr, param)) {
        return false;
      }
    }
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

bool validateExprTransforms(const Expr &expr, const std::string &context, std::string &error) {
  if (!validateTransformListContext(expr.transforms, context, false, error)) {
    return false;
  }
  for (const auto &arg : expr.args) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool applySemanticTransforms(Program &program,
                             const std::vector<std::string> &semanticTransforms,
                             std::string &error) {
  bool forceSingleTypeToReturn = false;
  for (const auto &name : semanticTransforms) {
    if (name == "single_type_to_return") {
      forceSingleTypeToReturn = true;
      continue;
    }
    error = "unsupported semantic transform: " + name;
    return false;
  }

  for (auto &def : program.definitions) {
    if (!validateTransformListContext(def.transforms, def.fullPath, true, error)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!validateExprTransforms(param, def.fullPath, error)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!validateExprTransforms(stmt, def.fullPath, error)) {
        return false;
      }
    }
  }

  for (auto &exec : program.executions) {
    if (!validateTransformListContext(exec.transforms, exec.fullPath, false, error)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
  }

  for (auto &def : program.definitions) {
    stripTextTransforms(def);
    rewriteIndexedAccessSugar(def);
  }
  for (auto &exec : program.executions) {
    stripTextTransforms(exec);
    rewriteIndexedAccessSugar(exec);
  }
  if (!rewriteEnumDefinitions(program, error)) {
    return false;
  }
  for (auto &def : program.definitions) {
    if (!rewriteSharedScopeStatements(def.statements, error)) {
      return false;
    }
  }
  for (auto &exec : program.executions) {
    if (!rewriteSharedScopeStatements(exec.bodyArguments, error)) {
      return false;
    }
  }
  for (auto &def : program.definitions) {
    if (!applySingleTypeToReturn(def.transforms, forceSingleTypeToReturn, def.fullPath, error)) {
      return false;
    }
  }

  return true;
}

} // namespace primec::semantics
