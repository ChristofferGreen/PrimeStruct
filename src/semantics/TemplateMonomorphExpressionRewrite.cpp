#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "primec/frontend/StringLiteral.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphCallableRewrites.h"
#include "TemplateMonomorphTextViewConversions.h"
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

namespace primec {


bool isCompileTimeTypeofPredicateArg(const std::string &arg) {
  const std::string trimmed = trimWhitespace(arg);
  return trimmed.rfind("typeof<", 0) == 0 && trimmed.size() > 8 &&
         trimmed.back() == '>';
}

bool rewriteCompileTimePredicateExpr(Expr &expr,
                                     const SubstMap &mapping,
                                     const std::unordered_set<std::string> &allowedParams,
                                     const std::string &namespacePrefix,
                                     Context &ctx,
                                     std::string &error) {
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return false;
  }
  for (std::string &templateArg : expr.templateArgs) {
    if (isCompileTimeTypeofPredicateArg(templateArg)) {
      continue;
    }
    ResolvedType resolved =
        resolveTypeString(templateArg, mapping, allowedParams, namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    templateArg = std::move(resolved.text);
  }
  for (Expr &arg : expr.args) {
    if (!rewriteCompileTimePredicateExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error)) {
      return false;
    }
  }
  return true;
}

namespace {

// TODO-4751: the public key/value wrapper struct lives at this path. Calls to
// the canonical key/value helper family (`/std/collections/map/<helper>` and
// its `<helper>_ref` borrowed form) whose receiver is that wrapper (or a
// Reference/Pointer to it) are routed to the wrapper's own methods instead of
// the MapValue-typed free helpers. This deliberately avoids adding wrapper
// overloads to the canonical helper family.
constexpr std::string_view KeyValueWrapperStructPath = collection_helpers::kCanonicalMapMapType;
constexpr std::string_view KeyValueHelperRoot = collection_helpers::kCanonicalMapPrefix;

bool isKeyValueWrapperMethodHelperName(std::string_view name) {
  return name == "count" || name == "contains" || name == "tryAt" ||
         name == "at" || name == "at_unsafe" || name == "insert";
}

bool isKeyValueWrapperStructTypeText(std::string typeText,
                                     const std::string &namespacePrefix,
                                     Context &ctx) {
  typeText = semantics::normalizeBindingTypeName(typeText);
  std::string base = typeText;
  std::string argText;
  if (semantics::splitTemplateTypeName(typeText, base, argText)) {
    base = semantics::normalizeBindingTypeName(base);
  }
  if (base.empty()) {
    return false;
  }
  const std::string wrapperPath(KeyValueWrapperStructPath);
  if (base.front() != '/') {
    base = resolveNameToPath(base,
                             namespacePrefix,
                             scopedImportAliasesForNamespace(namespacePrefix, ctx),
                             ctx.sourceDefs);
  }
  return base == wrapperPath || base.rfind(wrapperPath + "__", 0) == 0;
}

} // namespace

bool rewriteKeyValueWrapperHelperCallToMethod(Expr &expr,
                                              const std::string &namespacePrefix,
                                              Context &ctx,
                                              const LocalTypeMap &locals,
                                              const std::vector<semantics::ParameterInfo> &params,
                                              bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding ||
      expr.isFieldAccess || expr.args.empty() || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || hasNamedCallArguments(expr)) {
    return false;
  }
  std::string resolved =
      stripCollectionConstructorSuffixes(resolveCalleePath(expr, namespacePrefix, ctx));
  if (resolved.rfind(KeyValueHelperRoot, 0) != 0) {
    return false;
  }
  std::string helperName = resolved.substr(KeyValueHelperRoot.size());
  bool borrowedHelper = false;
  constexpr std::string_view RefSuffix = "_ref";
  if (helperName.size() > RefSuffix.size() &&
      helperName.compare(helperName.size() - RefSuffix.size(), RefSuffix.size(), RefSuffix) == 0) {
    helperName.erase(helperName.size() - RefSuffix.size());
    borrowedHelper = true;
  }
  if (!isKeyValueWrapperMethodHelperName(helperName)) {
    return false;
  }
  semantics::BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(
          expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo)) {
    return false;
  }
  const std::string receiverBase = semantics::normalizeBindingTypeName(receiverInfo.typeName);
  const bool receiverIsBorrowed =
      receiverBase == "Reference" || receiverBase == "Pointer";
  if (borrowedHelper && !receiverIsBorrowed) {
    return false;
  }
  const std::string receiverTypeText =
      receiverIsBorrowed ? receiverInfo.typeTemplateArg : bindingTypeToString(receiverInfo);
  if (!isKeyValueWrapperStructTypeText(receiverTypeText, namespacePrefix, ctx)) {
    return false;
  }
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  expr.name = helperName;
  expr.isMethodCall = true;
  expr.templateArgs.clear();
  expr.templateArgDetails.clear();
  return true;
}

