#include "SemanticsValidateCompileTimeIf.h"

#include "RequirementPredicateFacts.h"
#include "SemanticsHelpers.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec {


void eraseCompileTimeTypeBindingsFromExpr(Expr &expr);

std::string joinTypeLocalTemplateArgs(const std::vector<std::string> &args) {
  std::string text;
  for (size_t index = 0; index < args.size(); ++index) {
    if (index != 0) {
      text += ", ";
    }
    text += args[index];
  }
  return text;
}

bool splitTypeLocalTypeText(const std::string &typeText,
                            std::string &baseOut,
                            std::vector<std::string> &argsOut) {
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(typeText, base, argText)) {
    baseOut = semantics::normalizeBindingTypeName(typeText);
    argsOut.clear();
    return !baseOut.empty();
  }
  argsOut.clear();
  if (!semantics::splitTopLevelTemplateArgs(argText, argsOut)) {
    return false;
  }
  baseOut = semantics::normalizeBindingTypeName(base);
  return !baseOut.empty();
}

void applyTypeLocalFactsToBindingEnvelope(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &typeFacts) {
  if (!expr.isBinding || semantics::isCompileTimeTypeBinding(expr)) {
    return;
  }
  for (Transform &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" ||
        transform.name == "return" ||
        semantics::isBindingAuxTransformName(transform.name)) {
      continue;
    }
    for (std::string &templateArg : transform.templateArgs) {
      auto argIt = typeFacts.find(templateArg);
      if (argIt != typeFacts.end()) {
        templateArg = argIt->second;
      }
    }
    auto typeIt = typeFacts.find(transform.name);
    if (typeIt == typeFacts.end()) {
      continue;
    }
    std::string base;
    std::vector<std::string> args;
    if (splitTypeLocalTypeText(typeIt->second, base, args)) {
      transform.name = std::move(base);
      transform.templateArgs = std::move(args);
    }
  }
}

bool bindingEnvelopeTypeText(const Expr &expr, std::string &typeTextOut) {
  if (!expr.isBinding || !semantics::hasExplicitBindingTypeTransform(expr)) {
    return false;
  }
  for (const Transform &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" ||
        transform.name == "return" ||
        semantics::isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    typeTextOut = semantics::normalizeBindingTypeName(transform.name);
    if (!transform.templateArgs.empty()) {
      typeTextOut += "<" + joinTypeLocalTemplateArgs(transform.templateArgs) + ">";
    }
    return true;
  }
  return false;
}

bool resolveTypeLocalInitializer(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &valueFacts,
    const std::unordered_map<std::string, std::string> &typeFacts,
    std::string &typeTextOut) {
  if (!semantics::isCompileTimeTypeBinding(expr) || expr.args.size() != 1) {
    return false;
  }
  const Expr *typeExpr = &expr.args.front();
  const Expr &initializer = expr.args.front();
  if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
      initializer.hasBodyArguments && initializer.args.empty() &&
      initializer.templateArgs.empty() &&
      !semantics::hasNamedArguments(initializer.argNames)) {
    if (initializer.bodyArguments.size() != 1) {
      return false;
    }
    typeExpr = &initializer.bodyArguments.front();
  }
  if (typeExpr->kind == Expr::Kind::Name) {
    auto typeIt = typeFacts.find(typeExpr->name);
    if (typeIt != typeFacts.end()) {
      typeTextOut = typeIt->second;
      return true;
    }
    if (!typeExpr->name.empty() && typeExpr->name != "auto") {
      typeTextOut = semantics::normalizeBindingTypeName(typeExpr->name);
      return true;
    }
    return false;
  }
  if (typeExpr->kind != Expr::Kind::Call || typeExpr->name != "typeof" ||
      typeExpr->templateArgs.size() != 1 ||
      typeExpr->templateArgDetails.size() != 1 ||
      typeExpr->templateArgDetails.front().kind !=
          TemplateArgumentKind::Symbol) {
    return false;
  }
  const std::string &symbol = typeExpr->templateArgs.front();
  auto valueIt = valueFacts.find(symbol);
  if (valueIt != valueFacts.end()) {
    typeTextOut = valueIt->second;
    return true;
  }
  auto typeIt = typeFacts.find(symbol);
  if (typeIt != typeFacts.end()) {
    typeTextOut = typeIt->second;
    return true;
  }
  return false;
}

void eraseCompileTimeTypeBindingsFromExprs(
    std::vector<Expr> &exprs,
    std::unordered_map<std::string, std::string> valueFacts = {}) {
  std::unordered_map<std::string, std::string> typeFacts;
  for (Expr &expr : exprs) {
    if (semantics::isCompileTimeTypeBinding(expr)) {
      std::string typeText;
      if (resolveTypeLocalInitializer(expr, valueFacts, typeFacts, typeText)) {
        typeFacts[expr.name] = std::move(typeText);
      }
      continue;
    }
    applyTypeLocalFactsToBindingEnvelope(expr, typeFacts);
    eraseCompileTimeTypeBindingsFromExpr(expr);
    std::string valueType;
    if (bindingEnvelopeTypeText(expr, valueType)) {
      valueFacts[expr.name] = std::move(valueType);
    }
  }
  exprs.erase(std::remove_if(exprs.begin(), exprs.end(), [](const Expr &expr) {
                return semantics::isCompileTimeTypeBinding(expr);
              }),
              exprs.end());
}

