#include "SemanticsValidator.h"

#include <cctype>
#include <functional>
#include <string>
#include <vector>

namespace primec::semantics {
namespace {
struct TypeKey {
  std::string base;
  std::vector<TypeKey> args;
  bool isStruct = false;
};

std::string trimText(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

std::string formatTypeKey(const TypeKey &key) {
  if (key.args.empty()) {
    return key.base;
  }
  std::string out = key.base + "<";
  for (size_t i = 0; i < key.args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += formatTypeKey(key.args[i]);
  }
  out += ">";
  return out;
}

bool typeKeysMatch(const TypeKey &left, const TypeKey &right) {
  if (left.base != right.base) {
    return false;
  }
  if (left.args.size() != right.args.size()) {
    return false;
  }
  for (size_t i = 0; i < left.args.size(); ++i) {
    if (!typeKeysMatch(left.args[i], right.args[i])) {
      return false;
    }
  }
  return true;
}
} // namespace

bool SemanticsValidator::validateTraitConstraints() {
  auto resolveStructPath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return {};
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return structNames_.count(typeName) > 0 ? typeName : std::string{};
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string scoped = current + "/" + typeName;
        if (structNames_.count(scoped) > 0) {
          return scoped;
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
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    return {};
  };

  auto buildTypeKey = [&](const std::string &text,
                          const std::string &namespacePrefix,
                          TypeKey &out,
                          std::string &typeError) -> bool {
    std::function<bool(const std::string &, const std::string &, TypeKey &)> build;
    build = [&](const std::string &raw,
                const std::string &prefix,
                TypeKey &keyOut) -> bool {
      const std::string trimmed = trimText(raw);
      if (trimmed.empty()) {
        typeError = "trait type is empty";
        return false;
      }
      if (trimmed == "Self") {
        if (structNames_.count(prefix) == 0) {
          typeError = "Self is only valid inside struct definitions";
          return false;
        }
        keyOut.base = prefix;
        keyOut.isStruct = true;
        return true;
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(trimmed, base, arg)) {
        base = trimText(base);
        if (base.empty()) {
          typeError = "trait type has empty template base";
          return false;
        }
        if (base == "Self") {
          typeError = "Self is not a valid template base";
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args)) {
          typeError = "trait type has invalid template arguments";
          return false;
        }
        TypeKey result;
        if (isPrimitiveBindingTypeName(base)) {
          result.base = normalizeBindingTypeName(base);
        } else {
          std::string resolved = resolveStructPath(base, prefix);
          if (!resolved.empty()) {
            result.base = resolved;
            result.isStruct = true;
          } else {
            result.base = base;
          }
        }
        result.args.reserve(args.size());
        for (const auto &part : args) {
          TypeKey argKey;
          if (!build(part, prefix, argKey)) {
            return false;
          }
          result.args.push_back(std::move(argKey));
        }
        keyOut = std::move(result);
        return true;
      }
      if (trimmed == "void") {
        keyOut.base = "void";
        return true;
      }
      const std::string normalized = normalizeBindingTypeName(trimmed);
      if (isPrimitiveBindingTypeName(normalized)) {
        keyOut.base = normalized;
        return true;
      }
      if (trimmed == "FileError") {
        keyOut.base = trimmed;
        return true;
      }
      std::string resolved = resolveStructPath(trimmed, prefix);
      if (!resolved.empty()) {
        keyOut.base = resolved;
        keyOut.isStruct = true;
        return true;
      }
      if (trimmed == "array" || trimmed == "vector" || trimmed == "map" || trimmed == "Result" ||
          trimmed == "File" || trimmed == "Pointer" || trimmed == "Reference" || trimmed == "Buffer") {
        keyOut.base = trimmed;
        return true;
      }
      typeError = "trait type is not a supported type: " + trimmed;
      return false;
    };
    return build(text, namespacePrefix, out);
  };

  auto buildTypeKeyFromBinding = [&](const BindingInfo &binding,
                                     const std::string &namespacePrefix,
                                     TypeKey &out,
                                     std::string &typeError) -> bool {
    std::string typeText = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeText += "<" + binding.typeTemplateArg + ">";
    }
    return buildTypeKey(typeText, namespacePrefix, out, typeError);
  };

