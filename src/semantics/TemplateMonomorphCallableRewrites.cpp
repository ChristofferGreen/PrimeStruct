#include "TemplateMonomorphCallableRewrites.h"

#include "SemanticsValidateLambdaClosures.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphTypeResolution.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec {

namespace {

// Whether `typeName` (a binding's type) names a struct with a `call` method: a closure struct from the
// lambda-closures pass or a hand-written callable. Template instances (`/Foo__t1234`) answer for their base.
bool isCallableStructTypeName(const std::string &typeName, const std::string &namespacePrefix, const Context &ctx) {
  if (typeName.empty()) {
    return false;
  }
  std::string name = typeName;
  const size_t angle = name.find('<');
  if (angle != std::string::npos) {
    name.resize(angle);
  }
  auto hasCall = [&](const std::string &path) {
    if (ctx.sourceDefs.count(path + "/call") > 0) {
      return true;
    }
    const size_t instance = path.rfind("__t");
    return instance != std::string::npos && instance > path.rfind('/') &&
           ctx.sourceDefs.count(path.substr(0, instance) + "/call") > 0;
  };
  if (name.front() == '/') {
    return hasCall(name);
  }
  std::string scope = namespacePrefix;
  while (true) {
    if (hasCall(scope + "/" + name)) {
      return true;
    }
    if (scope.empty()) {
      // An imported struct spelled by its short name (`import /std/http/*` then `Router`).
      const std::string *imported = lookupScopedImportAliasForNamespace(name, namespacePrefix, ctx);
      return imported != nullptr && hasCall(*imported);
    }
    const size_t slash = scope.rfind('/');
    scope = slash == std::string::npos ? std::string() : scope.substr(0, slash);
  }
}

// `receiver.field(x)` where `field` is a field of a callable struct type (and the receiver's struct has no method
// of that name) becomes `receiver.field.call(x)`.
void rewriteCallableFieldCall(Expr &expr,
                              const std::string &namespacePrefix,
                              Context &ctx,
                              const LocalTypeMap &locals,
                              const std::vector<semantics::ParameterInfo> &params,
                              bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.isFieldAccess || expr.isBinding ||
      expr.args.empty() || expr.name.empty() || expr.name.find('/') != std::string::npos ||
      !expr.templateArgs.empty()) {
    return;
  }
  std::string receiverPath = resolveStructLikeExprPathForTemplatedVectorFallback(
      expr.args.front(), locals, namespacePrefix, ctx, allowMathBare);
  if (receiverPath.empty()) {
    semantics::BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo)) {
      receiverPath = receiverInfo.typeName;
      if ((receiverPath == "Reference" || receiverPath == "Pointer") && !receiverInfo.typeTemplateArg.empty()) {
        receiverPath = receiverInfo.typeTemplateArg;
      }
      if (receiverPath.empty() || receiverPath.front() != '/' || !receiverInfo.typeTemplateArg.empty()) {
        if (receiverPath != receiverInfo.typeTemplateArg) {
          receiverPath.clear();
        }
      }
    }
  }
  if (receiverPath.empty()) {
    // `items[i].field(x)`: the receiver is an element of an indexed collection.
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Call && !receiver.isMethodCall && receiver.args.size() == 2 &&
        (receiver.name == "at" || receiver.name == "at_unsafe")) {
      semantics::BindingInfo collectionInfo;
      if (inferBindingTypeForMonomorph(receiver.args.front(), params, locals, allowMathBare, ctx, collectionInfo) &&
          !collectionInfo.typeTemplateArg.empty()) {
        receiverPath = resolveStructLikeTypePathForTemplatedVectorFallback(collectionInfo.typeTemplateArg,
                                                                           namespacePrefix, ctx);
      }
      // `this.items[i].field(x)`: the collection is a field; its element type is in the field's declaration.
      const Expr &collection = receiver.args.front();
      if (receiverPath.empty() && collection.kind == Expr::Kind::Call && collection.isFieldAccess &&
          collection.args.size() == 1) {
        const Expr &owner = collection.args.front();
        std::string ownerPath =
            resolveStructLikeExprPathForTemplatedVectorFallback(owner, locals, namespacePrefix, ctx, allowMathBare);
        if (ownerPath.empty() && owner.kind == Expr::Kind::Name && owner.name == "this") {
          ownerPath = namespacePrefix;  // a method of the struct the `this` field belongs to
        }
        semantics::BindingInfo ownerInfo;
        if (ownerPath.empty() &&
            inferBindingTypeForMonomorph(owner, params, locals, allowMathBare, ctx, ownerInfo)) {
          const bool borrowed = ownerInfo.typeName == "Reference" || ownerInfo.typeName == "Pointer";
          ownerPath = resolveStructLikeTypePathForTemplatedVectorFallback(
              borrowed ? ownerInfo.typeTemplateArg : ownerInfo.typeName, namespacePrefix, ctx);
        }
        const auto ownerIt = ownerPath.empty() ? ctx.sourceDefs.end() : ctx.sourceDefs.find(ownerPath);
        if (ownerIt != ctx.sourceDefs.end()) {
          for (const Expr &field : ownerIt->second.statements) {
            if (!field.isBinding || field.name != collection.name) {
              continue;
            }
            for (const Transform &transform : field.transforms) {
              if (!isFieldQualifierTransformName(transform.name) && transform.templateArgs.size() == 1) {
                receiverPath = resolveStructLikeTypePathForTemplatedVectorFallback(
                    transform.templateArgs.front(), ownerIt->second.namespacePrefix, ctx);
                break;
              }
            }
            break;
          }
        }
      }
    }
  }
  if (receiverPath.empty() || ctx.sourceDefs.count(receiverPath + "/" + expr.name) > 0) {
    return;
  }
  auto defIt = ctx.sourceDefs.find(receiverPath);
  if (defIt == ctx.sourceDefs.end()) {
    return;
  }
  const Definition &receiverDef = defIt->second;
  for (const Expr &field : receiverDef.statements) {
    if (!field.isBinding || field.name != expr.name) {
      continue;
    }
    std::string fieldType;
    for (const Transform &transform : field.transforms) {
      if (isFieldQualifierTransformName(transform.name)) {
        continue;
      }
      fieldType = transform.name;
      break;
    }
      if (!isCallableStructTypeName(fieldType, receiverDef.namespacePrefix, ctx) &&
        !isCallableStructTypeName(fieldType, namespacePrefix, ctx)) {
      return;
    }
    Expr access;
    access.kind = Expr::Kind::Call;
    access.name = expr.name;
    access.sourceName = expr.name;
    access.namespacePrefix = expr.namespacePrefix;
    access.isMethodCall = true;
    access.sourceIsMethodCall = true;
    access.isFieldAccess = true;
    access.sourceLine = expr.sourceLine;
    access.sourceColumn = expr.sourceColumn;
    access.args.push_back(std::move(expr.args.front()));
    access.argNames.push_back(std::nullopt);
    expr.args.front() = std::move(access);
    expr.name = "call";
    return;
  }
}

