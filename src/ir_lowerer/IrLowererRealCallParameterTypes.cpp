#include "IrLowererRealCallParameterTypes.h"

#include "IrLowererCollectionRecordShapes.h"
#include "IrLowererRecursionAnalysis.h"

#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/CollectionHelperNames.h"

#include <string_view>

namespace primec::ir_lowerer {

// A builtin `vector<T>` type (not the stdlib struct of the capitalized spelling) whose element the lowerer knows how to copy and
// destroy.
bool isBuiltinVectorType(const Transform &transform,
                         const std::string &namespacePrefix,
                         const std::unordered_set<std::string> &structNames) {
  if (transform.name != "vector" || transform.templateArgs.size() != 1 ||
      !transform.arguments.empty()) {
    return false;
  }
  std::string elementPath;
  return resolveBuiltinVectorElementStructPath(
      transform.templateArgs.front(),
      namespacePrefix,
      [&](const std::string &typeName, const std::string &prefix, std::string &pathOut) {
        pathOut = resolveStructTypeName(typeName, prefix, structNames);
        if (pathOut.empty() && typeName == "String" && structNames.count("/std/text/String") > 0) {
          pathOut =
              "/std/text/String"; // the stdlib text, declared `[String]` after `import /std/text/*`
        }
        return !pathOut.empty();
      },
      elementPath);
}

// `Reference<vector<T>>` of a builtin vector the lowerer can copy and destroy elements of: the argument is the
// vector's record address, as for a `mut` vector parameter.
bool isReferenceToBuiltinVector(const Transform &transform,
                                const std::string &namespacePrefix,
                                const std::unordered_set<std::string> &structNames) {
  if (transform.name != "Reference" || transform.templateArgs.size() != 1 ||
      !transform.arguments.empty()) {
    return false;
  }
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, argText)) {
    return false;
  }
  Transform inner;
  inner.name = trimTemplateTypeText(base);
  inner.templateArgs.push_back(trimTemplateTypeText(argText));
  return isBuiltinVectorType(inner, namespacePrefix, structNames);
}

bool isBorrowedSliceType(const Transform &transform,
                         const std::unordered_set<std::string> &structNames) {
  if (!transform.arguments.empty() || !transform.templateArgs.empty() ||
      structNames.count(transform.name) == 0) {
    return false;
  }
  // `ReadSlice<T>` and `WriteSlice<T>` are instantiated as `<slice module>/ReadSlice__t<hash>`.
  const std::string_view slicePrefix = collection_helpers::kCanonicalSlicePrefix;
  const std::string_view name = transform.name;
  if (name.substr(0, slicePrefix.size()) != slicePrefix) {
    return false;
  }
  const std::string_view leaf = name.substr(slicePrefix.size());
  return leaf.rfind("ReadSlice__t", 0) == 0 || leaf.rfind("WriteSlice__t", 0) == 0;
}

} // namespace primec::ir_lowerer
