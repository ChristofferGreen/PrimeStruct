#include "primec/VmDebugDap.h"

#include "primec/VmDebugAdapter.h"

#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace primec {
namespace {

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

  const JsonValue *find(std::string_view key) const {
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
};

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
    const size_t instructionPointer =
        std::stoull(std::string(value.substr(colon + 3)), &consumed);
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
    const std::string key = lowercase(trimWhitespace(std::string_view(line).substr(0, colon)));
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

struct VmDebugDapRuntime {
  VmDebugAdapter adapter;
  bool launched = false;
  bool disconnected = false;
  bool observedProgramExit = false;
  int programExitCode = 0;
  uint64_t outgoingSeq = 1;
};

void appendResponse(std::vector<std::string> &messages,
                    VmDebugDapRuntime &runtime,
                    int64_t requestSeq,
                    std::string_view command,
                    bool success,
                    std::string_view body = {},
                    std::string_view message = {}) {
  messages.push_back(
      buildResponse(runtime.outgoingSeq++, requestSeq, command, success, body, message));
}

void appendEvent(std::vector<std::string> &messages,
                 VmDebugDapRuntime &runtime,
                 std::string_view event,
                 std::string_view body = {}) {
  messages.push_back(buildEvent(runtime.outgoingSeq++, event, body));
}

bool ensureLaunched(const VmDebugDapRuntime &runtime, std::string &error) {
  if (runtime.launched) {
    return true;
  }
  error = "debug adapter session is not launched";
  return false;
}

std::string stoppedBody(std::string_view reason, std::string_view text = {}) {
  std::string body = std::string("{\"reason\":\"") + jsonEscape(reason) + "\",\"threadId\":1,\"allThreadsStopped\":true";
  if (!text.empty()) {
    body += ",\"text\":\"" + jsonEscape(text) + "\"";
  }
  body += "}";
  return body;
}

bool parseSourceBreakpoints(const JsonValue *arguments,
                            std::vector<VmDebugAdapterSourceBreakpoint> &breakpoints,
                            std::string &error) {
  breakpoints.clear();
  if (arguments == nullptr) {
    return true;
  }
  const JsonValue *list = jsonObjectField(arguments, "breakpoints");
  if (list == nullptr) {
    return true;
  }
  if (list->kind != JsonValue::Kind::Array) {
    error = "setBreakpoints arguments.breakpoints must be an array";
    return false;
  }
  for (const JsonValue &entry : list->arrayValue) {
    if (entry.kind != JsonValue::Kind::Object) {
      error = "setBreakpoints entries must be objects";
      return false;
    }
    int64_t line = 0;
    if (!jsonNumberField(entry, "line", line) || line <= 0) {
      error = "setBreakpoints entries require positive line";
      return false;
    }
    VmDebugAdapterSourceBreakpoint breakpoint;
    breakpoint.line = static_cast<uint32_t>(line);
    int64_t column = 0;
    if (jsonNumberField(entry, "column", column) && column > 0) {
      breakpoint.column = static_cast<uint32_t>(column);
    }
    breakpoints.push_back(breakpoint);
  }
  return true;
}

bool parseInstructionBreakpoints(const JsonValue *arguments,
                                 std::vector<std::pair<size_t, size_t>> &breakpoints,
                                 std::string &error) {
  breakpoints.clear();
  if (arguments == nullptr) {
    return true;
  }
  const JsonValue *list = jsonObjectField(arguments, "breakpoints");
  if (list == nullptr) {
    list = jsonObjectField(arguments, "instructionBreakpoints");
  }
  if (list == nullptr) {
    return true;
  }
  if (list->kind != JsonValue::Kind::Array) {
    error = "setInstructionBreakpoints breakpoints must be an array";
    return false;
  }

  for (const JsonValue &entry : list->arrayValue) {
    if (entry.kind != JsonValue::Kind::Object) {
      error = "setInstructionBreakpoints entries must be objects";
      return false;
    }
    int64_t functionIndex = -1;
    int64_t instructionPointer = -1;
    if (jsonNumberField(entry, "functionIndex", functionIndex) &&
        jsonNumberField(entry, "instructionPointer", instructionPointer)) {
      if (functionIndex < 0 || instructionPointer < 0) {
        error = "setInstructionBreakpoints functionIndex/instructionPointer must be non-negative";
        return false;
      }
      breakpoints.push_back({static_cast<size_t>(functionIndex),
                             static_cast<size_t>(instructionPointer)});
      continue;
    }

    std::string instructionReference;
    if (!jsonStringField(entry, "instructionReference", instructionReference)) {
      error = "setInstructionBreakpoints entry requires functionIndex/instructionPointer or instructionReference";
      return false;
    }
    std::optional<std::pair<size_t, size_t>> parsed = parseInstructionReference(instructionReference);
    if (!parsed.has_value()) {
      error = "invalid instructionReference format (expected f<function>:ip<instruction>)";
      return false;
    }
    int64_t offset = 0;
    if (jsonNumberField(entry, "offset", offset)) {
      if (offset < 0) {
        error = "instruction breakpoint offset must be non-negative";
        return false;
      }
      parsed->second += static_cast<size_t>(offset);
    }
    breakpoints.push_back(*parsed);
  }
  return true;
}

std::string encodeSetBreakpointsBody(const std::vector<VmDebugAdapterBreakpointResult> &results) {
  std::string body = "{\"breakpoints\":[";
  for (size_t i = 0; i < results.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &result = results[i];
    body += std::string("{\"verified\":") + (result.verified ? "true" : "false") + ",\"line\":" +
            std::to_string(result.line) + ",\"column\":" + std::to_string(result.column);
    if (!result.message.empty()) {
      body += ",\"message\":\"" + jsonEscape(result.message) + "\"";
    }
    body += "}";
  }
  body += "]}";
  return body;
}

std::string encodeInstructionBreakpointsBody(const std::vector<std::pair<size_t, size_t>> &breakpoints) {
  std::string body = "{\"breakpoints\":[";
  for (size_t i = 0; i < breakpoints.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &bp = breakpoints[i];
    body += std::string("{\"verified\":true,\"instructionReference\":\"f") + std::to_string(bp.first) + ":ip" +
            std::to_string(bp.second) + "\"}";
  }
  body += "]}";
  return body;
}

std::string encodeThreadsBody(const std::vector<VmDebugAdapterThread> &threads) {
  std::string body = "{\"threads\":[";
  for (size_t i = 0; i < threads.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    body += std::string("{\"id\":") + std::to_string(threads[i].id) + ",\"name\":\"" +
            jsonEscape(threads[i].name) + "\"}";
  }
  body += "]}";
  return body;
}

std::string encodeStackTraceBody(const std::vector<VmDebugAdapterStackFrame> &frames, std::string_view sourcePath) {
  const std::string sourceName = fileNameFromPath(sourcePath);
  std::string body = "{\"stackFrames\":[";
  for (size_t i = 0; i < frames.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &frame = frames[i];
    body += std::string("{\"id\":") + std::to_string(frame.id) + ",\"name\":\"" + jsonEscape(frame.name) +
            "\",\"line\":" + std::to_string(frame.line) + ",\"column\":" + std::to_string(frame.column) +
            ",\"instructionPointer\":" + std::to_string(frame.instructionPointer) + ",\"source\":{\"name\":\"" +
            jsonEscape(sourceName) + "\",\"path\":\"" + jsonEscape(sourcePath) + "\"}}";
  }
  body += "],\"totalFrames\":" + std::to_string(frames.size()) + "}";
  return body;
}

std::string encodeScopesBody(const std::vector<VmDebugAdapterScope> &scopes) {
  std::string body = "{\"scopes\":[";
  for (size_t i = 0; i < scopes.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &scope = scopes[i];
    body += std::string("{\"name\":\"") + jsonEscape(scope.name) + "\",\"variablesReference\":" +
            std::to_string(scope.variablesReference) + ",\"namedVariables\":" +
            std::to_string(scope.namedVariables) + ",\"expensive\":" +
            (scope.expensive ? "true" : "false") + "}";
  }
  body += "]}";
  return body;
}

std::string encodeVariablesBody(const std::vector<VmDebugAdapterVariable> &variables) {
  std::string body = "{\"variables\":[";
  for (size_t i = 0; i < variables.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &variable = variables[i];
    body += std::string("{\"name\":\"") + jsonEscape(variable.name) + "\",\"value\":\"" + jsonEscape(variable.value) +
            "\",\"type\":\"" + jsonEscape(variable.typeName) + "\",\"variablesReference\":" +
            std::to_string(variable.variablesReference) + "}";
  }
  body += "]}";
  return body;
}

void appendStopEvents(std::vector<std::string> &messages,
                      VmDebugDapRuntime &runtime,
                      const VmDebugAdapterStopEvent &stopEvent) {
  if (stopEvent.reason == VmDebugStopReason::Exit) {
    runtime.observedProgramExit = true;
    runtime.programExitCode = static_cast<int>(static_cast<int32_t>(runtime.adapter.snapshot().result));
    appendEvent(messages,
                runtime,
                "exited",
                std::string("{\"exitCode\":") + std::to_string(runtime.programExitCode) + "}");
    appendEvent(messages, runtime, "terminated", "{}");
    return;
  }
  appendEvent(messages, runtime, "stopped", stoppedBody(stopEvent.dapReason, stopEvent.message));
}

void appendFailure(std::vector<std::string> &messages,
                   VmDebugDapRuntime &runtime,
                   int64_t requestSeq,
                   std::string_view command,
                   std::string_view message) {
  appendResponse(messages, runtime, requestSeq, command, false, "{}", message);
}

bool handleRequest(const JsonValue &request,
                   const IrModule &module,
                   const std::vector<std::string_view> &args,
                   std::string_view sourcePath,
                   VmDebugDapRuntime &runtime,
                   std::vector<std::string> &messages,
                   std::string &error) {
  messages.clear();
  if (request.kind != JsonValue::Kind::Object) {
    error = "DAP request payload must be a JSON object";
    return false;
  }

  int64_t requestSeq = 0;
  jsonNumberField(request, "seq", requestSeq);
  std::string command;
  if (!jsonStringField(request, "command", command)) {
    appendFailure(messages, runtime, requestSeq, "<unknown>", "missing request command");
    return true;
  }
  const JsonValue *arguments = jsonObjectField(request, "arguments");

  if (command == "initialize") {
    appendResponse(messages,
                   runtime,
                   requestSeq,
                   command,
                   true,
                   "{\"supportsConfigurationDoneRequest\":true,\"supportsInstructionBreakpoints\":true,"
                   "\"supportsSteppingGranularity\":false}",
                   {});
    return true;
  }

  if (command == "launch") {
    if (runtime.launched) {
      appendFailure(messages, runtime, requestSeq, command, "debug adapter session is already launched");
      return true;
    }
    std::string launchError;
    if (!runtime.adapter.launch(module, launchError, args)) {
      appendFailure(messages, runtime, requestSeq, command, launchError);
      return true;
    }
    runtime.launched = true;
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    appendEvent(messages, runtime, "initialized", "{}");
    appendEvent(messages, runtime, "stopped", stoppedBody("entry"));
    return true;
  }

  if (command == "configurationDone") {
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    return true;
  }

  if (command == "setExceptionBreakpoints") {
    appendResponse(messages, runtime, requestSeq, command, true, "{\"breakpoints\":[]}", {});
    return true;
  }

  if (command == "disconnect") {
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    runtime.disconnected = true;
    return true;
  }

  if (command == "setBreakpoints") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<VmDebugAdapterSourceBreakpoint> breakpoints;
    if (!parseSourceBreakpoints(arguments, breakpoints, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<VmDebugAdapterBreakpointResult> results;
    if (!runtime.adapter.setSourceBreakpoints(breakpoints, results, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeSetBreakpointsBody(results), {});
    return true;
  }

  if (command == "setInstructionBreakpoints") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<std::pair<size_t, size_t>> breakpoints;
    if (!parseInstructionBreakpoints(arguments, breakpoints, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    if (!runtime.adapter.setInstructionBreakpoints(breakpoints, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages,
                   runtime,
                   requestSeq,
                   command,
                   true,
                   encodeInstructionBreakpointsBody(breakpoints),
                   {});
    return true;
  }

  if (command == "threads") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<VmDebugAdapterThread> threads;
    if (!runtime.adapter.threads(threads, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeThreadsBody(threads), {});
    return true;
  }

  if (command == "stackTrace") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    int64_t threadId = 1;
    jsonNumberField(arguments, "threadId", threadId);
    std::vector<VmDebugAdapterStackFrame> frames;
    if (!runtime.adapter.stackTrace(threadId, frames, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeStackTraceBody(frames, sourcePath), {});
    return true;
  }

  if (command == "scopes") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    int64_t frameId = 0;
    if (!jsonNumberField(arguments, "frameId", frameId)) {
      appendFailure(messages, runtime, requestSeq, command, "scopes requires arguments.frameId");
      return true;
    }
    std::vector<VmDebugAdapterScope> scopes;
    if (!runtime.adapter.scopes(frameId, scopes, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeScopesBody(scopes), {});
    return true;
  }

  if (command == "variables") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    int64_t variablesReference = 0;
    if (!jsonNumberField(arguments, "variablesReference", variablesReference)) {
      appendFailure(messages, runtime, requestSeq, command, "variables requires arguments.variablesReference");
      return true;
    }
    std::vector<VmDebugAdapterVariable> variables;
    if (!runtime.adapter.variables(variablesReference, variables, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeVariablesBody(variables), {});
    return true;
  }

  if (command == "continue") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    VmDebugAdapterStopEvent stopEvent;
    if (!runtime.adapter.continueExecution(stopEvent, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, "{\"allThreadsContinued\":true}", {});
    appendStopEvents(messages, runtime, stopEvent);
    return true;
  }

  if (command == "next") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    VmDebugAdapterStopEvent stopEvent;
    if (!runtime.adapter.step(stopEvent, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    appendStopEvents(messages, runtime, stopEvent);
    return true;
  }

  if (command == "pause") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    if (!runtime.adapter.pause(error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    return true;
  }

  appendFailure(messages, runtime, requestSeq, command, "unsupported request command: " + command);
  return true;
}

} // namespace

bool runVmDebugDapSession(const IrModule &module,
                          const std::vector<std::string_view> &args,
                          std::string_view sourcePath,
                          std::istream &input,
                          std::ostream &output,
                          VmDebugDapRunResult &result,
                          std::string &error) {
  result = {};
  VmDebugDapRuntime runtime;

  while (!runtime.disconnected) {
    std::string payload;
    bool eof = false;
    if (!parseDapFrame(input, payload, eof, error)) {
      return false;
    }
    if (eof) {
      break;
    }

    JsonValue request;
    JsonParser parser;
    if (!parser.parse(payload, request, error)) {
      return false;
    }

    std::vector<std::string> responses;
    if (!handleRequest(request, module, args, sourcePath, runtime, responses, error)) {
      return false;
    }
    for (const std::string &response : responses) {
      writeDapFrame(output, response);
    }
  }

  result.disconnected = runtime.disconnected;
  result.observedProgramExit = runtime.observedProgramExit;
  result.programExitCode = runtime.programExitCode;
  return true;
}

} // namespace primec
