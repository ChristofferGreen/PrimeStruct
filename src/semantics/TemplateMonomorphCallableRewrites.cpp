#include "TemplateMonomorphCallableRewrites.h"

#include "TemplateMonomorphFallbackTypeInference.h"
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
      return false;
    }
    const size_t slash = scope.rfind('/');
    scope = slash == std::string::npos ? std::string() : scope.substr(0, slash);
  }
}

bool isFieldQualifierTransformName(const std::string &name) {
  return name == "mut" || name == "public" || name == "private" || name == "static" || name == "copy" ||
         name == "move" || name == "restrict" || name == "unsafe";
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

} // namespace

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
