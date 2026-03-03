#include "IrLowererStringLiteralHelpers.h"

#include <utility>

#include "primec/StringLiteral.h"

namespace primec::ir_lowerer {

bool parseLowererStringLiteral(const std::string &text, std::string &decoded, std::string &error) {
  ParsedStringLiteral parsed;
  if (!parseStringLiteralToken(text, parsed, error)) {
    return false;
  }
  if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
    error = "ascii string literal contains non-ASCII characters";
    return false;
  }
  decoded = std::move(parsed.decoded);
  return true;
}

} // namespace primec::ir_lowerer
