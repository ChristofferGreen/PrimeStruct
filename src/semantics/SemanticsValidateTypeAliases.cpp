#include "SemanticsValidateTypeAliases.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {

namespace {

struct TypeAlias {
  std::string path;
  std::string scope;
  std::vector<std::string> params;
  std::string target;
  bool isPublic = false;
};

bool isNameStart(char ch) {
  return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '/';
}

bool isNameChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '/';
}

std::string trimmed(const std::string &text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(begin, end - begin);
}

// Splits `Name<A, B<C>>` into its base and top-level arguments; false when the brackets do not balance.
bool splitType(const std::string &text, std::string &base, std::vector<std::string> &args) {
  base.clear();
  args.clear();
  const size_t open = text.find('<');
  if (open == std::string::npos) {
    base = trimmed(text);
    return true;
  }
  base = trimmed(text.substr(0, open));
  size_t depth = 0;
  size_t start = open + 1;
  for (size_t i = open; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '<') {
      ++depth;
    } else if (ch == '>') {
      if (--depth == 0) {
        if (!trimmed(text.substr(i + 1)).empty()) {
          return false;
        }
        args.push_back(trimmed(text.substr(start, i - start)));
        return true;
      }
    } else if (ch == ',' && depth == 1) {
      args.push_back(trimmed(text.substr(start, i - start)));
      start = i + 1;
    }
  }
  return false;
}

bool isDigits(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  for (char ch : text) {
    if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
      return false;
    }
  }
  return true;
}

std::vector<TemplateArgument> detailsFor(const std::vector<std::string> &args) {
  std::vector<TemplateArgument> details;
  for (const std::string &arg : args) {
    details.push_back(isDigits(arg) ? TemplateArgument::integer(arg, std::stoull(arg)) : TemplateArgument::type(arg));
  }
  return details;
}

