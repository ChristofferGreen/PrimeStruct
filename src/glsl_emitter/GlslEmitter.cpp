#include "primec/GlslEmitter.h"

#include <string>

namespace primec {

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
  if (!entryDef->parameters.empty()) {
    error = "glsl backend requires entry definition to have no parameters";
    return false;
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
  bool sawReturn = false;
  for (const auto &stmt : entryDef->statements) {
    if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && stmt.name == "return") {
      if (!stmt.args.empty()) {
        error = "glsl backend requires entry definition to return void";
        return false;
      }
      if (sawReturn) {
        error = "glsl backend only supports a single return statement";
        return false;
      }
      sawReturn = true;
      continue;
    }
    error = "glsl backend only supports empty entry definitions";
    return false;
  }
  out.clear();
  out += "#version 450\n";
  out += "void main() {\n";
  out += "}\n";
  return true;
}

} // namespace primec
