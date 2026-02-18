#include "primec/GlslEmitter.h"

#include "GlslEmitterInternal.h"

namespace primec {
using namespace glsl_emitter;

bool GlslEmitter::emitSource(const Program &program,
                             const std::string &entryPath,
                             std::string &out,
                             std::string &error) const {
  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDef = &def;
      break;
    }
  }
  if (!entryDef) {
    error = "glsl backend requires entry definition " + entryPath;
    return false;
  }
  for (const auto &def : program.definitions) {
    if (!rejectEffectTransforms(def.transforms, def.fullPath, error)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!rejectEffectTransforms(param, def.fullPath, error)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!rejectEffectTransforms(stmt, def.fullPath, error)) {
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      if (!rejectEffectTransforms(*def.returnExpr, def.fullPath, error)) {
        return false;
      }
    }
  }
  for (const auto &exec : program.executions) {
    if (!rejectEffectTransforms(exec.transforms, exec.fullPath, error)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!rejectEffectTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
    for (const auto &bodyArg : exec.bodyArguments) {
      if (!rejectEffectTransforms(bodyArg, exec.fullPath, error)) {
        return false;
      }
    }
  }
  if (!entryDef->parameters.empty()) {
    if (entryDef->parameters.size() != 1 || !isEntryArgsParam(entryDef->parameters.front())) {
      error = "glsl backend requires entry definition to have no parameters or a single array<string> parameter";
      return false;
    }
  }
  bool hasReturnTransform = false;
  bool returnsVoid = false;
  for (const auto &transform : entryDef->transforms) {
    if (transform.name != "return") {
      continue;
    }
    hasReturnTransform = true;
    returnsVoid = transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void";
  }
  if (hasReturnTransform && !returnsVoid) {
    error = "glsl backend requires entry definition to return void";
    return false;
  }
  if (!hasReturnTransform && entryDef->returnExpr.has_value()) {
    error = "glsl backend requires entry definition to return void";
    return false;
  }
  EmitState state;
  std::string body;
  bool sawReturn = false;
  for (const auto &stmt : entryDef->statements) {
    if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && stmt.name == "return") {
      if (sawReturn) {
        error = "glsl backend only supports a single return statement";
        return false;
      }
      sawReturn = true;
    }
    if (!emitStatement(stmt, state, body, error, "  ")) {
      return false;
    }
  }
  out.clear();
  out += "#version 450\n";
  if (state.needsInt64Ext) {
    out += "#extension GL_ARB_gpu_shader_int64 : require\n";
  }
  if (state.needsFp64Ext) {
    out += "#extension GL_ARB_gpu_shader_fp64 : require\n";
  }
  if (state.needsIntPow) {
    out += emitPowHelper(GlslType::Int, true);
  }
  if (state.needsUIntPow) {
    out += emitPowHelper(GlslType::UInt, false);
  }
  if (state.needsInt64Pow) {
    out += emitPowHelper(GlslType::Int64, true);
  }
  if (state.needsUInt64Pow) {
    out += emitPowHelper(GlslType::UInt64, false);
  }
  out += "void main() {\n";
  out += body;
  out += "}\n";
  return true;
}

} // namespace primec