void eraseCompileTimeTypeBindingsFromExpr(Expr &expr) {
  eraseCompileTimeTypeBindingsFromExprs(expr.args);
  eraseCompileTimeTypeBindingsFromExprs(expr.bodyArguments);
}

void applyEnclosingTypeLocalsToNestedStructs(Program &program) {
  std::unordered_map<std::string, Definition *> definitionsByPath;
  definitionsByPath.reserve(program.definitions.size());
  for (Definition &def : program.definitions) {
    definitionsByPath[def.fullPath] = &def;
  }

  auto sourceComesBefore = [](const Expr &expr, const Definition &def) {
    if (expr.sourceLine <= 0 || def.sourceLine <= 0) {
      return false;
    }
    if (expr.sourceLine != def.sourceLine) {
      return expr.sourceLine < def.sourceLine;
    }
    return expr.sourceColumn > 0 && expr.sourceColumn < def.sourceColumn;
  };

  for (Definition &nestedDef : program.definitions) {
    if (!nestedDef.isNested || !semantics::isStructLikeDefinition(nestedDef)) {
      continue;
    }
    auto parentIt = definitionsByPath.find(nestedDef.namespacePrefix);
    if (parentIt == definitionsByPath.end() || parentIt->second == nullptr ||
        semantics::isStructLikeDefinition(*parentIt->second)) {
      continue;
    }
    const Definition &parent = *parentIt->second;
    std::unordered_map<std::string, std::string> valueFacts;
    std::unordered_map<std::string, std::string> typeFacts;
    for (const Expr &param : parent.parameters) {
      std::string valueType;
      if (bindingEnvelopeTypeText(param, valueType)) {
        valueFacts[param.name] = std::move(valueType);
      }
    }
    for (const Expr &stmt : parent.statements) {
      if (!sourceComesBefore(stmt, nestedDef)) {
        continue;
      }
      if (semantics::isCompileTimeTypeBinding(stmt)) {
        std::string typeText;
        if (resolveTypeLocalInitializer(stmt, valueFacts, typeFacts, typeText)) {
          typeFacts[stmt.name] = std::move(typeText);
        }
        continue;
      }
      std::string valueType;
      if (bindingEnvelopeTypeText(stmt, valueType)) {
        valueFacts[stmt.name] = std::move(valueType);
      }
    }
    for (Expr &field : nestedDef.statements) {
      applyTypeLocalFactsToBindingEnvelope(field, typeFacts);
    }
  }
}

void eraseCompileTimeTypeBindings(Program &program) {
  applyEnclosingTypeLocalsToNestedStructs(program);
  for (Definition &def : program.definitions) {
    eraseCompileTimeTypeBindingsFromExprs(def.parameters);
    std::unordered_map<std::string, std::string> parameterValueFacts;
    for (const Expr &param : def.parameters) {
      std::string valueType;
      if (bindingEnvelopeTypeText(param, valueType)) {
        parameterValueFacts[param.name] = std::move(valueType);
      }
    }
    eraseCompileTimeTypeBindingsFromExprs(def.statements,
                                          std::move(parameterValueFacts));
    if (def.returnExpr.has_value()) {
      eraseCompileTimeTypeBindingsFromExpr(*def.returnExpr);
    }
  }
  for (Execution &exec : program.executions) {
    eraseCompileTimeTypeBindingsFromExprs(exec.arguments);
    eraseCompileTimeTypeBindingsFromExprs(exec.bodyArguments);
  }
}

std::string joinCompileTimeIfTemplateArgs(const std::vector<std::string> &args) {
  std::ostringstream out;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << args[i];
  }
  return out.str();
}

bool serializeCompileTimeIfPredicateExpr(const Expr &expr, std::string &out) {
  out.clear();
  switch (expr.kind) {
  case Expr::Kind::Name:
    out = expr.name;
    return !out.empty();
  case Expr::Kind::BoolLiteral:
    out = expr.boolValue ? "true" : "false";
    return true;
  case Expr::Kind::Literal:
    out = std::to_string(static_cast<std::int64_t>(expr.literalValue));
    if (expr.isUnsigned) {
      out += expr.intWidth == 64 ? "u64" : "u32";
    } else {
      out += expr.intWidth == 64 ? "i64" : "i32";
    }
    return true;
  case Expr::Kind::StringLiteral:
    out = expr.stringValue;
    return !out.empty();
  case Expr::Kind::FloatLiteral:
    out = expr.floatValue + (expr.floatWidth == 64 ? "f64" : "f32");
    return true;
  case Expr::Kind::Call:
    break;
  }

  if (expr.isMethodCall || expr.isFieldAccess || expr.isBinding ||
      expr.isBraceConstructor || !expr.transforms.empty() ||
      expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return false;
  }
  std::ostringstream printed;
  printed << (expr.sourceName.empty() ? expr.name : expr.sourceName);
  if (!expr.templateArgs.empty()) {
    printed << "<" << joinCompileTimeIfTemplateArgs(expr.templateArgs) << ">";
  }
  printed << "(";
  for (std::size_t i = 0; i < expr.args.size(); ++i) {
    if (i != 0) {
      printed << ", ";
    }
    std::string argText;
    if (!serializeCompileTimeIfPredicateExpr(expr.args[i], argText)) {
      return false;
    }
    printed << argText;
  }
  printed << ")";
  out = printed.str();
  return !expr.name.empty();
}

