#include "SemanticsValidateLambdaClosures.h"

#include "SemanticsHelpers.h"
#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"

#include <cctype>
#include <cstddef>
#include <optional>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {

namespace {

using NameSet = std::set<std::string>;

enum class CaptureKind { Value, Move, Ref, Init };

struct Capture {
  std::string name;
  CaptureKind kind = CaptureKind::Value;
  bool isMutable = false;
  std::optional<Expr> initializer;
};

std::vector<std::string> splitCaptureTokens(const std::string &text) {
  std::vector<std::string> tokens;
  std::string current;
  for (char ch : text) {
    if (ch == ' ' || ch == '\t') {
      if (!current.empty()) {
        tokens.push_back(std::move(current));
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty()) {
    tokens.push_back(std::move(current));
  }
  return tokens;
}

Transform namedTransform(std::string name) {
  Transform transform;
  transform.name = std::move(name);
  return transform;
}

Expr nameExpr(const std::string &name, const Expr &origin) {
  Expr out;
  out.kind = Expr::Kind::Name;
  out.name = name;
  out.namespacePrefix = origin.namespacePrefix;
  out.sourceLine = origin.sourceLine;
  out.sourceColumn = origin.sourceColumn;
  return out;
}

Expr thisFieldAccess(const std::string &field, const Expr &origin) {
  Expr access;
  access.kind = Expr::Kind::Call;
  access.name = field;
  access.sourceName = field;
  access.namespacePrefix = origin.namespacePrefix;
  access.isMethodCall = true;
  access.sourceIsMethodCall = true;
  access.isFieldAccess = true;
  access.sourceLine = origin.sourceLine;
  access.sourceColumn = origin.sourceColumn;
  access.args.push_back(nameExpr("this", origin));
  access.argNames.push_back(std::nullopt);
  return access;
}

bool isResultHelperCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (expr.name == "Result.map" || expr.name == "Result.and_then" || expr.name == "Result.map2" ||
      expr.name == "/Result/map" || expr.name == "/Result/and_then" || expr.name == "/Result/map2") {
    return true;
  }
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result") {
    return expr.name == "map" || expr.name == "and_then" || expr.name == "map2";
  }
  return false;
}

// Variables a lambda body reads (names only, not calls).
void collectValueNames(const Expr &expr, NameSet &out) {
  if (expr.kind == Expr::Kind::Name) {
    out.insert(expr.name);
    return;
  }
  for (const Expr &arg : expr.args) {
    collectValueNames(arg, out);
  }
  for (const Expr &arg : expr.bodyArguments) {
    collectValueNames(arg, out);
  }
}

// Names a lambda body reads (variables and bare calls), for `[=]`/`[&]`.
void collectUsedNames(const Expr &expr, NameSet &out) {
  if (expr.kind == Expr::Kind::Name) {
    out.insert(expr.name);
    return;
  }
  if (expr.kind == Expr::Kind::Call && !expr.isMethodCall && !expr.isBinding && !expr.isLambda &&
      expr.name.find('/') == std::string::npos) {
    out.insert(expr.name);
  }
  for (const Expr &arg : expr.args) {
    collectUsedNames(arg, out);
  }
  for (const Expr &arg : expr.bodyArguments) {
    collectUsedNames(arg, out);
  }
}

void collectBoundNames(const Expr &expr, NameSet &out) {
  if (expr.isBinding) {
    out.insert(expr.name);
  }
  for (const Expr &arg : expr.args) {
    collectBoundNames(arg, out);
  }
  for (const Expr &arg : expr.bodyArguments) {
    collectBoundNames(arg, out);
  }
}

// --- Function<R, A...> (type-erased closures) -------------------------------------------------------------------
//
// `Function<R, A0, ..., An>` is spelled as the arity-specific struct `/__Function<n+1>`; `Function<R, A...>{x}` becomes
// `/__FunctionN<R, A...>{[tag] 0i32, [env] /__fn_box(x)}`. The function-values pass (after monomorphization) assigns
// each converted closure type its tag and writes the dispatching `call`, `Copy` and `Destroy` bodies.

bool isIdentifierChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

// Rewrites `Function<...>` spellings inside a type text; records the arities it saw.
std::string rewriteFunctionTypeText(const std::string &text, std::set<size_t> &arities) {
  static const std::string Keyword = "Function";
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    const bool atWord = text.compare(i, Keyword.size(), Keyword) == 0 &&
                        (i == 0 || (!isIdentifierChar(text[i - 1]) && text[i - 1] != '/')) &&
                        i + Keyword.size() < text.size() && text[i + Keyword.size()] == '<';
    if (!atWord) {
      out.push_back(text[i]);
      ++i;
      continue;
    }
    size_t depth = 0;
    size_t commas = 0;
    size_t j = i + Keyword.size();
    for (; j < text.size(); ++j) {
      if (text[j] == '<') {
        ++depth;
      } else if (text[j] == '>') {
        if (--depth == 0) {
          break;
        }
      } else if (text[j] == ',' && depth == 1) {
        ++commas;
      }
    }
    arities.insert(commas);
    out += "/__Function" + std::to_string(commas);
    i += Keyword.size();
  }
  return out;
}

