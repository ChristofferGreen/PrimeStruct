#include "SemanticsValidateFunctionForwarders.h"

#include "SemanticsValidateLambdaClosures.h"

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace primec::semantics {

namespace {

using NameSet = std::set<std::string>;

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

struct ForwarderGenerator {
  Program &program;
  const std::unordered_set<std::string> &structPaths;
  std::vector<Definition> &generated;
  std::unordered_map<std::string, size_t> definitionIndex;
  std::set<std::string> forwarded;

  const Definition *findDefinition(const std::string &path) const {
    const auto it = definitionIndex.find(path);
    return it == definitionIndex.end() ? nullptr : &program.definitions[it->second];
  }

  bool isForwardable(const Definition &def) const {
    if (!def.templateArgs.empty() || structPaths.count(def.fullPath) > 0 ||
        structPaths.count(def.namespacePrefix) > 0 ||
        def.fullPath.find(LambdaClosureStructPrefix) != std::string::npos ||
        def.fullPath.find(FunctionForwarderSuffix) != std::string::npos) {
      return false;
    }
    for (const Transform &transform : def.transforms) {
      if (transform.name != "return" && transform.name != "effects" && transform.name != "capabilities" &&
          transform.name != "public" && transform.name != "private") {
        return false;  // compute kernels, sums, enums, tests and other special definitions stay names
      }
    }
    for (const Expr &param : def.parameters) {
      if (!param.isBinding || param.isSpread) {
        return false;
      }
      for (const Transform &transform : param.transforms) {
        if (transform.name == "args") {
          return false;
        }
      }
    }
    return true;
  }

  // The free function `name` refers to from `namespacePrefix`, when a forwarder can call it.
  std::string forwardableDefinition(const std::string &name, const std::string &namespacePrefix) const {
    if (name.empty() || name == "this") {
      return {};
    }
    std::vector<std::string> candidates;
    if (name.front() == '/') {
      candidates.push_back(name);
    } else {
      if (name.find('/') != std::string::npos) {
        return {};
      }
      std::string scope = namespacePrefix;
      while (true) {
        candidates.push_back(scope + "/" + name);
        if (scope.empty()) {
          break;
        }
        const size_t slash = scope.rfind('/');
        scope = slash == std::string::npos ? std::string() : scope.substr(0, slash);
      }
      for (const std::string &import : program.imports) {
        if (import.size() > 2 && import.compare(import.size() - 2, 2, "/*") == 0) {
          candidates.push_back(import.substr(0, import.size() - 2) + "/" + name);
        }
      }
    }
    for (const std::string &candidate : candidates) {
      if (const Definition *def = findDefinition(candidate)) {
        return isForwardable(*def) ? candidate : std::string();
      }
    }
    return {};
  }

  // `/ns/name__fnref` with `call(params) { return(/ns/name(params)) }`.
  std::string ensureForwarder(const std::string &targetPath) {
    const std::string structPath = targetPath + FunctionForwarderSuffix;
    if (!forwarded.insert(targetPath).second) {
      return structPath;
    }
    const Definition &target = *findDefinition(targetPath);
    Definition method;
    method.name = "call";
    method.namespacePrefix = structPath;
    method.fullPath = structPath + "/call";
    method.isNested = true;
    method.sourceLine = target.sourceLine;
    method.sourceColumn = target.sourceColumn;
    bool returnsValue = target.hasReturnStatement && target.returnExpr.has_value();
    for (const Transform &transform : target.transforms) {
      if (transform.name == "return" || transform.name == "effects" || transform.name == "capabilities") {
        method.transforms.push_back(transform);
      }
      if (transform.name == "return") {
        returnsValue = !(transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void");
      }
    }
    method.parameters = target.parameters;
    Expr origin;
    origin.namespacePrefix = structPath;
    origin.sourceLine = target.sourceLine;
    origin.sourceColumn = target.sourceColumn;
    Expr forward;
    forward.kind = Expr::Kind::Call;
    forward.name = targetPath;
    forward.namespacePrefix = structPath;
    forward.sourceLine = target.sourceLine;
    forward.sourceColumn = target.sourceColumn;
    for (const Expr &param : target.parameters) {
      forward.args.push_back(nameExpr(param.name, origin));
      forward.argNames.push_back(std::nullopt);
    }
    if (returnsValue) {
      Expr returnCall;
      returnCall.kind = Expr::Kind::Call;
      returnCall.name = "return";
      returnCall.sourceName = "return";
      returnCall.namespacePrefix = structPath;
      returnCall.sourceLine = target.sourceLine;
      returnCall.sourceColumn = target.sourceColumn;
      returnCall.args.push_back(forward);
      returnCall.argNames.push_back(std::nullopt);
      method.statements.push_back(std::move(returnCall));
      method.hasReturnStatement = true;
      method.returnExpr = std::move(forward);
    } else {
      method.statements.push_back(std::move(forward));
    }
    Definition closure;
    closure.name = target.name + FunctionForwarderSuffix;
    closure.namespacePrefix = target.namespacePrefix;
    closure.fullPath = structPath;
    closure.sourceLine = target.sourceLine;
    closure.sourceColumn = target.sourceColumn;
    closure.transforms.push_back(namedTransform("struct"));
    generated.push_back(std::move(method));
    generated.push_back(std::move(closure));
    return structPath;
  }

  void forwardNamedArguments(Expr &expr, const NameSet &bound, const std::string &namespacePrefix) {
    if (expr.kind == Expr::Kind::Call && expr.name != "dispatch") {
      const bool box = !expr.isMethodCall && expr.name == "/__fn_box";
      for (Expr &arg : expr.args) {
        Expr *named = &arg;
        if (box && arg.kind == Expr::Kind::Call && arg.name == "block" && arg.bodyArguments.size() == 1) {
          named = &arg.bodyArguments.front();  // `Function<R, A...>{name}` parses as a value block
        }
        if (named->kind != Expr::Kind::Name || bound.count(named->name) > 0) {
          continue;
        }
        const std::string target = forwardableDefinition(named->name, namespacePrefix);
        if (target.empty()) {
          continue;
        }
        const std::string structPath = ensureForwarder(target);
        if (box) {
          Expr construct;
          construct.kind = Expr::Kind::Call;
          construct.name = structPath;
          construct.sourceName = named->name;
          construct.namespacePrefix = named->namespacePrefix;
          construct.isBraceConstructor = true;
          construct.sourceLine = named->sourceLine > 0 ? named->sourceLine : expr.sourceLine;
          construct.sourceColumn = named->sourceLine > 0 ? named->sourceColumn : expr.sourceColumn;
          arg = std::move(construct);
        }
      }
    }
    for (Expr &arg : expr.args) {
      forwardNamedArguments(arg, bound, namespacePrefix);
    }
    for (Expr &arg : expr.bodyArguments) {
      forwardNamedArguments(arg, bound, namespacePrefix);
    }
    for (Expr &init : expr.lambdaInitCaptures) {
      forwardNamedArguments(init, bound, namespacePrefix);
    }
  }

  void collectForwarders(Definition &def) {
    NameSet bound;
    for (const Expr &param : def.parameters) {
      collectBoundNames(param, bound);
    }
    for (const Expr &stmt : def.statements) {
      collectBoundNames(stmt, bound);
    }
    for (Expr &stmt : def.statements) {
      forwardNamedArguments(stmt, bound, def.namespacePrefix);
    }
  }

};

} // namespace

void generateFunctionForwarders(Program &program,
                                const std::unordered_set<std::string> &structPaths,
                                std::vector<Definition> &generated) {
  ForwarderGenerator generator{program, structPaths, generated, {}, {}};
  for (size_t i = 0; i < program.definitions.size(); ++i) {
    generator.definitionIndex.emplace(program.definitions[i].fullPath, i);
  }
  for (Definition &def : program.definitions) {
    generator.collectForwarders(def);
  }
}

} // namespace primec::semantics
