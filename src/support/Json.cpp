#include "primec/support/Json.h"

namespace primec {

std::string jsonEscape(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (const char c : text) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        static constexpr char Hex[] = "0123456789abcdef";
        out += "\\u00";
        out.push_back(Hex[(static_cast<unsigned char>(c) >> 4) & 0x0f]);
        out.push_back(Hex[static_cast<unsigned char>(c) & 0x0f]);
      } else {
        out.push_back(c);
      }
    }
  }
  return out;
}

} // namespace primec
