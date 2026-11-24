#include "IrLowererCollectionRecordShapes.h"

#include "IrLowererPackedErrorTypes.h"
#include "IrLowererRecursionAnalysis.h"

#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/CollectionHelperNames.h"

#include <string_view>
#include <vector>

namespace primec::ir_lowerer {

std::string resolveStructTypeName(const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  const std::unordered_set<std::string> &structNames) {
  if (typeName.empty()) {
    return {};
  }
  if (typeName.front() == '/') {
    return structNames.count(typeName) != 0 ? typeName : std::string{};
  }
  std::string prefix = namespacePrefix;
  while (true) {
    const std::string candidate = prefix + "/" + typeName;
    if (structNames.count(candidate) != 0) {
      return candidate;
    }
    if (prefix.empty()) {
      return {};
    }
    const size_t cut = prefix.find_last_of('/');
    prefix = cut == std::string::npos ? std::string() : prefix.substr(0, cut);
  }
}

namespace {

bool isBuiltinCollectionBase(std::string_view name) {
  return name == "vector" || name == "map" || name == "array";
}

// `name<args...>` names a collection record, looking through `Reference` and `Pointer`.
bool isCollectionRecordType(const std::string &name,
                            const std::vector<std::string> &args,
                            const std::string &namespacePrefix,
                            const std::unordered_set<std::string> &structNames) {
  if ((name == "Reference" || name == "Pointer") && args.size() == 1) {
    std::string base;
    std::string argText;
    const std::string inner = trimTemplateTypeText(args.front());
    if (splitTemplateTypeName(inner, base, argText)) {
      return isCollectionRecordType(trimTemplateTypeText(base),
                                    {trimTemplateTypeText(argText)},
                                    namespacePrefix,
                                    structNames);
    }
    return isCollectionRecordType(inner, {}, namespacePrefix, structNames);
  }
  if (isBuiltinCollectionBase(name)) {
    return true;
  }
  // `ContainerError` lives under the collections root but is a one-field error struct, not a collection record.
  const std::string path = resolveStructTypeName(name, namespacePrefix, structNames);
  return path.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) == 0 &&
         !isPackedErrorStructTypeName(path);
}

} // namespace

bool takesOrReturnsCollectionRecord(const Definition &def,
                                    const std::unordered_set<std::string> &structNames) {
  for (const Expr &param : def.parameters) {
    const Transform *type = taskParameterTypeTransform(param);
    if (type != nullptr &&
        isCollectionRecordType(type->name, type->templateArgs, def.namespacePrefix, structNames)) {
      return true;
    }
  }
  for (const Transform &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string text = trimTemplateTypeText(transform.templateArgs.front());
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(text, base, argText)) {
      return isCollectionRecordType(
          trimTemplateTypeText(base), {trimTemplateTypeText(argText)}, def.namespacePrefix, structNames);
    }
    return isCollectionRecordType(text, {}, def.namespacePrefix, structNames);
  }
  return false;
}

} // namespace primec::ir_lowerer
