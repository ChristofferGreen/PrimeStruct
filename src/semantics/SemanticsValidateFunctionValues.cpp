#include "SemanticsValidateFunctionValues.h"

#include "SemanticsFunctionRoutines.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidateFunctionTypeRewrite.h"
#include "SemanticsValidateLambdaClosures.h"
#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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
  return name == "mut" || name == "public" || name == "private" || name == "static" ||
         name == "ctor_only" || name == "copy" || name == "move" || name == "restrict" ||
         name == "unsafe";
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

// The names an `effects` or `capabilities` transform of a definition declares.
std::set<std::string> declaredNames(const Definition &def, const char *transformName) {
  std::set<std::string> names;
  for (const Transform &transform : def.transforms) {
    if (transform.name == transformName) {
      names.insert(transform.arguments.begin(), transform.arguments.end());
    }
  }
  return names;
}

Transform namedTransformOf(const std::string &name, const std::vector<std::string> &arguments = {}) {
  Transform transform;
  transform.name = name;
  transform.arguments = arguments;
  return transform;
}

struct FunctionValueRewriter {
  Program &program;
  std::string &error;
  std::unordered_map<std::string, size_t> definitionIndex;
  // Function instance path -> converted closure types, in order of first conversion.
  std::map<std::string, std::vector<std::string>> variants;
  // (Function instance, closure type) -> the definition of its first conversion, for diagnostics.
  std::map<std::pair<std::string, std::string>, std::string> convertedIn;
  // The closure types whose clone and release routines are written.
  std::set<std::string> routinesWritten;
  std::string currentDefinition;

  const Definition *find(const std::string &path) const {
    const auto it = definitionIndex.find(path);
    return it == definitionIndex.end() ? nullptr : &program.definitions[it->second];
  }

  // The Function instance a brace constructor builds: its name after monomorphization.
  std::string functionInstanceOf(const Expr &expr) const {
    const std::string &path = calleePath(expr);
    if (find(path) != nullptr) {
      return path;
    }
    return {};
  }

  // A call of the getter of a routine's address, in the place of the placeholder of a conversion.
  static Expr addressGetterCall(const std::string &path, const Expr &like) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = path;
    call.namespacePrefix = like.namespacePrefix;
    call.sourceLine = like.sourceLine;
    call.sourceColumn = like.sourceColumn;
    return call;
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
    Expr *invoke = nullptr;
    Expr *clone = nullptr;
    Expr *release = nullptr;
    const Expr *env = nullptr;
    for (size_t i = 0; i < expr.args.size(); ++i) {
      const std::optional<std::string> &label = i < expr.argNames.size() ? expr.argNames[i] : std::nullopt;
      if (!label.has_value()) {
        continue;
      }
      if (*label == "invoke") {
        invoke = &expr.args[i];
      } else if (*label == "clone") {
        clone = &expr.args[i];
      } else if (*label == "release") {
        release = &expr.args[i];
      } else if (*label == "env") {
        env = &expr.args[i];
      }
    }
    if (invoke == nullptr || clone == nullptr || release == nullptr || env == nullptr ||
        env->kind != Expr::Kind::Call || !isFunctionBoxPath(calleePath(*env))) {
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
    if (std::find(known.begin(), known.end(), closureType) == known.end()) {
      known.push_back(closureType);
      convertedIn.emplace(std::make_pair(functionPath, closureType), currentDefinition);
    }
    *invoke = addressGetterCall(functionInvokeAddressPath(functionPath, closureType), *invoke);
    *clone = addressGetterCall(functionCloneAddressPath(closureType), *clone);
    *release = addressGetterCall(functionReleaseAddressPath(closureType), *release);
  }

  // A type as written for a diagnostic and for comparison: namespaces and template-instance suffixes dropped
  // (`/std/http/Request` and `Request` agree; `/Box__t1a2b` reads `Box`).
  static std::string plainTypeText(const std::string &text) {
    std::string out;
    std::string token;
    auto flush = [&]() {
      const size_t slash = token.rfind('/');
      std::string leaf = slash == std::string::npos ? token : token.substr(slash + 1);
      const size_t instance = leaf.find("__t");
      if (instance != std::string::npos && instance > 0) {
        leaf.resize(instance);
      }
      // `__FunctionN` is the lowered `Function` box; closure structs keep their names, so two stay distinct.
      out += userFacingLoweredTypeLeaf(leaf) == "Function" ? std::string("Function") : leaf;
      token.clear();
    };
    for (char ch : text) {
      if (ch == '<' || ch == '>' || ch == ',' || ch == ' ') {
        flush();
        out.push_back(ch);
      } else {
        token.push_back(ch);
      }
    }
    flush();
    return out;
  }