void rewriteFunctionTransform(Transform &transform, std::set<size_t> &arities) {
  if (transform.name == "Function" && !transform.templateArgs.empty()) {
    const size_t arity = transform.templateArgs.size() - 1;
    arities.insert(arity);
    transform.name = "/__Function" + std::to_string(arity);
  }
  for (std::string &arg : transform.templateArgs) {
    arg = rewriteFunctionTypeText(arg, arities);
  }
  for (TemplateArgument &detail : transform.templateArgDetails) {
    detail.text = rewriteFunctionTypeText(detail.text, arities);
  }
}

bool rewriteFunctionTypes(Expr &expr, std::set<size_t> &arities, std::string &error) {
  for (Transform &transform : expr.transforms) {
    rewriteFunctionTransform(transform, arities);
  }
  for (std::string &arg : expr.templateArgs) {
    arg = rewriteFunctionTypeText(arg, arities);
  }
  for (TemplateArgument &detail : expr.templateArgDetails) {
    detail.text = rewriteFunctionTypeText(detail.text, arities);
  }
  for (Expr &arg : expr.args) {
    if (!rewriteFunctionTypes(arg, arities, error)) {
      return false;
    }
  }
  for (Expr &arg : expr.bodyArguments) {
    if (!rewriteFunctionTypes(arg, arities, error)) {
      return false;
    }
  }
  for (Expr &init : expr.lambdaInitCaptures) {
    if (!rewriteFunctionTypes(init, arities, error)) {
      return false;
    }
  }
  // `Function<R, A...>{closure}`: box the closure.
  if (expr.kind == Expr::Kind::Call && expr.isBraceConstructor && expr.name == "Function" &&
      !expr.templateArgs.empty() && expr.args.size() == 1 &&
      (expr.argNames.empty() || !expr.argNames.front().has_value())) {
    // `Function<R>{[captures](...) {...}}` parses as a value block holding the lambda.
    const Expr *closurePtr = &expr.args.front();
    if (closurePtr->kind == Expr::Kind::Call && closurePtr->name == "block" && closurePtr->bodyArguments.size() == 1) {
      closurePtr = &closurePtr->bodyArguments.front();
    }
    const Expr &closure = *closurePtr;
    if (closure.isLambda) {
      for (const std::string &capture : closure.lambdaCaptures) {
        if (capture == "&" || capture.rfind("ref ", 0) == 0) {
          error = "closure escapes via Function (root: " +
                  (capture == "&" ? std::string("[&]") : capture.substr(4)) + ")";
          return false;
        }
      }
    }
    const size_t arity = expr.templateArgs.size() - 1;
    arities.insert(arity);
    expr.name = "/__Function" + std::to_string(arity);
    Expr tag;
    tag.kind = Expr::Kind::Literal;
    tag.literalValue = 0;
    tag.intWidth = 32;
    tag.namespacePrefix = expr.namespacePrefix;
    tag.sourceLine = expr.sourceLine;
    tag.sourceColumn = expr.sourceColumn;
    Expr box;
    box.kind = Expr::Kind::Call;
    box.name = "/__fn_box";
    box.sourceName = "Function";
    box.namespacePrefix = expr.namespacePrefix;
    box.sourceLine = expr.sourceLine;
    box.sourceColumn = expr.sourceColumn;
    box.args.push_back(std::move(expr.args.front()));
    box.argNames.push_back(std::nullopt);
    expr.args.clear();
    expr.argNames.clear();
    expr.args.push_back(std::move(tag));
    expr.argNames.push_back(std::string("tag"));
    expr.args.push_back(std::move(box));
    expr.argNames.push_back(std::string("env"));
  }
  return true;
}

bool parseGeneratedDefinitions(const std::string &text, std::vector<Definition> &out, std::string &error) {
  Lexer lexer(text);
  Parser parser(lexer.tokenize(), true);
  Program parsed;
  std::string parseError;
  if (!parser.parse(parsed, parseError)) {
    error = "internal error: generated definitions do not parse: " + parseError;
    return false;
  }
  for (Definition &def : parsed.definitions) {
    out.push_back(std::move(def));
  }
  return true;
}

std::string functionShellText(size_t arity) {
  std::string typeParams = "R";
  std::string params;
  for (size_t i = 0; i < arity; ++i) {
    typeParams += ", A" + std::to_string(i);
    params += (i == 0 ? "" : ", ") + std::string("[A") + std::to_string(i) + "] a" + std::to_string(i);
  }
  const std::string name = "__Function" + std::to_string(arity);
  // The bodies are placeholders: the function-values pass replaces them per instance once the closure types that
  // are converted to it are known.
  return "[struct]\n" + name + "<" + typeParams + "> {\n"
         "  [i32 mut] tag{0i32}\n"
         "  [Pointer<i32> mut] env\n"
         "  [public mut unsafe return<R>]\n"
         "  call(" + params + ") {\n"
         "    [Pointer<i32>] raw{this.env}\n"
         "    return(dereference(/std/intrinsics/memory/reinterpret<R>(raw)))\n"
         "  }\n"
         "  [unsafe effects(heap_alloc)]\n"
         "  Copy([Reference<Self>] other) {\n"
         "    assign(this.tag, other.tag)\n"
         "    assign(this.env, other.env)\n"
         "  }\n"
         "  [unsafe effects(heap_alloc)]\n"
         "  Destroy() {\n"
         "  }\n"
         "}\n";
}

