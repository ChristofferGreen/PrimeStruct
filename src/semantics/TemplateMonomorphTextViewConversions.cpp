#include "TemplateMonomorphTextViewConversions.h"

#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphImplicitTemplateInference.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec {

namespace {

constexpr const char *StringViewPath = "/std/text/view/StringView";
constexpr const char *TextOfPath = "/std/text/view/textOf";

// The type name without its template arguments.
std::string baseTypeName(const std::string &typeName) {
  const std::string normalized = semantics::normalizeBindingTypeName(typeName);
  const size_t angle = normalized.find('<');
  return angle == std::string::npos ? normalized : normalized.substr(0, angle);
}

// Whether `typeName`, declared in `typeNamespace`, names the stdlib StringView (and not a user type of that name).
bool namesStringView(const std::string &typeName, const std::string &typeNamespace, const Context &ctx) {
  const std::string name = baseTypeName(typeName);
  if (name == StringViewPath) {
    return true;
  }
  if (name != "StringView") {
    return false;
  }
  // A type of that name in the declaring namespace or one around it comes first: a user's `/app/StringView`, or the
  // view module's own when a member of `/std/text/view/StringView` names it by its short name.
  std::string scope = typeNamespace;
  while (!scope.empty()) {
    if (ctx.sourceDefs.count(scope + "/" + name) > 0) {
      return scope + "/" + name == StringViewPath;
    }
    const size_t slash = scope.rfind('/');
    scope = slash == std::string::npos ? std::string() : scope.substr(0, slash);
  }
  // Otherwise the name comes from an import (`import /std/text/view/*`) or is a root type.
  return resolveStructLikeTypePathForTemplatedVectorFallback(name, typeNamespace, ctx) == StringViewPath;
}

bool isStringTyped(const semantics::BindingInfo &binding) {
  return binding.typeName == "string" && binding.typeTemplateArg.empty();
}

// The binding a name refers to, if it is a local or a parameter.
const semantics::BindingInfo *nameBinding(const Expr &arg, const ExpectedTypeScope &scope) {
  if (auto localIt = scope.locals.find(arg.name); localIt != scope.locals.end()) {
    return &localIt->second;
  }
  for (const semantics::ParameterInfo &param : scope.params) {
    if (param.name == arg.name) {
      return &param.binding;
    }
  }
  return nullptr;
}

// Whether `arg` is a `string`: a literal, a `string` local or parameter, or an expression whose type is `string`.
bool isStringValue(const Expr &arg, const ExpectedTypeScope &scope) {
  if (arg.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (arg.kind == Expr::Kind::Name) {
    const semantics::BindingInfo *binding = nameBinding(arg, scope);
    return binding != nullptr && isStringTyped(*binding);
  }
  if (arg.kind != Expr::Kind::Call || arg.isBinding || arg.isLambda || arg.isBraceConstructor || arg.isSpread) {
    return false;
  }
  semantics::BindingInfo info;
  return inferBindingTypeForMonomorph(arg, scope.params, scope.locals, scope.allowMathBare, scope.ctx, info) &&
         isStringTyped(info);
}

} // namespace

bool TextViewConversion::enabled(const Context &ctx) const {
  return ctx.sourceDefs.count(StringViewPath) > 0 && ctx.sourceDefs.count(TextOfPath) > 0;
}

bool TextViewConversion::mayBeTarget(const std::string &typeName) const {
  return expectedTypePathLeaf(baseTypeName(typeName)) == "StringView";
}

bool TextViewConversion::mayConvert(const Expr &arg, const ExpectedTypeScope &scope) const {
  if (arg.isSpread) {
    return false;
  }
  if (arg.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (arg.kind == Expr::Kind::Name) {
    const semantics::BindingInfo *binding = nameBinding(arg, scope);
    return binding != nullptr && isStringTyped(*binding);
  }
  // A call may return a `string`; its type is inferred once the callee is known to expect a StringView.
  return arg.kind == Expr::Kind::Call && !arg.isBinding && !arg.isLambda && !arg.isBraceConstructor;
}

bool TextViewConversion::convert(Expr &arg,
                                 const std::vector<const ExpectedType *> &expected,
                                 const ExpectedTypeScope &scope,
                                 std::string &) const {
  if (expected.empty()) {
    return true;
  }
  // Every candidate callee must be known and take a StringView there. A same-arity overload that takes the `string`
  // as it is (or anything else) keeps the argument unconverted, so the exact match wins and overload selection,
  // which cannot type a nested `textOf(...)` call, is not changed.
  for (const ExpectedType *type : expected) {
    if (type == nullptr || !type->type.templateArgs.empty() ||
        !namesStringView(type->type.name, type->typeNamespace, scope.ctx)) {
      return true;
    }
  }
  if (!isStringValue(arg, scope)) {
    return true;
  }
  Expr view;
  view.kind = Expr::Kind::Call;
  view.name = TextOfPath;
  view.sourceName = "textOf";
  view.namespacePrefix = scope.namespacePrefix;
  view.sourceLine = arg.sourceLine;
  view.sourceColumn = arg.sourceColumn;
  view.args.push_back(std::move(arg));
  view.argNames.push_back(std::nullopt);
  arg = std::move(view);
  return true;
}

} // namespace primec
