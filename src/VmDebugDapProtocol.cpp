#include "VmDebugDapProtocol.h"

#include <cctype>
#include <limits>

namespace primec::vm_debug_dap_detail {
namespace {

class JsonParser {
public:
  bool parse(std::string_view text, JsonValue &out, std::string &error) {
    text_ = text;
    index_ = 0;
    if (!parseValue(out, error)) {
      return false;
    }
    skipWhitespace();
    if (index_ != text_.size()) {
      error = "trailing characters after JSON payload";
      return false;
    }
    return true;
  }

private:
  bool parseValue(JsonValue &out, std::string &error) {
    skipWhitespace();
    if (index_ >= text_.size()) {
      error = "unexpected end of JSON payload";
      return false;
    }
    const char c = text_[index_];
    if (c == 'n') {
      return parseNull(out, error);
    }
    if (c == 't' || c == 'f') {
      return parseBool(out, error);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
      return parseNumber(out, error);
    }
    if (c == '"') {
      return parseString(out, error);
    }
    if (c == '{') {
      return parseObject(out, error);
    }
    if (c == '[') {
      return parseArray(out, error);
    }
    error = "unexpected JSON token";
    return false;
  }

  bool parseNull(JsonValue &out, std::string &error) {
    if (!consumeLiteral("null")) {
      error = "invalid null literal";
      return false;
    }
    out = {};
    out.kind = JsonValue::Kind::Null;
    return true;
  }

  bool parseBool(JsonValue &out, std::string &error) {
    if (consumeLiteral("true")) {
      out = {};
      out.kind = JsonValue::Kind::Bool;
      out.boolValue = true;
      return true;
    }
    if (consumeLiteral("false")) {
      out = {};
      out.kind = JsonValue::Kind::Bool;
      out.boolValue = false;
      return true;
    }
    error = "invalid boolean literal";
    return false;
  }

  bool parseNumber(JsonValue &out, std::string &error) {
    size_t start = index_;
    bool negative = false;
    if (text_[index_] == '-') {
      negative = true;
      ++index_;
    }
    if (index_ >= text_.size() || std::isdigit(static_cast<unsigned char>(text_[index_])) == 0) {
      error = "invalid number literal";
      return false;
    }
    int64_t value = 0;
    while (index_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[index_])) != 0) {
      const int digit = text_[index_] - '0';
      if (value > (std::numeric_limits<int64_t>::max() - digit) / 10) {
        error = "number literal out of range";
        return false;
      }
      value = value * 10 + digit;
      ++index_;
    }
    if (index_ < text_.size() && (text_[index_] == '.' || text_[index_] == 'e' || text_[index_] == 'E')) {
      error = "floating-point JSON numbers are unsupported in DAP routing";
      return false;
    }
    if (negative) {
      value = -value;
    }
    out = {};
    out.kind = JsonValue::Kind::Number;
    out.numberValue = value;
    if (index_ == start) {
      error = "invalid number literal";
      return false;
    }
    return true;
  }

  bool parseString(JsonValue &out, std::string &error) {
    std::string parsed;
    if (!parseStringValue(parsed, error)) {
      return false;
    }
    out = {};
    out.kind = JsonValue::Kind::String;
    out.stringValue = std::move(parsed);
    return true;
  }

