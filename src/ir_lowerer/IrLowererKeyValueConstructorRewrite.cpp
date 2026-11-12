#include "IrLowererKeyValueConstructorRewrite.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

std::string extractParameterTypeName(const Expr &paramExpr) {
  for (const auto &transform : paramExpr.transforms) {
    if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
        transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
        transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
        transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    std::string typeName = transform.name;
    if (!transform.templateArgs.empty()) {
      typeName += "<";
      for (size_t index = 0; index < transform.templateArgs.size(); ++index) {
        if (index != 0) {
          typeName += ", ";
        }
        typeName += trimTemplateTypeText(transform.templateArgs[index]);
      }
      typeName += ">";
    }
    return typeName;
  }
  return {};
}

bool isPublishedKeyValueConstructorExpr(const Expr &callExpr, bool acceptBuiltinMap) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  std::string collectionName;
  if (acceptBuiltinMap && getBuiltinCollectionName(callExpr, collectionName) &&
      collectionName == "map") {
    return true;
  }
  const auto *metadata = keyValueConstructorSurfaceMetadata();
  return metadata != nullptr && isPublishedStdlibSurfaceConstructorExpr(callExpr, metadata->id);
}

bool rewritePublishedKeyValueConstructorExpr(const Expr &callExpr,
                                             LocalInfo::ValueKind fallbackKeyKind,
                                             LocalInfo::ValueKind fallbackValueKind,
                                             const ResolveCallDefinitionFn &resolveDefinitionCall,
                                             Expr &rewrittenExpr,
                                             const KeyValueConstructorRewriteOptions &options) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(callExpr) : nullptr;
  const auto *keyValueConstructorMetadata = keyValueConstructorSurfaceMetadata();
  const bool isResolvedPublishedConstructor =
      callee != nullptr && keyValueConstructorMetadata != nullptr &&
      isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(callee->fullPath,
                                                               keyValueConstructorMetadata->id);
  if (!isPublishedKeyValueConstructorExpr(callExpr, options.acceptBuiltinMap) &&
      !isResolvedPublishedConstructor) {
    return false;
  }
  rewrittenExpr = callExpr;
  // With `useResolvedCalleePath` the already-resolved concrete overload's own
  // path is used (callExpr's semanticNodeId arity-matched it above). The id is
  // zeroed below, so downstream resolution is a plain exact-path lookup with no
  // overload disambiguation; the shared family name would let whichever
  // overload is registered at that key win for every element of an args pack
  // (see docs/ReceiverTargetResolutionConsolidation.md).
  rewrittenExpr.name = options.useResolvedCalleePath && callee != nullptr
                           ? callee->fullPath
                           : canonicalKeyValueConstructorPath();
  rewrittenExpr.namespacePrefix.clear();
  rewrittenExpr.isMethodCall = false;
  rewrittenExpr.semanticNodeId = 0;
  if (rewrittenExpr.templateArgs.empty() && fallbackKeyKind != LocalInfo::ValueKind::Unknown &&
      fallbackValueKind != LocalInfo::ValueKind::Unknown) {
    rewrittenExpr.templateArgs = {
        typeNameForValueKind(fallbackKeyKind),
        typeNameForValueKind(fallbackValueKind),
    };
  } else if (rewrittenExpr.templateArgs.empty() && callee != nullptr &&
             callee->parameters.size() >= 2) {
    const std::string keyTypeName = extractParameterTypeName(callee->parameters[0]);
    const std::string valueTypeName = extractParameterTypeName(callee->parameters[1]);
    if (!keyTypeName.empty() && !valueTypeName.empty()) {
      rewrittenExpr.templateArgs = {keyTypeName, valueTypeName};
    }
  }
  return true;
}

} // namespace primec::ir_lowerer