bool isTransformNamed(const std::vector<Transform> &transforms,
                      std::string_view name) {
  for (const Transform &transform : transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

std::string compileTimeIfBindingTypeText(
    const semantics::BindingInfo &binding) {
  if (!binding.typeTemplateArg.empty()) {
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  }
  return binding.typeName;
}

std::string compileTimeIfReturnTypeText(const Definition &definition) {
  for (const Transform &transform : definition.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      return transform.templateArgs.front();
    }
  }
  return {};
}

void addCompileTimeIfImportAlias(
    std::unordered_map<std::string, std::string> &aliases,
    const std::string &path) {
  if (path.empty()) {
    return;
  }
  const std::size_t slash = path.find_last_of('/');
  const std::string alias =
      slash == std::string::npos ? path : path.substr(slash + 1);
  if (!alias.empty()) {
    aliases.try_emplace(alias, path);
  }
}

// Facts derived from *every* definition in the program (as a potential
// requirement-predicate "candidate": callable signature, struct field, or
// struct trait). These facts depend only on `program.definitions` and the
// `structNames`/`importAliases` state as of the start of
// `rewriteCompileTimeIfBranches`'s definitions loop - NOT on which
// definition is currently being processed. `structNames` does grow during
// that loop (ct_if branch resolution inserts freshly-generated, per-
// definition-nested struct paths - see the `structNames.insert(...)` call
// in the branch-selection helper below), but every path it gains is a
// synthesized nested path that is never itself a member of
// `program.definitions` at the time these facts are computed (generated
// definitions are only merged into `program.definitions` after the whole
// loop finishes), so `structNames.count(candidate.fullPath)` for every
// `candidate` here is unaffected by those later insertions. That makes
// this candidate-facts computation safe to hoist out of the per-definition
// loop and compute exactly once instead of once per definition - see
// TODO-5232 for the measured redundancy this eliminates.
struct CompileTimeIfCandidateFacts {
  std::vector<semantics::RequirementPredicateDefinitionContext::CallableFact> callables;
  std::vector<semantics::RequirementPredicateDefinitionContext::StructFieldFact> structFields;
  std::vector<semantics::RequirementPredicateDefinitionContext::StructTraitFact> structTraits;
};

CompileTimeIfCandidateFacts buildCompileTimeIfCandidateFacts(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases) {
  CompileTimeIfCandidateFacts facts;
  for (const Definition &candidate : program.definitions) {
    semantics::RequirementPredicateDefinitionContext::CallableFact callable;
    callable.fullPath = candidate.fullPath;
    callable.namespacePrefix = candidate.namespacePrefix;
    callable.templateArgs = candidate.templateArgs;
    callable.returnType = compileTimeIfReturnTypeText(candidate);
    callable.isPrivate = isTransformNamed(candidate.transforms, "private");
    for (const Transform &transform : candidate.transforms) {
      if (transform.name == "effects" && transform.templateArgs.empty()) {
        callable.effectNames.insert(callable.effectNames.end(),
                                    transform.arguments.begin(),
                                    transform.arguments.end());
      }
    }
    callable.hasReturnExpr = candidate.returnExpr.has_value();
    if (candidate.returnExpr.has_value() &&
        candidate.returnExpr->kind == Expr::Kind::BoolLiteral) {
      callable.returnExprIsBoolLiteral = true;
      callable.returnBoolValue = candidate.returnExpr->boolValue;
    }
    bool paramsOk = true;
    for (const Expr &param : candidate.parameters) {
      semantics::BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string ignoredError;
      if (!semantics::parseBindingInfo(param,
                                       candidate.namespacePrefix,
                                       structNames,
                                       importAliases,
                                       binding,
                                       restrictType,
                                       ignoredError,
                                       nullptr,
                                       nullptr,
                                       /*allowCapabilityArg=*/true)) {
        paramsOk = false;
        break;
      }
      callable.parameterTypes.push_back(compileTimeIfBindingTypeText(binding));
    }
    if (!callable.returnType.empty() && paramsOk) {
      facts.callables.push_back(std::move(callable));
    }

    if (structNames.count(candidate.fullPath) == 0) {
      continue;
    }
    for (const Transform &transform : candidate.transforms) {
      std::vector<std::string> traitNames;
      if (transform.name == "collection_type") {
        traitNames.push_back("Collection");
      } else if (transform.name == "key_value_type") {
        traitNames.push_back("Collection");
        traitNames.push_back("KeyValue");
      }
      for (const std::string &traitName : traitNames) {
        semantics::RequirementPredicateDefinitionContext::StructTraitFact trait;
        trait.structPath = candidate.fullPath;
        trait.traitName = traitName;
        trait.isPrivate = isTransformNamed(candidate.transforms, "private");
        facts.structTraits.push_back(std::move(trait));
      }
    }
    for (const Expr &stmt : candidate.statements) {
      if (!stmt.isBinding || isTransformNamed(stmt.transforms, "static") ||
          semantics::isCompileTimeTypeBinding(stmt)) {
        continue;
      }
      semantics::BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string ignoredError;
      if (!semantics::parseBindingInfo(stmt,
                                       candidate.namespacePrefix,
                                       structNames,
                                       importAliases,
                                       binding,
                                       restrictType,
                                       ignoredError)) {
        continue;
      }
      semantics::RequirementPredicateDefinitionContext::StructFieldFact field;
      field.structPath = candidate.fullPath;
      field.fieldName = stmt.name;
      field.typeText = compileTimeIfBindingTypeText(binding);
      field.isPrivate = isTransformNamed(stmt.transforms, "private");
      facts.structFields.push_back(std::move(field));
    }
  }
  return facts;
}

semantics::RequirementPredicateDefinitionContext
makeCompileTimeIfRequirementContext(
    const Definition &definition,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_set<std::string> &sumNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    const CompileTimeIfCandidateFacts &candidateFacts) {
  semantics::RequirementPredicateDefinitionContext context;
  context.definitionPath = definition.fullPath;
  context.namespacePrefix = definition.namespacePrefix;
  context.templateArgs = definition.templateArgs;
  context.structNames = structNames;
  context.sumNames = sumNames;
  context.importAliases = importAliases;

  for (const Expr &param : definition.parameters) {
    semantics::BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string ignoredError;
    if (semantics::parseBindingInfo(param,
                                    definition.namespacePrefix,
                                    structNames,
                                    importAliases,
                                    binding,
                                    restrictType,
                                    ignoredError,
                                    nullptr,
                                    nullptr,
                                    /*allowCapabilityArg=*/true)) {
      context.params.push_back(
          semantics::ParameterInfo{param.name, binding, nullptr});
    }
  }

  context.callables = candidateFacts.callables;
  context.structFields = candidateFacts.structFields;
  context.structTraits = candidateFacts.structTraits;

  return context;
}

// TODO-5238: `makeCompileTimeIfRequirementContext` deep-copies several
// program-wide structures into a fresh `RequirementPredicateDefinitionContext`
// (structNames/sumNames/importAliases sets/maps plus the candidateFacts
// callables/structFields/structTraits vectors) - real, non-trivial work
// (unordered_set/map node allocations, string/vector copies). Profiling with
// `valgrind --tool=dhat` post-TODO-5236 found this construction happening
// *eagerly*, once per `Definition` in `rewriteCompileTimeIfBranches`'
// definitions loop, even though `context` is only ever actually read inside
// `evaluateCompileTimeIfDecision` - which only runs for a `ct_if` envelope,
// and the overwhelming majority of definitions (all of them, for programs
// with no `ct_if` usage anywhere, e.g. the entire `/std/collections/*`
// module) contain zero. `LazyCompileTimeIfContext` defers the actual build to
// first use: it stores only pointers to the program-wide inputs (no copies)
// until `get()` is called, which happens exactly at the two call sites that
// dereference `context` today (`evaluateCompileTimeIfDecision`'s two call
// sites) - same build, same result, just skipped entirely for definitions
// whose statement/expression tree never contains a `ct_if`. The
// already-resolved-branch case (`rewriteCompileTimeIfStatement`'s
// `updatedContext`) uses the second constructor to wrap an already-built
// context with no extra work, matching today's behavior exactly for that
// path (which already pays this cost once per resolved `ct_if`, not once per
// AST node - unaffected by this change).
class LazyCompileTimeIfContext {
public:
  LazyCompileTimeIfContext(
      const Definition &definition,
      const std::unordered_set<std::string> &structNames,
      const std::unordered_set<std::string> &sumNames,
      const std::unordered_map<std::string, std::string> &importAliases,
      const CompileTimeIfCandidateFacts &candidateFacts)
      : definition_(&definition),
        structNames_(&structNames),
        sumNames_(&sumNames),
        importAliases_(&importAliases),
        candidateFacts_(&candidateFacts) {}

  explicit LazyCompileTimeIfContext(
      semantics::RequirementPredicateDefinitionContext built)
      : built_(std::move(built)) {}

  const semantics::RequirementPredicateDefinitionContext &get() const {
    if (!built_.has_value()) {
      built_ = makeCompileTimeIfRequirementContext(
          *definition_, *structNames_, *sumNames_, *importAliases_,
          *candidateFacts_);
    }
    return *built_;
  }

private:
  const Definition *definition_ = nullptr;
  const std::unordered_set<std::string> *structNames_ = nullptr;
  const std::unordered_set<std::string> *sumNames_ = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases_ = nullptr;
  const CompileTimeIfCandidateFacts *candidateFacts_ = nullptr;
  mutable std::optional<semantics::RequirementPredicateDefinitionContext> built_;
};

bool isCompileTimeIfEnvelope(const Expr &expr, std::string_view expectedName) {
  return expr.kind == Expr::Kind::Call && expr.name == expectedName &&
         expr.hasBodyArguments;
}

enum class CompileTimeIfDecision {
  SelectedThen,
  SelectedElse,
  Deferred,
};

std::string compileTimeIfDecisionText(CompileTimeIfDecision decision) {
  switch (decision) {
  case CompileTimeIfDecision::SelectedThen:
    return "then";
  case CompileTimeIfDecision::SelectedElse:
    return "else";
  case CompileTimeIfDecision::Deferred:
    break;
  }
  return "deferred";
}

std::string formatCompileTimeIfConditionShapeDiagnostic(
    const Expr &stmt,
    const std::string &definitionPath) {
  std::ostringstream out;
  out << "invalid ct_if condition on " << definitionPath << '\n';
  out << "category: invalid compile-time flow predicate\n";
  out << "ct_if site: " << definitionPath << " at " << stmt.sourceLine
      << ':' << stmt.sourceColumn << '\n';
  out << "predicate source: <not a supported compile-time predicate call>\n";
  out << "result: ct_if condition must be a compile-time predicate call\n";
  out << "hint: make the ct_if predicate evaluable from compile-time facts, "
         "or move non-flow constraints into require(...).";
  return out.str();
}

std::string formatCompileTimeIfPredicateDiagnostic(
    const Expr &stmt,
    const std::string &definitionPath,
    const semantics::RequirementPredicateFactDraft &fact) {
  std::ostringstream out;
  out << "invalid ct_if condition on " << definitionPath << '\n';
  out << "category: invalid compile-time flow predicate\n";
  out << "ct_if site: " << definitionPath << " at " << stmt.sourceLine
      << ':' << stmt.sourceColumn << '\n';
  out << "predicate source: "
      << (fact.sourceText.empty() ? std::string("<unknown>")
                                  : fact.sourceText)
      << '\n';
  out << "predicate path: "
      << (fact.predicateName.empty() ? std::string("<unknown>")
                                     : fact.predicateName)
      << '\n';
  out << "compile-time facts:";
  if (fact.operands.empty()) {
    out << " none\n";
  } else {
    out << '\n';
    for (const semantics::RequirementPredicateOperandFact &operand :
         fact.operands) {
      out << "- " << operand.stableHandle << " kind=" << operand.kind
          << " text=" << operand.text << " at " << operand.sourceLine
          << ':' << operand.sourceColumn << '\n';
    }
  }
  out << "result: " << fact.evaluationDiagnostic << '\n';
  out << "hint: make the ct_if predicate evaluable from compile-time facts, "
         "or move non-flow constraints into require(...).";
  return out.str();
}

bool evaluateCompileTimeIfDecision(
    const Expr &stmt,
    const LazyCompileTimeIfContext &lazyContext,
    const std::string &definitionPath,
    bool allowDeferred,
    CompileTimeIfDecision &decision,
    std::string &error) {
  if (stmt.args.size() != 3 ||
      !isCompileTimeIfEnvelope(stmt.args[1], "then") ||
      !isCompileTimeIfEnvelope(stmt.args[2], "else")) {
    error = "ct_if requires condition, then, else on " + definitionPath;
    return false;
  }
  std::string conditionText;
  if (!serializeCompileTimeIfPredicateExpr(stmt.args[0], conditionText)) {
    error = formatCompileTimeIfConditionShapeDiagnostic(stmt, definitionPath);
    return false;
  }
  // This is the only point in the whole tree walk that actually needs the
  // (expensive to build) context - see LazyCompileTimeIfContext's comment.
  const semantics::RequirementPredicateDefinitionContext &context =
      lazyContext.get();
  semantics::RequirementPredicateFactDraft fact =
      semantics::buildRequirementPredicateFactDraft(conditionText,
                                                    stmt.sourceLine,
                                                    stmt.sourceColumn,
                                                    context);
  if (fact.evaluationOutcome != "satisfied" &&
      fact.evaluationOutcome != "unsatisfied") {
    const bool templatedUnknownTypeFact =
        !context.templateArgs.empty() &&
        fact.evaluationDiagnostic.find("unknown type fact") !=
            std::string::npos;
    if (allowDeferred &&
        (fact.evaluationDiagnostic.find("deferred") != std::string::npos ||
         templatedUnknownTypeFact)) {
      decision = CompileTimeIfDecision::Deferred;
      return true;
    }
    error = formatCompileTimeIfPredicateDiagnostic(stmt, definitionPath, fact);
    return false;
  }
  decision = fact.evaluationOutcome == "satisfied"
                 ? CompileTimeIfDecision::SelectedThen
                 : CompileTimeIfDecision::SelectedElse;
  return true;
}

std::string rewriteCompileTimeIfBranchTypeText(
    const std::string &typeText,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  auto exactIt = branchTypes.find(typeText);
  if (exactIt != branchTypes.end()) {
    return exactIt->second;
  }
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(typeText, base, argText) ||
      base.empty()) {
    return typeText;
  }
  std::vector<std::string> args;
  if (!semantics::splitTopLevelTemplateArgs(argText, args)) {
    return typeText;
  }
  base = rewriteCompileTimeIfBranchTypeText(base, branchTypes);
  for (std::string &arg : args) {
    arg = rewriteCompileTimeIfBranchTypeText(arg, branchTypes);
  }
  std::ostringstream rebuilt;
  rebuilt << base << "<";
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      rebuilt << ", ";
    }
    rebuilt << args[i];
  }
  rebuilt << ">";
  return rebuilt.str();
}

