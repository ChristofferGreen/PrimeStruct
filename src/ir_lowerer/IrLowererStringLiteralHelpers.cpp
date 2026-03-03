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

bool resolveStringTableTarget(const Expr &expr,
                              const LocalMap &localsIn,
                              const std::vector<std::string> &stringTable,
                              const InternStringLiteralFn &internString,
                              int32_t &stringIndexOut,
                              size_t &lengthOut,
                              std::string &error) {
  stringIndexOut = -1;
  lengthOut = 0;
  if (expr.kind == Expr::Kind::StringLiteral) {
    std::string decoded;
    if (!parseLowererStringLiteral(expr.stringValue, decoded, error)) {
      return false;
    }
    stringIndexOut = internString(decoded);
    lengthOut = decoded.size();
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return false;
    }
    if (it->second.valueKind != LocalInfo::ValueKind::String ||
        it->second.stringSource != LocalInfo::StringSource::TableIndex) {
      return false;
    }
    if (it->second.stringIndex < 0) {
      error = "native backend missing string table data for: " + expr.name;
      return false;
    }
    if (static_cast<size_t>(it->second.stringIndex) >= stringTable.size()) {
      error = "native backend encountered invalid string table index";
      return false;
    }
    stringIndexOut = it->second.stringIndex;
    lengthOut = stringTable[static_cast<size_t>(stringIndexOut)].size();
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