// `Holder{[handler] h}` for a template struct `Holder<F>` without template arguments: infer them from the field
// arguments, the way a call infers a template definition's arguments from its parameters.
void inferBraceConstructorTemplateArgs(Expr &expr,
                                       const SubstMap &mapping,
                                       const std::unordered_set<std::string> &allowedParams,
                                       const std::string &namespacePrefix,
                                       Context &ctx,
                                       const LocalTypeMap &locals,
                                       const std::vector<semantics::ParameterInfo> &params,
                                       bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || !expr.isBraceConstructor || expr.isMethodCall || expr.isBinding ||
      !expr.templateArgs.empty() || expr.args.empty()) {
    return;
  }
  const std::string path = resolveCalleePath(expr, namespacePrefix, ctx, &locals, &params);
  auto defIt = ctx.sourceDefs.find(path);
  if (defIt == ctx.sourceDefs.end() || defIt->second.templateArgs.empty() || !isStructDefinition(defIt->second)) {
    return;
  }
  const Definition &structDef = defIt->second;
  for (bool isPack : structDef.templateArgIsPack) {
    if (isPack) {
      return;
    }
  }
  std::vector<const Expr *> fields;
  for (const Expr &stmt : structDef.statements) {
    if (!stmt.isBinding) {
      continue;
    }
    bool isStatic = false;
    for (const Transform &transform : stmt.transforms) {
      isStatic = isStatic || transform.name == "static";
    }
    if (!isStatic) {
      fields.push_back(&stmt);
    }
  }
  Definition shape;
  shape.name = structDef.name;
  shape.fullPath = structDef.fullPath;
  shape.namespacePrefix = structDef.namespacePrefix;
  shape.templateArgs = structDef.templateArgs;
  shape.templateArgIsPack = structDef.templateArgIsPack;
  Expr callShape;
  callShape.kind = Expr::Kind::Call;
  callShape.name = structDef.fullPath;
  callShape.namespacePrefix = expr.namespacePrefix;
  size_t positional = 0;
  for (size_t i = 0; i < expr.args.size(); ++i) {
    const Expr *field = nullptr;
    const bool labeled = i < expr.argNames.size() && expr.argNames[i].has_value();
    if (labeled) {
      for (const Expr *candidate : fields) {
        if (candidate->name == *expr.argNames[i]) {
          field = candidate;
          break;
        }
      }
    } else if (positional < fields.size()) {
      field = fields[positional++];
    }
    if (field == nullptr) {
      return;
    }
    Expr param = *field;
    param.args.clear();
    param.argNames.clear();
    const Expr *argument = &expr.args[i];
    // A `Reference<T>` field initialized from `location(name)` takes T from `name`.
    for (Transform &transform : param.transforms) {
      if (transform.name == "Reference" && transform.templateArgs.size() == 1 &&
          argument->kind == Expr::Kind::Call && argument->name == "location" && argument->args.size() == 1) {
        transform.name = transform.templateArgs.front();
        transform.templateArgs.clear();
        transform.templateArgDetails.clear();
        argument = &argument->args.front();
        break;
      }
    }
    shape.parameters.push_back(std::move(param));
    callShape.args.push_back(*argument);
    callShape.argNames.push_back(std::nullopt);
  }
  std::vector<std::string> inferredArgs;
  std::string inferError;
  if (inferImplicitTemplateArgs(shape, callShape, locals, params, mapping, allowedParams, namespacePrefix, ctx,
                                allowMathBare, inferredArgs, inferError) &&
      inferredArgs.size() == structDef.templateArgs.size()) {
    expr.templateArgs = std::move(inferredArgs);
    expr.templateArgDetails.clear();
  }
}