void rewriteCompileTimeIfBranchTemplateArgs(
    std::vector<std::string> &templateArgs,
    std::vector<TemplateArgument> &templateArgDetails,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  for (std::string &arg : templateArgs) {
    arg = rewriteCompileTimeIfBranchTypeText(arg, branchTypes);
  }
  for (TemplateArgument &detail : templateArgDetails) {
    if (detail.kind == TemplateArgumentKind::Type) {
      detail.text = rewriteCompileTimeIfBranchTypeText(detail.text,
                                                       branchTypes);
    }
  }
}

void rewriteCompileTimeIfBranchTypeReferences(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &branchTypes);

void rewriteCompileTimeIfBranchTransformReferences(
    Transform &transform,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  transform.name =
      rewriteCompileTimeIfBranchTypeText(transform.name, branchTypes);
  rewriteCompileTimeIfBranchTemplateArgs(transform.templateArgs,
                                         transform.templateArgDetails,
                                         branchTypes);
}

void rewriteCompileTimeIfBranchTypeReferences(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  expr.name = rewriteCompileTimeIfBranchTypeText(expr.name, branchTypes);
  rewriteCompileTimeIfBranchTemplateArgs(expr.templateArgs,
                                         expr.templateArgDetails,
                                         branchTypes);
  for (Transform &transform : expr.transforms) {
    rewriteCompileTimeIfBranchTransformReferences(transform, branchTypes);
  }
  for (Expr &arg : expr.args) {
    rewriteCompileTimeIfBranchTypeReferences(arg, branchTypes);
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    rewriteCompileTimeIfBranchTypeReferences(bodyArg, branchTypes);
  }
}

