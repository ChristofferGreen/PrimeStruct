#include "primec/pipeline/BuildGraphJson.h"

#include <cctype>
#include <cstdlib>
#include <memory>
#include <utility>

namespace primec {

namespace {

// A parsed JSON value. Only what the graph export uses is kept: strings, numbers, booleans, arrays and objects.
struct JsonValue {
  enum class Kind { Null, Bool, Number, String, Array, Object };
  Kind kind = Kind::Null;
  bool boolean = false;
  double number = 0;
  std::string text;
  std::vector<JsonValue> items;
  std::vector<std::pair<std::string, JsonValue>> members;

  const JsonValue *find(const std::string &key) const {
    for (const auto &member : members) {
      if (member.first == key) {
        return &member.second;
      }
    }
    return nullptr;
  }
};

class JsonReader {
public:
  explicit JsonReader(const std::string &text) : text_(text) {
  }

  bool parse(JsonValue &out, std::string &error) {
    skipSpace();
    if (!value(out, 0)) {
      error = error_ + " at offset " + std::to_string(pos_);
      return false;
    }
    skipSpace();
    if (pos_ != text_.size()) {
      error = "unexpected text after the JSON value at offset " + std::to_string(pos_);
      return false;
    }
    return true;
  }

private:
  const std::string &text_;
  size_t pos_ = 0;
  std::string error_;

  bool fail(const std::string &message) {
    error_ = message;
    return false;
  }

  void skipSpace() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
      ++pos_;
    }
  }

  bool consume(char c) {
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  bool literal(const char *word) {
    size_t i = 0;
    while (word[i] != '\0') {
      if (pos_ + i >= text_.size() || text_[pos_ + i] != word[i]) {
        return false;
      }
      ++i;
    }
    pos_ += i;
    return true;
  }

  static void appendUtf8(std::string &out, unsigned code) {
    if (code < 0x80) {
      out += static_cast<char>(code);
    } else if (code < 0x800) {
      out += static_cast<char>(0xC0 | (code >> 6));
      out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
      out += static_cast<char>(0xE0 | (code >> 12));
      out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (code & 0x3F));
    }
  }

  bool string(std::string &out) {
    if (!consume('"')) {
      return fail("expected a string");
    }
    while (pos_ < text_.size()) {
      const char c = text_[pos_++];
      if (c == '"') {
        return true;
      }
      if (c != '\\') {
        out += c;
        continue;
      }
      if (pos_ >= text_.size()) {
        break;
      }
      const char escape = text_[pos_++];
      switch (escape) {
      case '"':
        out += '"';
        break;
      case '\\':
        out += '\\';
        break;
      case '/':
        out += '/';
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'u': {
        if (pos_ + 4 > text_.size()) {
          return fail("truncated \\u escape");
        }
        const std::string digits = text_.substr(pos_, 4);
        char *end = nullptr;
        const unsigned long code = std::strtoul(digits.c_str(), &end, 16);
        if (end == nullptr || *end != '\0') {
          return fail("bad \\u escape");
        }
        appendUtf8(out, static_cast<unsigned>(code));
        pos_ += 4;
        break;
      }
      default:
        return fail("unknown escape");
      }
    }
    return fail("unterminated string");
  }