  static bool isNumericType(const std::string &type) {
    // Plain arrays, not static containers: those would be built in the compile arena and freed after it.
    static constexpr std::string_view Numeric[] = {"i8",  "i16", "i32", "i64", "u8",   "u16",  "u32",
                                                   "u64", "f32", "f64", "int", "bool", "float"};
    return std::find(std::begin(Numeric), std::end(Numeric), type) != std::end(Numeric);
  }

  // Whether a value of `passed` can never bind to `declared`. Numeric types widen and text and borrowed types
  // convert, so those pairs are left to the validator; numbers against anything else and distinct struct types
  // are certain mismatches.
  static bool certainlyMismatched(const std::string &declared, const std::string &passed) {
    if (declared == passed || declared.empty() || passed.empty()) {
      return false;
    }
    const bool declaredNumeric = isNumericType(declared);
    const bool passedNumeric = isNumericType(passed);
    if (declaredNumeric || passedNumeric) {
      return declaredNumeric != passedNumeric;
    }
    auto base = [](const std::string &type) {
      std::string name = type.substr(0, type.find('<'));
      for (char &ch : name) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      return name;
    };
    // Text, borrows and collections have more than one spelling or convert implicitly.
    static constexpr std::string_view Converting[] = {"string", "stringview", "reference", "pointer",
                                                      "auto",   "vector",     "map",       "array"};
    auto converts = [&](const std::string &type) {
      return std::find(std::begin(Converting), std::end(Converting), base(type)) != std::end(Converting);
    };
    if (converts(declared) || converts(passed)) {
      return false;
    }
    return base(declared) != base(passed);
  }

  // A diagnostic name for a converted closure type: the definition a forwarder calls, or the struct.
  static std::string closureDisplayName(const std::string &closureType) {
    std::string name = closureType;
    const size_t angle = name.find('<');
    if (angle != std::string::npos) {
      name.resize(angle);
    }
    const size_t instance = name.rfind("__t");
    if (instance != std::string::npos && instance > name.rfind('/')) {
      name.resize(instance);
    }
    const std::string forwarder = FunctionForwarderSuffix;
    if (name.size() > forwarder.size() && name.compare(name.size() - forwarder.size(), forwarder.size(), forwarder) == 0) {
      return name.substr(0, name.size() - forwarder.size());
    }
    return name;
  }

  // `Function<R, A...>` as a diagnostic spells the type of an instance, from its `call`.
  static std::string functionTypeName(const Definition &functionCall) {
    std::string name = "Function<" + plainTypeText(returnTypeOf(functionCall));
    for (const Expr &param : functionCall.parameters) {
      name += ", " + plainTypeText(bindingTypeText(param));
    }
    return name + ">";
  }