const char *FunctionBoxText =
    "[unsafe effects(heap_alloc) return<Pointer<i32>>]\n"
    "__fn_box<C>([C] closure) {\n"
    "  [Pointer<uninitialized<C>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<C>>(1i32)}\n"
    "  init(dereference(slot), closure)\n"
    "  return(/std/intrinsics/memory/reinterpret<i32>(slot))\n"
    "}\n";

class LambdaClosureRewriter {
public:
  LambdaClosureRewriter(Program &program, std::string &error) : program_(program), error_(error) {
    for (const Definition &def : program.definitions) {
      for (const Transform &transform : def.transforms) {
        if (transform.name == "sum") {
          break;
        }
        if (isStructTransformName(transform.name)) {
          structPaths_.insert(def.fullPath);
          break;
        }
      }
      if (def.fullPath.find(LambdaClosureStructPrefix) != std::string::npos) {
        ++counter_;
      }
    }
  }

  bool rewriteFunctionValues() {
    for (const Definition &def : program_.definitions) {
      if (def.fullPath == "/Function" || def.fullPath.rfind("/__Function", 0) == 0) {
        return true;  // a user type named Function, or already rewritten
      }
    }
    std::set<size_t> arities;
    for (Definition &def : program_.definitions) {
      for (Transform &transform : def.transforms) {
        rewriteFunctionTransform(transform, arities);
      }
      for (Expr &param : def.parameters) {
        if (!rewriteFunctionTypes(param, arities, error_)) {
          return false;
        }
      }
      for (Expr &stmt : def.statements) {
        if (!rewriteFunctionTypes(stmt, arities, error_)) {
          return false;
        }
      }
      if (def.returnExpr.has_value() && !rewriteFunctionTypes(*def.returnExpr, arities, error_)) {
        return false;
      }
    }
    for (Execution &exec : program_.executions) {
      for (Expr &arg : exec.arguments) {
        if (!rewriteFunctionTypes(arg, arities, error_)) {
          return false;
        }
      }
      for (Expr &arg : exec.bodyArguments) {
        if (!rewriteFunctionTypes(arg, arities, error_)) {
          return false;
        }
      }
    }
    if (arities.empty()) {
      return true;
    }
    std::string text = FunctionBoxText;
    for (size_t arity : arities) {
      text += functionShellText(arity);
    }
    return parseGeneratedDefinitions(text, generated_, error_);
  }

  bool run() {
    if (!rewriteFunctionValues()) {
      return false;
    }
    const size_t originalCount = program_.definitions.size();
    for (size_t index = 0; index < originalCount; ++index) {
      // Copy out: generated definitions are appended to program_.definitions while this one is rewritten.
      Definition def = std::move(program_.definitions[index]);
      const bool ok = rewriteDefinition(def);
      program_.definitions[index] = std::move(def);
      if (!ok) {
        return false;
      }
    }
    for (Definition &generated : generated_) {
      program_.definitions.push_back(std::move(generated));
    }
    return true;
  }

private:
  static bool hasMutTransform(const Expr &binding) {
    for (const Transform &transform : binding.transforms) {
      if (transform.name == "mut") {
        return true;
      }
    }
    return false;
  }

  void collectMutableNames(const Expr &expr) {
    if (expr.isBinding && hasMutTransform(expr)) {
      mutableNames_.insert(expr.name);
    }
    if (expr.isBinding && !expr.transforms.empty() &&
        (expr.transforms.front().name == "Reference" || expr.transforms.front().name == "Pointer")) {
      referenceNames_.insert(expr.name);
    }
    for (const Expr &arg : expr.args) {
      collectMutableNames(arg);
    }
    for (const Expr &arg : expr.bodyArguments) {
      collectMutableNames(arg);
    }
  }

  bool rewriteDefinition(Definition &def) {
    currentDef_ = &def;
    mutableNames_.clear();
    referenceNames_.clear();
    for (const Expr &param : def.parameters) {
      collectMutableNames(param);
    }
    for (const Expr &stmt : def.statements) {
      collectMutableNames(stmt);
    }
    closureNamespace_ = def.namespacePrefix;
    // Closures of a struct method live next to the struct, never inside it (a path under a struct is a member).
    while (!closureNamespace_.empty() && structPaths_.count(closureNamespace_) > 0) {
      const size_t slash = closureNamespace_.rfind('/');
      closureNamespace_ = slash == std::string::npos ? std::string() : closureNamespace_.substr(0, slash);
    }
    NameSet visible;
    for (const Expr &param : def.parameters) {
      if (param.isBinding) {
        visible.insert(param.name);
      }
    }
    for (Expr &param : def.parameters) {
      if (!rewriteExpr(param, visible, false)) {
        return false;
      }
    }
    if (!rewriteList(def.statements, visible)) {
      return false;
    }
    if (def.returnExpr.has_value()) {
      // The parser keeps a copy of the last return argument; refresh it from the rewritten statement.
      for (auto it = def.statements.rbegin(); it != def.statements.rend(); ++it) {
        if (it->kind == Expr::Kind::Call && it->name == "return" && !it->isMethodCall) {
          if (!it->args.empty()) {
            def.returnExpr = it->args.front();
          }
          break;
        }
      }
    }
    return true;
  }

