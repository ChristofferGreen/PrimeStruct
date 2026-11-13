#pragma once

// What the rules of rewriteExpr read and fill in: the expression and its scope, and the facts the rules settle as
// they run. Data only; the lookups the rules share are free functions over it.
#include "TemplateMonomorphExpressionRewrite.h"

#include <string>
#include <vector>

namespace primec {

enum class PhaseStatus { Continue, Done };

struct ExprRewriteContext {
  Expr &expr;
  const SubstMap &mapping;
  const std::unordered_set<std::string> &allowedParams;
  const std::string &namespacePrefix;
  Context &ctx;
  std::string &error;
  const LocalTypeMap &locals;
  const std::vector<semantics::ParameterInfo> &params;
  bool allowMathBare{};
  bool hadExplicitTemplateArgsOnEntry{};
  bool allConcrete{};
  // The outcome once a rule has returned Done.
  bool result{};
  PhaseStatus done(bool value) {
    result = value;
    return PhaseStatus::Done;
  }
};

Definition *resolvePickSumDefinition(ExprRewriteContext &rc, const Expr &target);
void appendPickPayloadLocal(ExprRewriteContext &rc,
                            const SumVariant &variant,
                            const Expr &arm,
                            LocalTypeMap &armLocals);
void recordBodyBindingLocal(ExprRewriteContext &rc, Expr &bodyExpr, LocalTypeMap &bodyLocals);
bool isCanonicalBuiltinKeyValueHelperPath(ExprRewriteContext &rc, const std::string &path);
bool isCanonicalStdlibCollectionHelperPath(ExprRewriteContext &rc, const std::string &path);
bool isTemplatedAutoCompatVectorHelperPath(ExprRewriteContext &rc, std::string_view path);
bool isSyntheticSamePathSoaHelperTemplateCarryPath(ExprRewriteContext &rc, const std::string &path);
const Expr *collectionHelperReceiverExpr(ExprRewriteContext &rc, const Expr &candidate);
Expr *mutableCollectionHelperReceiverExpr(ExprRewriteContext &rc, Expr &candidate);
bool resolvesBuiltinKeyValueReceiver(ExprRewriteContext &rc, const Expr *receiverExpr);
bool resolvesBuiltinVectorReceiver(ExprRewriteContext &rc, const Expr *receiverExpr);
std::string inferCollectionReceiverFamilyForRewrite(ExprRewriteContext &rc,
                                                    const Expr *receiverExpr);
bool resolvesSoaReceiverForRewrite(ExprRewriteContext &rc, const Expr &receiverExpr);
bool resolveExperimentalSoaVectorReceiverTemplateArgs(ExprRewriteContext &rc,
                                                      const Expr *receiverExpr,
                                                      std::vector<std::string> &templateArgsOut);
bool resolvesExperimentalSoaVectorReceiver(ExprRewriteContext &rc, const Expr *receiverExpr);
bool resolvesBorrowedExperimentalSoaVectorReceiver(ExprRewriteContext &rc,
                                                   const Expr *receiverExpr);
bool resolvesConcreteExperimentalSoaVectorReceiver(ExprRewriteContext &rc,
                                                   const Expr *receiverExpr);
std::string inferCollectionReceiverFamily(ExprRewriteContext &rc, const Expr *receiverExpr);
bool isCanonicalSoaBorrowedWrapperHelper(ExprRewriteContext &rc, const std::string &path);
std::string preferredBorrowedSoaWrapperPath(ExprRewriteContext &rc, const std::string &path);
std::string preferCanonicalStdlibCollectionHelperPath(ExprRewriteContext &rc,
                                                      const std::string &path);
bool shouldDeferStdlibCollectionHelperTemplateRewrite(ExprRewriteContext &rc,
                                                      const std::string &path);
bool rewriteNestedExperimentalKeyValueConstructorValue(ExprRewriteContext &rc, Expr &candidate);
bool rewriteNestedExperimentalVectorConstructorValue(ExprRewriteContext &rc, Expr &candidate);
bool rewriteKeyValueTargetValueForResolvedType(ExprRewriteContext &rc,
                                               const std::string &typeText,
                                               Expr &valueExpr);
bool rewriteVectorTargetValueForResolvedType(ExprRewriteContext &rc,
                                             const std::string &typeText,
                                             Expr &valueExpr);
bool rewriteNestedExperimentalKeyValueResultOkPayloadValue(ExprRewriteContext &rc, Expr &candidate);
bool resolvesExperimentalSoaReceiverForRewrite(ExprRewriteContext &rc, const Expr &receiverExpr);
std::string preferredBorrowedPathForPublicOrCompatibility(ExprRewriteContext &rc,
                                                          const std::string &canonicalPath,
                                                          std::string_view borrowedHelper);
PhaseStatus rewriteNameAndTransforms(ExprRewriteContext &rc);
PhaseStatus rewriteLiteralAndTaskCalls(ExprRewriteContext &rc);
PhaseStatus rewriteCollectionHelperTemplate(ExprRewriteContext &rc);
PhaseStatus rewritePlainCallReference(ExprRewriteContext &rc);
PhaseStatus rewriteMethodAndBuiltinTargets(ExprRewriteContext &rc);
PhaseStatus rewriteChildExpressions(ExprRewriteContext &rc);

// Helpers of rewriteExpr shared by its phase units.
bool rewriteKeyValueWrapperHelperCallToMethod(Expr &expr,
                                              const std::string &namespacePrefix,
                                              Context &ctx,
                                              const LocalTypeMap &locals,
                                              const std::vector<semantics::ParameterInfo> &params,
                                              bool allowMathBare);
bool foldLiteralTextCall(Expr &expr, std::string &error);
bool rewriteVectorViewMethod(Expr &expr,
                             Context &ctx,
                             const LocalTypeMap &locals,
                             const std::vector<semantics::ParameterInfo> &params,
                             bool allowMathBare);
bool rewriteStructIndexAssign(Expr &expr,
                              const std::string &namespacePrefix,
                              Context &ctx,
                              const LocalTypeMap &locals,
                              const std::vector<semantics::ParameterInfo> &params,
                              bool allowMathBare);
bool rewriteStructOperatorCallToMethod(Expr &expr,
                                       const std::string &namespacePrefix,
                                       Context &ctx,
                                       const LocalTypeMap &locals,
                                       const std::vector<semantics::ParameterInfo> &params,
                                       bool allowMathBare);
void unwrapDereferencedBorrowedVectorReceiver(Expr &expr,
                                              const std::vector<semantics::ParameterInfo> &params,
                                              const LocalTypeMap &locals,
                                              bool allowMathBare,
                                              Context &ctx);
bool rewriteBorrowedVectorBareHelperCall(Expr &expr,
                                         const std::vector<semantics::ParameterInfo> &params,
                                         const LocalTypeMap &locals,
                                         bool allowMathBare,
                                         Context &ctx);

} // namespace primec