  // The closure's `call` must take the Function's arguments (`Function<i32, i32>` cannot hold a closure that takes a
  // string or two numbers); checked here, where both are concrete, so the diagnostic names the conversion.
  bool checkSignature(const std::string &functionPath, const std::string &closure) {
    const Definition *functionCall = find(functionPath + "/call");
    const Definition *closureCall = find(closure + "/call");
    if (functionCall == nullptr || closureCall == nullptr || !closureCall->templateArgs.empty()) {
      return true;
    }
    for (const Expr &param : closureCall->parameters) {
      if (param.isSpread || !param.isBinding) {
        return true;
      }
    }
    const std::string returnType = returnTypeOf(*functionCall);
    const std::string functionName = functionTypeName(*functionCall);
    const auto siteIt = convertedIn.find(std::make_pair(functionPath, closure));
    const std::string site =
        siteIt == convertedIn.end() || siteIt->second.empty() ? std::string() : " in " + siteIt->second;
    const std::string prefix = functionName + " cannot hold " + closureDisplayName(closure) + site + ": ";
    const size_t passed = functionCall->parameters.size();
    // A `call` written outside its struct takes the receiver as an explicit `self` first.
    const size_t first =
        !closureCall->parameters.empty() && closureCall->parameters.front().name == "self" ? 1 : 0;
    const size_t takes = closureCall->parameters.size() - first;
    size_t required = 0;
    for (size_t i = first; i < closureCall->parameters.size(); ++i) {
      if (closureCall->parameters[i].args.empty()) {
        ++required;
      }
    }
    if (passed < required || passed > takes) {
      error = prefix + "it takes " + std::to_string(takes) + " argument" + (takes == 1 ? "" : "s") +
              ", the Function passes " + std::to_string(passed);
      return false;
    }
    for (size_t i = 0; i < passed; ++i) {
      const Expr &param = closureCall->parameters[first + i];
      const std::string declared = plainTypeText(bindingTypeText(param));
      const std::string given = plainTypeText(bindingTypeText(functionCall->parameters[i]));
      // The Function passes a `Reference<T>` where the closure takes a `T` (a `[T mut]` or by-value parameter).
      std::string referent = given;
      if (given.rfind("Reference<", 0) == 0 && given.back() == '>') {
        referent = given.substr(10, given.size() - 11);
      }
      if (certainlyMismatched(declared, referent)) {
        error = prefix + "its parameter " + param.name + " is " + declared + ", the Function passes " + given;
        return false;
      }
      // A `mut` parameter changes the caller's value, so the Function passes it as a `Reference<T>`: the
      // generated `call` could not give a by-value parameter to the closure as a mutable place.
      bool isMut = false;
      for (const Transform &transform : param.transforms) {
        isMut = isMut || transform.name == "mut";
      }
      if (isMut && given.rfind("Reference<", 0) != 0 && declared.rfind("Reference<", 0) != 0 &&
          declared.rfind("Pointer<", 0) != 0) {
        error = prefix + "its parameter " + param.name +
                " is mut, spell the Function's parameter Reference<" + given + ">";
        return false;
      }
    }
    bool explicitReturn = false;
    for (const Transform &transform : closureCall->transforms) {
      explicitReturn = explicitReturn || transform.name == "return";
    }
    if (explicitReturn && returnType != "void") {
      const std::string returned = plainTypeText(returnTypeOf(*closureCall));
      const std::string expected = plainTypeText(returnType);
      if (returned == "void" || certainlyMismatched(expected, returned)) {
        error = prefix + "it returns " + returned + ", the Function returns " + expected;
        return false;
      }
    }
    return true;
  }

