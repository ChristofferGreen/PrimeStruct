#include "SemanticsValidateTransformsInternal.h"

#include "SemanticsHelpers.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

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
      type.width == 32 ? static_cast<int64_t>(std::numeric_limits<int32_t>::min())
                       : std::numeric_limits<int64_t>::min();
  const int64_t maxValue =
      type.width == 32 ? static_cast<int64_t>(std::numeric_limits<int32_t>::max())
                       : std::numeric_limits<int64_t>::max();
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
    constructorCall.isBraceConstructor = true;
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

} // namespace

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
    call.isBraceConstructor = true;
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

} // namespace primec::semantics