  bool parseStringValue(std::string &out, std::string &error) {
    if (!consumeChar('"')) {
      error = "invalid string literal start";
      return false;
    }
    out.clear();
    while (index_ < text_.size()) {
      const char c = text_[index_++];
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (index_ >= text_.size()) {
          error = "invalid string escape sequence";
          return false;
        }
        const char escaped = text_[index_++];
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            out.push_back(escaped);
            break;
          case 'b':
            out.push_back('\b');
            break;
          case 'f':
            out.push_back('\f');
            break;
          case 'n':
            out.push_back('\n');
            break;
          case 'r':
            out.push_back('\r');
            break;
          case 't':
            out.push_back('\t');
            break;
          case 'u': {
            if (index_ + 4 > text_.size()) {
              error = "short unicode escape in JSON string";
              return false;
            }
            uint32_t codePoint = 0;
            for (size_t i = 0; i < 4; ++i) {
              const char hex = text_[index_ + i];
              codePoint <<= 4;
              if (hex >= '0' && hex <= '9') {
                codePoint |= static_cast<uint32_t>(hex - '0');
              } else if (hex >= 'a' && hex <= 'f') {
                codePoint |= static_cast<uint32_t>(hex - 'a' + 10);
              } else if (hex >= 'A' && hex <= 'F') {
                codePoint |= static_cast<uint32_t>(hex - 'A' + 10);
              } else {
                error = "invalid unicode escape in JSON string";
                return false;
              }
            }
            index_ += 4;
            if (codePoint <= 0x7f) {
              out.push_back(static_cast<char>(codePoint));
            } else {
              out.push_back('?');
            }
            break;
          }
          default:
            error = "unsupported JSON string escape";
            return false;
        }
        continue;
      }
      if (static_cast<unsigned char>(c) < 0x20) {
        error = "control characters are invalid in JSON strings";
        return false;
      }
      out.push_back(c);
    }
    error = "unterminated JSON string";
    return false;
  }

  bool parseArray(JsonValue &out, std::string &error) {
    if (!consumeChar('[')) {
      error = "invalid array start";
      return false;
    }
    out = {};
    out.kind = JsonValue::Kind::Array;
    skipWhitespace();
    if (consumeChar(']')) {
      return true;
    }
    while (true) {
      JsonValue element;
      if (!parseValue(element, error)) {
        return false;
      }
      out.arrayValue.push_back(std::move(element));
      skipWhitespace();
      if (consumeChar(']')) {
        return true;
      }
      if (!consumeChar(',')) {
        error = "expected ',' or ']' in JSON array";
        return false;
      }
    }
  }

  bool parseObject(JsonValue &out, std::string &error) {
    if (!consumeChar('{')) {
      error = "invalid object start";
      return false;
    }
    out = {};
    out.kind = JsonValue::Kind::Object;
    skipWhitespace();
    if (consumeChar('}')) {
      return true;
    }
    while (true) {
      std::string key;
      if (!parseStringValue(key, error)) {
        return false;
      }
      skipWhitespace();
      if (!consumeChar(':')) {
        error = "expected ':' after JSON object key";
        return false;
      }
      JsonValue value;
      if (!parseValue(value, error)) {
        return false;
      }
      out.objectValue.push_back({std::move(key), std::move(value)});
      skipWhitespace();
      if (consumeChar('}')) {
        return true;
      }
      if (!consumeChar(',')) {
        error = "expected ',' or '}' in JSON object";
        return false;
      }
      skipWhitespace();
    }
  }

  bool consumeLiteral(std::string_view literal) {
    if (index_ + literal.size() > text_.size()) {
      return false;
    }
    if (text_.substr(index_, literal.size()) != literal) {
      return false;
    }
    index_ += literal.size();
    return true;
  }

  bool consumeChar(char expected) {
    skipWhitespace();
    if (index_ >= text_.size() || text_[index_] != expected) {
      return false;
    }
    ++index_;
    return true;
  }

  void skipWhitespace() {
    while (index_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[index_])) != 0) {
      ++index_;
    }
  }

  std::string_view text_;
  size_t index_ = 0;
};

} // namespace

std::string jsonEscape(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (const char c : text) {
    switch (c) {
      case '\"':
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
          out += "\\u00";
          static constexpr char Hex[] = "0123456789abcdef";
          out.push_back(Hex[(static_cast<unsigned char>(c) >> 4) & 0x0f]);
          out.push_back(Hex[static_cast<unsigned char>(c) & 0x0f]);
        } else {
          out.push_back(c);
        }
        break;
    }
  }
  return out;
}

std::string trimWhitespace(std::string_view text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return std::string(text.substr(start, end - start));
}