bool isCompileTimeIfBranchGeneratedStructExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || !expr.isBinding ||
      expr.name.empty() || expr.args.size() != 1) {
    return false;
  }
  if (!std::any_of(expr.transforms.begin(),
                   expr.transforms.end(),
                   [](const Transform &transform) {
                     return transform.name == "struct";
                   })) {
    return false;
  }
  const Expr &initializer = expr.args.front();
  return initializer.kind == Expr::Kind::Call &&
         initializer.name == "struct" &&
         initializer.isBraceConstructor;
}

void normalizeCompileTimeIfGeneratedStructField(Expr &field) {
  if (field.isBinding || field.transforms.empty()) {
    return;
  }
  field.isBinding = true;
  if (field.args.size() != 1) {
    return;
  }
  Expr &initializer = field.args.front();
  if (initializer.kind != Expr::Kind::Call || initializer.name != "block" ||
      !initializer.hasBodyArguments || initializer.bodyArguments.size() != 1) {
    return;
  }
  field.args.front() = std::move(initializer.bodyArguments.front());
  field.argNames.assign(1, std::nullopt);
}

std::vector<Expr> takeCompileTimeIfGeneratedStructFields(Expr &initializer) {
  std::vector<Expr> fields = std::move(initializer.args);
  for (std::size_t i = 0; i < fields.size() && i < initializer.argNames.size();
       ++i) {
    if (!initializer.argNames[i].has_value() || !fields[i].transforms.empty()) {
      continue;
    }
    Transform typeTransform;
    typeTransform.name = *initializer.argNames[i];
    typeTransform.sourceLine = fields[i].sourceLine;
    typeTransform.sourceColumn = fields[i].sourceColumn;
    fields[i].transforms.push_back(std::move(typeTransform));
  }
  for (Expr &field : fields) {
    normalizeCompileTimeIfGeneratedStructField(field);
  }
  return fields;
}

