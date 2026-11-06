#include "SemanticsValidateFunctionValues.h"

#include "SemanticsHelpers.h"
#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec::semantics {

namespace {

std::string leafOf(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool isFunctionInstancePath(const std::string &path) {
  return leafOf(path).rfind("__Function", 0) == 0;
}

bool isFunctionBoxPath(const std::string &path) {
  return leafOf(path).rfind("__fn_box", 0) == 0;
}

bool isQualifierTransform(const std::string &name) {
  return name == "mut" || name == "public" || name == "private" || name == "static" || name == "copy" ||
         name == "move" || name == "restrict" || name == "unsafe";
}

std::string transformTypeText(const Transform &transform) {
  std::string text = transform.name;
  if (!transform.templateArgs.empty()) {
    text += "<";
    for (size_t i = 0; i < transform.templateArgs.size(); ++i) {
      text += (i == 0 ? "" : ", ") + transform.templateArgs[i];
    }
    text += ">";
  }
  return text;
}

std::string bindingTypeText(const Expr &binding) {
  for (const Transform &transform : binding.transforms) {
    if (!isQualifierTransform(transform.name)) {
      return transformTypeText(transform);
    }
  }
  return {};
}

const std::string &calleePath(const Expr &expr) {
  return expr.resolvedCallPath.empty() ? expr.name : expr.resolvedCallPath;
}

struct FunctionValueRewriter {
  Program &program;
  std::string &error;
  std::unordered_map<std::string, size_t> definitionIndex;
  // Function instance path -> converted closure types, in order of first conversion (tag = position + 1).
  std::map<std::string, std::vector<std::string>> variants;

  const Definition *find(const std::string &path) const {
    const auto it = definitionIndex.find(path);
    return it == definitionIndex.end() ? nullptr : &program.definitions[it->second];
  }

  // The Function instance a brace constructor builds: its name after monomorphization, or the one instance whose
  // base matches when the name still carries its template arguments.
  std::string functionInstanceOf(const Expr &expr) const {
    const std::string &path = calleePath(expr);
    if (find(path) != nullptr) {
      return path;
    }
    return {};
  }

  void collect(Expr &expr) {
    for (Expr &arg : expr.args) {
      collect(arg);
    }
    for (Expr &arg : expr.bodyArguments) {
      collect(arg);
    }
    if (expr.kind != Expr::Kind::Call || !expr.isBraceConstructor || !isFunctionInstancePath(expr.name)) {
      return;
    }
    const std::string functionPath = functionInstanceOf(expr);
    if (functionPath.empty()) {
      return;
    }
    Expr *tag = nullptr;
    const Expr *env = nullptr;
    for (size_t i = 0; i < expr.args.size(); ++i) {
      const std::optional<std::string> &label = i < expr.argNames.size() ? expr.argNames[i] : std::nullopt;
      if (label.has_value() && *label == "tag") {
        tag = &expr.args[i];
      } else if (label.has_value() && *label == "env") {
        env = &expr.args[i];
      }
    }
    if (tag == nullptr || env == nullptr || tag->kind != Expr::Kind::Literal || env->kind != Expr::Kind::Call ||
        !isFunctionBoxPath(calleePath(*env))) {
      return;
    }
    const Definition *box = find(calleePath(*env));
    if (box == nullptr || box->parameters.empty()) {
      return;
    }
    const std::string closureType = bindingTypeText(box->parameters.front());
    if (closureType.empty()) {
      return;
    }
    std::vector<std::string> &known = variants[functionPath];
    size_t index = 0;
    while (index < known.size() && known[index] != closureType) {
      ++index;
    }
    if (index == known.size()) {
      known.push_back(closureType);
    }
    tag->literalValue = index + 1;
  }

  bool parseBody(const std::string &body, std::vector<Expr> &statements) {
    // The lexer keeps a view of its source: keep the text alive while parsing.
    const std::string source = "__function_value_body() {\n" + body + "}\n";
    Lexer lexer(source);
    Parser parser(lexer.tokenize(), true);
    Program parsed;
    std::string parseError;
    if (!parser.parse(parsed, parseError) || parsed.definitions.empty()) {
      error = "internal error: generated Function body does not parse: " + parseError;
      return false;
    }
    statements = std::move(parsed.definitions.back().statements);
    return true;
  }

  static void refreshReturn(Definition &def) {
    def.hasReturnStatement = false;
    def.returnExpr.reset();
    for (const Expr &stmt : def.statements) {
      if (stmt.kind == Expr::Kind::Call && stmt.name == "return" && !stmt.isMethodCall) {
        def.hasReturnStatement = true;
        if (!stmt.args.empty()) {
          def.returnExpr = stmt.args.front();
        }
      }
    }
  }

  static std::string returnTypeOf(const Definition &def) {
    for (const Transform &transform : def.transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        return transform.templateArgs.front();
      }
    }
    return "void";
  }

  bool completeInstance(const std::string &functionPath, const std::vector<std::string> &closures) {
    auto callIt = definitionIndex.find(functionPath + "/call");
    auto copyIt = definitionIndex.find(functionPath + "/Copy");
    auto destroyIt = definitionIndex.find(functionPath + "/Destroy");
    if (callIt == definitionIndex.end() || copyIt == definitionIndex.end() || destroyIt == definitionIndex.end()) {
      error = "internal error: Function instance without its methods: " + functionPath;
      return false;
    }
    Definition &call = program.definitions[callIt->second];
    const std::string returnType = returnTypeOf(call);
    const bool returnsVoid = returnType == "void";
    std::string arguments;
    for (size_t i = 0; i < call.parameters.size(); ++i) {
      arguments += (i == 0 ? "" : ", ") + call.parameters[i].name;
    }
    const std::string reinterpret = "/std/intrinsics/memory/reinterpret";

    // call: dispatch on the tag; the last closure type is the fallback branch.
    std::string body = "  [Pointer<i32>] raw{this.env}\n";
    std::set<std::string> effects;
    std::set<std::string> capabilities;
    for (size_t k = 0; k < closures.size(); ++k) {
      const std::string &closure = closures[k];
      const std::string invoke = "dereference(target).call(" + arguments + ")";
      const std::string branch = "[Pointer<" + closure + "> mut] target{" + reinterpret + "<" + closure + ">(raw)}\n" +
                                 (returnsVoid ? invoke + "\nreturn()\n" : "return(" + invoke + ")\n");
      if (k + 1 < closures.size()) {
        body += "  if(equal(this.tag, " + std::to_string(k + 1) + "i32)) {\n" + branch + "  }\n";
      } else {
        body += branch;
      }
      if (const Definition *closureCall = find(closure + "/call")) {
        for (const Transform &transform : closureCall->transforms) {
          if (transform.name == "effects") {
            effects.insert(transform.arguments.begin(), transform.arguments.end());
          } else if (transform.name == "capabilities") {
            capabilities.insert(transform.arguments.begin(), transform.arguments.end());
          }
        }
      }
    }
    if (closures.empty() && !returnsVoid) {
      // Never constructed: no closure was converted to this Function type.
      body += "  return(dereference(" + reinterpret + "<" + returnType + ">(raw)))\n";
    }
    if (!parseBody(body, call.statements)) {
      return false;
    }
    refreshReturn(call);
    std::vector<Transform> transforms;
    for (Transform &transform : call.transforms) {
      if (transform.name != "effects" && transform.name != "capabilities") {
        transforms.push_back(std::move(transform));
      }
    }
    if (!effects.empty()) {
      Transform effectTransform;
      effectTransform.name = "effects";
      effectTransform.arguments.assign(effects.begin(), effects.end());
      transforms.push_back(std::move(effectTransform));
    }
    if (!capabilities.empty()) {
      Transform capabilityTransform;
      capabilityTransform.name = "capabilities";
      capabilityTransform.arguments.assign(capabilities.begin(), capabilities.end());
      transforms.push_back(std::move(capabilityTransform));
    }
    call.transforms = std::move(transforms);

    // Copy: copy the tag, then clone the boxed closure into a block of its own.
    std::string copyBody = "  assign(this.tag, other.tag)\n  assign(this.env, other.env)\n"
                           "  [Pointer<i32>] raw{other.env}\n";
    for (size_t k = 0; k < closures.size(); ++k) {
      const std::string &closure = closures[k];
      copyBody += "  if(equal(other.tag, " + std::to_string(k + 1) + "i32)) {\n"
                  "    [Pointer<" + closure + "> mut] source{" + reinterpret + "<" + closure + ">(raw)}\n"
                  "    [Pointer<uninitialized<" + closure + ">> mut] slot{/std/intrinsics/memory/alloc<uninitialized<" +
                  closure + ">>(1i32)}\n"
                  "    [" + closure + "] copied{dereference(source)}\n"
                  "    init(dereference(slot), copied)\n"
                  "    assign(this.env, " + reinterpret + "<i32>(slot))\n"
                  "  }\n";
    }
    Definition &copy = program.definitions[copyIt->second];
    if (!parseBody(copyBody, copy.statements)) {
      return false;
    }
    refreshReturn(copy);

    // Destroy: drop the boxed closure and free its block.
    std::string destroyBody = "  [Pointer<i32>] raw{this.env}\n";
    for (size_t k = 0; k < closures.size(); ++k) {
      const std::string &closure = closures[k];
      destroyBody += "  if(equal(this.tag, " + std::to_string(k + 1) + "i32)) {\n"
                     "    [Pointer<uninitialized<" + closure + ">> mut] slot{" + reinterpret + "<uninitialized<" +
                     closure + ">>(raw)}\n"
                     "    drop(dereference(slot))\n"
                     "    /std/intrinsics/memory/free(slot)\n"
                     "  }\n";
    }
    Definition &destroy = program.definitions[destroyIt->second];
    if (!parseBody(destroyBody, destroy.statements)) {
      return false;
    }
    refreshReturn(destroy);
    return true;
  }

  bool run() {
    for (size_t i = 0; i < program.definitions.size(); ++i) {
      definitionIndex.emplace(program.definitions[i].fullPath, i);
    }
    std::vector<std::string> instances;
    for (const Definition &def : program.definitions) {
      bool isStruct = false;
      for (const Transform &transform : def.transforms) {
        isStruct = isStruct || isStructTransformName(transform.name);
      }
      if (isStruct && def.templateArgs.empty() && isFunctionInstancePath(def.fullPath)) {
        instances.push_back(def.fullPath);
      }
    }
    if (instances.empty()) {
      return true;
    }
    for (Definition &def : program.definitions) {
      for (Expr &stmt : def.statements) {
        collect(stmt);
      }
      if (def.returnExpr.has_value()) {
        collect(*def.returnExpr);
      }
    }
    for (Execution &exec : program.executions) {
      for (Expr &arg : exec.arguments) {
        collect(arg);
      }
    }
    for (const std::string &instance : instances) {
      if (!completeInstance(instance, variants[instance])) {
        return false;
      }
    }
    return true;
  }
};

} // namespace

bool rewriteFunctionValues(Program &program, std::string &error) {
  FunctionValueRewriter rewriter{program, error, {}, {}};
  return rewriter.run();
}

} // namespace primec::semantics
