#include "primec/Semantics.h"

#include "SemanticsValidator.h"
#include "primec/StringLiteral.h"
#include "primec/TransformRegistry.h"

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
}

namespace {
bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" || name == "enum" || name == "compute" || name == "workgroup_size" ||
         name == "unsafe" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" || name == "buffer" ||
         name == "reflect" || name == "generate" || name == "Additive" || name == "Multiplicative" ||
         name == "Comparable" || name == "Indexable";
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

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == nameToMatch;
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
    typeName = semantics::normalizeBindingTypeName(transform.templateArgs.front());
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
    if (stmt.kind == Expr::Kind::Call && semantics::isAssignCall(stmt) && stmt.args.size() == 2) {
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
    const std::string structPath = semantics::resolveTypePath(receiver.name, receiver.namespacePrefix);
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

bool rewriteConvertConstructors(Program &program, std::string &error) {
  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> publicDefinitions;
  auto isDefinitionPublic = [](const Definition &def) -> bool {
    bool sawVisibility = false;
    bool isPublic = false;
    for (const auto &transform : def.transforms) {
      if (transform.name != "public" && transform.name != "private") {
        continue;
      }
      if (sawVisibility) {
        return false;
      }
      sawVisibility = true;
      isPublic = (transform.name == "public");
    }
    return isPublic;
  };
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
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
    if (isDefinitionPublic(def)) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  for (const auto &importPath : program.imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      const std::string scopedPrefix = prefix + "/";
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

  struct ResolvedStructTarget {
    std::string path;
    bool fromImport = false;
  };
  auto resolveStructTarget = [&](const std::string &typeName,
                                 const std::string &namespacePrefix) -> ResolvedStructTarget {
    if (typeName == "Self") {
      return ResolvedStructTarget{structNames.count(namespacePrefix) > 0 ? namespacePrefix : std::string{}, false};
    }
    if (typeName.find('<') != std::string::npos) {
      return {};
    }
    std::string resolved = semantics::resolveStructTypePath(typeName, namespacePrefix, structNames);
    if (!resolved.empty()) {
      return ResolvedStructTarget{std::move(resolved), false};
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
      return ResolvedStructTarget{importIt->second, true};
    }
    return {};
  };

  auto resolveReturnStructTarget =
      [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    if (typeName == "Self") {
      return structNames.count(namespacePrefix) > 0 ? namespacePrefix : std::string{};
    }
    if (typeName.find('<') != std::string::npos) {
      return {};
    }
    std::string resolved = semantics::resolveStructTypePath(typeName, namespacePrefix, structNames);
    if (!resolved.empty()) {
      return resolved;
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
      return importIt->second;
    }
    return {};
  };

  auto isConvertHelperPath = [](const std::string &helperBase, const std::string &path) -> bool {
    if (path == helperBase) {
      return true;
    }
    const std::string mangledPrefix = helperBase + "__t";
    return path.rfind(mangledPrefix, 0) == 0;
  };

  auto convertHelperMatches = [&](const Definition &def, const std::string &targetStruct) -> bool {
    if (def.parameters.size() != 1) {
      return false;
    }
    bool sawStatic = false;
    bool sawReturn = false;
    std::string returnType;
    for (const auto &transform : def.transforms) {
      if (transform.name == "static") {
        sawStatic = true;
      }
      if (transform.name != "return") {
        continue;
      }
      sawReturn = true;
      if (transform.templateArgs.size() != 1) {
        return false;
      }
      returnType = transform.templateArgs.front();
      break;
    }
    if (!sawStatic) {
      return false;
    }
    if (!sawReturn || returnType.empty() || returnType == "auto") {
      return false;
    }
    std::string resolvedReturn = resolveReturnStructTarget(returnType, def.namespacePrefix);
    return !resolvedReturn.empty() && resolvedReturn == targetStruct;
  };

  auto collectConvertHelpers = [&](const std::string &structPath,
                                   bool requirePublic) -> std::vector<std::string> {
    std::vector<std::string> matches;
    const std::string helperBase = structPath + "/Convert";
    for (const auto &def : program.definitions) {
      if (!isConvertHelperPath(helperBase, def.fullPath)) {
        continue;
      }
      if (requirePublic && publicDefinitions.count(def.fullPath) == 0) {
        continue;
      }
      if (!convertHelperMatches(def, structPath)) {
        continue;
      }
      matches.push_back(def.fullPath);
    }
    std::sort(matches.begin(), matches.end());
    return matches;
  };

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
    std::string builtinName;
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
        !semantics::getBuiltinConvertName(expr, builtinName) ||
        expr.templateArgs.size() != 1) {
      return true;
    }
    const std::string targetName = expr.templateArgs.front();
    const std::string normalizedTarget = semantics::normalizeBindingTypeName(targetName);
    if (normalizedTarget == "i32" || normalizedTarget == "i64" || normalizedTarget == "u64" ||
        normalizedTarget == "bool" || normalizedTarget == "f32" || normalizedTarget == "f64") {
      return true;
    }
    if (semantics::isSoftwareNumericTypeName(normalizedTarget)) {
      return true;
    }
    ResolvedStructTarget target = resolveStructTarget(targetName, expr.namespacePrefix);
    if (target.path.empty()) {
      return true;
    }
    std::vector<std::string> helpers = collectConvertHelpers(target.path, target.fromImport);
    if (helpers.empty()) {
      error = "no conversion found for convert<" + targetName + ">";
      return false;
    }
    if (helpers.size() > 1) {
      std::ostringstream message;
      message << "ambiguous conversion for convert<" << targetName << ">: ";
      for (size_t i = 0; i < helpers.size(); ++i) {
        if (i > 0) {
          message << ", ";
        }
        message << helpers[i];
      }
      error = message.str();
      return false;
    }
    expr.name = helpers.front();
    expr.templateArgs.clear();
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

bool rewriteMaybeConstructors(Program &program, std::string &error) {
  (void)error;
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
    if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isMethodCall) {
      return true;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return true;
    }
    if (expr.args.size() != 1) {
      return true;
    }
    std::string resolved = semantics::resolveStructTypePath(expr.name, expr.namespacePrefix, structNames);
    if (resolved.empty()) {
      auto it = importAliases.find(expr.name);
      if (it != importAliases.end() && structNames.count(it->second) > 0) {
        resolved = it->second;
      }
    }
    if (resolved.empty()) {
      return true;
    }
    const size_t lastSlash = resolved.find_last_of('/');
    const std::string_view base =
        lastSlash == std::string::npos ? std::string_view(resolved) : std::string_view(resolved).substr(lastSlash + 1);
    if (base != "Maybe") {
      return true;
    }
    Expr call;
    call.kind = Expr::Kind::Call;
    call.templateArgs = expr.templateArgs;
    call.args = std::move(expr.args);
    call.argNames = std::move(expr.argNames);
    call.transforms = std::move(expr.transforms);
    if (!resolved.empty() && resolved[0] == '/') {
      std::string prefix = resolved.substr(0, resolved.size() - 6);
      call.name = prefix.empty() ? "/some" : (prefix + "/some");
      call.namespacePrefix.clear();
    } else {
      call.name = "some";
      call.namespacePrefix = expr.namespacePrefix;
    }
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

bool rewriteReflectionMetadataQueries(Program &program, std::string &error) {
  struct FieldMetadata {
    std::string name;
    std::string typeName;
    std::string visibility;
  };

  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> reflectedStructNames;
  std::unordered_map<std::string, std::vector<FieldMetadata>> structFieldMetadata;
  std::unordered_map<std::string, const Definition *> definitionByPath;
  std::unordered_set<std::string> publicDefinitions;
  auto isDefinitionPublic = [](const Definition &def) -> bool {
    bool sawVisibility = false;
    bool isPublic = false;
    for (const auto &transform : def.transforms) {
      if (transform.name != "public" && transform.name != "private") {
        continue;
      }
      if (sawVisibility) {
        return false;
      }
      sawVisibility = true;
      isPublic = (transform.name == "public");
    }
    return isPublic;
  };

  for (const auto &def : program.definitions) {
    definitionByPath.emplace(def.fullPath, &def);
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
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
      if (std::any_of(def.transforms.begin(), def.transforms.end(), [](const Transform &transform) {
            return transform.name == "reflect";
          })) {
        reflectedStructNames.insert(def.fullPath);
      }
    }
    if (isDefinitionPublic(def)) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  for (const auto &importPath : program.imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      const std::string scopedPrefix = prefix + "/";
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

  auto trim = [](const std::string &text) -> std::string {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      return {};
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
  };
  auto escapeStringLiteral = [](const std::string &text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    escaped.push_back('"');
    for (char c : text) {
      if (c == '"' || c == '\\') {
        escaped.push_back('\\');
      }
      escaped.push_back(c);
    }
    escaped += "\"utf8";
    return escaped;
  };

  std::function<bool(const std::string &, const std::string &, std::string &, std::string &)> canonicalizeTypeName;
  canonicalizeTypeName = [&](const std::string &typeName,
                             const std::string &namespacePrefix,
                             std::string &canonicalOut,
                             std::string &structPathOut) {
    canonicalOut.clear();
    structPathOut.clear();
    const std::string trimmed = trim(typeName);
    if (trimmed.empty()) {
      error = "reflection query requires a type argument";
      return false;
    }
    if (trimmed == "Self" && structNames.count(namespacePrefix) > 0) {
      canonicalOut = namespacePrefix;
      structPathOut = namespacePrefix;
      return true;
    }

    const std::string normalized = semantics::normalizeBindingTypeName(trimmed);
    std::string resolvedStruct = semantics::resolveStructTypePath(normalized, namespacePrefix, structNames);
    if (resolvedStruct.empty()) {
      auto importIt = importAliases.find(normalized);
      if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
        resolvedStruct = importIt->second;
      }
    }
    if (!resolvedStruct.empty()) {
      canonicalOut = resolvedStruct;
      structPathOut = resolvedStruct;
      return true;
    }

    std::string base;
    std::string arg;
    if (!semantics::splitTemplateTypeName(normalized, base, arg)) {
      canonicalOut = normalized;
      return true;
    }

    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(arg, args) || args.empty()) {
      canonicalOut = normalized;
      return true;
    }

    std::string canonicalBase;
    std::string baseStructPath;
    if (!canonicalizeTypeName(base, namespacePrefix, canonicalBase, baseStructPath)) {
      return false;
    }
    std::vector<std::string> canonicalArgs;
    canonicalArgs.reserve(args.size());
    for (const auto &nestedArg : args) {
      std::string canonicalArg;
      std::string nestedStructPath;
      if (!canonicalizeTypeName(nestedArg, namespacePrefix, canonicalArg, nestedStructPath)) {
        return false;
      }
      canonicalArgs.push_back(std::move(canonicalArg));
    }
    canonicalOut = canonicalBase + "<" + semantics::joinTemplateArgs(canonicalArgs) + ">";
    return true;
  };

  auto resolveFieldTypeName = [&](const Definition &def, const Expr &stmt) {
    std::string typeCandidate;
    bool hasExplicitType = false;
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (transform.name == "restrict" && transform.templateArgs.size() == 1) {
        typeCandidate = transform.templateArgs.front();
        hasExplicitType = true;
        continue;
      }
      if (semantics::isBindingAuxTransformName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      if (!transform.templateArgs.empty()) {
        typeCandidate = transform.name + "<" + semantics::joinTemplateArgs(transform.templateArgs) + ">";
      } else {
        typeCandidate = transform.name;
      }
      hasExplicitType = true;
      break;
    }

    if (!hasExplicitType && stmt.args.size() == 1) {
      BindingInfo inferred;
      if (semantics::tryInferBindingTypeFromInitializer(stmt.args.front(), {}, {}, inferred, true)) {
        typeCandidate = inferred.typeName;
        if (!inferred.typeTemplateArg.empty()) {
          typeCandidate += "<" + inferred.typeTemplateArg + ">";
        }
      }
    }
    if (typeCandidate.empty()) {
      typeCandidate = "int";
    }

    std::string canonicalType;
    std::string ignoredStructPath;
    if (canonicalizeTypeName(typeCandidate, def.namespacePrefix, canonicalType, ignoredStructPath)) {
      return canonicalType;
    }
    return semantics::normalizeBindingTypeName(typeCandidate);
  };

  for (const auto &def : program.definitions) {
    if (structNames.count(def.fullPath) == 0) {
      continue;
    }
    std::vector<FieldMetadata> fields;
    fields.reserve(def.statements.size());
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      FieldMetadata field;
      field.name = stmt.name;
      field.typeName = resolveFieldTypeName(def, stmt);
      field.visibility = "public";
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "private") {
          field.visibility = "private";
          break;
        }
        if (transform.name == "public") {
          field.visibility = "public";
          break;
        }
      }
      fields.push_back(std::move(field));
    }
    structFieldMetadata.emplace(def.fullPath, std::move(fields));
  }

  auto typeKindForCanonical = [](const std::string &canonicalType, const std::string &structPath) {
    if (!structPath.empty()) {
      return std::string("struct");
    }
    std::string base;
    std::string arg;
    if (semantics::splitTemplateTypeName(canonicalType, base, arg)) {
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if (normalizedBase == "array" || normalizedBase == "vector" || normalizedBase == "map" ||
          normalizedBase == "soa_vector" || normalizedBase == "Result") {
        return normalizedBase;
      }
      if (normalizedBase == "Pointer") {
        return std::string("pointer");
      }
      if (normalizedBase == "Reference") {
        return std::string("reference");
      }
      if (normalizedBase == "Buffer") {
        return std::string("buffer");
      }
      if (normalizedBase == "uninitialized") {
        return std::string("uninitialized");
      }
      return std::string("template");
    }
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    if (normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "integer") {
      return std::string("integer");
    }
    if (normalized == "f32" || normalized == "f64") {
      return std::string("float");
    }
    if (normalized == "decimal") {
      return std::string("decimal");
    }
    if (normalized == "complex") {
      return std::string("complex");
    }
    if (normalized == "bool") {
      return std::string("bool");
    }
    if (normalized == "string") {
      return std::string("string");
    }
    return std::string("type");
  };
  auto parseFieldIndex = [&](const Expr &indexExpr, const std::string &queryName, size_t &indexOut) {
    if (indexExpr.kind != Expr::Kind::Literal) {
      error = "meta." + queryName + " requires constant integer index argument";
      return false;
    }
    if (indexExpr.isUnsigned) {
      indexOut = static_cast<size_t>(indexExpr.literalValue);
      return true;
    }
    if (indexExpr.intWidth == 64) {
      const int64_t value = static_cast<int64_t>(indexExpr.literalValue);
      if (value < 0) {
        error = "meta." + queryName + " requires non-negative field index";
        return false;
      }
      indexOut = static_cast<size_t>(value);
      return true;
    }
    const int32_t value = static_cast<int32_t>(static_cast<uint32_t>(indexExpr.literalValue));
    if (value < 0) {
      error = "meta." + queryName + " requires non-negative field index";
      return false;
    }
    indexOut = static_cast<size_t>(value);
    return true;
  };
  auto resolveDefinitionTarget = [&](const std::string &canonicalType,
                                     const std::string &structPath,
                                     const std::string &namespacePrefix) -> const Definition * {
    auto findByPath = [&](const std::string &path) -> const Definition * {
      auto it = definitionByPath.find(path);
      return it == definitionByPath.end() ? nullptr : it->second;
    };
    if (!structPath.empty()) {
      return findByPath(structPath);
    }
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return nullptr;
    }
    if (canonicalType[0] == '/') {
      return findByPath(canonicalType);
    }
    if (!namespacePrefix.empty()) {
      std::string prefix = namespacePrefix;
      while (!prefix.empty()) {
        const std::string candidate = prefix + "/" + canonicalType;
        if (const Definition *resolved = findByPath(candidate)) {
          return resolved;
        }
        const size_t slash = prefix.find_last_of('/');
        if (slash == std::string::npos) {
          break;
        }
        prefix = prefix.substr(0, slash);
      }
    }
    auto importIt = importAliases.find(canonicalType);
    if (importIt != importAliases.end()) {
      if (const Definition *resolved = findByPath(importIt->second)) {
        return resolved;
      }
    }
    return findByPath("/" + canonicalType);
  };
  auto parseNamedQueryName = [&](const Expr &argExpr,
                                 const std::string &queryName,
                                 const std::string &nameLabel,
                                 std::string &nameOut) {
    if (argExpr.kind == Expr::Kind::Name) {
      nameOut = argExpr.name;
      return true;
    }
    if (argExpr.kind != Expr::Kind::StringLiteral) {
      error = "meta." + queryName + " requires constant string or identifier argument";
      return false;
    }
    ParsedStringLiteral parsed;
    std::string parseError;
    if (!parseStringLiteralToken(argExpr.stringValue, parsed, parseError)) {
      error = "meta." + queryName + " requires utf8/ascii string literal argument";
      return false;
    }
    nameOut = parsed.decoded;
    if (nameOut.empty()) {
      error = "meta." + queryName + " requires non-empty " + nameLabel + " name";
      return false;
    }
    return true;
  };
  auto typePathForCanonical = [&](const std::string &canonicalType, const std::string &structPath) {
    if (!structPath.empty()) {
      return structPath;
    }
    if (!canonicalType.empty() && canonicalType.front() == '/') {
      return canonicalType;
    }
    return std::string("/") + canonicalType;
  };
  auto isNumericTraitType = [&](const std::string &canonicalType) {
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return false;
    }
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    return normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "f32" ||
           normalized == "f64" || normalized == "integer" || normalized == "decimal" ||
           normalized == "complex";
  };
  auto isComparableBuiltinTraitType = [&](const std::string &canonicalType) {
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return false;
    }
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    if (normalized == "complex") {
      return false;
    }
    if (isNumericTraitType(canonicalType)) {
      return true;
    }
    return normalized == "bool" || normalized == "string";
  };
  auto isIndexableBuiltinTraitType = [&](const std::string &canonicalType, const std::string &elemCanonicalType) {
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    if (normalized == "string") {
      return elemCanonicalType == "i32";
    }
    std::string base;
    std::string arg;
    if (!semantics::splitTemplateTypeName(canonicalType, base, arg)) {
      return false;
    }
    const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
    if (normalizedBase != "array" && normalizedBase != "vector") {
      return false;
    }
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
      return false;
    }
    return args.front() == elemCanonicalType;
  };
  auto resolveBindingCanonicalType = [&](const Definition &ownerDef,
                                         const Expr &bindingExpr,
                                         std::string &canonicalOut) -> bool {
    BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!semantics::parseBindingInfo(bindingExpr, ownerDef.namespacePrefix, structNames, importAliases, binding,
                                     restrictType, parseError)) {
      return false;
    }
    std::string typeName = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeName += "<" + binding.typeTemplateArg + ">";
    }
    if (typeName.empty()) {
      return false;
    }
    std::string ignoredStructPath;
    return canonicalizeTypeName(typeName, ownerDef.namespacePrefix, canonicalOut, ignoredStructPath);
  };
  auto resolveDefinitionReturnCanonicalType = [&](const Definition &definition, std::string &canonicalOut) -> bool {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return") {
        continue;
      }
      if (transform.templateArgs.size() != 1 || transform.templateArgs.front() == "auto") {
        return false;
      }
      std::string ignoredStructPath;
      return canonicalizeTypeName(transform.templateArgs.front(), definition.namespacePrefix, canonicalOut,
                                  ignoredStructPath);
    }
    return false;
  };
  auto definitionMatchesSignature = [&](const std::string &functionPath,
                                        const std::vector<std::string> &expectedParamTypes,
                                        const std::string &expectedReturnType) -> bool {
    auto defIt = definitionByPath.find(functionPath);
    if (defIt == definitionByPath.end()) {
      return false;
    }
    const Definition &definition = *defIt->second;
    if (definition.parameters.size() != expectedParamTypes.size()) {
      return false;
    }
    for (size_t index = 0; index < expectedParamTypes.size(); ++index) {
      std::string canonicalParamType;
      if (!resolveBindingCanonicalType(definition, definition.parameters[index], canonicalParamType)) {
        return false;
      }
      if (canonicalParamType != expectedParamTypes[index]) {
        return false;
      }
    }
    std::string canonicalReturnType;
    if (!resolveDefinitionReturnCanonicalType(definition, canonicalReturnType)) {
      return false;
    }
    return canonicalReturnType == expectedReturnType;
  };
  auto hasTraitConformance = [&](const std::string &traitName,
                                 const std::string &canonicalType,
                                 const std::string &structPath,
                                 const std::optional<std::string> &elemCanonicalType) {
    if (traitName == "Additive") {
      if (isNumericTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      return definitionMatchesSignature(basePath + "/plus", params, canonicalType);
    }
    if (traitName == "Multiplicative") {
      if (isNumericTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      return definitionMatchesSignature(basePath + "/multiply", params, canonicalType);
    }
    if (traitName == "Comparable") {
      if (isComparableBuiltinTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      if (!definitionMatchesSignature(basePath + "/equal", params, "bool")) {
        return false;
      }
      return definitionMatchesSignature(basePath + "/less_than", params, "bool");
    }
    if (traitName == "Indexable") {
      if (!elemCanonicalType.has_value()) {
        return false;
      }
      if (isIndexableBuiltinTraitType(canonicalType, *elemCanonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> countParams = {canonicalType};
      if (!definitionMatchesSignature(basePath + "/count", countParams, "i32")) {
        return false;
      }
      std::vector<std::string> atParams = {canonicalType, "i32"};
      return definitionMatchesSignature(basePath + "/at", atParams, *elemCanonicalType);
    }
    return false;
  };

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
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return true;
    }

    std::string queryName;
    if (expr.isMethodCall) {
      if (expr.args.empty() || expr.args.front().kind != Expr::Kind::Name || expr.args.front().name != "meta") {
        return true;
      }
      queryName = expr.name;
    } else if (expr.name.rfind("/meta/", 0) == 0) {
      queryName = expr.name.substr(6);
    } else {
      return true;
    }

    if (queryName != "type_name" && queryName != "type_kind" && queryName != "is_struct" &&
        queryName != "field_count" && queryName != "field_name" && queryName != "field_type" &&
        queryName != "field_visibility" && queryName != "has_transform" && queryName != "has_trait") {
      return true;
    }

    const bool needsTransformName = queryName == "has_transform";
    const bool needsTraitName = queryName == "has_trait";
    const bool needsFieldIndex =
        queryName == "field_name" || queryName == "field_type" || queryName == "field_visibility";
    const size_t expectedArgs =
        expr.isMethodCall ? (needsFieldIndex || needsTransformName || needsTraitName ? 2u : 1u)
                          : (needsFieldIndex || needsTransformName || needsTraitName ? 1u : 0u);
    if (expr.args.size() != expectedArgs) {
      if (needsFieldIndex) {
        error = "meta." + queryName + " requires exactly one index argument";
      } else if (needsTransformName) {
        error = "meta.has_transform requires exactly one transform-name argument";
      } else if (needsTraitName) {
        error = "meta.has_trait requires exactly one trait-name argument";
      } else {
        error = "meta." + queryName + " does not accept call arguments";
      }
      return false;
    }
    if (semantics::hasNamedArguments(expr.argNames)) {
      error = "meta." + queryName + " does not accept named arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error = "meta." + queryName + " does not accept block arguments";
      return false;
    }
    if (queryName == "has_trait") {
      if (expr.templateArgs.empty() || expr.templateArgs.size() > 2) {
        error = "meta.has_trait requires one or two template arguments";
        return false;
      }
    } else if (expr.templateArgs.size() != 1) {
      error = "meta." + queryName + " requires exactly one template argument";
      return false;
    }

    std::string canonicalType;
    std::string structPath;
    if (!canonicalizeTypeName(expr.templateArgs.front(), expr.namespacePrefix, canonicalType, structPath)) {
      return false;
    }

    Expr rewritten;
    rewritten.sourceLine = expr.sourceLine;
    rewritten.sourceColumn = expr.sourceColumn;
    if (queryName == "type_name") {
      rewritten.kind = Expr::Kind::StringLiteral;
      rewritten.stringValue = escapeStringLiteral(canonicalType);
    } else if (queryName == "type_kind") {
      rewritten.kind = Expr::Kind::StringLiteral;
      rewritten.stringValue = escapeStringLiteral(typeKindForCanonical(canonicalType, structPath));
    } else if (queryName == "is_struct") {
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = !structPath.empty();
    } else if (queryName == "field_count") {
      if (structPath.empty()) {
        error = "meta.field_count requires struct type argument: " + canonicalType;
        return false;
      }
      if (reflectedStructNames.count(structPath) == 0) {
        error = "meta.field_count requires reflect-enabled struct type argument: " + canonicalType;
        return false;
      }
      size_t fieldCount = 0;
      auto fieldMetaIt = structFieldMetadata.find(structPath);
      if (fieldMetaIt != structFieldMetadata.end()) {
        fieldCount = fieldMetaIt->second.size();
      }
      rewritten.kind = Expr::Kind::Literal;
      rewritten.intWidth = 32;
      rewritten.isUnsigned = false;
      rewritten.literalValue = static_cast<uint64_t>(fieldCount);
    } else if (queryName == "has_transform") {
      const size_t nameArg = expr.isMethodCall ? 1u : 0u;
      std::string transformName;
      if (!parseNamedQueryName(expr.args[nameArg], "has_transform", "transform", transformName)) {
        return false;
      }
      bool hasTransform = false;
      if (const Definition *targetDef = resolveDefinitionTarget(canonicalType, structPath, expr.namespacePrefix)) {
        for (const auto &transform : targetDef->transforms) {
          if (transform.name == transformName) {
            hasTransform = true;
            break;
          }
        }
      }
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = hasTransform;
    } else if (queryName == "has_trait") {
      const size_t nameArg = expr.isMethodCall ? 1u : 0u;
      std::string traitName;
      if (!parseNamedQueryName(expr.args[nameArg], "has_trait", "trait", traitName)) {
        return false;
      }
      traitName = trim(traitName);
      if (traitName.empty()) {
        error = "meta.has_trait requires non-empty trait name";
        return false;
      }
      const bool isAdditive = traitName == "Additive";
      const bool isMultiplicative = traitName == "Multiplicative";
      const bool isComparable = traitName == "Comparable";
      const bool isIndexable = traitName == "Indexable";
      if (!isAdditive && !isMultiplicative && !isComparable && !isIndexable) {
        error = "meta.has_trait does not support trait: " + traitName;
        return false;
      }
      if (isIndexable) {
        if (expr.templateArgs.size() != 2) {
          error = "meta.has_trait Indexable requires type and element template arguments";
          return false;
        }
      } else if (expr.templateArgs.size() != 1) {
        error = "meta.has_trait " + traitName + " requires exactly one type template argument";
        return false;
      }
      std::optional<std::string> elemCanonicalType;
      if (isIndexable) {
        std::string elemStructPath;
        std::string elemCanonical;
        if (!canonicalizeTypeName(expr.templateArgs[1], expr.namespacePrefix, elemCanonical, elemStructPath)) {
          return false;
        }
        elemCanonicalType = std::move(elemCanonical);
      }
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = hasTraitConformance(traitName, canonicalType, structPath, elemCanonicalType);
    } else {
      if (structPath.empty()) {
        error = "meta." + queryName + " requires struct type argument: " + canonicalType;
        return false;
      }
      if (reflectedStructNames.count(structPath) == 0) {
        error = "meta." + queryName + " requires reflect-enabled struct type argument: " + canonicalType;
        return false;
      }
      const size_t indexArg = expr.isMethodCall ? 1u : 0u;
      size_t fieldIndex = 0;
      if (!parseFieldIndex(expr.args[indexArg], queryName, fieldIndex)) {
        return false;
      }
      auto fieldMetaIt = structFieldMetadata.find(structPath);
      if (fieldMetaIt == structFieldMetadata.end() || fieldIndex >= fieldMetaIt->second.size()) {
        error = "meta." + queryName + " field index out of range for " + structPath + ": " +
                std::to_string(fieldIndex);
        return false;
      }
      const FieldMetadata &field = fieldMetaIt->second[fieldIndex];
      rewritten.kind = Expr::Kind::StringLiteral;
      if (queryName == "field_name") {
        rewritten.stringValue = escapeStringLiteral(field.name);
      } else if (queryName == "field_type") {
        rewritten.stringValue = escapeStringLiteral(field.typeName);
      } else {
        rewritten.stringValue = escapeStringLiteral(field.visibility);
      }
    }
    expr = std::move(rewritten);
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
  }
  for (auto &exec : program.executions) {
    stripTextTransforms(exec);
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
  if (!applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  if (!rewriteMaybeConstructors(program, error)) {
    return false;
  }
  if (!semantics::monomorphizeTemplates(program, entryPath, error)) {
    return false;
  }
  if (!rewriteReflectionMetadataQueries(program, error)) {
    return false;
  }
  if (!rewriteConvertConstructors(program, error)) {
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
  return true;
}

} // namespace primec