std::string makeCompileTimeIfGeneratedStructName(
    const Expr &expr,
    CompileTimeIfDecision decision) {
  return expr.name + "__ct_if_" + compileTimeIfDecisionText(decision) +
         "_" + std::to_string(expr.sourceLine) +
         "_" + std::to_string(expr.sourceColumn);
}

bool materializeCompileTimeIfGeneratedStructs(
    std::vector<Expr> &selected,
    const std::string &definitionPath,
    CompileTimeIfDecision decision,
    std::unordered_set<std::string> &structNames,
    std::vector<Definition> &generatedDefinitions,
    std::string &error) {
  std::unordered_map<std::string, std::string> branchTypes;
  std::vector<Expr> retained;
  retained.reserve(selected.size());
  const std::size_t generatedStart = generatedDefinitions.size();

  for (Expr &stmt : selected) {
    if (!isCompileTimeIfBranchGeneratedStructExpr(stmt)) {
      retained.push_back(std::move(stmt));
      continue;
    }
    if (branchTypes.count(stmt.name) > 0) {
      error = "duplicate branch-local generated struct: " + stmt.name +
              " on " + definitionPath;
      return false;
    }
    Definition generated;
    generated.name = makeCompileTimeIfGeneratedStructName(stmt, decision);
    generated.namespacePrefix = definitionPath;
    generated.fullPath = definitionPath + "/" + generated.name;
    generated.sourceLine = stmt.sourceLine;
    generated.sourceColumn = stmt.sourceColumn;
    generated.transforms = stmt.transforms;
    generated.isNested = true;
    generated.statements =
        takeCompileTimeIfGeneratedStructFields(stmt.args.front());
    if (structNames.count(generated.fullPath) > 0) {
      error = "duplicate branch-local generated struct path: " +
              generated.fullPath;
      return false;
    }
    branchTypes.emplace(stmt.name, generated.fullPath);
    structNames.insert(generated.fullPath);
    generatedDefinitions.push_back(std::move(generated));
  }

  if (branchTypes.empty()) {
    selected = std::move(retained);
    return true;
  }

  for (Expr &stmt : retained) {
    if (stmt.isBinding && branchTypes.count(stmt.name) > 0) {
      error = "branch-local generated struct name shadows local binding: " +
              stmt.name;
      return false;
    }
    rewriteCompileTimeIfBranchTypeReferences(stmt, branchTypes);
  }
  for (std::size_t i = generatedStart; i < generatedDefinitions.size(); ++i) {
    for (Expr &field : generatedDefinitions[i].statements) {
      rewriteCompileTimeIfBranchTypeReferences(field, branchTypes);
    }
  }
  selected = std::move(retained);
  return true;
}