namespace {

bool decodedStringLiteral(const Expr &expr, std::string &decoded, StringEncoding &encoding) {
  if (expr.kind != Expr::Kind::StringLiteral) {
    return false;
  }
  ParsedStringLiteral parsed;
  std::string parseError;
  if (!parseStringLiteralToken(expr.stringValue, parsed, parseError)) {
    return false;
  }
  decoded = std::move(parsed.decoded);
  encoding = parsed.encoding;
  return true;
}

// `textOf("literal")` (the StringView of a literal): the literal's text.
bool decodedTextOfLiteral(const Expr &expr, std::string &decoded) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || expr.args.size() != 1 ||
      !expr.templateArgs.empty() || hasNamedCallArguments(expr)) {
    return false;
  }
  const std::string &callee = expr.name;
  if (callee != "textOf" && callee != "/std/text/view/textOf") {
    return false;
  }
  StringEncoding encoding = StringEncoding::Utf8;
  return decodedStringLiteral(expr.args.front(), decoded, encoding);
}

std::string stringLiteralToken(const std::string &decoded, StringEncoding encoding) {
  std::string token = "\"";
  for (const char c : decoded) {
    switch (c) {
    case '\n': token += "\\n"; break;
    case '\r': token += "\\r"; break;
    case '\t': token += "\\t"; break;
    case '\\': token += "\\\\"; break;
    case '"': token += "\\\""; break;
    case '\0': token += "\\0"; break;
    default: token.push_back(c); break;
    }
  }
  token += "\"";
  token += encoding == StringEncoding::Ascii ? "ascii" : "utf8";
  return token;
}

void makeBoolLiteral(Expr &expr, bool value) {
  expr.kind = Expr::Kind::BoolLiteral;
  expr.boolValue = value;
  expr.args.clear();
  expr.argNames.clear();
  expr.templateArgs.clear();
  expr.isMethodCall = false;
  expr.name.clear();
}

void makeIntLiteral(Expr &expr, uint64_t value, int width, bool isUnsigned) {
  expr.kind = Expr::Kind::Literal;
  expr.literalValue = value;
  expr.intWidth = width;
  expr.isUnsigned = isUnsigned;
  expr.args.clear();
  expr.argNames.clear();
  expr.templateArgs.clear();
  expr.isMethodCall = false;
  expr.name.clear();
}

} // namespace

// Operations whose operands are all literals are folded where the AST is rewritten, so no run-time text work remains:
// `"a" + "b"`, `"a" == "b"`, `at("text", 1)` and, on views of literals, `textOf("a") == textOf("b")`, `.count()`,
// `.startsWith`, `.endsWith`, `.contains`, `.equals` and `textOf("text")[1]`. A literal index out of range is an error.
bool foldLiteralTextCall(Expr &expr, std::string &error) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isFieldAccess || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || hasNamedCallArguments(expr)) {
    return true;
  }
  for (Expr &arg : expr.args) {
    if (!foldLiteralTextCall(arg, error)) {
      return false;
    }
  }
  const std::string &name = expr.name;
  if (expr.args.empty() || expr.args.size() > 2 || !expr.templateArgs.empty()) {
    return true;
  }
  std::string left;
  std::string right;
  StringEncoding leftEncoding = StringEncoding::Utf8;
  StringEncoding rightEncoding = StringEncoding::Utf8;
  const bool plain = !expr.isMethodCall;
  if (expr.args.size() == 2 && plain) {
    const bool bothLiterals =
        decodedStringLiteral(expr.args[0], left, leftEncoding) && decodedStringLiteral(expr.args[1], right, rightEncoding);
    if (bothLiterals) {
      if (name == "plus") {
        const StringEncoding joined =
            leftEncoding == StringEncoding::Ascii && rightEncoding == StringEncoding::Ascii ? StringEncoding::Ascii
                                                                                              : StringEncoding::Utf8;
        const std::string sourceNameKept = expr.sourceName;
        const int line = expr.sourceLine;
        const int column = expr.sourceColumn;
        expr = Expr();
        expr.kind = Expr::Kind::StringLiteral;
        expr.stringValue = stringLiteralToken(left + right, joined);
        expr.sourceLine = line;
        expr.sourceColumn = column;
        return true;
      }
      if (name == "equal" || name == "not_equal") {
        makeBoolLiteral(expr, (left == right) == (name == "equal"));
        return true;
      }
      return true;
    }
    int64_t index = 0;
    if (name == "at" && decodedStringLiteral(expr.args[0], left, leftEncoding) &&
        expr.args[1].kind == Expr::Kind::Literal && !expr.args[1].isUnsigned) {
      index = static_cast<int64_t>(static_cast<int32_t>(expr.args[1].literalValue));
      if (index < 0 || static_cast<size_t>(index) >= left.size()) {
        error = "string index " + std::to_string(index) + " is out of bounds for a literal of " +
                std::to_string(left.size()) + " bytes";
        return false;
      }
      makeIntLiteral(expr, static_cast<unsigned char>(left[static_cast<size_t>(index)]), 32, false);
      return true;
    }
  }
  // Views of literals: textOf("a") compared with textOf("b"), indexed, counted or searched.
  const bool methodForm = expr.isMethodCall;
  if (expr.args.size() == 2 && decodedTextOfLiteral(expr.args[0], left)) {
    if (decodedTextOfLiteral(expr.args[1], right)) {
      const int order = left < right ? -1 : (left == right ? 0 : 1); // std::string compares unsigned bytes
      bool value = false;
      bool known = true;
      if (name == "equal" || name == "equals") {
        value = order == 0;
      } else if (name == "not_equal") {
        value = order != 0;
      } else if (name == "less_than") {
        value = order < 0;
      } else if (name == "less_equal") {
        value = order <= 0;
      } else if (name == "greater_than") {
        value = order > 0;
      } else if (name == "greater_equal") {
        value = order >= 0;
      } else if (name == "startsWith") {
        value = left.rfind(right, 0) == 0;
      } else if (name == "endsWith") {
        value = left.size() >= right.size() && left.compare(left.size() - right.size(), right.size(), right) == 0;
      } else if (name == "contains") {
        value = left.find(right) != std::string::npos;
      } else {
        known = false;
      }
      if (known && (plain || methodForm)) {
        makeBoolLiteral(expr, value);
        return true;
      }
    }
    if (name == "at" && plain && expr.args[1].kind == Expr::Kind::Literal && !expr.args[1].isUnsigned) {
      const int64_t index = static_cast<int64_t>(static_cast<int32_t>(expr.args[1].literalValue));
      if (index < 0 || static_cast<size_t>(index) >= left.size()) {
        error = "string index " + std::to_string(index) + " is out of bounds for a literal of " +
                std::to_string(left.size()) + " bytes";
        return false;
      }
      makeIntLiteral(expr, static_cast<unsigned char>(left[static_cast<size_t>(index)]), 8, true);
      return true;
    }
  }
  if (expr.args.size() == 1 && name == "count" && decodedTextOfLiteral(expr.args[0], left)) {
    makeIntLiteral(expr, left.size(), 32, false);
    return true;
  }
  return true;
}