  bool rewriteList(std::vector<Expr> &list, const NameSet &visibleIn) {
    NameSet visible = visibleIn;
    for (Expr &expr : list) {
      if (!rewriteExpr(expr, visible, false)) {
        return false;
      }
      if (expr.isBinding) {
        visible.insert(expr.name);
      }
    }
    return true;
  }

  bool rewriteExpr(Expr &expr, NameSet &visible, bool skipLambda) {
    if (expr.isLambda) {
      NameSet inner = visible;
      for (const Expr &param : expr.args) {
        inner.insert(param.name);
      }
      for (Expr &init : expr.lambdaInitCaptures) {
        for (Expr &arg : init.args) {
          if (!rewriteExpr(arg, visible, false)) {
            return false;
          }
        }
        inner.insert(init.name);
      }
      if (!rewriteList(expr.bodyArguments, inner)) {
        return false;
      }
      if (skipLambda) {
        // The helpers inline a plain lambda, which keeps their Result payload inference; one with an early `return`
        // or a capture that needs closure storage (`mut`, `move`, `ref`, init) goes through a closure.
        if (!resultHelperLambdaNeedsClosure(expr)) {
          return true;
        }
        return desugarResultHelperLambda(expr, visible);
      }
      return desugarLambda(expr, visible);
    }
    const bool resultHelper = isResultHelperCall(expr);
    NameSet scoped = visible;
    for (Expr &arg : expr.args) {
      if (!rewriteExpr(arg, scoped, resultHelper)) {
        return false;
      }
      if (arg.isBinding) {
        scoped.insert(arg.name);
      }
    }
    return rewriteList(expr.bodyArguments, scoped);
  }

  bool fail(const Expr &at, const std::string &message) {
    error_ = message;
    if (at.sourceLine > 0) {
      error_ += " (line " + std::to_string(at.sourceLine) + ")";
    }
    return false;
  }