// `f(x)` where `f` is a local or parameter of a callable struct type becomes `f.call(x)`.
void rewriteCallableBindingCall(Expr &expr,
                                const std::string &namespacePrefix,
                                const Context &ctx,
                                const LocalTypeMap &locals,
                                const std::vector<semantics::ParameterInfo> &params) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || expr.isLambda ||
      expr.isFieldAccess || expr.isBraceConstructor || expr.name.empty() ||
      expr.name.find('/') != std::string::npos || !expr.templateArgs.empty()) {
    return;
  }
  const semantics::BindingInfo *binding = nullptr;
  auto localIt = locals.find(expr.name);
  if (localIt != locals.end()) {
    binding = &localIt->second;
  } else {
    for (const semantics::ParameterInfo &param : params) {
      if (param.name == expr.name) {
        binding = &param.binding;
        break;
      }
    }
  }
  if (binding == nullptr) {
    return;
  }
  std::string typeName = binding->typeName;
  if ((typeName == "Reference" || typeName == "Pointer") && !binding->typeTemplateArg.empty()) {
    typeName = binding->typeTemplateArg;
  }
  if (!isCallableStructTypeName(typeName, namespacePrefix, ctx)) {
    return;
  }
  Expr receiver;
  receiver.kind = Expr::Kind::Name;
  receiver.name = expr.name;
  receiver.namespacePrefix = expr.namespacePrefix;
  receiver.sourceLine = expr.sourceLine;
  receiver.sourceColumn = expr.sourceColumn;
  expr.args.insert(expr.args.begin(), std::move(receiver));
  expr.argNames.insert(expr.argNames.begin(), std::nullopt);
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  expr.name = "call";
  expr.isMethodCall = true;
}

bool isFunctionTypePath(const std::string &typeName) {
  return expectedTypePathLeaf(typeName).rfind("__Function", 0) == 0;
}

// The concrete Function instance (`/__Function1__t...`) a declared type names, resolved in the declaring scope the
// way the monomorphizer resolves the declaration itself; empty for any other type or one that is not concrete.
std::string functionInstanceOf(const Transform &type,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx) {
  if (!isFunctionTypePath(type.name)) {
    return {};
  }
  std::vector<Transform> resolved{type};
  std::string resolveError;
  if (!rewriteTransforms(resolved, mapping, allowedParams, namespacePrefix, ctx, resolveError) ||
      !resolveError.empty() || !resolved.front().templateArgs.empty() ||
      !isFunctionTypePath(resolved.front().name) || resolved.front().name.find("__t") == std::string::npos) {
    return {};
  }
  return resolved.front().name;
}

