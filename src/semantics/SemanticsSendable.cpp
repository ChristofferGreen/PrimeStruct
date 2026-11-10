#include "SemanticsSendable.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateLambdaClosures.h"

#include <optional>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

std::string leafName(const std::string &name) {
  const size_t slash = name.find_last_of('/');
  return slash == std::string::npos ? name : name.substr(slash + 1);
}

// The leaf of a type name without the suffix a template instantiation adds (`Slice__t0123` is `Slice`).
std::string typeLeafName(const std::string &name) {
  std::string leaf = leafName(name);
  const size_t suffix = leaf.find("__t");
  if (suffix != std::string::npos) {
    leaf.erase(suffix);
  }
  return leaf;
}

bool isNonSendableLeafStruct(const std::string &leaf) {
  return leaf == "StringView" || leaf == "Slice" || leaf == "ReadSlice" || leaf == "WriteSlice";
}

bool isClosureStruct(const Definition &def) {
  return leafName(def.fullPath).find(LambdaClosureStructPrefix) != std::string::npos;
}

bool isStaticField(const Expr &fieldStmt) {
  for (const Transform &transform : fieldStmt.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

class SendableWalker {
public:
  explicit SendableWalker(const SendableTypeLookup &lookup) : lookup_(lookup) {}

  // The violation below `typeText` as {path segments, leaf type}, or nothing.
  std::optional<std::string> check(const std::string &typeText, const std::string &namespacePrefix) {
    const std::string normalized = normalizeBindingTypeName(typeText);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalized, base, argText)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      const std::string leaf = typeLeafName(normalizedBase);
      if (leaf == "Reference" || leaf == "Pointer" || leaf == "Task" || isNonSendableLeafStruct(leaf)) {
        return leafText(normalized);
      }
      if (leaf == "Function") {
        return std::nullopt; // sendable by construction: the conversion checks the captures
      }
      if (lookup_.resolveDefinition(normalizedBase, namespacePrefix) == nullptr) {
        // A builtin template (`vector`, `map`, `array`, `Maybe`, `Result`, `uninitialized`, ...): its elements.
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args)) {
          return std::nullopt;
        }
        for (const std::string &arg : args) {
          segments_.push_back("element of " + lookup_.display(normalized));
          if (auto violation = check(arg, namespacePrefix)) {
            return violation;
          }
          segments_.pop_back();
        }
        return std::nullopt;
      }
    }
    const Definition *def = lookup_.resolveDefinition(normalized, namespacePrefix);
    if (def == nullptr) {
      return std::nullopt;  // scalars, `string`, enums by value, unknown names
    }
    if (isNonSendableLeafStruct(typeLeafName(def->fullPath))) {
      return leafText(def->fullPath);
    }
    if (typeLeafName(def->fullPath).rfind("__Function", 0) == 0) {
      // `Function<R, A...>` erases its closure behind a pointer; it is sendable by construction, since the
      // validator checks the captures where a closure is converted (`validateFunctionBoxSendable`).
      return std::nullopt;
    }
    if (!visiting_.insert(def->fullPath).second) {
      return std::nullopt;
    }
    struct VisitingScope {
      std::unordered_set<std::string> &set;
      std::string path;
      ~VisitingScope() { set.erase(path); }
    } visitingScope{visiting_, def->fullPath};

    for (const SumVariant &variant : def->sumVariants) {
      if (!variant.hasPayload) {
        continue;
      }
      const std::string payload = !variant.payloadTypeText.empty() ? variant.payloadTypeText : variant.payloadType;
      if (payload.empty()) {
        continue;
      }
      segments_.push_back("variant `" + variant.name + "` of " + lookup_.display(def->fullPath));
      if (auto violation = check(payload, def->namespacePrefix)) {
        return violation;
      }
      segments_.pop_back();
    }

    const bool assertedSendable = hasSendableTransform(*def);
    const bool closure = isClosureStruct(*def);
    for (const Expr &fieldStmt : def->statements) {
      if (!fieldStmt.isBinding || isStaticField(fieldStmt)) {
        continue;
      }
      std::string fieldType;
      if (!lookup_.fieldType(*def, fieldStmt, fieldType) || fieldType.empty()) {
        continue;
      }
      segments_.push_back(closure ? "capture `" + fieldStmt.name + "`"
                                  : "field `" + fieldStmt.name + "` of " + lookup_.display(def->fullPath));
      std::optional<std::string> violation;
      if (assertedSendable) {
        // The struct owns what its pointers point to: check the pointee (through `uninitialized<T>`).
        std::string pointee = normalizeBindingTypeName(fieldType);
        std::string wrapperBase;
        std::string wrapperArg;
        while (splitTemplateTypeName(pointee, wrapperBase, wrapperArg)) {
          const std::string wrapperLeaf = typeLeafName(normalizeBindingTypeName(wrapperBase));
          if (wrapperLeaf != "Pointer" && wrapperLeaf != "Reference" && wrapperLeaf != "uninitialized") {
            break;
          }
          pointee = normalizeBindingTypeName(wrapperArg);
        }
        violation = check(pointee, def->namespacePrefix);
      } else {
        violation = check(fieldType, def->namespacePrefix);
      }
      if (violation) {
        return violation;
      }
      segments_.pop_back();
    }
    return std::nullopt;
  }

  std::string describe(const std::string &leaf) const {
    if (segments_.empty()) {
      return "it is " + leaf;
    }
    std::string text;
    for (size_t i = 0; i < segments_.size(); ++i) {
      text += (i == 0 ? "" : ", ") + segments_[i];
    }
    return text + " is " + leaf;
  }

private:
  std::string leafText(const std::string &typeText) const { return lookup_.display(typeText); }

  const SendableTypeLookup &lookup_;
  std::unordered_set<std::string> visiting_;
  std::vector<std::string> segments_;
};

} // namespace

bool hasSendableTransform(const Definition &def) {
  for (const Transform &transform : def.transforms) {
    if (transform.name == "sendable") {
      return true;
    }
  }
  return false;
}

std::string sendableViolation(const std::string &typeText,
                              const std::string &namespacePrefix,
                              const SendableTypeLookup &lookup) {
  SendableWalker walker(lookup);
  const std::optional<std::string> leaf = walker.check(typeText, namespacePrefix);
  return leaf ? walker.describe(*leaf) : std::string();
}

} // namespace primec::semantics
