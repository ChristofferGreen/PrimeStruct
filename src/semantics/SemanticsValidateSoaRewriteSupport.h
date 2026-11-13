#pragma once

// The walk every SoA rewrite pass shares. A pass supplies one function that rewrites a single
// expression given the bindings visible at it; the statement walker, the per-definition setup and
// the receiver lookups below are common to all passes.

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateSoaBindingExtraction.h"
#include "primec/semantics/Semantics.h"

namespace primec {

using SoaBindingMap = std::unordered_map<std::string, semantics::BindingInfo>;

// Rewrites every statement of `statements` in order with `rewriteExpr(stmt, bindings)`, then walks
// the statement's body with a copy of the bindings, then records the statement's own binding with
// `trackBinding(stmt, bindings)`. Bindings a body declares do not leak out of it.
template <typename Bindings, typename RewriteExpr, typename TrackBinding>
void rewriteSoaStatements(std::vector<Expr> &statements,
                          Bindings bindings,
                          const RewriteExpr &rewriteExpr,
                          const TrackBinding &trackBinding) {
  for (Expr &stmt : statements) {
    rewriteExpr(stmt, bindings);
    if (!stmt.bodyArguments.empty()) {
      rewriteSoaStatements(stmt.bodyArguments, bindings, rewriteExpr, trackBinding);
    }
    if (stmt.isBinding) {
      trackBinding(stmt, bindings);
    }
  }
}

// The namespace of a definition: its path up to the last slash (empty at the root).
std::string soaDefinitionNamespace(const std::string &definitionPath);

// What a builtin SoA rewrite pass reads about the program while it walks one definition.
struct BuiltinSoaRewriteScope {
  const SoaBindingMap &vectorReturnDefinitions;
  const SoaBindingMap &soaCollectionReturnDefinitions;
  const std::string &definitionNamespace;
};

using SoaReturnBindingExtractor = std::optional<semantics::BindingInfo> (*)(const Definition &);

// The binding each definition returns, under its full path and under its bare name.
SoaBindingMap collectSoaReturnDefinitionBindings(const Program &program,
                                                 SoaReturnBindingExtractor extractReturnBinding);

// Records a vector or SoA vector binding declared by `expr` (a parameter or a binding statement).
void trackBuiltinSoaBinding(const Expr &expr, SoaBindingMap &bindings);

// Runs `rewriteExpr(expr, bindings, scope)` over the statements and the return expression of every
// definition, with the bindings its parameters and statements declare. `extractSoaReturnBinding`
// picks which definitions count as returning an SoA collection.
template <typename RewriteExpr>
void rewriteBuiltinSoaDefinitions(Program &program,
                                  SoaReturnBindingExtractor extractSoaReturnBinding,
                                  const RewriteExpr &rewriteExpr) {
  const SoaBindingMap vectorReturnDefinitions =
      collectSoaReturnDefinitionBindings(program, extractBuiltinVectorReturnBinding);
  const SoaBindingMap soaCollectionReturnDefinitions =
      collectSoaReturnDefinitionBindings(program, extractSoaReturnBinding);
  for (Definition &def : program.definitions) {
    SoaBindingMap bindings;
    for (const Expr &param : def.parameters) {
      trackBuiltinSoaBinding(param, bindings);
    }
    const std::string definitionNamespace = soaDefinitionNamespace(def.fullPath);
    const BuiltinSoaRewriteScope scope{
        vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace};
    rewriteSoaStatements(
        def.statements,
        bindings,
        [&](Expr &expr, const SoaBindingMap &visible) { rewriteExpr(expr, visible, scope); },
        [](const Expr &stmt, SoaBindingMap &visible) { trackBuiltinSoaBinding(stmt, visible); });
    if (def.returnExpr.has_value()) {
      rewriteExpr(*def.returnExpr, bindings, scope);
    }
  }
}

// The vector `candidate` evaluates to: a vector binding, a vector literal, a call to a function
// that returns a vector, or a dereference of one.
std::optional<semantics::BindingInfo> findBuiltinVectorValueBinding(
    const Expr &candidate, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope);

// The SoA vector `candidate` evaluates to, with whether it is borrowed. The access and count
// passes use this lookup; the conversion and mutator passes have their own.
std::optional<BuiltinSoaReceiverBindingInfo> findBuiltinSoaReceiverValueBinding(
    const Expr &candidate, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope);

// What the experimental SoA field-view passes read about the whole program.
struct ExperimentalSoaProgramData {
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaFieldHelpers;
  SoaBindingMap soaCollectionReturnDefinitions;
  std::unordered_map<std::string, std::string> specializedSoaVectorElementTypes;
};

ExperimentalSoaProgramData collectExperimentalSoaProgramData(const Program &program);

// Records an SoA binding, parsed or experimental, declared by `expr`.
void trackExperimentalSoaBinding(const Expr &expr,
                                 const std::unordered_set<std::string> &structPaths,
                                 SoaBindingMap &bindings);

// What an experimental SoA field-view pass reads while it walks one definition.
struct ExperimentalSoaRewriteScope {
  const SoaBindingMap &allBindings;
  const ExperimentalSoaProgramData &program;
  const std::string &definitionNamespace;
};

// Runs `rewriteExpr(expr, bindings, scope)` over the statements and the return expression of every
// definition. `allBindings` holds every binding the definition declares anywhere in its body.
template <typename RewriteExpr>
void rewriteExperimentalSoaDefinitions(Program &program,
                                       const ExperimentalSoaProgramData &data,
                                       const RewriteExpr &rewriteExpr) {
  for (Definition &def : program.definitions) {
    SoaBindingMap bindings;
    for (const Expr &param : def.parameters) {
      trackExperimentalSoaBinding(param, data.structPaths, bindings);
    }
    SoaBindingMap allBindings = bindings;
    const auto collectBindings = [&](const auto &self,
                                     const std::vector<Expr> &statements) -> void {
      for (const Expr &stmt : statements) {
        if (stmt.isBinding) {
          trackExperimentalSoaBinding(stmt, data.structPaths, allBindings);
        }
        if (!stmt.bodyArguments.empty()) {
          self(self, stmt.bodyArguments);
        }
      }
    };
    collectBindings(collectBindings, def.statements);
    const std::string definitionNamespace = soaDefinitionNamespace(def.fullPath);
    const ExperimentalSoaRewriteScope scope{allBindings, data, definitionNamespace};
    rewriteSoaStatements(
        def.statements,
        bindings,
        [&](Expr &expr, const SoaBindingMap &visible) { rewriteExpr(expr, visible, scope); },
        [&](const Expr &stmt, SoaBindingMap &visible) {
          trackExperimentalSoaBinding(stmt, data.structPaths, visible);
        });
    if (def.returnExpr.has_value()) {
      rewriteExpr(*def.returnExpr, bindings, scope);
    }
  }
}

} // namespace primec