// `values.view()`, `values.slice(start, end)` and `values.sliceMut(start, end)` on a `vector<T>` are the calls
// `/std/collections/vector/view<T>(values)` and friends (the vector surface resolves its methods by name, so the three
// slice makers are spelled out here).
bool rewriteVectorViewMethod(Expr &expr,
                             Context &ctx,
                             const LocalTypeMap &locals,
                             const std::vector<semantics::ParameterInfo> &params,
                             bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.isBinding || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || hasNamedCallArguments(expr) || !expr.templateArgs.empty()) {
    return false;
  }
  const bool viewForm = expr.name == "view" && expr.args.size() == 1;
  const bool sliceForm = (expr.name == "slice" || expr.name == "sliceMut") && expr.args.size() == 3;
  if (!viewForm && !sliceForm) {
    return false;
  }
  const Expr &receiver = expr.args.front();
  if (receiver.kind != Expr::Kind::Name) {
    return false;
  }
  semantics::BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(receiver, params, locals, allowMathBare, ctx, receiverInfo)) {
    return false;
  }
  const std::string base = semantics::normalizeBindingTypeName(receiverInfo.typeName);
  const bool isVector = base == "vector" || base == "Vector" || base == collection_helpers::kCanonicalVectorVectorType;
  if (!isVector && !receiverInfo.typeTemplateArg.empty()) {
    // An `array<T>` (or a reference to one): the view helpers take the array by reference.
    std::string arrayElement;
    bool wrapInLocation = false;
    if (base == "array") {
      arrayElement = receiverInfo.typeTemplateArg;
      wrapInLocation = true;
    } else if (base == "Reference") {
      std::string wrapper;
      std::string wrapped;
      if (semantics::splitTemplateTypeName(semantics::normalizeBindingTypeName(receiverInfo.typeTemplateArg), wrapper,
                                           wrapped) &&
          semantics::normalizeBindingTypeName(wrapper) == "array") {
        arrayElement = wrapped;
      }
    }
    if (!arrayElement.empty()) {
      if (expr.sourceName.empty()) {
        expr.sourceName = expr.name;
      }
      const std::string helper = expr.name == "view" ? "arrayView" : (expr.name == "slice" ? "arraySlice" : "arraySliceMut");
      expr.name = std::string(collection_helpers::kCanonicalSlicePrefix) + helper;
      expr.isMethodCall = false;
      expr.templateArgs.push_back(arrayElement);
      if (wrapInLocation) {
        Expr reference;
        reference.kind = Expr::Kind::Call;
        reference.name = "location";
        reference.namespacePrefix = expr.args.front().namespacePrefix;
        reference.sourceLine = expr.args.front().sourceLine;
        reference.sourceColumn = expr.args.front().sourceColumn;
        reference.args.push_back(expr.args.front());
        expr.args.front() = std::move(reference);
      }
      return true;
    }
  }
  if (!isVector || receiverInfo.typeTemplateArg.empty()) {
    return false;
  }
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  expr.name = std::string(collection_helpers::kCanonicalVectorPrefix) + expr.name;
  expr.isMethodCall = false;
  expr.templateArgs.push_back(receiverInfo.typeTemplateArg);
  return true;
}

