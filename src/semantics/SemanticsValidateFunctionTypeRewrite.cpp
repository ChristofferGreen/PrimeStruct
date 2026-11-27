#include "SemanticsValidateFunctionTypeRewrite.h"

#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"

#include <cctype>
#include <optional>
#include <string>
#include <utility>

namespace primec::semantics {

namespace {

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

} // namespace

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
    if (closurePtr->kind == Expr::Kind::Call && closurePtr->name == "block" &&
        closurePtr->bodyArguments.size() == 1) {
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
    fillFunctionConstruction(expr, std::move(expr.args.front()));
  }
  return true;
}

void fillFunctionConstruction(Expr &construct, Expr closure) {
  const auto placeholder = [&]() {
    Expr zero;
    zero.kind = Expr::Kind::Literal;
    zero.literalValue = 0;
    zero.intWidth = 64;
    zero.isUnsigned = true;
    zero.namespacePrefix = construct.namespacePrefix;
    zero.sourceLine = construct.sourceLine;
    zero.sourceColumn = construct.sourceColumn;
    return zero;
  };
  Expr box;
  box.kind = Expr::Kind::Call;
  box.name = "/__fn_box";
  box.sourceName = "Function";
  box.namespacePrefix = construct.namespacePrefix;
  box.sourceLine = construct.sourceLine;
  box.sourceColumn = construct.sourceColumn;
  box.args.push_back(std::move(closure));
  box.argNames.push_back(std::nullopt);
  construct.args.clear();
  construct.argNames.clear();
  for (const char *label : {"invoke", "clone", "release"}) {
    construct.args.push_back(placeholder());
    construct.argNames.push_back(std::string(label));
  }
  construct.args.push_back(std::move(box));
  construct.argNames.push_back(std::string("env"));
}

bool parseGeneratedDefinitions(const std::string &text,
                               std::vector<Definition> &out,
                               std::string &error) {
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
    params +=
        (i == 0 ? "" : ", ") + std::string("[A") + std::to_string(i) + "] a" + std::to_string(i);
  }
  const std::string name = "__Function" + std::to_string(arity);
  // A `Function` holds the addresses of the routines of its closure and the closure's heap block (docs/spec/
  // value-lifecycle.md, Function values). `Copy` and `Destroy` are the same text for every instance: they reach the
  // routines through their addresses. The `call` is a placeholder: the function-values pass writes it per instance,
  // as a call through the address of the invoke routine.
  return "[struct]\n" + name +
         "<" + typeParams +
         "> {\n"
         "  [u64 mut] invoke{0u64}\n"
         "  [u64 mut] clone{0u64}\n"
         "  [u64 mut] release{0u64}\n"
         "  [Pointer<i32> mut] env\n"
         "  [public mut unsafe return<R>]\n"
         "  call(" +
         params +
         ") {\n"
         "    [Pointer<i32>] raw{this.env}\n"
         "    return(dereference(/std/intrinsics/memory/reinterpret<R>(raw)))\n"
         "  }\n"
         "  [unsafe effects(heap_alloc)]\n"
         "  Copy([Reference<Self>] other) {\n"
         "    assign(this.invoke, other.invoke)\n"
         "    assign(this.clone, other.clone)\n"
         "    assign(this.release, other.release)\n"
         "    assign(this.env, other.env)\n"
         "    if(not_equal(other.clone, 0u64)) {\n"
         "      assign(this.env, /__fn_clone_call(other.clone, other.env))\n"
         "    }\n"
         "  }\n"
         "  [unsafe effects(heap_alloc)]\n"
         "  Destroy() {\n"
         "    if(not_equal(this.release, 0u64)) {\n"
         "      /__fn_release_call(this.release, this.env)\n"
         "    }\n"
         "  }\n"
         "}\n";
}

// The box takes its own copy of the closure (a `copy` parameter) and hands it to the heap block, so the block owns
// every capture: a borrowed closure would leave the caller and the block both destroying a `String` capture.
const char *functionBoxText() {
  return "[unsafe effects(heap_alloc) return<Pointer<i32>>]\n"
         "__fn_box<C>([C copy] closure) {\n"
         "  [Pointer<uninitialized<C>> mut] "
         "slot{/std/intrinsics/memory/alloc<uninitialized<C>>(1i32)}\n"
         "  init(dereference(slot), closure)\n"
         "  return(/std/intrinsics/memory/reinterpret<i32>(slot))\n"
         "}\n";
}

} // namespace primec::semantics