std::string lowercase(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

std::string fileNameFromPath(std::string_view path) {
  size_t slash = path.find_last_of("/\\");
  if (slash == std::string_view::npos || slash + 1 >= path.size()) {
    return std::string(path);
  }
  return std::string(path.substr(slash + 1));
}

const JsonValue *JsonValue::find(std::string_view key) const {
  if (kind != Kind::Object) {
    return nullptr;
  }
  for (const auto &entry : objectValue) {
    if (entry.first == key) {
      return &entry.second;
    }
  }
  return nullptr;
}

bool parseJsonPayload(std::string_view text, JsonValue &out, std::string &error) {
  JsonParser parser;
  return parser.parse(text, out, error);
}

const JsonValue *jsonObjectField(const JsonValue *object, std::string_view key) {
  if (object == nullptr || object->kind != JsonValue::Kind::Object) {
    return nullptr;
  }
  return object->find(key);
}

const JsonValue *jsonObjectField(const JsonValue &object, std::string_view key) {
  return jsonObjectField(&object, key);
}

bool jsonNumberField(const JsonValue *object, std::string_view key, int64_t &out) {
  const JsonValue *value = jsonObjectField(object, key);
  if (value == nullptr || value->kind != JsonValue::Kind::Number) {
    return false;
  }
  out = value->numberValue;
  return true;
}

bool jsonNumberField(const JsonValue &object, std::string_view key, int64_t &out) {
  return jsonNumberField(&object, key, out);
}

bool jsonStringField(const JsonValue *object, std::string_view key, std::string &out) {
  const JsonValue *value = jsonObjectField(object, key);
  if (value == nullptr || value->kind != JsonValue::Kind::String) {
    return false;
  }
  out = value->stringValue;
  return true;
}

bool jsonStringField(const JsonValue &object, std::string_view key, std::string &out) {
  return jsonStringField(&object, key, out);
}

std::optional<std::pair<size_t, size_t>> parseInstructionReference(std::string_view value) {
  if (value.size() < 4 || value[0] != 'f') {
    return std::nullopt;
  }
  const size_t colon = value.find(':');
  if (colon == std::string_view::npos || colon < 2 || colon + 3 > value.size()) {
    return std::nullopt;
  }
  if (value.substr(colon + 1, 2) != "ip") {
    return std::nullopt;
  }
  try {
    size_t consumed = 0;
    const size_t functionIndex = std::stoull(std::string(value.substr(1, colon - 1)), &consumed);
    if (consumed != colon - 1) {
      return std::nullopt;
    }
    consumed = 0;
    const size_t instructionPointer = std::stoull(std::string(value.substr(colon + 3)), &consumed);
    if (consumed != value.size() - (colon + 3)) {
      return std::nullopt;
    }
    return std::make_pair(functionIndex, instructionPointer);
  } catch (...) {
    return std::nullopt;
  }
}

bool parseDapFrame(std::istream &input, std::string &payload, bool &eof, std::string &error) {
  payload.clear();
  eof = false;

  std::string line;
  bool sawHeaderLine = false;
  size_t contentLength = 0;
  bool hasContentLength = false;
  while (true) {
    if (!std::getline(input, line)) {
      if (!sawHeaderLine) {
        eof = true;
        return true;
      }
      error = "unexpected end of input while reading DAP headers";
      return false;
    }
    sawHeaderLine = true;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    const size_t colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    const std::string key = lowercase(std::string_view(line).substr(0, colon));
    const std::string value = trimWhitespace(std::string_view(line).substr(colon + 1));
    if (key != "content-length") {
      continue;
    }
    try {
      size_t consumed = 0;
      const unsigned long parsed = std::stoul(value, &consumed);
      if (consumed != value.size()) {
        error = "invalid Content-Length header value";
        return false;
      }
      contentLength = static_cast<size_t>(parsed);
      hasContentLength = true;
    } catch (...) {
      error = "invalid Content-Length header value";
      return false;
    }
  }

  if (!hasContentLength) {
    error = "missing Content-Length header";
    return false;
  }
  payload.assign(contentLength, '\0');
  input.read(payload.data(), static_cast<std::streamsize>(contentLength));
  if (input.gcount() != static_cast<std::streamsize>(contentLength)) {
    error = "unexpected end of input while reading DAP payload";
    return false;
  }
  return true;
}

void writeDapFrame(std::ostream &output, std::string_view payload) {
  output << "Content-Length: " << payload.size() << "\r\n\r\n";
  output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  output.flush();
}

std::string buildResponse(uint64_t seq,
                          int64_t requestSeq,
                          std::string_view command,
                          bool success,
                          std::string_view body,
                          std::string_view message) {
  std::string json = std::string("{\"seq\":") + std::to_string(seq) +
                     ",\"type\":\"response\",\"request_seq\":" + std::to_string(requestSeq) + ",\"success\":" +
                     (success ? "true" : "false") + ",\"command\":\"" + jsonEscape(command) + "\"";
  if (!message.empty()) {
    json += ",\"message\":\"" + jsonEscape(message) + "\"";
  }
  if (!body.empty()) {
    json += ",\"body\":" + std::string(body);
  }
  json += "}";
  return json;
}

std::string buildEvent(uint64_t seq, std::string_view event, std::string_view body) {
  std::string json =
      std::string("{\"seq\":") + std::to_string(seq) + ",\"type\":\"event\",\"event\":\"" + jsonEscape(event) + "\"";
  if (!body.empty()) {
    json += ",\"body\":" + std::string(body);
  }
  json += "}";
  return json;
}

} // namespace primec::vm_debug_dap_detail