bool rewriteCompileTimeIfStatements(std::vector<Expr> &statements,
                                    const LazyCompileTimeIfContext &context,
                                    const std::string &definitionPath,
                                    std::unordered_set<std::string> &structNames,
                                    std::vector<Definition> &generatedDefinitions,
                                    bool allowDeferred,
                                    std::string &error);

bool rewriteCompileTimeIfExpression(
    Expr &expr,
    const LazyCompileTimeIfContext &context,
    const std::string &definitionPath,
    bool allowDeferred,
    std::string &error);

bool rewriteCompileTimeIfStatement(Expr &stmt,
                                   std::vector<Expr> &out,
                                   const LazyCompileTimeIfContext &context,
                                   const std::string &definitionPath,
                                   std::unordered_set<std::string> &structNames,
                                   std::vector<Definition> &generatedDefinitions,
                                   bool allowDeferred,
                                   std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.name != "ct_if") {
    if (!rewriteCompileTimeIfExpression(
            stmt, context, definitionPath, allowDeferred, error)) {
      return false;
    }
    out.push_back(std::move(stmt));
    return true;
  }
  CompileTimeIfDecision decision = CompileTimeIfDecision::Deferred;
  if (!evaluateCompileTimeIfDecision(
          stmt, context, definitionPath, allowDeferred, decision, error)) {
    return false;
  }
  if (decision == CompileTimeIfDecision::Deferred) {
    out.push_back(std::move(stmt));
    return true;
  }
  std::vector<Expr> selected =
      decision == CompileTimeIfDecision::SelectedThen
          ? std::move(stmt.args[1].bodyArguments)
          : std::move(stmt.args[2].bodyArguments);
  if (!materializeCompileTimeIfGeneratedStructs(selected,
                                                definitionPath,
                                                decision,
                                                structNames,
                                                generatedDefinitions,
                                                error)) {
    return false;
  }
  // TODO-5236: only copy `context` here, at the one call site that actually
  // needs a mutated (updated structNames) copy for the selected branch's
  // nested statements - not on every statement in every definition. See
  // this function's and rewriteCompileTimeIfStatements'/
  // rewriteCompileTimeIfBranches' comments for the profiling and reasoning.
  // TODO-5238: `context.get()` here is guaranteed already-built (this branch
  // only runs once `evaluateCompileTimeIfDecision` above already forced the
  // build), so this is exactly the same one copy as before - wrapped in a
  // LazyCompileTimeIfContext so the nested walk keeps the same "build only if
  // a further nested ct_if is found" laziness (there usually isn't one).
  semantics::RequirementPredicateDefinitionContext updatedContext =
      context.get();
  updatedContext.structNames = structNames;
  LazyCompileTimeIfContext updatedLazyContext(std::move(updatedContext));
  if (!rewriteCompileTimeIfStatements(
          selected,
          updatedLazyContext,
          definitionPath,
          structNames,
          generatedDefinitions,
          allowDeferred,
          error)) {
    return false;
  }
  out.insert(out.end(),
             std::make_move_iterator(selected.begin()),
             std::make_move_iterator(selected.end()));
  return true;
}