std::string leafOf(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

class TypeAliasRewriter {
public:
  TypeAliasRewriter(Program &programIn, std::string &errorIn) : program(programIn), error(errorIn) {}

  bool run() {
    if (!collect()) {
      return false;
    }
    if (aliases.empty()) {
      return true;
    }
    for (const auto &entry : aliases) {
      std::string expanded;
      if (!expandTarget(entry.second, expanded)) {
        return false;
      }
    }
    for (Definition &def : program.definitions) {
      if (!rewriteDefinition(def)) {
        return false;
      }
    }
    for (Execution &exec : program.executions) {
      if (!rewriteExecution(exec)) {
        return false;
      }
    }
    return dropAliasImports();
  }

private:
  Program &program;
  std::string &error;
  std::map<std::string, TypeAlias> aliases;
  std::map<std::string, std::string> expandedTargets;
  std::vector<std::string> resolving;

  bool collect() {
    for (Definition &def : program.definitions) {
      size_t aliasIndex = def.transforms.size();
      bool isPublic = false;
      for (size_t i = 0; i < def.transforms.size(); ++i) {
        if (def.transforms[i].name == "alias") {
          aliasIndex = i;
        } else if (def.transforms[i].name == "public") {
          isPublic = true;
        }
      }
      if (aliasIndex == def.transforms.size()) {
        continue;
      }
      const Transform &marker = def.transforms[aliasIndex];
      if (marker.templateArgs.size() != 1 || trimmed(marker.templateArgs.front()).empty()) {
        error = "type alias " + def.fullPath + " needs one target type: [alias<Type>] " + def.name + "() {}";
        return false;
      }
      if (!def.parameters.empty() || !def.statements.empty() || def.hasReturnStatement) {
        error = "type alias " + def.fullPath + " has no parameters and no body: [alias<Type>] " + def.name + "() {}";
        return false;
      }
      TypeAlias alias;
      alias.path = def.fullPath;
      alias.scope = def.namespacePrefix;
      alias.params = def.templateArgs;
      alias.target = trimmed(marker.templateArgs.front());
      alias.isPublic = isPublic;
      aliases.emplace(alias.path, std::move(alias));
      // The alias stays as a private empty struct under another name, so a wildcard import of its namespace still
      // finds a definition, and the alias name is free for a field of the same spelling in lower case.
      def.transforms[aliasIndex].name = "struct";
      def.transforms[aliasIndex].templateArgs.clear();
      def.transforms[aliasIndex].templateArgDetails.clear();
      std::erase_if(def.transforms, [](const Transform &transform) { return transform.name == "public"; });
      def.name = "__alias_" + def.name;
      def.fullPath = def.namespacePrefix + "/" + def.name;
      def.templateArgs.clear();
      def.templateArgIsPack.clear();
    }
    return true;
  }

  // `import /lib/Handler` of an alias has done its work (the lookup above saw it); the path names no definition.
  bool dropAliasImports() {
    for (std::vector<std::string> *list : {&program.imports, &program.sourceImports}) {
      std::vector<std::string> kept;
      for (const std::string &path : *list) {
        const auto it = aliases.find(path);
        if (it == aliases.end()) {
          kept.push_back(path);
        } else if (!it->second.isPublic) {
          error = "import path refers to private definition: " + path;
          return false;
        }
      }
      *list = std::move(kept);
    }
    return true;
  }

  static std::string stripScope(std::string scope) {
    while (scope.size() > 1 && scope.back() == '/') {
      scope.pop_back();
    }
    return scope == "/" ? std::string() : scope;
  }

  const TypeAlias *lookup(const std::string &token, const std::string &scopeIn, const std::set<std::string> &shadow) const {
    if (token.empty() || token == "/") {
      return nullptr;
    }
    if (token.front() == '/') {
      const auto it = aliases.find(token);
      return it == aliases.end() ? nullptr : &it->second;
    }
    if (shadow.count(token) > 0) {
      return nullptr;
    }
    std::string scope = stripScope(scopeIn);
    while (true) {
      const auto it = aliases.find(scope + "/" + token);
      if (it != aliases.end()) {
        return &it->second;
      }
      if (scope.empty()) {
        break;
      }
      const size_t slash = scope.rfind('/');
      scope.resize(slash == std::string::npos ? 0 : slash);
    }
    for (const std::string &importPath : program.imports) {
      if (importPath.size() > 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
        const auto it = aliases.find(importPath.substr(0, importPath.size() - 2) + "/" + token);
        if (it != aliases.end() && it->second.isPublic) {
          return &it->second;
        }
      } else if (leafOf(importPath) == token) {
        const auto it = aliases.find(importPath);
        if (it != aliases.end() && it->second.isPublic) {
          return &it->second;
        }
      }
    }
    return nullptr;
  }

  // The target of `alias` with every alias in it expanded; the parameters of a generic alias stay as written.
  bool expandTarget(const TypeAlias &alias, std::string &out) {
    const auto known = expandedTargets.find(alias.path);
    if (known != expandedTargets.end()) {
      out = known->second;
      return true;
    }
    for (size_t i = 0; i < resolving.size(); ++i) {
      if (resolving[i] == alias.path) {
        std::string chain;
        for (size_t j = i; j < resolving.size(); ++j) {
          chain += resolving[j] + " -> ";
        }
        error = "type alias cycle: " + chain + alias.path;
        return false;
      }
    }
    resolving.push_back(alias.path);
    const std::set<std::string> shadow(alias.params.begin(), alias.params.end());
    const bool ok = expandText(alias.target, alias.scope, shadow, out);
    resolving.pop_back();
    if (ok) {
      expandedTargets.emplace(alias.path, out);
    }
    return ok;
  }

  // Replaces each name in `text` that is a type alias by its target, applying the alias's parameters to the
  // template arguments written after it.
  bool expandText(const std::string &text, const std::string &scope, const std::set<std::string> &shadow,
                  std::string &out) {
    out.clear();
    size_t i = 0;
    while (i < text.size()) {
      if (!isNameStart(text[i]) || (i > 0 && isNameChar(text[i - 1]))) {
        out.push_back(text[i]);
        ++i;
        continue;
      }
      size_t end = i;
      while (end < text.size() && isNameChar(text[end])) {
        ++end;
      }
      const std::string token = text.substr(i, end - i);
      const TypeAlias *alias = lookup(token, scope, shadow);
      if (alias == nullptr) {
        out += token;
        i = end;
        continue;
      }
      // The template arguments written after the alias name.
      std::vector<std::string> args;
      size_t after = end;
      if (after < text.size() && text[after] == '<') {
        size_t depth = 0;
        size_t close = after;
        for (; close < text.size(); ++close) {
          if (text[close] == '<') {
            ++depth;
          } else if (text[close] == '>' && --depth == 0) {
            break;
          }
        }
        if (close >= text.size()) {
          error = "type alias " + alias->path + ": unbalanced template arguments in " + text;
          return false;
        }
        std::string base;
        if (!splitType("X" + text.substr(after, close - after + 1), base, args)) {
          error = "type alias " + alias->path + ": unbalanced template arguments in " + text;
          return false;
        }
        for (std::string &arg : args) {
          std::string expanded;
          if (!expandText(arg, scope, shadow, expanded)) {
            return false;
          }
          arg = expanded;
        }
        after = close + 1;
      }
      std::string target;
      if (!applyAlias(*alias, args, target)) {
        return false;
      }
      out += target;
      i = after;
    }
    return true;
  }

  bool applyAlias(const TypeAlias &alias, const std::vector<std::string> &args, std::string &out) {
    if (args.size() != alias.params.size()) {
      error = "type alias " + alias.path + " takes " + std::to_string(alias.params.size()) + " template argument" +
              (alias.params.size() == 1 ? "" : "s") + ", " + std::to_string(args.size()) + " given";
      return false;
    }
    std::string expanded;
    if (!expandTarget(alias, expanded)) {
      return false;
    }
    if (alias.params.empty()) {
      out = expanded;
      return true;
    }
    out.clear();
    size_t i = 0;
    while (i < expanded.size()) {
      if (!isNameStart(expanded[i]) || (i > 0 && isNameChar(expanded[i - 1]))) {
        out.push_back(expanded[i]);
        ++i;
        continue;
      }
      size_t end = i;
      while (end < expanded.size() && isNameChar(expanded[end])) {
        ++end;
      }
      const std::string token = expanded.substr(i, end - i);
      size_t param = 0;
      while (param < alias.params.size() && alias.params[param] != token) {
        ++param;
      }
      out += param < alias.params.size() ? args[param] : token;
      i = end;
    }
    return true;
  }

  // The base name and arguments of an expanded alias target.
  bool splitTarget(const std::string &text, std::string &base, std::vector<std::string> &args) {
    if (!splitType(text, base, args) || base.empty()) {
      error = "type alias target is not a type: " + text;
      return false;
    }
    return true;
  }

  bool rewriteArgs(std::vector<std::string> &args, std::vector<TemplateArgument> &details, const std::string &scope,
                   const std::set<std::string> &shadow) {
    for (size_t i = 0; i < args.size(); ++i) {
      std::string expanded;
      if (!expandText(args[i], scope, shadow, expanded)) {
        return false;
      }
      if (expanded != args[i]) {
        args[i] = expanded;
        if (i < details.size() && details[i].kind == TemplateArgumentKind::Type) {
          details[i].text = expanded;
        }
      }
    }
    return true;
  }

  // A name written where a type is expected: an alias becomes its target (base and arguments).
  bool rewriteTypeName(std::string &name, std::vector<std::string> &args, std::vector<TemplateArgument> &details,
                       const std::string &scope, const std::set<std::string> &shadow) {
    const TypeAlias *alias = lookup(name, scope, shadow);
    if (alias == nullptr) {
      return rewriteArgs(args, details, scope, shadow);
    }
    std::vector<std::string> written = args;
    for (std::string &arg : written) {
      std::string expanded;
      if (!expandText(arg, scope, shadow, expanded)) {
        return false;
      }
      arg = expanded;
    }
    std::string target;
    if (!applyAlias(*alias, written, target)) {
      return false;
    }
    std::string base;
    std::vector<std::string> targetArgs;
    if (!splitTarget(target, base, targetArgs)) {
      return false;
    }
    name = base;
    args = targetArgs;
    if (!details.empty() || !args.empty()) {
      details = detailsFor(args);
    }
    return true;
  }

  bool rewriteTransforms(std::vector<Transform> &transforms, const std::string &scope,
                         const std::set<std::string> &shadow) {
    for (Transform &transform : transforms) {
      if (transform.name == "alias") {
        continue;
      }
      if (!rewriteTypeName(transform.name, transform.templateArgs, transform.templateArgDetails, scope, shadow)) {
        return false;
      }
    }
    return true;
  }

  bool rewriteExpr(Expr &expr, const std::string &scope, const std::set<std::string> &shadow) {
    if (!rewriteTransforms(expr.transforms, scope, shadow)) {
      return false;
    }
    if (expr.kind == Expr::Kind::Call && expr.isBraceConstructor && !expr.isBinding) {
      if (!rewriteTypeName(expr.name, expr.templateArgs, expr.templateArgDetails, scope, shadow)) {
        return false;
      }
    } else if (!rewriteArgs(expr.templateArgs, expr.templateArgDetails, scope, shadow)) {
      return false;
    }
    for (Expr &arg : expr.args) {
      if (!rewriteExpr(arg, scope, shadow)) {
        return false;
      }
    }
    for (Expr &arg : expr.bodyArguments) {
      if (!rewriteExpr(arg, scope, shadow)) {
        return false;
      }
    }
    for (Expr &init : expr.lambdaInitCaptures) {
      if (!rewriteExpr(init, scope, shadow)) {
        return false;
      }
    }
    return true;
  }

  bool rewriteDefinition(Definition &def) {
    const std::set<std::string> shadow(def.templateArgs.begin(), def.templateArgs.end());
    const std::string &scope = def.namespacePrefix;
    if (!rewriteTransforms(def.transforms, scope, shadow)) {
      return false;
    }
    for (Expr &param : def.parameters) {
      if (!rewriteExpr(param, scope, shadow)) {
        return false;
      }
    }
    for (Expr &stmt : def.statements) {
      if (!rewriteExpr(stmt, scope, shadow)) {
        return false;
      }
    }
    if (def.returnExpr.has_value() && !rewriteExpr(*def.returnExpr, scope, shadow)) {
      return false;
    }
    for (SumVariant &variant : def.sumVariants) {
      if (!variant.hasPayload || variant.payloadTypeText.empty()) {
        continue;
      }
      std::string expanded;
      if (!expandText(variant.payloadTypeText, scope, shadow, expanded)) {
        return false;
      }
      if (expanded != variant.payloadTypeText) {
        variant.payloadTypeText = expanded;
        if (!splitTarget(expanded, variant.payloadType, variant.payloadTemplateArgs)) {
          return false;
        }
      }
    }
    return true;
  }

  bool rewriteExecution(Execution &exec) {
    const std::set<std::string> shadow;
    const std::string &scope = exec.namespacePrefix;
    if (!rewriteTransforms(exec.transforms, scope, shadow) ||
        !rewriteArgs(exec.templateArgs, exec.templateArgDetails, scope, shadow)) {
      return false;
    }
    for (Expr &arg : exec.arguments) {
      if (!rewriteExpr(arg, scope, shadow)) {
        return false;
      }
    }
    for (Expr &arg : exec.bodyArguments) {
      if (!rewriteExpr(arg, scope, shadow)) {
        return false;
      }
    }
    return true;
  }
};

} // namespace

bool rewriteTypeAliases(Program &program, std::string &error) {
  TypeAliasRewriter rewriter(program, error);
  return rewriter.run();
}

} // namespace primec::semantics
