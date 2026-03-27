#pragma once

#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace primec::vm_debug_dap_detail {

std::string jsonEscape(std::string_view text);
std::string trimWhitespace(std::string_view text);
std::string lowercase(std::string_view text);
std::string fileNameFromPath(std::string_view path);

struct JsonValue {
  enum class Kind : uint8_t {
    Null,
    Bool,
    Number,
    String,
    Object,
    Array,
  };

  Kind kind = Kind::Null;
  bool boolValue = false;
  int64_t numberValue = 0;
  std::string stringValue;
  std::vector<std::pair<std::string, JsonValue>> objectValue;
  std::vector<JsonValue> arrayValue;

  const JsonValue *find(std::string_view key) const;
};

bool parseJsonPayload(std::string_view text, JsonValue &out, std::string &error);

const JsonValue *jsonObjectField(const JsonValue *object, std::string_view key);
const JsonValue *jsonObjectField(const JsonValue &object, std::string_view key);
bool jsonNumberField(const JsonValue *object, std::string_view key, int64_t &out);
bool jsonNumberField(const JsonValue &object, std::string_view key, int64_t &out);
bool jsonStringField(const JsonValue *object, std::string_view key, std::string &out);
bool jsonStringField(const JsonValue &object, std::string_view key, std::string &out);

std::optional<std::pair<size_t, size_t>> parseInstructionReference(std::string_view value);

bool parseDapFrame(std::istream &input, std::string &payload, bool &eof, std::string &error);
void writeDapFrame(std::ostream &output, std::string_view payload);

std::string buildResponse(uint64_t seq,
                          int64_t requestSeq,
                          std::string_view command,
                          bool success,
                          std::string_view body,
                          std::string_view message);
std::string buildEvent(uint64_t seq, std::string_view event, std::string_view body);

} // namespace primec::vm_debug_dap_detail