// `view[i] = value` on a struct with an `opSet` member becomes `view.opSet(i, value)`.
bool rewriteStructIndexAssign(Expr &expr,
                              const std::string &namespacePrefix,
                              Context &ctx,
                              const LocalTypeMap &locals,
                              const std::vector<semantics::ParameterInfo> &params,
                              bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || expr.isFieldAccess ||
      expr.name != "assign" || expr.args.size() != 2 || hasNamedCallArguments(expr)) {
    return false;
  }
  const Expr &target = expr.args[0];
  if (target.kind != Expr::Kind::Call || target.isMethodCall || target.isBinding || target.isFieldAccess ||
      target.name != "at" || target.args.size() != 2 || target.args[0].kind != Expr::Kind::Name) {
    return false;
  }
  semantics::BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(target.args[0], params, locals, allowMathBare, ctx, receiverInfo)) {
    return false;
  }
  std::string base = semantics::normalizeBindingTypeName(receiverInfo.typeName);
  if (base.empty() || base == "Reference" || base == "Pointer" || base == "array" || base == "vector" || base == "map" ||
      base == "string") {
    return false;
  }
  if (base.front() != '/') {
    base = resolveNameToPath(base, namespacePrefix, scopedImportAliasesForNamespace(namespacePrefix, ctx), ctx.sourceDefs);
  }
  if (base.empty() || base.front() != '/') {
    return false;
  }
  const std::string memberPath = base + "/opSet";
  if (ctx.sourceDefs.count(memberPath) == 0 && ctx.helperOverloads.count(memberPath) == 0 &&
      !anySourceDefStartsWith(ctx, memberPath + "<") && !anySourceDefStartsWith(ctx, memberPath + "__t")) {
    return false;
  }
  std::vector<Expr> args = {target.args[0], target.args[1], expr.args[1]};
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  expr.args = std::move(args);
  expr.argNames.clear();
  expr.name = "opSet";
  expr.isMethodCall = true;
  return true;
}

