#pragma once

// State shared by the rewriteExpr* phase functions (split out of rewriteExpr, TODO-5385).
#include "TemplateMonomorphExpressionRewrite.h"

#include <functional>
#include <optional>
#include <string>

namespace primec {

enum class PhaseStatus { Continue, Done };

struct RewriteExprState {
  bool result{};
  PhaseStatus done(bool value) {
    result = std::move(value);
    return PhaseStatus::Done;
  }
  bool hadExplicitTemplateArgsOnEntry{};
  std::function<Definition *(const Expr &target)> resolvePickSumDefinition;
  std::function<void(const SumVariant &variant, const Expr &arm, LocalTypeMap &armLocals)> appendPickPayloadLocal;
  std::function<void(Expr &bodyExpr, LocalTypeMap &bodyLocals)> recordBodyBindingLocal;
  std::function<bool(const std::string &path)> isCanonicalBuiltinKeyValueHelperPath;
  std::function<bool(const std::string &path)> isCanonicalStdlibCollectionHelperPath;
  std::function<bool(std::string_view path)> isTemplatedAutoCompatVectorHelperPath;
  std::function<bool(const std::string &path)> isSyntheticSamePathSoaHelperTemplateCarryPath;
  std::function<const Expr *(const Expr &candidate)> collectionHelperReceiverExpr;
  std::function<Expr *(Expr &candidate)> mutableCollectionHelperReceiverExpr;
  std::function<bool(const Expr *receiverExpr)> resolvesBuiltinKeyValueReceiver;
  std::function<bool(const Expr *receiverExpr)> resolvesBuiltinVectorReceiver;
  std::function<std::string(const Expr *receiverExpr)> inferCollectionReceiverFamilyForRewrite;
  std::function<bool(const Expr &receiverExpr)> resolvesSoaReceiverForRewrite;
  std::function<bool(const Expr *receiverExpr, std::vector<std::string> &templateArgsOut)> resolveExperimentalSoaVectorReceiverTemplateArgs;
  std::function<bool(const Expr *receiverExpr)> resolvesExperimentalSoaVectorReceiver;
  std::function<bool(const Expr *receiverExpr)> resolvesBorrowedExperimentalSoaVectorReceiver;
  std::function<bool(const Expr *receiverExpr)> resolvesConcreteExperimentalSoaVectorReceiver;
  std::function<std::string(const Expr *receiverExpr)> inferCollectionReceiverFamily;
  std::function<bool(const std::string &path)> isCanonicalSoaBorrowedWrapperHelper;
  std::function<std::string(const std::string &path)> preferredBorrowedSoaWrapperPath;
  std::function<std::string(const std::string &path)> preferCanonicalStdlibCollectionHelperPath;
  std::function<bool(const std::string &path)> shouldDeferStdlibCollectionHelperTemplateRewrite;
  std::function<bool(Expr &)> rewriteNestedExperimentalKeyValueConstructorValue{};
  std::function<bool(Expr &)> rewriteNestedExperimentalVectorConstructorValue{};
  std::function<bool(const std::string &, Expr &)> rewriteKeyValueTargetValueForResolvedType{};
  std::function<bool(const std::string &, Expr &)> rewriteVectorTargetValueForResolvedType{};
  bool allConcrete{};
  bool allowMathBare{};
};

PhaseStatus rewriteExprPhase1(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);
PhaseStatus rewriteExprPhase2(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);
PhaseStatus rewriteExprPhase3(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);
PhaseStatus rewriteExprPhase4(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);
PhaseStatus rewriteExprPhase5(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);
PhaseStatus rewriteExprPhase6(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);
PhaseStatus rewriteExprPhase7(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);
PhaseStatus rewriteExprPhase8(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st);

// Helpers of rewriteExpr shared by its phase units.
bool rewriteKeyValueWrapperHelperCallToMethod(Expr &expr, const std::string &namespacePrefix, Context &ctx, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, bool allowMathBare);
void unwrapDereferencedBorrowedVectorReceiver(Expr &expr, const std::vector<semantics::ParameterInfo> &params, const LocalTypeMap &locals, bool allowMathBare, Context &ctx);
bool rewriteBorrowedVectorBareHelperCall(Expr &expr, const std::vector<semantics::ParameterInfo> &params, const LocalTypeMap &locals, bool allowMathBare, Context &ctx);

} // namespace primec