bool rewriteCompileTimeIfExpression(
    Expr &expr,
    const LazyCompileTimeIfContext &context,
    const std::string &definitionPath,
    bool allowDeferred,
    std::string &error) {
  if (expr.kind == Expr::Kind::Call && expr.name == "ct_if") {
    CompileTimeIfDecision decision = CompileTimeIfDecision::Deferred;
    if (!evaluateCompileTimeIfDecision(
            expr, context, definitionPath, allowDeferred, decision, error)) {
      return false;
    }
    if (decision == CompileTimeIfDecision::Deferred) {
      return true;
    }
    std::vector<Expr> &selected =
        decision == CompileTimeIfDecision::SelectedThen
            ? expr.args[1].bodyArguments
            : expr.args[2].bodyArguments;
    if (selected.size() != 1 || selected.front().isBinding) {
      error = "ct_if expression requires exactly one selected branch value on " +
              definitionPath;
      return false;
    }
    Expr selectedExpr = std::move(selected.front());
    if (!rewriteCompileTimeIfExpression(
            selectedExpr, context, definitionPath, allowDeferred, error)) {
      return false;
    }
    expr = std::move(selectedExpr);
    return true;
  }

  for (Expr &arg : expr.args) {
    if (!rewriteCompileTimeIfExpression(
            arg, context, definitionPath, allowDeferred, error)) {
      return false;
    }
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    if (!rewriteCompileTimeIfExpression(
            bodyArg, context, definitionPath, allowDeferred, error)) {
      return false;
    }
  }
  return true;
}

bool rewriteCompileTimeIfStatements(std::vector<Expr> &statements,
                                    const LazyCompileTimeIfContext &context,
                                    const std::string &definitionPath,
                                    std::unordered_set<std::string> &structNames,
                                    std::vector<Definition> &generatedDefinitions,
                                    bool allowDeferred,
                                    std::string &error) {
  std::vector<Expr> rewritten;
  rewritten.reserve(statements.size());
  for (Expr &stmt : statements) {
    if (!rewriteCompileTimeIfStatement(
            stmt,
            rewritten,
            context,
            definitionPath,
            structNames,
            generatedDefinitions,
            allowDeferred,
            error)) {
      return false;
    }
  }
  statements = std::move(rewritten);
  return true;
}

bool rewriteCompileTimeIfBranches(Program &program,
                                  bool allowDeferred,
                                  std::string &error) {
  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> sumNames;
  std::unordered_map<std::string, std::string> importAliases;
  for (const std::string &path : program.imports) {
    addCompileTimeIfImportAlias(importAliases, path);
  }
  for (const std::string &path : program.sourceImports) {
    addCompileTimeIfImportAlias(importAliases, path);
  }
  for (const Definition &definition : program.definitions) {
    if (isTransformNamed(definition.transforms, "sum")) {
      sumNames.insert(definition.fullPath);
    } else if (definition.returnExpr.has_value()) {
      continue;
    } else {
      structNames.insert(definition.fullPath);
    }
  }

  const CompileTimeIfCandidateFacts candidateFacts =
      buildCompileTimeIfCandidateFacts(program, structNames, importAliases);

  std::vector<Definition> generatedDefinitions;
  for (Definition &definition : program.definitions) {
    // TODO-5238: this used to eagerly call `makeCompileTimeIfRequirementContext`
    // (a deep copy of structNames/sumNames/importAliases/candidateFacts) for
    // *every* definition, regardless of whether that definition's body
    // contains a `ct_if` anywhere. `LazyCompileTimeIfContext` defers that
    // build to first actual use - see its comment above
    // `makeCompileTimeIfRequirementContext` for the profiling and reasoning.
    LazyCompileTimeIfContext context(definition,
                                     structNames,
                                     sumNames,
                                     importAliases,
                                     candidateFacts);
    if (!rewriteCompileTimeIfStatements(
            definition.statements,
            context,
            definition.fullPath,
            structNames,
            generatedDefinitions,
            allowDeferred,
            error)) {
      return false;
    }
    if (definition.returnExpr.has_value() &&
        !rewriteCompileTimeIfExpression(*definition.returnExpr,
                                        context,
                                        definition.fullPath,
                                        allowDeferred,
                                        error)) {
      return false;
    }
  }
  program.definitions.insert(program.definitions.end(),
                             std::make_move_iterator(
                                 generatedDefinitions.begin()),
                             std::make_move_iterator(
                                 generatedDefinitions.end()));
  return true;
}

} // namespace primec