// Operators and index sugar for stdlib structs: `a + b`, `a == b`, `a < b` and
// `a[i]` on a struct value whose type has a member of the matching name
// (`/std/text/String/plus`) become the method call `a.plus(b)`.
bool rewriteStructOperatorCallToMethod(Expr &expr,
                                       const std::string &namespacePrefix,
                                       Context &ctx,
                                       const LocalTypeMap &locals,
                                       const std::vector<semantics::ParameterInfo> &params,
                                       bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding ||
      expr.isFieldAccess || expr.args.size() != 2 || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || hasNamedCallArguments(expr) ||
      !expr.templateArgs.empty() || expr.name.empty() || expr.name.front() == '/') {
    return false;
  }
  const std::string name = expr.name;
  if (name != "plus" && name != "equal" && name != "not_equal" && name != "less_than" &&
      name != "less_equal" && name != "greater_than" && name != "greater_equal" && name != "at") {
    return false;
  }
  // Operands first, so `a == b + c` sees the type of `b.opPlus(c)`.
  for (Expr &operand : expr.args) {
    (void)rewriteStructOperatorCallToMethod(operand, namespacePrefix, ctx, locals, params, allowMathBare);
  }
  // Only operands whose type the inference reports reliably: names, calls of free functions, and operator
  // results; a field access or an arbitrary method chain is left alone (bind it to a local to use an operator).
  for (const Expr &operand : expr.args) {
    const bool operatorResult = operand.isMethodCall && operand.name.rfind("op", 0) == 0;
    const bool plainOperand = operand.kind == Expr::Kind::Name ||
        (operand.kind == Expr::Kind::Call && !operand.isMethodCall && !operand.isFieldAccess);
    // One method on a named receiver (`owned.view() == wanted`): the receiver's type gives the method's result.
    const bool namedMethodOperand = operand.kind == Expr::Kind::Call && operand.isMethodCall &&
        !operand.isFieldAccess && !operand.args.empty() && operand.args.front().kind == Expr::Kind::Name;
    if (!operatorResult && !plainOperand && !namedMethodOperand && operand.kind != Expr::Kind::Literal) {
      return false;
    }
  }
  semantics::BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(
          expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo)) {
    return false;
  }
  std::string base = semantics::normalizeBindingTypeName(receiverInfo.typeName);
  if (base.empty() || base == "Reference" || base == "Pointer" || base == "array" ||
      base == "vector" || base == "map" || base == "string") {
    return false;
  }
  if (base.front() != '/') {
    base = resolveNameToPath(base,
                             namespacePrefix,
                             scopedImportAliasesForNamespace(namespacePrefix, ctx),
                             ctx.sourceDefs);
  }
  if (base.empty() || base.front() != '/') {
    return false;
  }
  if (name != "at") {
    // Compare the resolved types: a parameter's type can already be a full path (`/std/text/view/StringView`)
    // while a local of the same type is spelled as written (`StringView`).
    semantics::BindingInfo otherInfo;
    if (!inferBindingTypeForMonomorph(expr.args[1], params, locals, allowMathBare, ctx, otherInfo)) {
      return false;
    }
    std::string otherBase = semantics::normalizeBindingTypeName(otherInfo.typeName);
    if (!otherBase.empty() && otherBase.front() != '/') {
      otherBase = resolveNameToPath(otherBase,
                                    namespacePrefix,
                                    scopedImportAliasesForNamespace(namespacePrefix, ctx),
                                    ctx.sourceDefs);
    }
    if (otherBase != base) {
      return false;
    }
  }
  // Operator members are spelled opPlus, opEqual, opLessThan, ... (`at` is opAt): a member named `plus` would
  // capture every bare `plus(a, b)` written inside the struct's own scope.
  std::string opName = "op";
  opName += static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
  for (size_t i = 1; i < name.size(); ++i) {
    if (name[i] == '_' && i + 1 < name.size()) {
      opName += static_cast<char>(std::toupper(static_cast<unsigned char>(name[++i])));
    } else {
      opName += name[i];
    }
  }
  auto hasMember = [&](const std::string &memberName) {
    const std::string memberPath = base + "/" + memberName;
    return ctx.sourceDefs.count(memberPath) > 0 || ctx.helperOverloads.count(memberPath) > 0 ||
           anySourceDefStartsWith(ctx, memberPath + "<") ||
           anySourceDefStartsWith(ctx, memberPath + "__t");
  };
  if (!hasMember(opName)) {
    return false;
  }
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  // A chain of three or more Strings is one call: `a + b + c` and `a + (b + c)` become
  // a.opPlusMany(b, c), which sums the counts and allocates once. The chain is flattened (text `+` is
  // associative) and the parts are evaluated left to right, as they are written.
  if (name == "plus" && base == "/std/text/String" && hasMember("opPlusMany")) {
    std::vector<Expr> leaves;
    auto collectLeaves = [&](const Expr &part, auto &&self) -> void {
      if (part.kind == Expr::Kind::Call && part.isMethodCall && part.name == "opPlus" && part.args.size() == 2) {
        self(part.args[0], self);
        self(part.args[1], self);
        return;
      }
      if (part.kind == Expr::Kind::Call && part.isMethodCall && part.name == "opPlusMany" && !part.args.empty()) {
        self(part.args[0], self);
        for (size_t k = 1; k < part.args.size(); ++k) {
          self(part.args[k], self);
        }
        return;
      }
      leaves.push_back(part);
    };
    collectLeaves(expr.args[0], collectLeaves);
    collectLeaves(expr.args[1], collectLeaves);
    if (leaves.size() >= 3) {
      expr.args = std::move(leaves);
      expr.argNames.clear();
      expr.name = "opPlusMany";
      expr.isMethodCall = true;
      return true;
    }
  }
  expr.name = opName;
  expr.isMethodCall = true;
  return true;
}

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<semantics::ParameterInfo> &params,
                 bool allowMathBare) {
    RewriteExprState st;
    st.allowMathBare = allowMathBare;
    if (rewriteExprPhase1(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase2(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase3(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase4(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase5(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase6(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase7(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase8(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    return st.result;
}

PhaseStatus rewriteExprPhase1([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  expr.namespacePrefix = namespacePrefix;
  rewriteCallableShapes(expr, mapping, allowedParams, namespacePrefix, ctx, locals, params, allowMathBare);
  st.hadExplicitTemplateArgsOnEntry = !expr.templateArgs.empty();
  [[maybe_unused]] auto &hadExplicitTemplateArgsOnEntry = st.hadExplicitTemplateArgsOnEntry;
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return st.done(false);
  }
  {
    const FunctionValueConversion functionValues;
    const TextViewConversion textViews;
    const ExpectedTypeScope scope{
        mapping, allowedParams, namespacePrefix, ctx, locals, params, allowMathBare};
    if (!rewriteExpectedTypeSites(expr, {&functionValues, &textViews}, scope, error)) {
      return st.done(false);
    }
  }
  if (expr.kind == Expr::Kind::Call && expr.name == "ct_if" &&
      expr.args.size() == 3) {
    if (!rewriteCompileTimePredicateExpr(
            expr.args[0], mapping, allowedParams, namespacePrefix, ctx, error)) {
      return st.done(false);
    }
    for (std::size_t branchIndex = 1; branchIndex < expr.args.size();
         ++branchIndex) {
      LocalTypeMap branchLocals = locals;
      for (Expr &bodyArg : expr.args[branchIndex].bodyArguments) {
        if (!rewriteExpr(bodyArg,
                         mapping,
                         allowedParams,
                         namespacePrefix,
                         ctx,
                         error,
                         branchLocals,
                         params,
                         allowMathBare)) {
          return st.done(false);
        }
      }
    }
    return st.done(true);
  }
  if (expr.kind == Expr::Kind::Name) {
    auto isRuntimeParameter = [&]() {
      for (const semantics::ParameterInfo &param : params) {
        if (param.name == expr.name) {
          return true;
        }
      }
      return false;
    };
    auto appendUniquePath = [](std::vector<std::string> &paths,
                               const std::string &path) {
      if (!path.empty() && std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
      }
    };
    auto appendZeroArgCallablePathsForTarget =
        [&](const std::string &targetPath, std::vector<std::string> &paths) {
      auto sourceIt = ctx.sourceDefs.find(targetPath);
      if (sourceIt != ctx.sourceDefs.end()) {
        if (sourceIt->second.parameters.empty() &&
            sourceIt->second.templateArgs.empty() &&
            !isStructDefinition(sourceIt->second)) {
          appendUniquePath(paths, targetPath);
        }
        return;
      }
      auto overloadIt = ctx.helperOverloads.find(targetPath);
      if (overloadIt == ctx.helperOverloads.end()) {
        return;
      }
      for (const HelperOverloadEntry &entry : overloadIt->second) {
        if (entry.parameterCount != 0 || entry.isVariadic) {
          continue;
        }
        auto overloadDefIt = ctx.sourceDefs.find(entry.internalPath);
        if (overloadDefIt == ctx.sourceDefs.end() ||
            !overloadDefIt->second.templateArgs.empty() ||
            isStructDefinition(overloadDefIt->second)) {
          continue;
        }
        appendUniquePath(paths, entry.internalPath);
      }
    };
    auto visibleZeroArgDefinitionPaths = [&]() -> std::vector<std::string> {
      std::vector<std::string> paths;
      if (expr.name.empty() || expr.name.find('/') != std::string::npos) {
        return paths;
      }
      const std::string resolvedPath =
          resolveNameToPath(expr.name,
                            namespacePrefix,
                            scopedImportAliasesForNamespace(namespacePrefix, ctx),
                            ctx.sourceDefs);
      appendZeroArgCallablePathsForTarget(resolvedPath, paths);
      const auto &aliasTargets =
          scopedImportAliasTargetsForNamespace(namespacePrefix, ctx);
      auto aliasIt = aliasTargets.find(expr.name);
      if (aliasIt != aliasTargets.end()) {
        for (const std::string &targetPath : aliasIt->second) {
          appendZeroArgCallablePathsForTarget(targetPath, paths);
        }
      }
      std::sort(paths.begin(), paths.end());
      paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
      return paths;
    };
    auto appendPathList = [](std::string &message,
                             const std::vector<std::string> &paths) {
      for (size_t index = 0; index < paths.size(); ++index) {
        if (index == 0) {
          message += paths[index];
        } else if (index + 1 == paths.size()) {
          message += " and " + paths[index];
        } else {
          message += ", " + paths[index];
        }
      }
    };
    std::vector<std::string> zeroArgPaths = visibleZeroArgDefinitionPaths();
    const bool hasLocalValue = isRuntimeParameter() || locals.count(expr.name) > 0;
    if (hasLocalValue && !zeroArgPaths.empty()) {
      error = "ambiguous bare name: " + expr.name +
              " could refer to a local value or ";
      appendPathList(error, zeroArgPaths);
      return st.done(false);
    }
    if (zeroArgPaths.size() > 1) {
      error = "ambiguous bare name: " + expr.name + " could refer to ";
      appendPathList(error, zeroArgPaths);
      return st.done(false);
    }
    if (zeroArgPaths.size() == 1) {
      Expr callExpr;
      callExpr.kind = Expr::Kind::Call;
      callExpr.name = expr.name;
      callExpr.sourceName = expr.sourceName.empty() ? expr.name : expr.sourceName;
      callExpr.namespacePrefix = expr.namespacePrefix;
      callExpr.sourceLine = expr.sourceLine;
      callExpr.sourceColumn = expr.sourceColumn;
      callExpr.semanticNodeId = expr.semanticNodeId;
      expr = std::move(callExpr);
    }
  }
  if (expr.kind != Expr::Kind::Call) {
    return st.done(true);
  }
  if (expr.isLambda) {
    std::unordered_set<std::string> lambdaAllowed = allowedParams;
    for (const auto &param : expr.templateArgs) {
      lambdaAllowed.insert(param);
    }
    SubstMap lambdaMapping = mapping;
    for (const auto &param : expr.templateArgs) {
      lambdaMapping.erase(param);
    }
    LocalTypeMap lambdaLocals = locals;
    lambdaLocals.reserve(lambdaLocals.size() + expr.args.size() + expr.bodyArguments.size());
    for (auto &param : expr.args) {
      if (!rewriteExpr(param, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return st.done(false);
      }
      semantics::BindingInfo info;
      if (extractExplicitBindingType(param, info)) {
        if (info.typeName == "auto" && param.args.size() == 1 &&
            inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        } else {
          lambdaLocals[param.name] = info;
        }
      } else if (param.isBinding && param.args.size() == 1) {
        if (inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        }
      }
    }
    for (auto &bodyArg : expr.bodyArguments) {
      if (!rewriteExpr(bodyArg, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return st.done(false);
      }
      semantics::BindingInfo info;
      if (extractExplicitBindingType(bodyArg, info)) {
        if (info.typeName == "auto" && bodyArg.args.size() == 1 &&
            inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        } else {
          lambdaLocals[bodyArg.name] = info;
        }
      } else if (bodyArg.isBinding && bodyArg.args.size() == 1) {
        if (inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        }
      }
    }
    return st.done(true);
  }
  if (expr.templateArgs.empty()) {
    std::string explicitBase;
    std::string explicitArgText;
    if (semantics::splitTemplateTypeName(expr.name, explicitBase, explicitArgText)) {
      std::vector<std::string> explicitArgs;
      if (!semantics::splitTopLevelTemplateArgs(explicitArgText, explicitArgs)) {
        error = "invalid template arguments for " + expr.name;
        return st.done(false);
      }
      expr.name = explicitBase;
      expr.templateArgs = std::move(explicitArgs);
    }
  }
  st.resolvePickSumDefinition = [&](const Expr &target) -> Definition * {
    semantics::BindingInfo targetInfo;
    if (!inferBindingTypeForMonomorph(target, params, locals, allowMathBare, ctx,
                                      targetInfo)) {
      return nullptr;
    }
    std::string targetTypeText = bindingTypeToString(targetInfo);
    if (targetTypeText.empty()) {
      return nullptr;
    }
    std::string resolveError;
    ResolvedType resolvedType =
        resolveTypeString(targetTypeText, mapping, allowedParams, namespacePrefix,
                          ctx, resolveError);
    if (!resolveError.empty() || resolvedType.text.empty()) {
      return nullptr;
    }
    targetTypeText = semantics::normalizeBindingTypeName(resolvedType.text);
    std::string base;
    std::string argText;
    if (semantics::splitTemplateTypeName(targetTypeText, base, argText)) {
      targetTypeText = semantics::normalizeBindingTypeName(base);
    }
    std::string sumPath = targetTypeText;
    if (sumPath.empty()) {
      return nullptr;
    }
    if (sumPath.front() != '/') {
      sumPath = resolveNameToPath(sumPath,
                                  namespacePrefix,
                                  scopedImportAliasesForNamespace(namespacePrefix,
                                                                  ctx),
                                  ctx.sourceDefs);
    }
    auto sumIt = ctx.sourceDefs.find(sumPath);
    if (sumIt == ctx.sourceDefs.end() ||
        !isSumDefinitionForMonomorphRefresh(sumIt->second)) {
      return nullptr;
    }
    return &sumIt->second;
  };
  [[maybe_unused]] auto &resolvePickSumDefinition = st.resolvePickSumDefinition;
  st.appendPickPayloadLocal = [](const SumVariant &variant,
                                   const Expr &arm,
                                   LocalTypeMap &armLocals) {
    if (!variant.hasPayload || arm.args.size() != 1) {
      return;
    }
    const Expr &payloadBinder = arm.args.front();
    if (payloadBinder.kind != Expr::Kind::Name || payloadBinder.name.empty()) {
      return;
    }
    semantics::BindingInfo payloadInfo;
    payloadInfo.typeName = variant.payloadType;
    if (!variant.payloadTemplateArgs.empty()) {
      payloadInfo.typeTemplateArg = semantics::joinTemplateArgs(variant.payloadTemplateArgs);
    }
    armLocals[payloadBinder.name] = std::move(payloadInfo);
  };
  [[maybe_unused]] auto &appendPickPayloadLocal = st.appendPickPayloadLocal;
  st.recordBodyBindingLocal = [&](Expr &bodyExpr, LocalTypeMap &bodyLocals) {
    semantics::BindingInfo info;
    if (extractExplicitBindingType(bodyExpr, info)) {
      if (info.typeName == "auto" && bodyExpr.args.size() == 1 &&
          inferBindingTypeForMonomorph(bodyExpr.args.front(),
                                       params,
                                       bodyLocals,
                                       allowMathBare,
                                       ctx,
                                       info)) {
        bodyLocals[bodyExpr.name] = info;
      } else {
        bodyLocals[bodyExpr.name] = info;
      }
      return;
    }
    if (bodyExpr.isBinding && bodyExpr.args.size() == 1 &&
        inferBindingTypeForMonomorph(bodyExpr.args.front(),
                                     params,
                                     bodyLocals,
                                     allowMathBare,
                                     ctx,
                                     info)) {
      bodyLocals[bodyExpr.name] = info;
    }
  };
  [[maybe_unused]] auto &recordBodyBindingLocal = st.recordBodyBindingLocal;
  {
    auto isStdlibTuplePathEarly = [](const std::string &path) {
      return path == "/std/tuple/tuple" ||
             path.rfind("/std/tuple/tuple__t", 0) == 0;
    };
    auto resolveTupleBasePathEarly = [&](const std::string &base) -> std::string {
      std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if (normalizedBase.empty()) {
        return {};
      }
      if (isStdlibTuplePathEarly(normalizedBase)) {
        return normalizedBase;
      }
      if (!normalizedBase.empty() && normalizedBase.front() == '/') {
        return {};
      }
      return resolveNameToPath(normalizedBase,
                               namespacePrefix,
                               scopedImportAliasesForNamespace(namespacePrefix, ctx),
                               ctx.sourceDefs);
    };
    auto extractTupleReceiverTemplateArgsEarly =
        [&](std::string typeText,
            bool &borrowedOut,
            std::vector<std::string> &tupleArgsOut) -> bool {
      borrowedOut = false;
      tupleArgsOut.clear();
      typeText = semantics::normalizeBindingTypeName(std::move(typeText));
      while (true) {
        std::string wrapperBase;
        std::string wrapperArgText;
        if (!semantics::splitTemplateTypeName(typeText, wrapperBase, wrapperArgText)) {
          break;
        }
        wrapperBase = semantics::normalizeBindingTypeName(wrapperBase);
        if (wrapperBase != "Reference" && wrapperBase != "Pointer") {
          break;
        }
        if (wrapperBase == "Pointer") {
          return false;
        }
        std::vector<std::string> wrapperArgs;
        if (!semantics::splitTopLevelTemplateArgs(wrapperArgText, wrapperArgs) ||
            wrapperArgs.size() != 1) {
          return false;
        }
        borrowedOut = true;
        typeText = semantics::normalizeBindingTypeName(wrapperArgs.front());
      }

      std::string tupleBase;
      std::string tupleArgText;
      if (semantics::splitTemplateTypeName(typeText, tupleBase, tupleArgText)) {
        if (!isStdlibTuplePathEarly(resolveTupleBasePathEarly(tupleBase))) {
          return false;
        }
        if (tupleArgText.empty()) {
          return true;
        }
        return semantics::splitTopLevelTemplateArgs(tupleArgText, tupleArgsOut);
      }

      const std::string tuplePath = resolveTupleBasePathEarly(typeText);
      if (!isStdlibTuplePathEarly(tuplePath)) {
        return false;
      }
      auto defIt = ctx.sourceDefs.find(tuplePath);
      if (defIt == ctx.sourceDefs.end()) {
        return false;
      }
      for (const TemplatePackBinding &packBinding : defIt->second.templatePackBindings) {
        if (packBinding.parameterName == "Ts") {
          tupleArgsOut = packBinding.arguments;
          return true;
        }
      }
      return false;
    };
    auto rewriteTupleIndexCandidate = [&](Expr &candidate,
                                          const auto &self,
                                          bool &handledOut) -> bool {
      handledOut = false;
      std::string builtinAccessName;
      if (candidate.kind != Expr::Kind::Call) {
        return true;
      }
      if (semantics::getBuiltinArrayAccessName(candidate, builtinAccessName) &&
          candidate.args.size() == 2 && !hasNamedCallArguments(candidate) &&
          !candidate.hasBodyArguments && candidate.bodyArguments.empty() &&
          candidate.templateArgs.empty()) {
        semantics::BindingInfo receiverInfo;
        if (inferBindingTypeForMonomorph(candidate.args.front(),
                                         params,
                                         locals,
                                         allowMathBare,
                                         ctx,
                                         receiverInfo)) {
          bool borrowedReceiver = false;
          std::vector<std::string> tupleArgs;
          if (extractTupleReceiverTemplateArgsEarly(bindingTypeToString(receiverInfo),
                                                   borrowedReceiver,
                                                   tupleArgs)) {
            const Expr &indexExpr = candidate.args[1];
            if (indexExpr.kind != Expr::Kind::Literal ||
                indexExpr.isUnsigned ||
                (indexExpr.intWidth != 32 && indexExpr.intWidth != 64)) {
              error = "tuple index must be a compile-time integer";
              return false;
            }

            std::vector<std::string> templateArgs;
            templateArgs.reserve(tupleArgs.size() + 1);
            templateArgs.push_back(std::to_string(indexExpr.literalValue));
            templateArgs.insert(templateArgs.end(), tupleArgs.begin(), tupleArgs.end());

            std::vector<TemplateArgument> templateArgDetails;
            templateArgDetails.reserve(templateArgs.size());
            templateArgDetails.push_back(
                TemplateArgument::integer(templateArgs.front(), indexExpr.literalValue));
            for (size_t i = 1; i < templateArgs.size(); ++i) {
              templateArgDetails.push_back(TemplateArgument::type(templateArgs[i]));
            }

            Expr receiver = candidate.args.front();
            candidate.name = borrowedReceiver ? "/std/tuple/get_ref" : "/std/tuple/get";
            candidate.sourceName = candidate.name;
            candidate.sourceIsMethodCall = false;
            candidate.namespacePrefix.clear();
            candidate.args.clear();
            candidate.args.push_back(std::move(receiver));
            candidate.argNames.assign(1, std::nullopt);
            candidate.templateArgs = std::move(templateArgs);
            candidate.templateArgDetails = std::move(templateArgDetails);
            candidate.isMethodCall = false;
            candidate.isFieldAccess = false;
            handledOut = true;
            return rewriteExpr(candidate,
                               mapping,
                               allowedParams,
                               namespacePrefix,
                               ctx,
                               error,
                               locals,
                               params,
                               allowMathBare);
          }
        }
      }
      for (Expr &arg : candidate.args) {
        bool childHandled = false;
        if (!self(arg, self, childHandled)) {
          return false;
        }
      }
      for (Expr &arg : candidate.bodyArguments) {
        bool childHandled = false;
        if (!self(arg, self, childHandled)) {
          return false;
        }
      }
      return true;
    };
    bool handledTupleIndexAccess = false;
    if (!rewriteTupleIndexCandidate(expr,
                                    rewriteTupleIndexCandidate,
                                    handledTupleIndexAccess)) {
      return st.done(false);
    }
    if (handledTupleIndexAccess) {
      return st.done(true);
    }
  }
  return PhaseStatus::Continue;
}

} // namespace primec