  auto resolveReturnTypeKey = [&](const Definition &def, TypeKey &out, std::string &typeError) -> bool {
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string &typeName = transform.templateArgs.front();
      if (typeName == "auto") {
        break;
      }
      return buildTypeKey(typeName, def.namespacePrefix, out, typeError);
    }
    auto structIt = returnStructs_.find(def.fullPath);
    if (structIt != returnStructs_.end()) {
      out.base = structIt->second;
      out.isStruct = true;
      return true;
    }
    auto kindIt = returnKinds_.find(def.fullPath);
    if (kindIt == returnKinds_.end()) {
      typeError = "trait return type is unknown";
      return false;
    }
    if (kindIt->second == ReturnKind::Void) {
      out.base = "void";
      return true;
    }
    std::string typeName = typeNameForReturnKind(kindIt->second);
    if (typeName.empty()) {
      typeError = "trait return type is unknown";
      return false;
    }
    out.base = typeName;
    return true;
  };

  auto typeKeyForPrimitive = [&](const char *name) -> TypeKey {
    TypeKey key;
    key.base = normalizeBindingTypeName(name);
    return key;
  };

  auto isNumericType = [&](const TypeKey &key) -> bool {
    if (!key.args.empty()) {
      return false;
    }
    return key.base == "i32" || key.base == "i64" || key.base == "u64" || key.base == "f32" ||
           key.base == "f64" || key.base == "integer" || key.base == "decimal" || key.base == "complex";
  };

  auto isComparableBuiltin = [&](const TypeKey &key) -> bool {
    if (!key.args.empty()) {
      return false;
    }
    if (key.base == "complex") {
      return false;
    }
    if (isNumericType(key)) {
      return true;
    }
    return key.base == "bool" || key.base == "string";
  };

  auto isIndexableBuiltin = [&](const TypeKey &typeKey, const TypeKey &elemKey) -> bool {
    if (typeKey.base == "array" || typeKey.base == "vector") {
      if (typeKey.args.size() != 1) {
        return false;
      }
      return typeKeysMatch(typeKey.args.front(), elemKey);
    }
    if (typeKey.base == "string") {
      if (!elemKey.args.empty()) {
        return false;
      }
      return elemKey.base == "i32";
    }
    return false;
  };

  auto typeKeyPath = [&](const TypeKey &key) -> std::string {
    if (!key.base.empty() && key.base[0] == '/') {
      return key.base;
    }
    return "/" + key.base;
  };

  auto makeTraitError = [&](const Definition &contextDef,
                            const std::string &traitDisplay,
                            const std::string &requirementText) -> bool {
    error_ = "trait constraint not satisfied on " + contextDef.fullPath + ": " + traitDisplay +
             " requires " + requirementText;
    return false;
  };

  auto validateTraitFunction = [&](const Definition &contextDef,
                                   const std::string &traitDisplay,
                                   const std::string &requirementText,
                                   const std::string &functionPath,
                                   const std::vector<TypeKey> &expectedParams,
                                   const TypeKey &expectedReturn) -> bool {
    auto defIt = defMap_.find(functionPath);
    if (defIt == defMap_.end()) {
      return makeTraitError(contextDef, traitDisplay, requirementText);
    }
    const Definition &def = *defIt->second;
    auto paramIt = paramsByDef_.find(functionPath);
    if (paramIt == paramsByDef_.end()) {
      return makeTraitError(contextDef, traitDisplay, requirementText);
    }
    const auto &params = paramIt->second;
    if (params.size() != expectedParams.size()) {
      return makeTraitError(contextDef, traitDisplay, requirementText);
    }
    for (size_t i = 0; i < expectedParams.size(); ++i) {
      TypeKey actualParam;
      std::string typeError;
      if (!buildTypeKeyFromBinding(params[i].binding, def.namespacePrefix, actualParam, typeError)) {
        error_ = "trait constraint type error on " + contextDef.fullPath + ": " + typeError;
        return false;
      }
      if (!typeKeysMatch(actualParam, expectedParams[i])) {
        return makeTraitError(contextDef, traitDisplay, requirementText);
      }
    }
    TypeKey actualReturn;
    std::string returnError;
    if (!resolveReturnTypeKey(def, actualReturn, returnError)) {
      error_ = "trait constraint type error on " + contextDef.fullPath + ": " + returnError;
      return false;
    }
    if (!typeKeysMatch(actualReturn, expectedReturn)) {
      return makeTraitError(contextDef, traitDisplay, requirementText);
    }
    return true;
  };

  const TypeKey boolKey = typeKeyForPrimitive("bool");
  const TypeKey intKey = typeKeyForPrimitive("i32");

  for (const auto &def : program_.definitions) {
    for (const auto &transform : def.transforms) {
      const bool isAdditive = transform.name == "Additive";
      const bool isMultiplicative = transform.name == "Multiplicative";
      const bool isComparable = transform.name == "Comparable";
      const bool isIndexable = transform.name == "Indexable";
      if (!isAdditive && !isMultiplicative && !isComparable && !isIndexable) {
        continue;
      }
      if (!transform.arguments.empty()) {
        error_ = "trait transforms do not accept arguments on " + def.fullPath;
        return false;
      }
      if (isIndexable) {
        if (transform.templateArgs.size() != 2) {
          error_ = "Indexable requires exactly two template arguments on " + def.fullPath;
          return false;
        }
      } else if (transform.templateArgs.size() != 1) {
        error_ = transform.name + " requires exactly one template argument on " + def.fullPath;
        return false;
      }

      TypeKey typeKey;
      std::string typeError;
      if (!buildTypeKey(transform.templateArgs.front(), def.namespacePrefix, typeKey, typeError)) {
        error_ = "trait constraint type error on " + def.fullPath + ": " + typeError;
        return false;
      }
      if (isAdditive) {
        if (isNumericType(typeKey)) {
          continue;
        }
        const std::string traitDisplay = "Additive<" + formatTypeKey(typeKey) + ">";
        const std::string requirement = "plus(" + formatTypeKey(typeKey) + ", " + formatTypeKey(typeKey) + ") -> " +
                                        formatTypeKey(typeKey);
        const std::string functionPath = typeKeyPath(typeKey) + "/plus";
        std::vector<TypeKey> params = {typeKey, typeKey};
        if (!validateTraitFunction(def, traitDisplay, requirement, functionPath, params, typeKey)) {
          return false;
        }
        continue;
      }
      if (isMultiplicative) {
        if (isNumericType(typeKey)) {
          continue;
        }
        const std::string traitDisplay = "Multiplicative<" + formatTypeKey(typeKey) + ">";
        const std::string requirement = "multiply(" + formatTypeKey(typeKey) + ", " + formatTypeKey(typeKey) +
                                        ") -> " + formatTypeKey(typeKey);
        const std::string functionPath = typeKeyPath(typeKey) + "/multiply";
        std::vector<TypeKey> params = {typeKey, typeKey};
        if (!validateTraitFunction(def, traitDisplay, requirement, functionPath, params, typeKey)) {
          return false;
        }
        continue;
      }
      if (isComparable) {
        if (isComparableBuiltin(typeKey)) {
          continue;
        }
        const std::string traitDisplay = "Comparable<" + formatTypeKey(typeKey) + ">";
        const std::string equalRequirement =
            "equal(" + formatTypeKey(typeKey) + ", " + formatTypeKey(typeKey) + ") -> bool";
        const std::string lessRequirement =
            "less_than(" + formatTypeKey(typeKey) + ", " + formatTypeKey(typeKey) + ") -> bool";
        const std::string basePath = typeKeyPath(typeKey);
        std::vector<TypeKey> params = {typeKey, typeKey};
        if (!validateTraitFunction(def, traitDisplay, equalRequirement, basePath + "/equal", params, boolKey)) {
          return false;
        }
        if (!validateTraitFunction(def, traitDisplay, lessRequirement, basePath + "/less_than", params, boolKey)) {
          return false;
        }
        continue;
      }
      if (isIndexable) {
        TypeKey elemKey;
        if (!buildTypeKey(transform.templateArgs[1], def.namespacePrefix, elemKey, typeError)) {
          error_ = "trait constraint type error on " + def.fullPath + ": " + typeError;
          return false;
        }
        if (isIndexableBuiltin(typeKey, elemKey)) {
          continue;
        }
        const std::string traitDisplay =
            "Indexable<" + formatTypeKey(typeKey) + ", " + formatTypeKey(elemKey) + ">";
        const std::string countRequirement =
            "count(" + formatTypeKey(typeKey) + ") -> " + formatTypeKey(intKey);
        const std::string atRequirement =
            "at(" + formatTypeKey(typeKey) + ", " + formatTypeKey(intKey) + ") -> " + formatTypeKey(elemKey);
        const std::string basePath = typeKeyPath(typeKey);
        std::vector<TypeKey> countParams = {typeKey};
        if (!validateTraitFunction(def, traitDisplay, countRequirement, basePath + "/count", countParams, intKey)) {
          return false;
        }
        std::vector<TypeKey> atParams = {typeKey, intKey};
        if (!validateTraitFunction(def, traitDisplay, atRequirement, basePath + "/at", atParams, elemKey)) {
          return false;
        }
      }
    }
  }

  return true;
}

} // namespace primec::semantics