  bool collectCaptures(const Expr &lambda, const NameSet &visible, std::vector<Capture> &captures) {
    NameSet params;
    for (const Expr &param : lambda.args) {
      params.insert(param.name);
    }
    NameSet explicitNames;
    std::optional<CaptureKind> captureAll;
    for (const std::string &text : lambda.lambdaCaptures) {
      std::vector<std::string> tokens = splitCaptureTokens(text);
      if (tokens.empty()) {
        return fail(lambda, "invalid lambda capture");
      }
      if (tokens.size() == 1 && (tokens.front() == "=" || tokens.front() == "&")) {
        if (captureAll.has_value()) {
          return fail(lambda, "invalid lambda capture");
        }
        captureAll = tokens.front() == "=" ? CaptureKind::Value : CaptureKind::Ref;
        continue;
      }
      Capture capture;
      capture.name = tokens.back();
      bool sawKind = false;
      for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        const std::string &qualifier = tokens[i];
        if (qualifier == "mut") {
          if (capture.isMutable) {
            return fail(lambda, "invalid lambda capture");
          }
          capture.isMutable = true;
          continue;
        }
        if (sawKind) {
          return fail(lambda, "invalid lambda capture");
        }
        sawKind = true;
        if (qualifier == "value") {
          capture.kind = CaptureKind::Value;
        } else if (qualifier == "move") {
          capture.kind = CaptureKind::Move;
        } else if (qualifier == "ref") {
          capture.kind = CaptureKind::Ref;
        } else {
          return fail(lambda, "invalid lambda capture");
        }
      }
      if (capture.name == "=" || capture.name == "&") {
        return fail(lambda, "invalid lambda capture");
      }
      if (capture.kind == CaptureKind::Ref && capture.isMutable) {
        return fail(lambda, "ref capture cannot be mut; capture a mut binding by ref to write it: " + capture.name);
      }
      if (!explicitNames.insert(capture.name).second) {
        return fail(lambda, "duplicate lambda capture: " + capture.name);
      }
      if (visible.count(capture.name) == 0) {
        return fail(lambda, "unknown capture: " + capture.name);
      }
      if (params.count(capture.name) > 0) {
        return fail(lambda, "duplicate binding name: " + capture.name);
      }
      captures.push_back(std::move(capture));
    }
    for (const Expr &init : lambda.lambdaInitCaptures) {
      if (!explicitNames.insert(init.name).second) {
        return fail(lambda, "duplicate lambda capture: " + init.name);
      }
      if (params.count(init.name) > 0) {
        return fail(lambda, "duplicate binding name: " + init.name);
      }
      Capture capture;
      capture.name = init.name;
      capture.kind = CaptureKind::Init;
      for (const Transform &transform : init.transforms) {
        if (transform.name == "mut") {
          capture.isMutable = true;
        }
      }
      capture.initializer = init.args.front();
      captures.push_back(std::move(capture));
    }
    if (!captureAll.has_value()) {
      // A body that reads an enclosing binding it did not capture: say so here, where the lambda is written.
      NameSet read;
      NameSet bound;
      for (const Expr &stmt : lambda.bodyArguments) {
        collectValueNames(stmt, read);
        collectBoundNames(stmt, bound);
      }
      for (const std::string &name : read) {
        if (visible.count(name) > 0 && params.count(name) == 0 && bound.count(name) == 0 &&
            explicitNames.count(name) == 0 && name != "this") {
          return fail(lambda, "unknown identifier: " + name + " (the lambda does not capture it; add [" + name + "])");
        }
      }
    }
    if (captureAll.has_value()) {
      NameSet used;
      NameSet bound;
      for (const Expr &stmt : lambda.bodyArguments) {
        collectUsedNames(stmt, used);
        collectBoundNames(stmt, bound);
      }
      for (const std::string &name : used) {
        if (visible.count(name) == 0 || params.count(name) > 0 || bound.count(name) > 0 ||
            explicitNames.count(name) > 0 || name == "this") {
          continue;
        }
        Capture capture;
        capture.name = name;
        capture.kind = *captureAll;
        captures.push_back(std::move(capture));
      }
    }
    return true;
  }

  // A `[ref name]` capture is read through a `Reference` local of the same name that `call` binds from its field
  // first, so the body keeps using the plain name.
  bool renameCaptures(Expr &expr, const NameSet &captured) {
    if (expr.kind == Expr::Kind::Name) {
      if (captured.count(expr.name) > 0 && refCaptures_.count(expr.name) == 0) {
        expr = thisFieldAccess(expr.name, expr);
      }
      return true;
    }
    if (expr.isBinding && captured.count(expr.name) > 0) {
      return fail(expr, "lambda binding shadows a capture: " + expr.name);
    }
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall && !expr.isBinding && captured.count(expr.name) > 0 &&
        refCaptures_.count(expr.name) == 0) {
      // Calling a captured callable: `f(x)` becomes `this.f(x)`, which resolves to the field's `call`.
      expr.isMethodCall = true;
      expr.args.insert(expr.args.begin(), nameExpr("this", expr));
      expr.argNames.insert(expr.argNames.begin(), std::nullopt);
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (!renameCaptures(expr.args[i], captured)) {
          return false;
        }
      }
      for (Expr &arg : expr.bodyArguments) {
        if (!renameCaptures(arg, captured)) {
          return false;
        }
      }
      return true;
    }
    for (Expr &arg : expr.args) {
      if (!renameCaptures(arg, captured)) {
        return false;
      }
    }
    for (Expr &arg : expr.bodyArguments) {
      if (!renameCaptures(arg, captured)) {
        return false;
      }
    }
    return true;
  }

  static bool containsReturn(const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && expr.name == "return" && !expr.isMethodCall) {
      return true;
    }
    for (const Expr &arg : expr.args) {
      if (containsReturn(arg)) {
        return true;
      }
    }
    for (const Expr &arg : expr.bodyArguments) {
      if (containsReturn(arg)) {
        return true;
      }
    }
    return false;
  }

  static bool resultHelperLambdaNeedsClosure(const Expr &lambda) {
    if (!lambda.lambdaInitCaptures.empty()) {
      return true;
    }
    for (const std::string &capture : lambda.lambdaCaptures) {
      const std::vector<std::string> tokens = splitCaptureTokens(capture);
      for (const std::string &token : tokens) {
        if (token == "mut" || token == "move" || token == "ref" || token == "&") {
          return true;
        }
      }
    }
    // A `return` that is not in tail position leaves the body early.
    return !returnsOnlyInTail(lambda.bodyArguments);
  }

  // Whether every `return` in `block` is its last statement, or sits in tail position of a final `if`
  // (`if(c) { return(a) } else { return(b) }`), the shapes the inline Result-helper lowering handles.
  static bool returnsOnlyInTail(const std::vector<Expr> &block) {
    for (size_t i = 0; i < block.size(); ++i) {
      const Expr &stmt = block[i];
      const bool last = i + 1 == block.size();
      if (!last) {
        if (containsReturn(stmt)) {
          return false;
        }
        continue;
      }
      if (stmt.kind == Expr::Kind::Call && stmt.name == "return" && !stmt.isMethodCall) {
        for (const Expr &arg : stmt.args) {
          if (containsReturn(arg)) {
            return false;
          }
        }
        continue;
      }
      if (stmt.kind == Expr::Kind::Call && stmt.name == "if" && !stmt.isMethodCall) {
        for (size_t k = 0; k < stmt.args.size(); ++k) {
          const Expr &arg = stmt.args[k];
          if (k == 0) {
            if (containsReturn(arg)) {
              return false;
            }
            continue;
          }
          if (!arg.bodyArguments.empty() || arg.hasBodyArguments) {
            if (!returnsOnlyInTail(arg.bodyArguments)) {
              return false;
            }
          } else if (containsReturn(arg)) {
            return false;
          }
        }
        continue;
      }
      if (containsReturn(stmt)) {
        return false;
      }
    }
    return true;
  }

  // `Result.map`/`and_then`/`map2` inline their lambda argument. The user lambda becomes a closure like any other, and
  // the helper gets a one-statement trampoline `[names](params) { return(<closure>.call(params)) }`: the body's own
  // `return`s stay inside `call`, and by-value captures are real copies.
  bool desugarResultHelperLambda(Expr &lambda, const NameSet &visible) {
    std::vector<Expr> params = lambda.args;
    const std::string namespacePrefix = lambda.namespacePrefix;
    const Expr origin = nameExpr("lambda", lambda);
    if (!desugarLambda(lambda, visible)) {
      return false;
    }
    Expr factoryCall = std::move(lambda);
    NameSet captured;
    collectUsedNames(factoryCall, captured);
    Expr invoke;
    invoke.kind = Expr::Kind::Call;
    invoke.name = "call";
    invoke.sourceName = "call";
    invoke.namespacePrefix = namespacePrefix;
    invoke.isMethodCall = true;
    invoke.sourceLine = origin.sourceLine;
    invoke.sourceColumn = origin.sourceColumn;
    invoke.args.push_back(std::move(factoryCall));
    invoke.argNames.push_back(std::nullopt);
    for (const Expr &param : params) {
      invoke.args.push_back(nameExpr(param.name, origin));
      invoke.argNames.push_back(std::nullopt);
    }
    Expr returnCall;
    returnCall.kind = Expr::Kind::Call;
    returnCall.name = "return";
    returnCall.sourceName = "return";
    returnCall.namespacePrefix = namespacePrefix;
    returnCall.sourceLine = origin.sourceLine;
    returnCall.sourceColumn = origin.sourceColumn;
    returnCall.args.push_back(std::move(invoke));
    returnCall.argNames.push_back(std::nullopt);

    Expr trampoline;
    trampoline.kind = Expr::Kind::Call;
    trampoline.name = "lambda";
    trampoline.namespacePrefix = namespacePrefix;
    trampoline.sourceLine = origin.sourceLine;
    trampoline.sourceColumn = origin.sourceColumn;
    trampoline.isLambda = true;
    trampoline.hasBodyArguments = true;
    for (const std::string &name : captured) {
      if (visible.count(name) > 0) {
        trampoline.lambdaCaptures.push_back(name);
      }
    }
    for (Expr &param : params) {
      param.args.clear();
      param.argNames.clear();
    }
    trampoline.args = std::move(params);
    trampoline.argNames.resize(trampoline.args.size());
    trampoline.bodyArguments.push_back(std::move(returnCall));
    lambda = std::move(trampoline);
    return true;
  }

  bool desugarLambda(Expr &lambda, const NameSet &visible) {
    if (lambda.sourceLine <= 0) {
      // A lambda written as a statement has no position of its own: diagnostics in its body point at the body.
      for (const Expr &stmt : lambda.bodyArguments) {
        if (stmt.sourceLine > 0) {
          lambda.sourceLine = stmt.sourceLine;
          lambda.sourceColumn = stmt.sourceColumn;
          break;
        }
      }
      if (lambda.sourceLine <= 0 && currentDef_ != nullptr) {
        lambda.sourceLine = currentDef_->sourceLine;
        lambda.sourceColumn = currentDef_->sourceColumn;
      }
    }
    std::vector<Capture> captures;
    if (!collectCaptures(lambda, visible, captures)) {
      return false;
    }
    for (Capture &capture : captures) {
      // A binding that already is a Reference or Pointer is captured as that reference (copied).
      if (capture.kind == CaptureKind::Ref && referenceNames_.count(capture.name) > 0) {
        capture.kind = CaptureKind::Value;
        continue;
      }
      // A reference capture holds a pointer to the binding; it can write through it when the binding is mut.
      if (capture.kind == CaptureKind::Ref && mutableNames_.count(capture.name) > 0) {
        capture.isMutable = true;
      }
    }

    // Named after the enclosing definition (`/main__lambda0`), so diagnostics in a lambda body point at its owner.
    const std::string index = std::to_string(counter_++);
    std::string ownerLeaf = currentDef_->name;
    for (char &ch : ownerLeaf) {
      if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
        ch = '_';
      }
    }
    const std::string structName = ownerLeaf + LambdaClosureStructPrefix + index;
    const std::string structPath = closureNamespace_ + "/" + structName;
    const std::string factoryName = structName + "_make";
    const std::string factoryPath = closureNamespace_ + "/" + factoryName;

    std::vector<std::string> typeParams;
    typeParams.reserve(captures.size());
    for (size_t i = 0; i < captures.size(); ++i) {
      typeParams.push_back("C" + std::to_string(i));
    }
    std::string structType = structPath;
    if (!typeParams.empty()) {
      structType += "<";
      for (size_t i = 0; i < typeParams.size(); ++i) {
        structType += (i == 0 ? "" : ", ") + typeParams[i];
      }
      structType += ">";
    }

    // Writing through a `[ref name]` capture does not change the closure, so only value captures make `call` mut.
    bool mutableCall = false;
    for (const Capture &capture : captures) {
      mutableCall = mutableCall || (capture.isMutable && capture.kind != CaptureKind::Ref);
    }

    // The `call` method: the lambda body with captured names read through `this`.
    Definition method;
    method.name = "call";
    method.namespacePrefix = structPath;
    method.fullPath = structPath + "/call";
    method.isNested = true;
    method.sourceLine = lambda.sourceLine;
    method.sourceColumn = lambda.sourceColumn;
    if (mutableCall) {
      method.transforms.push_back(namedTransform("mut"));
    }
    for (const Transform &transform : currentDef_->transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        method.transforms.push_back(transform);
      }
    }
    method.templateArgs = lambda.templateArgs;
    method.parameters = lambda.args;
    NameSet capturedNames;
    refCaptures_.clear();
    for (const Capture &capture : captures) {
      capturedNames.insert(capture.name);
      if (capture.kind == CaptureKind::Ref) {
        refCaptures_.insert(capture.name);
      }
    }
    method.statements = std::move(lambda.bodyArguments);
    for (Expr &stmt : method.statements) {
      if (!renameCaptures(stmt, capturedNames)) {
        return false;
      }
    }
    {
      // `[Reference<Ci> mut] name{this.name}` for each `[ref name]` capture.
      std::vector<Expr> prologue;
      for (size_t i = 0; i < captures.size(); ++i) {
        if (captures[i].kind != CaptureKind::Ref) {
          continue;
        }
        Expr local;
        local.kind = Expr::Kind::Call;
        local.isBinding = true;
        local.name = captures[i].name;
        local.namespacePrefix = lambda.namespacePrefix;
        local.sourceLine = lambda.sourceLine;
        local.sourceColumn = lambda.sourceColumn;
        Transform reference = namedTransform("Reference");
        reference.templateArgs.push_back("C" + std::to_string(i));
        local.transforms.push_back(std::move(reference));
        if (captures[i].isMutable) {
          local.transforms.push_back(namedTransform("mut"));
        }
        local.args.push_back(thisFieldAccess(captures[i].name, lambda));
        local.argNames.push_back(std::nullopt);
        prologue.push_back(std::move(local));
      }
      method.statements.insert(method.statements.begin(), std::make_move_iterator(prologue.begin()),
                               std::make_move_iterator(prologue.end()));
    }
    for (Expr &stmt : method.statements) {
      if (stmt.kind == Expr::Kind::Call && stmt.name == "return" && !stmt.isMethodCall) {
        method.hasReturnStatement = true;
        if (!stmt.args.empty()) {
          method.returnExpr = stmt.args.front();
        }
      }
    }

    // The closure struct: one field per capture, typed by a template parameter the factory call infers.
    Definition closure;
    closure.name = structName;
    closure.namespacePrefix = closureNamespace_;
    closure.fullPath = structPath;
    closure.sourceLine = lambda.sourceLine;
    closure.sourceColumn = lambda.sourceColumn;
    closure.transforms.push_back(namedTransform("struct"));
    closure.templateArgs = typeParams;
    closure.templateArgIsPack.assign(typeParams.size(), false);
    for (size_t i = 0; i < captures.size(); ++i) {
      Expr field;
      field.kind = Expr::Kind::Call;
      field.isBinding = true;
      field.name = captures[i].name;
      field.namespacePrefix = structPath;
      field.sourceLine = lambda.sourceLine;
      field.sourceColumn = lambda.sourceColumn;
      if (captures[i].kind == CaptureKind::Ref) {
        Transform reference = namedTransform("Reference");
        reference.templateArgs.push_back(typeParams[i]);
        field.transforms.push_back(std::move(reference));
      } else {
        field.transforms.push_back(namedTransform(typeParams[i]));
      }
      if (captures[i].isMutable) {
        field.transforms.push_back(namedTransform("mut"));
      }
      closure.statements.push_back(std::move(field));
    }

    // The factory: `__lambdaN_make<C0, ...>([C0] a, ...) { return(__lambdaN<C0, ...>{[a] a, ...}) }`.
    Definition factory;
    factory.name = factoryName;
    factory.namespacePrefix = closureNamespace_;
    factory.fullPath = factoryPath;
    factory.sourceLine = lambda.sourceLine;
    factory.sourceColumn = lambda.sourceColumn;
    factory.templateArgs = typeParams;
    factory.templateArgIsPack.assign(typeParams.size(), false);
    Transform returnTransform = namedTransform("return");
    returnTransform.templateArgs.push_back(structType);
    factory.transforms.push_back(std::move(returnTransform));
    for (const Transform &transform : currentDef_->transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        factory.transforms.push_back(transform);
      }
    }
    Expr construct;
    construct.kind = Expr::Kind::Call;
    construct.name = structPath;
    construct.namespacePrefix = closureNamespace_;
    construct.isBraceConstructor = true;
    construct.templateArgs = typeParams;
    construct.sourceLine = lambda.sourceLine;
    construct.sourceColumn = lambda.sourceColumn;
    for (size_t i = 0; i < captures.size(); ++i) {
      Expr param;
      param.kind = Expr::Kind::Call;
      param.isBinding = true;
      param.name = captures[i].name;
      param.namespacePrefix = factoryPath;
      param.sourceLine = lambda.sourceLine;
      param.sourceColumn = lambda.sourceColumn;
      param.transforms.push_back(namedTransform(typeParams[i]));
      factory.parameters.push_back(std::move(param));
      if (captures[i].kind == CaptureKind::Move) {
        Expr moved;
        moved.kind = Expr::Kind::Call;
        moved.name = "move";
        moved.sourceName = "move";
        moved.namespacePrefix = closureNamespace_;
        moved.sourceLine = lambda.sourceLine;
        moved.sourceColumn = lambda.sourceColumn;
        moved.args.push_back(nameExpr(captures[i].name, lambda));
        moved.argNames.push_back(std::nullopt);
        construct.args.push_back(std::move(moved));
      } else {
        construct.args.push_back(nameExpr(captures[i].name, lambda));
      }
      construct.argNames.push_back(captures[i].name);
    }
    Expr returnCall;
    returnCall.kind = Expr::Kind::Call;
    returnCall.name = "return";
    returnCall.sourceName = "return";
    returnCall.namespacePrefix = closureNamespace_;
    returnCall.sourceLine = lambda.sourceLine;
    returnCall.sourceColumn = lambda.sourceColumn;
    returnCall.args.push_back(construct);
    returnCall.argNames.push_back(std::nullopt);
    factory.statements.push_back(std::move(returnCall));
    factory.hasReturnStatement = true;
    factory.returnExpr = std::move(construct);

    // The lambda expression becomes the factory call.
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = factoryPath;
    call.sourceName = "lambda";
    call.namespacePrefix = lambda.namespacePrefix;
    call.sourceLine = lambda.sourceLine;
    call.sourceColumn = lambda.sourceColumn;
    for (Capture &capture : captures) {
      Expr arg;
      if (capture.kind == CaptureKind::Init) {
        arg = std::move(*capture.initializer);
      } else if (capture.kind == CaptureKind::Ref) {
        arg.kind = Expr::Kind::Call;
        arg.name = "location";
        arg.sourceName = "location";
        arg.namespacePrefix = lambda.namespacePrefix;
        arg.sourceLine = lambda.sourceLine;
        arg.sourceColumn = lambda.sourceColumn;
        arg.args.push_back(nameExpr(capture.name, lambda));
        arg.argNames.push_back(std::nullopt);
      } else if (capture.kind == CaptureKind::Move) {
        arg.kind = Expr::Kind::Call;
        arg.name = "move";
        arg.sourceName = "move";
        arg.namespacePrefix = lambda.namespacePrefix;
        arg.sourceLine = lambda.sourceLine;
        arg.sourceColumn = lambda.sourceColumn;
        arg.args.push_back(nameExpr(capture.name, lambda));
        arg.argNames.push_back(std::nullopt);
      } else {
        arg = nameExpr(capture.name, lambda);
      }
      call.args.push_back(std::move(arg));
      call.argNames.push_back(std::nullopt);
    }

    bool hasRefCapture = false;
    for (const Capture &capture : captures) {
      hasRefCapture = hasRefCapture || capture.kind == CaptureKind::Ref;
    }
    generated_.push_back(std::move(method));
    generated_.push_back(std::move(closure));
    if (!hasRefCapture) {
      generated_.push_back(std::move(factory));
      lambda = std::move(call);
      return true;
    }
    // A closure with a `[ref name]` capture is built in place: `Reference` fields are initialized from
    // `location(name)` where the binding lives (a reference does not pass through a factory parameter). The
    // monomorphizer infers the struct's template arguments from the field arguments.
    Expr inPlace;
    inPlace.kind = Expr::Kind::Call;
    inPlace.name = structPath;
    inPlace.sourceName = "lambda";
    inPlace.namespacePrefix = lambda.namespacePrefix;
    inPlace.isBraceConstructor = true;
    inPlace.sourceLine = lambda.sourceLine;
    inPlace.sourceColumn = lambda.sourceColumn;
    for (size_t i = 0; i < captures.size(); ++i) {
      inPlace.args.push_back(std::move(call.args[i]));
      inPlace.argNames.push_back(captures[i].name);
    }
    lambda = std::move(inPlace);
    return true;
  }

  Program &program_;
  std::string &error_;
  std::unordered_set<std::string> structPaths_;
  std::vector<Definition> generated_;
  const Definition *currentDef_ = nullptr;
  std::string closureNamespace_;
  NameSet mutableNames_;
  NameSet referenceNames_;
  NameSet refCaptures_;
  size_t counter_ = 0;
};

} // namespace

bool rewriteLambdaClosures(Program &program, std::string &error) {
  LambdaClosureRewriter rewriter(program, error);
  return rewriter.run();
}

} // namespace primec::semantics
