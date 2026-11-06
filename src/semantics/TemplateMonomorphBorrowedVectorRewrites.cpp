#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "primec/frontend/StringLiteral.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphCallableRewrites.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExpressionRewrite.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphFinalOrchestration.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphTypeResolution.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include <sstream>
#include "primec/support/CompileArena.h"
#include "primec/support/CollectionHelperNames.h"
#include "TemplateMonomorphExpressionRewriteState.h"

// Bare collection helper calls on a borrowed vector (`Reference<vector<T>>`) route to the borrowed-vector helpers.
namespace primec {

// TODO-5381: `count(dereference(r))` / `dereference(r).count()` where `r` is a
// `Reference<vector<T>>` means the borrowed helper applied to `r` itself;
// unwrap the dereference so the borrowed-vector routing below applies.
void unwrapDereferencedBorrowedVectorReceiver(Expr &expr,
                                              const std::vector<semantics::ParameterInfo> &params,
                                              const LocalTypeMap &locals,
                                              bool allowMathBare,
                                              Context &ctx) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isFieldAccess || expr.args.empty() ||
      hasNamedCallArguments(expr) || !expr.templateArgs.empty()) {
    return;
  }
  std::string helperName = expr.name;
  if (!helperName.empty() && helperName.front() == '/') {
    helperName.erase(helperName.begin());
  }
  if (collection_helpers::borrowedVectorHelperLeaf(helperName).empty()) {
    return;
  }
  const Expr &receiver = expr.args.front();
  if (receiver.kind != Expr::Kind::Call || receiver.isMethodCall || receiver.isBinding ||
      receiver.args.size() != 1 || !semantics::isSimpleCallName(receiver, "dereference")) {
    return;
  }
  semantics::BindingInfo pointerInfo;
  if (!inferBindingTypeForMonomorph(receiver.args.front(), params, locals, allowMathBare, ctx,
                                    pointerInfo) ||
      semantics::normalizeBindingTypeName(pointerInfo.typeName) != "Reference") {
    return;
  }
  std::string vectorBase;
  std::string elementType;
  if (!semantics::splitTemplateTypeName(semantics::normalizeBindingTypeName(pointerInfo.typeTemplateArg), vectorBase,
                             elementType) ||
      semantics::normalizeBindingTypeName(vectorBase) != "vector") {
    return;
  }
  Expr unwrapped = receiver.args.front();
  expr.args.front() = std::move(unwrapped);
}

// TODO-5375: bare `count(r)` / `push(r, x)` / ... where `r` is a
// `Reference<vector<T>>` routes to the canonical borrowed-vector helper with
// the element type as its template argument (method sugar does the same via
// resolveMethodCallTemplateTarget).
bool rewriteBorrowedVectorBareHelperCall(Expr &expr,
                                         const std::vector<semantics::ParameterInfo> &params,
                                         const LocalTypeMap &locals,
                                         bool allowMathBare,
                                         Context &ctx) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || expr.isFieldAccess ||
      expr.args.empty() || !expr.templateArgs.empty() ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() || hasNamedCallArguments(expr)) {
    return false;
  }
  const std::string_view leaf = collection_helpers::borrowedVectorHelperLeaf(expr.name);
  if (leaf.empty() || ctx.sourceDefs.count("/" + expr.name) > 0 ||
      ctx.helperOverloads.count("/" + expr.name) > 0) {
    return false;
  }
  // A bare call inside a namespace or a struct method (`vs[i]` in `/Holder/get`) carries that scope as
  // its prefix; it is still the builtin helper unless the scope defines its own.
  if (!expr.namespacePrefix.empty()) {
    const std::string scoped = expr.namespacePrefix + "/" + expr.name;
    if (ctx.sourceDefs.count(scoped) > 0 || ctx.helperOverloads.count(scoped) > 0) {
      return false;
    }
  }
  const std::string borrowedPath =
      std::string(collection_helpers::kCanonicalVectorPrefix) + std::string(leaf);
  if (ctx.sourceDefs.count(borrowedPath) == 0 && ctx.helperOverloads.count(borrowedPath) == 0) {
    return false;
  }
  semantics::BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo) ||
      semantics::normalizeBindingTypeName(receiverInfo.typeName) != "Reference") {
    return false;
  }
  std::string vectorBase;
  std::string elementType;
  if (!semantics::splitTemplateTypeName(semantics::normalizeBindingTypeName(receiverInfo.typeTemplateArg), vectorBase,
                             elementType) ||
      semantics::normalizeBindingTypeName(vectorBase) != "vector" || elementType.empty()) {
    return false;
  }
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  expr.name = borrowedPath;
  expr.templateArgs = {elementType};
  return true;
}

} // namespace primec
