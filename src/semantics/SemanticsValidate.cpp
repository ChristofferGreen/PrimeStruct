#include "primec/Semantics.h"

#include "SemanticsValidator.h"
#include "primec/TransformRegistry.h"

#include <string>
#include <vector>

namespace primec {

namespace semantics {
bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error);
}

namespace {
bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "struct" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "public" || name == "private" ||
         name == "package" || name == "static" || name == "single_type_to_return" || name == "stack" ||
         name == "heap" || name == "buffer";
}

bool isSingleTypeReturnCandidate(const Transform &transform) {
  if (!transform.arguments.empty()) {
    return false;
  }
  if (isNonTypeTransformName(transform.name)) {
    return false;
  }
  return true;
}

bool isTextOnlyTransform(const Transform &transform) {
  if (transform.phase == TransformPhase::Text) {
    return true;
  }
  if (transform.phase == TransformPhase::Semantic) {
    return false;
  }
  const bool isText = primec::isTextTransformName(transform.name);
  const bool isSemantic = primec::isSemanticTransformName(transform.name);
  return isText && !isSemantic;
}

void stripTextTransforms(std::vector<Transform> &transforms) {
  size_t out = 0;
  for (size_t i = 0; i < transforms.size(); ++i) {
    auto &transform = transforms[i];
    if (isTextOnlyTransform(transform)) {
      continue;
    }
    if (out != i) {
      transforms[out] = std::move(transform);
    }
    ++out;
  }
  transforms.resize(out);
}

void stripTextTransforms(Expr &expr) {
  stripTextTransforms(expr.transforms);
  for (auto &arg : expr.args) {
    stripTextTransforms(arg);
  }
  for (auto &arg : expr.bodyArguments) {
    stripTextTransforms(arg);
  }
}

void stripTextTransforms(Definition &def) {
  stripTextTransforms(def.transforms);
  for (auto &param : def.parameters) {
    stripTextTransforms(param);
  }
  for (auto &stmt : def.statements) {
    stripTextTransforms(stmt);
  }
  if (def.returnExpr.has_value()) {
    stripTextTransforms(*def.returnExpr);
  }
}

void stripTextTransforms(Execution &exec) {
  stripTextTransforms(exec.transforms);
  for (auto &arg : exec.arguments) {
    stripTextTransforms(arg);
  }
  for (auto &arg : exec.bodyArguments) {
    stripTextTransforms(arg);
  }
}

std::string formatTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

std::string formatTransformType(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + formatTemplateArgs(transform.templateArgs) + ">";
}

bool validateTransformPhaseList(const std::vector<Transform> &transforms,
                                const std::string &context,
                                std::string &error) {
  for (const auto &transform : transforms) {
    const bool isText = primec::isTextTransformName(transform.name);
    const bool isSemantic = primec::isSemanticTransformName(transform.name);
    if (transform.phase == TransformPhase::Text) {
      if (!isText) {
        error = "text(...) group requires text transforms on " + context + ": " + transform.name;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Semantic) {
      if (!isSemantic && isText) {
        error = "text transform cannot appear in semantic(...) group on " + context + ": " + transform.name;
        return false;
      }
      continue;
    }
    if (transform.phase == TransformPhase::Auto && isText && isSemantic) {
      error = "ambiguous transform name on " + context + ": " + transform.name;
      return false;
    }
  }
  return true;
}

bool validateTransformListContext(const std::vector<Transform> &transforms,
                                  const std::string &context,
                                  bool allowSingleTypeToReturn,
                                  std::string &error) {
  if (!validateTransformPhaseList(transforms, context, error)) {
    return false;
  }
  if (!allowSingleTypeToReturn) {
    for (const auto &transform : transforms) {
      if (transform.name == "single_type_to_return") {
        error = "single_type_to_return is only valid on definitions: " + context;
        return false;
      }
    }
  }
  return true;
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == nameToMatch;
}

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, "loop");
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, "while");
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, "for");
}

bool isLoopBodyEnvelope(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
    return false;
  }
  if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
    return false;
  }
  return true;
}