  bool value(JsonValue &out, int depth) {
    if (depth > 64) {
      return fail("nesting is too deep");
    }
    skipSpace();
    if (pos_ >= text_.size()) {
      return fail("unexpected end of the JSON text");
    }
    const char c = text_[pos_];
    if (c == '"') {
      out.kind = JsonValue::Kind::String;
      return string(out.text);
    }
    if (c == '{') {
      ++pos_;
      out.kind = JsonValue::Kind::Object;
      skipSpace();
      if (consume('}')) {
        return true;
      }
      while (true) {
        skipSpace();
        std::string key;
        if (!string(key)) {
          return false;
        }
        skipSpace();
        if (!consume(':')) {
          return fail("expected ':'");
        }
        JsonValue member;
        if (!value(member, depth + 1)) {
          return false;
        }
        out.members.emplace_back(std::move(key), std::move(member));
        skipSpace();
        if (consume(',')) {
          continue;
        }
        if (consume('}')) {
          return true;
        }
        return fail("expected ',' or '}'");
      }
    }
    if (c == '[') {
      ++pos_;
      out.kind = JsonValue::Kind::Array;
      skipSpace();
      if (consume(']')) {
        return true;
      }
      while (true) {
        JsonValue item;
        if (!value(item, depth + 1)) {
          return false;
        }
        out.items.push_back(std::move(item));
        skipSpace();
        if (consume(',')) {
          continue;
        }
        if (consume(']')) {
          return true;
        }
        return fail("expected ',' or ']'");
      }
    }
    if (literal("true")) {
      out.kind = JsonValue::Kind::Bool;
      out.boolean = true;
      return true;
    }
    if (literal("false")) {
      out.kind = JsonValue::Kind::Bool;
      return true;
    }
    if (literal("null")) {
      return true;
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
      char *end = nullptr;
      out.number = std::strtod(text_.c_str() + pos_, &end);
      if (end == text_.c_str() + pos_) {
        return fail("bad number");
      }
      pos_ = static_cast<size_t>(end - text_.c_str());
      out.kind = JsonValue::Kind::Number;
      return true;
    }
    return fail("unexpected character");
  }
};

std::string stringField(const JsonValue &object, const std::string &key) {
  const JsonValue *field = object.find(key);
  return field != nullptr && field->kind == JsonValue::Kind::String ? field->text : std::string();
}

std::vector<std::string> stringList(const JsonValue &object, const std::string &key) {
  std::vector<std::string> result;
  const JsonValue *field = object.find(key);
  if (field == nullptr || field->kind != JsonValue::Kind::Array) {
    return result;
  }
  for (const JsonValue &item : field->items) {
    if (item.kind == JsonValue::Kind::String) {
      result.push_back(item.text);
    }
  }
  return result;
}

} // namespace

bool parseBuildGraphJson(const std::string &text, BuildGraphData &graph, std::string &error) {
  graph = {};
  JsonValue root;
  if (!JsonReader(text).parse(root, error)) {
    error = "the graph is not valid JSON: " + error;
    return false;
  }
  const JsonValue *nodes = root.kind == JsonValue::Kind::Object ? root.find("nodes") : nullptr;
  if (nodes == nullptr || nodes->kind != JsonValue::Kind::Array) {
    error = "the graph has no \"nodes\" array";
    return false;
  }
  for (const JsonValue &node : nodes->items) {
    if (node.kind != JsonValue::Kind::Object) {
      error = "a graph node is not an object";
      return false;
    }
    const std::string kind = stringField(node, "kind");
    const std::string id = stringField(node, "id");
    if (kind != "compile" && kind != "executable") {
      graph.unsupportedNodes.push_back(id);
      continue;
    }
    BuildStep step;
    step.id = id;
    step.kind = kind;
    step.name = stringField(node, "name");
    step.source = stringField(node, "source");
    step.output = stringField(node, "output");
    step.inputDirs = stringList(node, "inputDirs");
    const JsonValue *compiler = node.find("compiler");
    if (compiler == nullptr || compiler->kind != JsonValue::Kind::Object) {
      error = "node " + id + " has no compiler settings";
      return false;
    }
    step.emitKind = stringField(*compiler, "emit");
    const JsonValue *optimize = compiler->find("optimize");
    if (optimize != nullptr && optimize->kind == JsonValue::Kind::Number) {
      step.optimization = static_cast<int>(optimize->number);
    }
    step.entry = stringField(*compiler, "entry");
    step.importPaths = stringList(*compiler, "import_paths");
    step.transforms = stringList(*compiler, "transforms");
    step.defaultEffects = stringList(*compiler, "default_effects");
    step.linkArgs = stringList(*compiler, "link_args");
    graph.steps.push_back(std::move(step));
  }
  const JsonValue *incomplete = root.find("incomplete");
  graph.incomplete =
      incomplete != nullptr && incomplete->kind == JsonValue::Kind::Bool && incomplete->boolean;
  const JsonValue *problems = root.find("problems");
  if (problems != nullptr && problems->kind == JsonValue::Kind::Array) {
    for (const JsonValue &problem : problems->items) {
      if (problem.kind == JsonValue::Kind::String) {
        graph.problems.push_back(problem.text);
      }
    }
  }
  return true;
}

} // namespace primec