// Whether a binding type is a callable struct other than a Function.
bool isClosureTypeName(std::string typeName,
                       const std::string &typeTemplateArg,
                       const std::string &namespacePrefix,
                       const Context &ctx) {
  if (typeName == "Reference" || typeName == "Pointer") {
    return false;  // a borrowed closure is not moved into a Function
  }
  if (!typeTemplateArg.empty() && typeName.find('<') == std::string::npos) {
    typeName += "<" + typeTemplateArg + ">";
  }
  return !isFunctionTypePath(typeName) && isCallableStructTypeName(typeName, namespacePrefix, ctx);
}

// Whether `arg` is a closure value that must be boxed to become a Function: a lambda, a local or parameter of a
// callable struct type, a callable struct's brace constructor, or a named free function with a forwarder (whose path
// is stored in `forwarderOut`). Other expressions are left alone: bind them to a local first.
bool isClosureArgument(const Expr &arg,
                       const std::string &namespacePrefix,
                       const Context &ctx,
                       const LocalTypeMap &locals,
                       const std::vector<semantics::ParameterInfo> &params,
                       std::string &forwarderOut) {
  if (arg.isSpread || arg.isBinding || arg.isLambda) {
    return false;
  }
  if (arg.kind == Expr::Kind::Name) {
    const semantics::BindingInfo *binding = nullptr;
    if (auto localIt = locals.find(arg.name); localIt != locals.end()) {
      binding = &localIt->second;
    } else {
      for (const semantics::ParameterInfo &param : params) {
        if (param.name == arg.name) {
          binding = &param.binding;
          break;
        }
      }
    }
    if (binding != nullptr) {
      return isClosureTypeName(binding->typeName, binding->typeTemplateArg, namespacePrefix, ctx);
    }
    const std::string target = resolveNameToPath(arg.name, namespacePrefix,
                                                 scopedImportAliasesForNamespace(namespacePrefix, ctx), ctx.sourceDefs);
    if (target.empty() || ctx.sourceDefs.count(target + semantics::FunctionForwarderSuffix) == 0) {
      return false;
    }
    forwarderOut = target + semantics::FunctionForwarderSuffix;
    return true;
  }
  if (arg.kind != Expr::Kind::Call || arg.isFieldAccess || arg.isMethodCall) {
    return false;
  }
  if (expectedTypePathLeaf(arg.name).find(semantics::LambdaClosureStructPrefix) !=
      std::string::npos) {
    return true; // a lambda: its factory call, or the in-place constructor of a closure with `ref` captures
  }
  return arg.isBraceConstructor && !isFunctionTypePath(arg.name) &&
         isCallableStructTypeName(arg.name, namespacePrefix, ctx);
}

// Whether `arg` is a closure value that must be boxed to become a Function; a named free function is replaced by the
// constructor of its forwarder.
bool closureArgument(Expr &arg,
                     const std::string &namespacePrefix,
                     Context &ctx,
                     const LocalTypeMap &locals,
                     const std::vector<semantics::ParameterInfo> &params) {
  std::string forwarder;
  if (!isClosureArgument(arg, namespacePrefix, ctx, locals, params, forwarder)) {
    return false;
  }
  if (!forwarder.empty()) {
    Expr construct;
    construct.kind = Expr::Kind::Call;
    construct.name = forwarder;
    construct.sourceName = arg.name;
    construct.namespacePrefix = arg.namespacePrefix;
    construct.isBraceConstructor = true;
    construct.sourceLine = arg.sourceLine;
    construct.sourceColumn = arg.sourceColumn;
    arg = std::move(construct);
  }
  return true;
}

// `arg` becomes `/__Function1__t...{[tag] 0i32, [env] /__fn_box(arg)}`, the shape of the explicit conversion.
void boxIntoFunction(Expr &arg, const std::string &functionPath) {
  Expr tag;
  tag.kind = Expr::Kind::Literal;
  tag.literalValue = 0;
  tag.intWidth = 32;
  tag.namespacePrefix = arg.namespacePrefix;
  tag.sourceLine = arg.sourceLine;
  tag.sourceColumn = arg.sourceColumn;
  Expr box;
  box.kind = Expr::Kind::Call;
  box.name = "/__fn_box";
  box.sourceName = "Function";
  box.namespacePrefix = arg.namespacePrefix;
  box.sourceLine = arg.sourceLine;
  box.sourceColumn = arg.sourceColumn;
  Expr function;
  function.kind = Expr::Kind::Call;
  function.name = functionPath;
  function.sourceName = "Function";
  function.namespacePrefix = arg.namespacePrefix;
  function.isBraceConstructor = true;
  function.sourceLine = arg.sourceLine;
  function.sourceColumn = arg.sourceColumn;
  box.args.push_back(std::move(arg));
  box.argNames.push_back(std::nullopt);
  function.args.push_back(std::move(tag));
  function.argNames.push_back(std::string("tag"));
  function.args.push_back(std::move(box));
  function.argNames.push_back(std::string("env"));
  arg = std::move(function);
}

