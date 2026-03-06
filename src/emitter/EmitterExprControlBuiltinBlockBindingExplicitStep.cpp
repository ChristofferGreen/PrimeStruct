#include "EmitterExprControlBuiltinBlockBindingExplicitStep.h"

#include <sstream>

#include "EmitterHelpers.h"

namespace primec::emitter {

EmitterExprControlBuiltinBlockBindingExplicitStepResult runEmitterExprControlBuiltinBlockBindingExplicitStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const std::string &namespacePrefix,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlBuiltinBlockBindingExplicitEmitExprFn &emitExpr) {
  if (!hasExplicitType) {
    return {};
  }

  EmitterExprControlBuiltinBlockBindingExplicitStepResult result;
  result.handled = true;

  std::ostringstream out;
  const std::string type = bindingTypeToCpp(binding, namespacePrefix, importAliases, structTypeMap);
  const bool isReference = binding.typeName == "Reference";

  if (useRef) {
    if (type.rfind("const ", 0) != 0) {
      out << "const " << type << " & " << stmt.name;
    } else {
      out << type << " & " << stmt.name;
    }
  } else {
    out << (needsConst ? "const " : "") << type << " " << stmt.name;
  }

  if (!stmt.args.empty() && emitExpr) {
    if (isReference) {
      out << " = *(" << emitExpr(stmt.args.front()) << ")";
    } else {
      out << " = " << emitExpr(stmt.args.front());
    }
  }

  out << "; ";
  result.emittedStatement = out.str();
  return result;
}

} // namespace primec::emitter