bool stripSharedScopeTransform(std::vector<Transform> &transforms,
                               bool &found,
                               const std::string &context,
                               std::string &error) {
  found = false;
  for (auto it = transforms.begin(); it != transforms.end(); ++it) {
    if (it->name != "shared_scope") {
      continue;
    }
    if (found) {
      error = "duplicate shared_scope transform on " + context;
      return false;
    }
    if (!it->templateArgs.empty()) {
      error = "shared_scope does not accept template arguments on " + context;
      return false;
    }
    if (!it->arguments.empty()) {
      error = "shared_scope does not accept arguments on " + context;
      return false;
    }
    found = true;
    transforms.erase(it);
    break;
  }
  return true;
}

bool rewriteSharedScopeStatements(std::vector<Expr> &statements, std::string &error);

bool rewriteSharedScopeStatement(Expr &stmt, std::string &error) {
  if (stmt.kind == Expr::Kind::Call) {
    bool hasSharedScope = false;
    if (!stripSharedScopeTransform(stmt.transforms, hasSharedScope, "statement", error)) {
      return false;
    }
    if (hasSharedScope) {
      const bool isLoop = isLoopCall(stmt);
      const bool isWhile = isWhileCall(stmt);
      const bool isFor = isForCall(stmt);
      if (!isLoop && !isWhile && !isFor) {
        error = "shared_scope is only valid on loop/while/for statements";
        return false;
      }
      const size_t bodyIndex = isFor ? 3 : 1;
      if (stmt.args.size() <= bodyIndex) {
        error = "shared_scope requires loop body";
        return false;
      }
      Expr &body = stmt.args[bodyIndex];
      if (!isLoopBodyEnvelope(body)) {
        error = "shared_scope requires loop body in do() { ... }";
        return false;
      }
      std::vector<Expr> hoisted;
      std::vector<Expr> remaining;
      for (auto &bodyStmt : body.bodyArguments) {
        if (bodyStmt.isBinding) {
          hoisted.push_back(std::move(bodyStmt));
        } else {
          remaining.push_back(std::move(bodyStmt));
        }
      }
      body.bodyArguments = std::move(remaining);
      if (!rewriteSharedScopeStatements(body.bodyArguments, error)) {
        return false;
      }
      if (hoisted.empty()) {
        return true;
      }

      Expr blockCall;
      blockCall.kind = Expr::Kind::Call;
      blockCall.name = "block";
      blockCall.namespacePrefix = stmt.namespacePrefix;
      blockCall.hasBodyArguments = true;
      blockCall.bodyArguments.reserve(hoisted.size() + 2);

      if (isFor) {
        Expr initStmt = std::move(stmt.args[0]);
        Expr cond = std::move(stmt.args[1]);
        Expr step = std::move(stmt.args[2]);

        Expr whileCall;
        whileCall.kind = Expr::Kind::Call;
        whileCall.name = "while";
        whileCall.namespacePrefix = stmt.namespacePrefix;
        whileCall.transforms = std::move(stmt.transforms);
        whileCall.args.push_back(std::move(cond));
        whileCall.argNames.push_back(std::nullopt);

        Expr whileBody = std::move(body);
        whileBody.bodyArguments.push_back(std::move(step));
        whileCall.args.push_back(std::move(whileBody));
        whileCall.argNames.push_back(std::nullopt);

        blockCall.bodyArguments.push_back(std::move(initStmt));
        for (auto &binding : hoisted) {
          blockCall.bodyArguments.push_back(std::move(binding));
        }
        blockCall.bodyArguments.push_back(std::move(whileCall));
        stmt = std::move(blockCall);
        return true;
      }

      for (auto &binding : hoisted) {
        blockCall.bodyArguments.push_back(std::move(binding));
      }
      blockCall.bodyArguments.push_back(std::move(stmt));
      stmt = std::move(blockCall);
      return true;
    }

    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return rewriteSharedScopeStatements(stmt.bodyArguments, error);
    }
  }
  return true;
}

bool rewriteSharedScopeStatements(std::vector<Expr> &statements, std::string &error) {
  for (auto &stmt : statements) {
    if (!rewriteSharedScopeStatement(stmt, error)) {
      return false;
    }
  }
  return true;
}

