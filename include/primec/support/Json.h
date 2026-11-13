#pragma once

#include <string>
#include <string_view>

namespace primec {

// `text` escaped for the inside of a JSON string: `"`, `\\`, the named control characters, and
// `\u00XX` for the other bytes below 0x20.
std::string jsonEscape(std::string_view text);

} // namespace primec