bool convertArgument(Expr &arg,
                     const std::string &functionPath,
                     const std::string &namespacePrefix,
                     Context &ctx,
                     const LocalTypeMap &locals,
                     const std::vector<semantics::ParameterInfo> &params,
                     std::string &error) {
  if (functionPath.empty() || !closureArgument(arg, namespacePrefix, ctx, locals, params)) {
    return true;
  }
  // A lambda with `[ref name]` captures is built in place from `location(name)`: it borrows, so it cannot move into
  // the Function's heap block. (A named closure that borrows is caught by the loan check of `/__fn_box`.)
  if (arg.kind == Expr::Kind::Call && arg.isBraceConstructor) {
    for (const Expr &field : arg.args) {
      if (field.kind == Expr::Kind::Call && field.name == "location" && field.args.size() == 1 &&
          field.args.front().kind == Expr::Kind::Name) {
        error = "closure escapes via Function (root: " + field.args.front().name + ")";
        if (ctx.currentRewriteDefinition != nullptr) {
          error += " in " + ctx.currentRewriteDefinition->fullPath;
        }
        return false;
      }
    }
  }
  boxIntoFunction(arg, functionPath);
  return true;
}

} // namespace

bool FunctionValueConversion::enabled(const Context &ctx) const {
  return anySourceDefStartsWith(ctx, "/__Function");
}

bool FunctionValueConversion::mayBeTarget(const std::string &typeName) const {
  return isFunctionTypePath(typeName);
}

bool FunctionValueConversion::skipsConstructor(const std::string &structName) const {
  return isFunctionTypePath(structName);
}

bool FunctionValueConversion::mayConvert(const Expr &arg, const ExpectedTypeScope &scope) const {
  std::string ignoredForwarder;
  return isClosureArgument(
      arg, scope.namespacePrefix, scope.ctx, scope.locals, scope.params, ignoredForwarder);
}

bool FunctionValueConversion::convert(Expr &arg,
                                      const std::vector<const ExpectedType *> &expected,
                                      const ExpectedTypeScope &scope,
                                      std::string &error) const {
  // A call converts for its first candidate callee only.
  if (expected.empty() || expected.front() == nullptr) {
    return true;
  }
  const ExpectedType &type = *expected.front();
  std::string functionPath;
  if (type.resolved) {
    // An assignment to a `Function` local: its type is already the concrete instance.
    if (isFunctionTypePath(type.type.name) && type.type.templateArgs.empty() &&
        type.type.name.find("__t") != std::string::npos) {
      functionPath = type.type.name;
    }
  } else {
    const SubstMap noMapping;
    const std::unordered_set<std::string> noParams;
    functionPath =
        functionInstanceOf(type.type,
                           type.mapping != nullptr ? *type.mapping : noMapping,
                           type.allowedParams != nullptr ? *type.allowedParams : noParams,
                           type.typeNamespace,
                           scope.ctx);
  }
  return convertArgument(
      arg, functionPath, scope.namespacePrefix, scope.ctx, scope.locals, scope.params, error);
}

void rewriteCallableShapes(Expr &expr,
                           const SubstMap &mapping,
                           const std::unordered_set<std::string> &allowedParams,
                           const std::string &namespacePrefix,
                           Context &ctx,
                           const LocalTypeMap &locals,
                           const std::vector<semantics::ParameterInfo> &params,
                           bool allowMathBare) {
  rewriteCallableBindingCall(expr, namespacePrefix, ctx, locals, params);
  rewriteCallableFieldCall(expr, namespacePrefix, ctx, locals, params, allowMathBare);
  inferBraceConstructorTemplateArgs(expr, mapping, allowedParams, namespacePrefix, ctx, locals, params, allowMathBare);
}

} // namespace primec