bool applySingleTypeToReturn(std::vector<Transform> &transforms,
                             bool force,
                             const std::string &context,
                             std::string &error) {
  size_t markerIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (transforms[i].name != "single_type_to_return") {
      continue;
    }
    if (markerIndex != transforms.size()) {
      error = "duplicate single_type_to_return transform on " + context;
      return false;
    }
    if (!transforms[i].templateArgs.empty()) {
      error = "single_type_to_return does not accept template arguments on " + context;
      return false;
    }
    if (!transforms[i].arguments.empty()) {
      error = "single_type_to_return does not accept arguments on " + context;
      return false;
    }
    markerIndex = i;
  }
  const bool hasExplicitMarker = markerIndex != transforms.size();
  if (!hasExplicitMarker && !force) {
    return true;
  }
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      if (hasExplicitMarker) {
        error = "single_type_to_return cannot be combined with return transform on " + context;
        return false;
      }
      return true;
    }
  }
  size_t typeIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (!isSingleTypeReturnCandidate(transforms[i])) {
      continue;
    }
    if (typeIndex != transforms.size()) {
      if (hasExplicitMarker) {
        error = "single_type_to_return requires a single type transform on " + context;
        return false;
      }
      return true;
    }
    typeIndex = i;
  }
  if (typeIndex == transforms.size()) {
    if (hasExplicitMarker) {
      error = "single_type_to_return requires a type transform on " + context;
      return false;
    }
    return true;
  }
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.phase = TransformPhase::Semantic;
  returnTransform.templateArgs.push_back(formatTransformType(transforms[typeIndex]));
  if (!hasExplicitMarker) {
    transforms[typeIndex] = std::move(returnTransform);
    return true;
  }
  std::vector<Transform> rewritten;
  rewritten.reserve(transforms.size() - 1);
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (i == typeIndex) {
      rewritten.push_back(std::move(returnTransform));
      continue;
    }
    rewritten.push_back(std::move(transforms[i]));
  }
  transforms = std::move(rewritten);
  return true;
}

bool validateExprTransforms(const Expr &expr, const std::string &context, std::string &error) {
  if (!validateTransformListContext(expr.transforms, context, false, error)) {
    return false;
  }
  for (const auto &arg : expr.args) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    if (!validateExprTransforms(arg, context, error)) {
      return false;
    }
  }
  return true;
}

bool applySemanticTransforms(Program &program,
                             const std::vector<std::string> &semanticTransforms,
                             std::string &error) {
  bool forceSingleTypeToReturn = false;
  for (const auto &name : semanticTransforms) {
    if (name == "single_type_to_return") {
      forceSingleTypeToReturn = true;
      continue;
    }
    error = "unsupported semantic transform: " + name;
    return false;
  }

  for (auto &def : program.definitions) {
    if (!validateTransformListContext(def.transforms, def.fullPath, true, error)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!validateExprTransforms(param, def.fullPath, error)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!validateExprTransforms(stmt, def.fullPath, error)) {
        return false;
      }
    }
  }

  for (auto &exec : program.executions) {
    if (!validateTransformListContext(exec.transforms, exec.fullPath, false, error)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (!validateExprTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
  }

  for (auto &def : program.definitions) {
    stripTextTransforms(def);
  }
  for (auto &exec : program.executions) {
    stripTextTransforms(exec);
  }
  for (auto &def : program.definitions) {
    if (!rewriteSharedScopeStatements(def.statements, error)) {
      return false;
    }
  }
  for (auto &exec : program.executions) {
    if (!rewriteSharedScopeStatements(exec.bodyArguments, error)) {
      return false;
    }
  }
  for (auto &def : program.definitions) {
    if (!applySingleTypeToReturn(def.transforms, forceSingleTypeToReturn, def.fullPath, error)) {
      return false;
    }
  }

  return true;
}
} // namespace

bool Semantics::validate(Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects,
                         const std::vector<std::string> &semanticTransforms) const {
  if (!applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  if (!semantics::monomorphizeTemplates(program, entryPath, error)) {
    return false;
  }
  semantics::SemanticsValidator validator(program, entryPath, error, defaultEffects);
  return validator.run();
}

} // namespace primec
