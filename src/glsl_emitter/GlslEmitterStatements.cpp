#include "GlslEmitterInternal.h"

namespace primec::glsl_emitter {

bool emitStatement(const Expr &stmt, EmitState &state, std::string &out, std::string &error, const std::string &indent);

bool emitBlock(const std::vector<Expr> &stmts,
               EmitState &state,
               std::string &out,
               std::string &error,
               const std::string &indent) {
  auto savedLocals = state.locals;
  for (const auto &stmt : stmts) {
    if (!emitStatement(stmt, state, out, error, indent)) {
      state.locals = savedLocals;
      return false;
    }
  }
  state.locals = savedLocals;
  return true;
}

bool emitValueBlock(const Expr &blockExpr,
                    EmitState &state,
                    std::string &out,
                    std::string &error,
                    const std::string &indent,
                    ExprResult &result) {
  if (blockExpr.bodyArguments.empty()) {
    error = "glsl backend requires block to yield a value";
    return false;
  }
  for (size_t i = 0; i < blockExpr.bodyArguments.size(); ++i) {
    const Expr &stmt = blockExpr.bodyArguments[i];
    const bool isLast = (i + 1 == blockExpr.bodyArguments.size());
    if (stmt.isBinding) {
      if (isLast) {
        error = "glsl backend requires block to end with a value expression";
        return false;
      }
      if (!emitStatement(stmt, state, out, error, indent)) {
        return false;
      }
      continue;
    }
    if (isSimpleCallName(stmt, "return")) {
      if (stmt.args.size() != 1) {
        error = "glsl backend requires return(value) in value blocks";
        return false;
      }
      result = emitExpr(stmt.args.front(), state, error);
      if (!error.empty()) {
        return false;
      }
      appendIndented(out, result.prelude, indent);
      result.prelude.clear();
      return true;
    }
    if (isLast) {
      result = emitExpr(stmt, state, error);
      if (!error.empty()) {
        return false;
      }
      appendIndented(out, result.prelude, indent);
      result.prelude.clear();
      return true;
    }
    if (!emitStatement(stmt, state, out, error, indent)) {
      return false;
    }
  }
  error = "glsl backend requires block to yield a value";
  return false;
}

bool emitStatement(const Expr &stmt, EmitState &state, std::string &out, std::string &error, const std::string &indent) {
  if (stmt.isBinding) {
    bool isMutable = false;
    bool isStatic = false;
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "mut" && transform.templateArgs.empty() && transform.arguments.empty()) {
        isMutable = true;
      }
      if (transform.name == "static" && transform.templateArgs.empty() && transform.arguments.empty()) {
        isStatic = true;
      }
    }
    if (isStatic) {
      error = "glsl backend does not support static bindings";
      return false;
    }
    if (stmt.args.empty()) {
      error = "glsl backend requires binding initializers";
      return false;
    }
    std::string explicitType;
    bool hasExplicitType = getExplicitBindingTypeName(stmt, explicitType);
    const Expr &initExpr = stmt.args.front();
    if (isSimpleCallName(initExpr, "block") && initExpr.hasBodyArguments) {
      if (!initExpr.args.empty() || !initExpr.templateArgs.empty() || hasNamedArguments(initExpr.argNames)) {
        error = "glsl backend requires block() { ... }";
        return false;
      }
      EmitState blockState = state;
      std::string blockBody;
      ExprResult blockResult;
      if (!emitValueBlock(initExpr, blockState, blockBody, error, indent + "  ", blockResult)) {
        return false;
      }
      GlslType bindingType = blockResult.type;
      if (hasExplicitType) {
        bindingType = glslTypeFromName(explicitType, state, error);
        if (!error.empty()) {
          return false;
        }
        blockResult = castExpr(blockResult, bindingType);
      }
      std::string typeName = glslTypeName(bindingType);
      if (typeName.empty()) {
        error = "glsl backend requires numeric or boolean binding types";
        return false;
      }
      state.needsInt64Ext = state.needsInt64Ext || blockState.needsInt64Ext;
      state.needsFp64Ext = state.needsFp64Ext || blockState.needsFp64Ext;
      state.needsIntPow = state.needsIntPow || blockState.needsIntPow;
      state.needsUIntPow = state.needsUIntPow || blockState.needsUIntPow;
      state.needsInt64Pow = state.needsInt64Pow || blockState.needsInt64Pow;
      state.needsUInt64Pow = state.needsUInt64Pow || blockState.needsUInt64Pow;
      state.tempIndex = blockState.tempIndex;
      state.locals[stmt.name] = {bindingType, isMutable};
      // GLSL const requires an initializer, so block initializers emit a scoped assignment.
      out += indent + typeName + " " + stmt.name + ";\n";
      out += indent + "{\n";
      out += blockBody;
      out += indent + "  " + stmt.name + " = " + blockResult.code + ";\n";
      out += indent + "}\n";
      return true;
    }
    ExprResult init = emitExpr(initExpr, state, error);
    if (!error.empty()) {
      return false;
    }
    appendIndented(out, init.prelude, indent);
    GlslType bindingType = init.type;
    if (hasExplicitType) {
      bindingType = glslTypeFromName(explicitType, state, error);
      if (!error.empty()) {
        return false;
      }
      init = castExpr(init, bindingType);
    }
    std::string typeName = glslTypeName(bindingType);
    if (typeName.empty()) {
      error = "glsl backend requires numeric or boolean binding types";
      return false;
    }
    state.locals[stmt.name] = {bindingType, isMutable};
    out += indent;
    if (!isMutable) {
      out += "const ";
    }
    out += typeName + " " + stmt.name + " = " + init.code + ";\n";
    return true;
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding) {
    if (isSimpleCallName(stmt, "return")) {
      if (!stmt.args.empty()) {
        error = "glsl backend requires entry definition to return void";
        return false;
      }
      out += indent + "return;\n";
      return true;
    }
    if (isSimpleCallName(stmt, "assign")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires assign(target, value)";
        return false;
      }
      const Expr &target = stmt.args.front();
      if (target.kind != Expr::Kind::Name) {
        error = "glsl backend requires assign target to be a local name";
        return false;
      }
      auto it = state.locals.find(target.name);
      if (it == state.locals.end()) {
        error = "glsl backend requires local binding for assign target";
        return false;
      }
      if (!it->second.isMutable) {
        error = "glsl backend requires assign target to be mutable";
        return false;
      }
      ExprResult value = emitExpr(stmt.args[1], state, error);
      if (!error.empty()) {
        return false;
      }
      value = castExpr(value, it->second.type);
      appendIndented(out, value.prelude, indent);
      out += indent + target.name + " = " + value.code + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "increment") || isSimpleCallName(stmt, "decrement")) {
      if (stmt.args.size() != 1 || stmt.args.front().kind != Expr::Kind::Name) {
        error = "glsl backend requires increment/decrement target to be a local name";
        return false;
      }
      const std::string &targetName = stmt.args.front().name;
      auto it = state.locals.find(targetName);
      if (it == state.locals.end()) {
        error = "glsl backend requires local binding for increment/decrement target";
        return false;
      }
      if (!it->second.isMutable) {
        error = "glsl backend requires increment/decrement target to be mutable";
        return false;
      }
      out += indent + targetName + (isSimpleCallName(stmt, "increment") ? "++" : "--") + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "block")) {
      if (!stmt.args.empty() || !stmt.templateArgs.empty() || hasNamedArguments(stmt.argNames)) {
        error = "glsl backend requires block() { ... }";
        return false;
      }
      out += indent + "{\n";
      if (!emitBlock(stmt.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}\n";
      return true;
    }
    if (isSimpleCallName(stmt, "notify") || isSimpleCallName(stmt, "insert") || isSimpleCallName(stmt, "take")) {
      const char *name = stmt.name.c_str();
      const size_t expectedArgs = isSimpleCallName(stmt, "take") ? 1 : 2;
      if (hasNamedArguments(stmt.argNames)) {
        error = "glsl backend requires " + std::string(name) + " to use positional arguments";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error = "glsl backend does not support template arguments on " + std::string(name);
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = "glsl backend does not support block arguments on " + std::string(name);
        return false;
      }
      if (stmt.args.size() != expectedArgs) {
        error = "glsl backend requires " + std::string(name) + " with " + std::to_string(expectedArgs) +
                " argument" + (expectedArgs == 1 ? "" : "s");
        return false;
      }
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i == 0) {
          continue;
        }
        ExprResult arg = emitExpr(stmt.args[i], state, error);
        if (!error.empty()) {
          return false;
        }
        appendIndented(out, arg.prelude, indent);
        out += indent + arg.code + ";\n";
      }
      return true;
    }
    if (isSimpleCallName(stmt, "print") || isSimpleCallName(stmt, "print_line") || isSimpleCallName(stmt, "print_error") ||
        isSimpleCallName(stmt, "print_line_error")) {
      const std::string name = normalizeName(stmt);
      if (hasNamedArguments(stmt.argNames)) {
        error = "glsl backend requires " + name + " to use positional arguments";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error = "glsl backend does not support template arguments on " + name;
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = "glsl backend does not support block arguments on " + name;
        return false;
      }
      if (stmt.args.size() != 1) {
        error = "glsl backend requires " + name + " with exactly one argument";
        return false;
      }
      const Expr &argExpr = stmt.args.front();
      if (argExpr.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      ExprResult arg = emitExpr(argExpr, state, error);
      if (!error.empty()) {
        return false;
      }
      appendIndented(out, arg.prelude, indent);
      out += indent + arg.code + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "if")) {
      if (stmt.args.size() < 2 || stmt.args.size() > 3) {
        error = "glsl backend requires if(cond, then() { ... }, else() { ... })";
        return false;
      }
      ExprResult cond = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      if (cond.type != GlslType::Bool) {
        error = "glsl backend requires if condition to be bool";
        return false;
      }
      appendIndented(out, cond.prelude, indent);
      const Expr &thenExpr = stmt.args[1];
      auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
          return false;
        }
        if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
          return false;
        }
        return true;
      };
      if (!isIfBlockEnvelope(thenExpr)) {
        error = "glsl backend requires then() { ... } block";
        return false;
      }
      const Expr *elseExpr = nullptr;
      if (stmt.args.size() == 3) {
        elseExpr = &stmt.args[2];
        if (!isIfBlockEnvelope(*elseExpr)) {
          error = "glsl backend requires else() { ... } block";
          return false;
        }
      }
      out += indent + "if (" + cond.code + ") {\n";
      if (!emitBlock(thenExpr.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}";
      if (elseExpr) {
        out += " else {\n";
        if (!emitBlock(elseExpr->bodyArguments, state, out, error, indent + "  ")) {
          return false;
        }
        out += indent + "}";
      }
      out += "\n";
      return true;
    }
    auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
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
    };
    if (isSimpleCallName(stmt, "loop")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires loop(count, body() { ... })";
        return false;
      }
      const Expr &countExpr = stmt.args[0];
      const Expr &bodyExpr = stmt.args[1];
      if (!isLoopBlockEnvelope(bodyExpr)) {
        error = "glsl backend requires loop body block";
        return false;
      }
      ExprResult count = emitExpr(countExpr, state, error);
      if (!error.empty()) {
        return false;
      }
      if (!isIntegerType(count.type)) {
        error = "glsl backend requires loop count to be integer";
        return false;
      }
      noteTypeExtensions(count.type, state);
      appendIndented(out, count.prelude, indent);
      const std::string counterName = allocTempName(state, "_ps_loop_");
      const std::string counterType = glslTypeName(count.type);
      const std::string zeroLiteral = literalForType(count.type, 0);
      const std::string oneLiteral = literalForType(count.type, 1);
      const std::string cond =
          isUnsignedInteger(count.type) ? (counterName + " != " + zeroLiteral)
                                         : (counterName + " > " + zeroLiteral);

      auto savedLocals = state.locals;
      state.locals[counterName] = {count.type, true};
      out += indent + "{\n";
      out += indent + "  " + counterType + " " + counterName + " = " + count.code + ";\n";
      out += indent + "  while (" + cond + ") {\n";
      if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      out += indent + "    " + counterName + " = " + counterName + " - " + oneLiteral + ";\n";
      out += indent + "  }\n";
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    if (isSimpleCallName(stmt, "repeat")) {
      if (stmt.args.size() != 1 || !stmt.hasBodyArguments) {
        error = "glsl backend requires repeat(count) { ... }";
        return false;
      }
      ExprResult count = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      GlslType counterType = count.type;
      if (counterType == GlslType::Bool) {
        counterType = GlslType::Int;
      }
      if (!isIntegerType(counterType)) {
        error = "glsl backend requires repeat count to be integer or bool";
        return false;
      }
      noteTypeExtensions(counterType, state);
      appendIndented(out, count.prelude, indent);
      ExprResult countCast = castExpr(count, counterType);
      const std::string counterName = allocTempName(state, "_ps_repeat_");
      const std::string counterTypeName = glslTypeName(counterType);
      const std::string zeroLiteral = literalForType(counterType, 0);
      const std::string oneLiteral = literalForType(counterType, 1);
      const std::string cond =
          isUnsignedInteger(counterType) ? (counterName + " != " + zeroLiteral)
                                          : (counterName + " > " + zeroLiteral);

      auto savedLocals = state.locals;
      state.locals[counterName] = {counterType, true};
      out += indent + "{\n";
      out += indent + "  " + counterTypeName + " " + counterName + " = " + countCast.code + ";\n";
      out += indent + "  while (" + cond + ") {\n";
      if (!emitBlock(stmt.bodyArguments, state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      out += indent + "    " + counterName + " = " + counterName + " - " + oneLiteral + ";\n";
      out += indent + "  }\n";
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    if (isSimpleCallName(stmt, "while")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires while(cond, body() { ... })";
        return false;
      }
      ExprResult cond = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      if (cond.type != GlslType::Bool) {
        error = "glsl backend requires while condition to be bool";
        return false;
      }
      const Expr &bodyExpr = stmt.args[1];
      if (!isLoopBlockEnvelope(bodyExpr)) {
        error = "glsl backend requires while body block";
        return false;
      }
      if (cond.prelude.empty()) {
        out += indent + "while (" + cond.code + ") {\n";
        if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "  ")) {
          return false;
        }
        out += indent + "}\n";
        return true;
      }
      out += indent + "while (true) {\n";
      appendIndented(out, cond.prelude, indent + "  ");
      out += indent + "  if (!(" + cond.code + ")) { break; }\n";
      if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}\n";
      return true;
    }
    if (isSimpleCallName(stmt, "for")) {
      if (stmt.args.size() != 4) {
        error = "glsl backend requires for(init, cond, step, body() { ... })";
        return false;
      }
      auto savedLocals = state.locals;
      out += indent + "{\n";
      if (!emitStatement(stmt.args[0], state, out, error, indent + "  ")) {
        state.locals = savedLocals;
        return false;
      }
      const Expr &condExpr = stmt.args[1];
      const Expr &stepExpr = stmt.args[2];
      const Expr &bodyExpr = stmt.args[3];
      if (!isLoopBlockEnvelope(bodyExpr)) {
        error = "glsl backend requires for body block";
        state.locals = savedLocals;
        return false;
      }
      if (condExpr.isBinding) {
        out += indent + "  while (true) {\n";
        if (!emitStatement(condExpr, state, out, error, indent + "    ")) {
          state.locals = savedLocals;
          return false;
        }
        auto condIt = state.locals.find(condExpr.name);
        if (condIt == state.locals.end() || condIt->second.type != GlslType::Bool) {
          error = "glsl backend requires for condition to be bool";
          state.locals = savedLocals;
          return false;
        }
        out += indent + "    if (!" + condExpr.name + ") { break; }\n";
        if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
          state.locals = savedLocals;
          return false;
        }
        if (!emitStatement(stepExpr, state, out, error, indent + "    ")) {
          state.locals = savedLocals;
          return false;
        }
        out += indent + "  }\n";
      } else {
        ExprResult cond = emitExpr(condExpr, state, error);
        if (!error.empty()) {
          state.locals = savedLocals;
          return false;
        }
        if (cond.type != GlslType::Bool) {
          error = "glsl backend requires for condition to be bool";
          state.locals = savedLocals;
          return false;
        }
        if (cond.prelude.empty()) {
          out += indent + "  while (" + cond.code + ") {\n";
          if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          if (!emitStatement(stepExpr, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          out += indent + "  }\n";
        } else {
          out += indent + "  while (true) {\n";
          appendIndented(out, cond.prelude, indent + "    ");
          out += indent + "    if (!(" + cond.code + ")) { break; }\n";
          if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          if (!emitStatement(stepExpr, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          out += indent + "  }\n";
        }
      }
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    ExprResult exprResult = emitExpr(stmt, state, error);
    if (!error.empty()) {
      return false;
    }
    appendIndented(out, exprResult.prelude, indent);
    out += indent + exprResult.code + ";\n";
    return true;
  }
  error = "glsl backend encountered unsupported statement";
  return false;
}

} // namespace primec::glsl_emitter