  // A closure converted to the `Function` of a library runs where the library calls it, under the effects that
  // library's `call` declares: the closure may not need more.
  bool checkLibraryEffects(const std::string &functionPath, const std::string &closure) {
    const Definition *libraryCall = find(functionPath + "/call");
    const Definition *closureCall = find(closure + "/call");
    if (libraryCall == nullptr || closureCall == nullptr) {
      return true;
    }
    for (const char *kind : {"effects", "capabilities"}) {
      const std::set<std::string> allowed = declaredNames(*libraryCall, kind);
      for (const std::string &name : declaredNames(*closureCall, kind)) {
        if (allowed.count(name) == 0) {
          const auto siteIt = convertedIn.find(std::make_pair(functionPath, closure));
          const std::string site =
              siteIt == convertedIn.end() || siteIt->second.empty() ? std::string() : " in " + siteIt->second;
          error = functionTypeName(*libraryCall) + " cannot hold " + closureDisplayName(closure) + site + ": it has " +
                  kind + "(" + name + "), which the library that defines the Function does not allow";
          return false;
        }
      }
    }
    return true;
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

  // An instance a library's interface carries: its `call` is an imported declaration, so the library's code makes the
  // calls and a consumer only supplies the routines of its own closures.
  bool isLibraryOwned(const std::string &functionPath) const {
    const auto callIt = definitionIndex.find(functionPath + "/call");
    if (callIt == definitionIndex.end()) {
      return false;
    }
    for (const Transform &transform : program.definitions[callIt->second].transforms) {
      if (transform.name == "imported") {
        return true;
      }
    }
    return false;
  }

  // Appends the definitions the text defines; one whose path exists already (a routine two conversions share, or the
  // shared declarations of a second run) is left as it is.
  bool addDefinitions(const std::string &text) {
    std::vector<Definition> parsed;
    if (!parseGeneratedDefinitions(text, parsed, error)) {
      return false;
    }
    for (Definition &def : parsed) {
      addDefinition(std::move(def));
    }
    return true;
  }

  void addDefinition(Definition def) {
    if (definitionIndex.count(def.fullPath) != 0) {
      return;
    }
    definitionIndex.emplace(def.fullPath, program.definitions.size());
    program.definitions.push_back(std::move(def));
  }

  // The clone and release routines of a closure type and the getters of their addresses, written once.
  bool writeClosureRoutines(const std::string &closure) {
    if (!routinesWritten.insert(closure).second) {
      return true;
    }
    return addDefinitions(functionCloneRoutineText(closure) + functionReleaseRoutineText(closure) +
                          functionAddressGetterText(functionCloneAddressPath(closure), functionCloneRoutinePath(closure)) +
                          functionAddressGetterText(functionReleaseAddressPath(closure),
                                                    functionReleaseRoutinePath(closure)));
  }

  // The parameters `address` (a u64) and `env` (the closure's block) of the routines.
  bool leadingParameters(std::vector<Expr> &out) {
    std::vector<Definition> parsed;
    if (!parseGeneratedDefinitions("[return<void>]\n__function_value_parameters([u64] address, [Pointer<i32>] env) {}\n",
                                   parsed,
                                   error) ||
        parsed.empty() || parsed.front().parameters.size() != 2) {
      return false;
    }
    out = std::move(parsed.front().parameters);
    return true;
  }

  // A free definition with the signature of the instance's `call`: `extra` parameters first, then the `call`'s own.
  Definition routineLike(const Definition &call, const std::string &path, std::vector<Expr> extra) {
    Definition routine = call;
    routine.fullPath = path;
    routine.name = leafOf(path);
    routine.namespacePrefix = "/";
    routine.isNested = false;
    routine.templateArgs.clear();
    routine.templateArgIsPack.clear();
    routine.templatePackBindings.clear();
    routine.instanceOfTemplate.clear();
    routine.instanceTemplateArgs.clear();
    routine.instanceRequestedBy.clear();
    routine.statements.clear();
    routine.returnExpr.reset();
    routine.hasReturnStatement = false;
    routine.transforms.clear();
    for (const Transform &transform : call.transforms) {
      if (transform.name == "return") {
        routine.transforms.push_back(transform);
      }
    }
    for (const Expr &param : call.parameters) {
      extra.push_back(param);
    }
    routine.parameters = std::move(extra);
    return routine;
  }

  static std::string argumentNames(const Definition &call) {
    std::string arguments;
    for (size_t i = 0; i < call.parameters.size(); ++i) {
      arguments += (i == 0 ? "" : ", ") + call.parameters[i].name;
    }
    return arguments;
  }

  // The routine that enters a closure type's `call` for an instance: it takes the block and the `Function`'s
  // arguments, and its address is what the conversion stores in the `Function`.
  bool writeInvokeRoutine(const std::string &functionPath, const std::string &closure) {
    const Definition *call = find(functionPath + "/call");
    if (call == nullptr) {
      error = "internal error: Function instance without its call: " + functionPath;
      return false;
    }
    std::vector<Expr> leading;
    if (!leadingParameters(leading)) {
      return false;
    }
    const std::string path = functionInvokeRoutinePath(functionPath, closure);
    std::vector<Expr> envOnly;
    envOnly.push_back(std::move(leading[1]));
    Definition routine = routineLike(*call, path, std::move(envOnly));
    routine.transforms.push_back(namedTransformOf("unsafe"));
    if (const Definition *closureCall = find(closure + "/call")) {
      for (const char *kind : {"effects", "capabilities"}) {
        const std::set<std::string> names = declaredNames(*closureCall, kind);
        if (!names.empty()) {
          routine.transforms.push_back(namedTransformOf(kind, {names.begin(), names.end()}));
        }
      }
    }
    if (!parseBody(functionInvokeBodyText(closure, argumentNames(*call), returnTypeOf(*call) == "void"),
                   routine.statements)) {
      return false;
    }
    refreshReturn(routine);
    addDefinition(std::move(routine));
    return addDefinitions(functionAddressGetterText(functionInvokeAddressPath(functionPath, closure), path));
  }

  // The `call` of an instance this program defines calls the routine its address names through a declaration of the
  // instance's signature, so it declares the effects of every closure converted to the instance.
  bool completeInstance(const std::string &functionPath, const std::vector<std::string> &closures) {
    const auto callIt = definitionIndex.find(functionPath + "/call");
    if (callIt == definitionIndex.end()) {
      error = "internal error: Function instance without its call: " + functionPath;
      return false;
    }
    std::set<std::string> effects;
    std::set<std::string> capabilities;
    for (const std::string &closure : closures) {
      if (const Definition *closureCall = find(closure + "/call")) {
        const std::set<std::string> closureEffects = declaredNames(*closureCall, "effects");
        const std::set<std::string> closureCapabilities = declaredNames(*closureCall, "capabilities");
        effects.insert(closureEffects.begin(), closureEffects.end());
        capabilities.insert(closureCapabilities.begin(), closureCapabilities.end());
      }
    }
    const Definition call = program.definitions[callIt->second];
    std::vector<Expr> leading;
    if (!leadingParameters(leading)) {
      return false;
    }
    const std::string protoPath = functionCallDeclarationPath(functionPath);
    Definition proto = routineLike(call, protoPath, std::move(leading));
    proto.transforms.push_back(namedTransformOf("indirect_call"));
    proto.transforms.push_back(namedTransformOf("unsafe"));
    if (!effects.empty()) {
      proto.transforms.push_back(namedTransformOf("effects", {effects.begin(), effects.end()}));
    }
    if (!capabilities.empty()) {
      proto.transforms.push_back(namedTransformOf("capabilities", {capabilities.begin(), capabilities.end()}));
    }
    addDefinition(std::move(proto));

    const std::string arguments = argumentNames(call);
    const std::string invoke = protoPath + "(this.invoke, this.env" + (arguments.empty() ? "" : ", " + arguments) + ")";
    const bool returnsVoid = returnTypeOf(call) == "void";
    Definition &target = program.definitions[callIt->second];
    if (!parseBody(returnsVoid ? "  " + invoke + "\n  return()\n" : "  return(" + invoke + ")\n", target.statements)) {
      return false;
    }
    refreshReturn(target);
    std::vector<Transform> transforms;
    for (Transform &transform : target.transforms) {
      if (transform.name != "effects" && transform.name != "capabilities") {
        transforms.push_back(std::move(transform));
      }
    }
    if (!effects.empty()) {
      transforms.push_back(namedTransformOf("effects", {effects.begin(), effects.end()}));
    }
    if (!capabilities.empty()) {
      transforms.push_back(namedTransformOf("capabilities", {capabilities.begin(), capabilities.end()}));
    }
    target.transforms = std::move(transforms);
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
    for (size_t i = 0; i < program.definitions.size(); ++i) {
      currentDefinition = program.definitions[i].fullPath;
      for (Expr &stmt : program.definitions[i].statements) {
        collect(stmt);
      }
      if (program.definitions[i].returnExpr.has_value()) {
        collect(*program.definitions[i].returnExpr);
      }
    }
    currentDefinition.clear();
    for (Execution &exec : program.executions) {
      for (Expr &arg : exec.arguments) {
        collect(arg);
      }
    }
    // The `Copy` and `Destroy` of every instance, a library's included, call the clone and release routines of the
    // closure through these two declarations.
    if (!addDefinitions(functionSharedCallDeclarationsText())) {
      return false;
    }
    for (const std::string &instance : instances) {
      const bool libraryOwned = isLibraryOwned(instance);
      const std::vector<std::string> closures = variants[instance];
      for (const std::string &closure : closures) {
        if (!checkSignature(instance, closure) || (libraryOwned && !checkLibraryEffects(instance, closure)) ||
            !writeClosureRoutines(closure) || !writeInvokeRoutine(instance, closure)) {
          return false;
        }
      }
      if (!libraryOwned && !completeInstance(instance, closures)) {
        return false;
      }
    }
    return true;
  }
};

} // namespace

bool rewriteFunctionValues(Program &program, std::string &error) {
  FunctionValueRewriter rewriter{program, error, {}, {}, {}, {}, {}};
  return rewriter.run();
}

} // namespace primec::semantics
