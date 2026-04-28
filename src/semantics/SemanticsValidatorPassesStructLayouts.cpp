#include "SemanticsValidator.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>

namespace primec::semantics {

bool SemanticsValidator::validateStructLayouts() {
  auto failPassesStructLayoutsDiagnostic = [&](std::string message) -> bool {
    if (currentDefinitionContext_ != nullptr) {
      return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
  };
  struct LayoutInfo {
    uint32_t sizeBytes = 0;
    uint32_t alignmentBytes = 1;
  };
  auto isStructDefinition = [&](const Definition &def) {
    bool hasStruct = false;
    bool hasReturn = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "sum") {
        return false;
      }
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
  auto alignTo = [](uint32_t value, uint32_t alignment) -> uint32_t {
    if (alignment == 0) {
      return value;
    }
    uint32_t remainder = value % alignment;
    if (remainder == 0) {
      return value;
    }
    return value + (alignment - remainder);
  };
  auto extractAlignment = [&](const std::vector<Transform> &transforms,
                              const std::string &context,
                              uint32_t &alignmentOut,
                              bool &hasAlignment) -> bool {
    alignmentOut = 1;
    hasAlignment = false;
    for (const auto &transform : transforms) {
      if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
        continue;
      }
      if (hasAlignment) {
        return failPassesStructLayoutsDiagnostic("duplicate " + transform.name + " transform on " + context);
      }
      if (!validateAlignTransform(transform, context, error_)) {
        return failPassesStructLayoutsDiagnostic(error_);
      }
      auto parsePositiveInt = [](const std::string &text, int &valueOut) -> bool {
        std::string digits = text;
        if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
          digits.resize(digits.size() - 3);
        }
        if (digits.empty()) {
          return false;
        }
        int parsed = 0;
        for (char c : digits) {
          if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
          }
          int digit = c - '0';
          if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
            return false;
          }
          parsed = parsed * 10 + digit;
        }
        if (parsed <= 0) {
          return false;
        }
        valueOut = parsed;
        return true;
      };
      int value = 0;
      if (!parsePositiveInt(transform.arguments[0], value)) {
        return failPassesStructLayoutsDiagnostic(transform.name + " requires a positive integer argument");
      }
      uint64_t bytes = static_cast<uint64_t>(value);
      if (transform.name == "align_kbytes") {
        bytes *= 1024ull;
      }
      if (bytes > std::numeric_limits<uint32_t>::max()) {
        return failPassesStructLayoutsDiagnostic(transform.name + " alignment too large on " + context);
      }
      alignmentOut = static_cast<uint32_t>(bytes);
      hasAlignment = true;
    }
    return true;
  };
  auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    if (!typeName.empty() && typeName[0] == '/') {
      return typeName;
    }
    std::string resolved = resolveTypePath(typeName, namespacePrefix);
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string direct = current + "/" + typeName;
        if (structNames_.count(direct) > 0) {
          return direct;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structNames_.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + typeName;
        if (structNames_.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end()) {
      return importIt->second;
    }
    return resolved;
  };

  std::unordered_map<std::string, LayoutInfo> layoutCache;
  std::unordered_set<std::string> layoutStack;

  std::function<bool(const Definition &, LayoutInfo &)> computeStructLayout;
  std::function<bool(const BindingInfo &, const std::string &, LayoutInfo &)> typeLayoutForBinding;

  computeStructLayout = [&](const Definition &def, LayoutInfo &out) -> bool {
    DefinitionContextScope definitionScope(*this, def);
    auto isStaticField = [&](const Expr &stmt) -> bool {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    auto cached = layoutCache.find(def.fullPath);
    if (cached != layoutCache.end()) {
      out = cached->second;
      return true;
    }
    if (!layoutStack.insert(def.fullPath).second) {
      return failPassesStructLayoutsDiagnostic("recursive struct layout not supported: " + def.fullPath);
    }
    bool requireNoPadding = false;
    bool requirePlatformPadding = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "no_padding") {
        requireNoPadding = true;
      } else if (transform.name == "platform_independent_padding") {
        requirePlatformPadding = true;
      }
    }
    uint32_t structAlign = 1;
    uint32_t explicitStructAlign = 1;
    bool hasStructAlign = false;
    if (!extractAlignment(def.transforms, "struct " + def.fullPath, explicitStructAlign, hasStructAlign)) {
      return false;
    }
    uint32_t offset = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      const bool fieldIsStatic = isStaticField(stmt);
      if (fieldIsStatic) {
        continue;
      }
      BindingInfo binding;
      if (!resolveStructFieldBinding(def, stmt, binding)) {
        return failPassesStructLayoutsDiagnostic(error_);
      }
      LayoutInfo fieldLayout;
      if (!typeLayoutForBinding(binding, def.namespacePrefix, fieldLayout)) {
        return false;
      }
      uint32_t explicitFieldAlign = 1;
      bool hasFieldAlign = false;
      const std::string fieldContext = "field " + def.fullPath + "/" + stmt.name;
      if (!extractAlignment(stmt.transforms, fieldContext, explicitFieldAlign, hasFieldAlign)) {
        return false;
      }
      if (hasFieldAlign && explicitFieldAlign < fieldLayout.alignmentBytes) {
        return failPassesStructLayoutsDiagnostic("alignment requirement on " + fieldContext +
                                                 " is smaller than required alignment of " +
                                                 std::to_string(fieldLayout.alignmentBytes));
      }
      uint32_t fieldAlign = hasFieldAlign ? std::max(explicitFieldAlign, fieldLayout.alignmentBytes)
                                          : fieldLayout.alignmentBytes;
      uint32_t *activeOffset = &offset;
      uint32_t alignedOffset = alignTo(*activeOffset, fieldAlign);
      if (requireNoPadding && alignedOffset != *activeOffset) {
        return failPassesStructLayoutsDiagnostic("no_padding disallows alignment padding on " + fieldContext);
      }
      if (requirePlatformPadding && alignedOffset != *activeOffset && !hasFieldAlign) {
        return failPassesStructLayoutsDiagnostic("platform_independent_padding requires explicit alignment on " +
                                                 fieldContext);
      }
      *activeOffset = alignedOffset + fieldLayout.sizeBytes;
      if (!fieldIsStatic) {
        structAlign = std::max(structAlign, fieldAlign);
      }
    }
    if (hasStructAlign && explicitStructAlign < structAlign) {
      return failPassesStructLayoutsDiagnostic("alignment requirement on struct " + def.fullPath +
                                               " is smaller than required alignment of " +
                                               std::to_string(structAlign));
    }
    structAlign = hasStructAlign ? std::max(structAlign, explicitStructAlign) : structAlign;
    uint32_t totalSize = alignTo(offset, structAlign);
    if (requireNoPadding && totalSize != offset) {
      return failPassesStructLayoutsDiagnostic("no_padding disallows trailing padding on struct " + def.fullPath);
    }
    if (requirePlatformPadding && totalSize != offset && !hasStructAlign) {
      return failPassesStructLayoutsDiagnostic("platform_independent_padding requires explicit struct alignment on struct " +
                                               def.fullPath);
    }
    LayoutInfo layout{totalSize, structAlign};
    layoutCache.emplace(def.fullPath, layout);
    layoutStack.erase(def.fullPath);
    out = layout;
    return true;
  };

  typeLayoutForBinding = [&](const BindingInfo &binding,
                             const std::string &namespacePrefix,
                             LayoutInfo &layoutOut) -> bool {
    std::string normalized = normalizeBindingTypeName(binding.typeName);
    if (normalized == "i32" || normalized == "int" || normalized == "f32") {
      layoutOut = {4u, 4u};
      return true;
    }
    if (normalized == "i64" || normalized == "u64" || normalized == "f64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "bool") {
      layoutOut = {1u, 1u};
      return true;
    }
    if (normalized == "string") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "Pointer" || normalized == "Reference") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "array" || normalized == "vector" || normalized == "map" || normalized == "soa_vector") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "Result") {
      if (binding.typeTemplateArg.empty()) {
        return failPassesStructLayoutsDiagnostic("Result requires one or two template arguments");
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(binding.typeTemplateArg, args) ||
          (args.size() != 1 && args.size() != 2)) {
        return failPassesStructLayoutsDiagnostic("Result requires one or two template arguments");
      }
      layoutOut = (args.size() == 1) ? LayoutInfo{4u, 4u} : LayoutInfo{8u, 8u};
      return true;
    }
    if (normalized == "uninitialized") {
      if (binding.typeTemplateArg.empty()) {
        return failPassesStructLayoutsDiagnostic("uninitialized requires exactly one template argument");
      }
      std::string base;
      std::string arg;
      BindingInfo inner;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
        if (base == "uninitialized") {
          return failPassesStructLayoutsDiagnostic("nested uninitialized storage is not supported");
        }
        inner.typeName = base;
        inner.typeTemplateArg = arg;
      } else {
        inner.typeName = binding.typeTemplateArg;
      }
      return typeLayoutForBinding(inner, namespacePrefix, layoutOut);
    }
    std::string structPath = resolveStructTypePath(binding.typeName, namespacePrefix);
    auto defIt = defMap_.find(structPath);
    if (defIt == defMap_.end()) {
      return failPassesStructLayoutsDiagnostic("unknown struct type for layout: " + binding.typeName);
    }
    LayoutInfo nested;
    if (!computeStructLayout(*defIt->second, nested)) {
      return false;
    }
    layoutOut = nested;
    return true;
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    if (!isStructDefinition(def)) {
      continue;
    }
    LayoutInfo layout;
    if (!computeStructLayout(def, layout)) {
      return false;
    }
  }
  return true;
}

}  // namespace primec::semantics
