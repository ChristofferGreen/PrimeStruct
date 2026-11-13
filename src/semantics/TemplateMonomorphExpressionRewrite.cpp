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
#include "TemplateMonomorphExpressionRewriteContext.h"

namespace primec {

bool isCompileTimeTypeofPredicateArg(const std::string &arg) {
  const std::string trimmed = trimWhitespace(arg);
  return trimmed.rfind("typeof<", 0) == 0 && trimmed.size() > 8 && trimmed.back() == '>';
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
    if (!rewriteCompileTimePredicateExpr(
            arg, mapping, allowedParams, namespacePrefix, ctx, error)) {
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
  return name == "count" || name == "contains" || name == "tryAt" || name == "at" ||
         name == "at_unsafe" || name == "insert";
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
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || expr.isFieldAccess ||
      expr.args.empty() || expr.hasBodyArguments || !expr.bodyArguments.empty() ||
      hasNamedCallArguments(expr)) {
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
  const bool receiverIsBorrowed = receiverBase == "Reference" || receiverBase == "Pointer";
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
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding ||
      expr.args.size() != 1 || !expr.templateArgs.empty() || hasNamedCallArguments(expr)) {
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
    case '\n':
      token += "\\n";
      break;
    case '\r':
      token += "\\r";
      break;
    case '\t':
      token += "\\t";
      break;
    case '\\':
      token += "\\\\";
      break;
    case '"':
      token += "\\\"";
      break;
    case '\0':
      token += "\\0";
      break;
    default:
      token.push_back(c);
      break;
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
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isFieldAccess ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() || hasNamedCallArguments(expr)) {
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
    const bool bothLiterals = decodedStringLiteral(expr.args[0], left, leftEncoding) &&
                              decodedStringLiteral(expr.args[1], right, rightEncoding);
    if (bothLiterals) {
      if (name == "plus") {
        const StringEncoding joined =
            leftEncoding == StringEncoding::Ascii && rightEncoding == StringEncoding::Ascii
                ? StringEncoding::Ascii
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
      const int order =
          left < right ? -1 : (left == right ? 0 : 1); // std::string compares unsigned bytes
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
        value = left.size() >= right.size() &&
                left.compare(left.size() - right.size(), right.size(), right) == 0;
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
    if (name == "at" && plain && expr.args[1].kind == Expr::Kind::Literal &&
        !expr.args[1].isUnsigned) {
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
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.isBinding ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() || hasNamedCallArguments(expr) ||
      !expr.templateArgs.empty()) {
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
  const bool isVector = base == "vector" || base == "Vector" ||
                        base == collection_helpers::kCanonicalVectorVectorType;
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
      if (semantics::splitTemplateTypeName(
              semantics::normalizeBindingTypeName(receiverInfo.typeTemplateArg),
              wrapper,
              wrapped) &&
          semantics::normalizeBindingTypeName(wrapper) == "array") {
        arrayElement = wrapped;
      }
    }
    if (!arrayElement.empty()) {
      if (expr.sourceName.empty()) {
        expr.sourceName = expr.name;
      }
      const std::string helper = expr.name == "view"
                                     ? "arrayView"
                                     : (expr.name == "slice" ? "arraySlice" : "arraySliceMut");
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
  if (target.kind != Expr::Kind::Call || target.isMethodCall || target.isBinding ||
      target.isFieldAccess || target.name != "at" || target.args.size() != 2 ||
      target.args[0].kind != Expr::Kind::Name) {
    return false;
  }
  semantics::BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(
          target.args[0], params, locals, allowMathBare, ctx, receiverInfo)) {
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
  const std::string memberPath = base + "/opSet";
  if (ctx.sourceDefs.count(memberPath) == 0 && ctx.helperOverloads.count(memberPath) == 0 &&
      !anySourceDefStartsWith(ctx, memberPath + "<") &&
      !anySourceDefStartsWith(ctx, memberPath + "__t")) {
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
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || expr.isFieldAccess ||
      expr.args.size() != 2 || expr.hasBodyArguments || !expr.bodyArguments.empty() ||
      hasNamedCallArguments(expr) || !expr.templateArgs.empty() || expr.name.empty() ||
      expr.name.front() == '/') {
    return false;
  }
  const std::string name = expr.name;
  if (name != "plus" && name != "equal" && name != "not_equal" && name != "less_than" &&
      name != "less_equal" && name != "greater_than" && name != "greater_equal" && name != "at") {
    return false;
  }
  // Operands first, so `a == b + c` sees the type of `b.opPlus(c)`.
  for (Expr &operand : expr.args) {
    (void)rewriteStructOperatorCallToMethod(
        operand, namespacePrefix, ctx, locals, params, allowMathBare);
  }
  // Only operands whose type the inference reports reliably: names, calls of free functions, and operator
  // results; a field access or an arbitrary method chain is left alone (bind it to a local to use an operator).
  for (const Expr &operand : expr.args) {
    const bool operatorResult = operand.isMethodCall && operand.name.rfind("op", 0) == 0;
    const bool plainOperand =
        operand.kind == Expr::Kind::Name ||
        (operand.kind == Expr::Kind::Call && !operand.isMethodCall && !operand.isFieldAccess);
    // One method on a named receiver (`owned.view() == wanted`): the receiver's type gives the method's result.
    const bool namedMethodOperand = operand.kind == Expr::Kind::Call && operand.isMethodCall &&
                                    !operand.isFieldAccess && !operand.args.empty() &&
                                    operand.args.front().kind == Expr::Kind::Name;
    if (!operatorResult && !plainOperand && !namedMethodOperand &&
        operand.kind != Expr::Kind::Literal) {
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
    if (!inferBindingTypeForMonomorph(
            expr.args[1], params, locals, allowMathBare, ctx, otherInfo)) {
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
      if (part.kind == Expr::Kind::Call && part.isMethodCall && part.name == "opPlus" &&
          part.args.size() == 2) {
        self(part.args[0], self);
        self(part.args[1], self);
        return;
      }
      if (part.kind == Expr::Kind::Call && part.isMethodCall && part.name == "opPlusMany" &&
          !part.args.empty()) {
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
  ExprRewriteContext rc{expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params};
  rc.allowMathBare = allowMathBare;
  // Ordered rules: each either decides the expression (Done) or leaves it to the next.
  using Rule = PhaseStatus (*)(ExprRewriteContext &);
  static constexpr Rule Rules[] = {rewriteNameAndTransforms,
                                   rewriteLiteralAndTaskCalls,
                                   rewriteCollectionHelperTemplate,
                                   rewritePlainCallReference,
                                   rewriteMethodAndBuiltinTargets,
                                   rewriteChildExpressions};
  for (const Rule rule : Rules) {
    if (rule(rc) == PhaseStatus::Done) {
      return rc.result;
    }
  }
  return rc.result;
}

} // namespace primec
