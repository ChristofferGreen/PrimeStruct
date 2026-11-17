#include "primec/pipeline/BuildGraphJson.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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

// The text of `value` in the compact form of the graph export (no spaces); members keep their order.
void writeJson(const JsonValue &value, std::string &out) {
  switch (value.kind) {
  case JsonValue::Kind::Null:
    out += "null";
    break;
  case JsonValue::Kind::Bool:
    out += value.boolean ? "true" : "false";
    break;
  case JsonValue::Kind::Number: {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value.number));
    out += buffer;
    break;
  }
  case JsonValue::Kind::String:
    out += '"';
    for (const char c : value.text) {
      if (c == '"' || c == '\\') {
        out += '\\';
        out += c;
      } else if (static_cast<unsigned char>(c) < 0x20) {
        char buffer[8];
        std::snprintf(buffer,
                      sizeof(buffer),
                      "\\u%04x",
                      static_cast<unsigned>(static_cast<unsigned char>(c)));
        out += buffer;
      } else {
        out += c;
      }
    }
    out += '"';
    break;
  case JsonValue::Kind::Array: {
    out += '[';
    bool first = true;
    for (const JsonValue &item : value.items) {
      out += first ? "" : ",";
      first = false;
      writeJson(item, out);
    }
    out += ']';
    break;
  }
  case JsonValue::Kind::Object: {
    out += '{';
    bool first = true;
    for (const auto &member : value.members) {
      out += first ? "" : ",";
      first = false;
      JsonValue key;
      key.kind = JsonValue::Kind::String;
      key.text = member.first;
      writeJson(key, out);
      out += ':';
      writeJson(member.second, out);
    }
    out += '}';
    break;
  }
  }
}

// `path` as seen from the project root, for a subproject folder `directory` (relative to the root); a path that is
// empty or absolute stays as it is.
std::string rebasePath(const std::string &directory, const std::string &path) {
  if (path.empty() || std::filesystem::path(path).is_absolute()) {
    return path;
  }
  return (std::filesystem::path(directory) / path).lexically_normal().generic_string();
}

void rebaseStringList(const std::string &directory, JsonValue *list) {
  if (list == nullptr || list->kind != JsonValue::Kind::Array) {
    return;
  }
  for (JsonValue &item : list->items) {
    if (item.kind == JsonValue::Kind::String) {
      item.text = rebasePath(directory, item.text);
    }
  }
}

JsonValue *mutableField(JsonValue &object, const std::string &key) {
  for (auto &member : object.members) {
    if (member.first == key) {
      return &member.second;
    }
  }
  return nullptr;
}

// A node of the subproject `directory` as the parent graph sees it: the id and the link ends carry the folder as a
// prefix, and every path moves from "relative to the subproject" to "relative to the project".
void moveNodeIntoParent(const std::string &directory,
                        const std::string &projectDir,
                        JsonValue &node) {
  for (const char *key : {"id", "from", "to"}) {
    JsonValue *field = mutableField(node, key);
    if (field != nullptr && field->kind == JsonValue::Kind::String) {
      field->text = directory + "/" + field->text;
    }
  }
  JsonValue *source = mutableField(node, "source");
  if (source != nullptr && source->kind == JsonValue::Kind::String) {
    source->text = rebasePath(directory, source->text);
  }
  // The output keeps its name under the folder, so two subprojects can both build an `app`.
  JsonValue *output = mutableField(node, "output");
  if (output != nullptr && output->kind == JsonValue::Kind::String) {
    output->text = rebasePath(directory, output->text);
  }
  rebaseStringList(directory, mutableField(node, "inputDirs"));
  JsonValue *compiler = mutableField(node, "compiler");
  if (compiler == nullptr || compiler->kind != JsonValue::Kind::Object) {
    return;
  }
  rebaseStringList(directory, mutableField(*compiler, "import_paths"));
  // A link argument names a file when it is not an option and the file exists in the subproject.
  JsonValue *linkArgs = mutableField(*compiler, "link_args");
  if (linkArgs != nullptr && linkArgs->kind == JsonValue::Kind::Array) {
    for (JsonValue &item : linkArgs->items) {
      if (item.kind != JsonValue::Kind::String || item.text.empty() || item.text[0] == '-') {
        continue;
      }
      std::error_code ec;
      const std::filesystem::path candidate =
          std::filesystem::path(projectDir) / directory / item.text;
      if (std::filesystem::is_regular_file(candidate, ec)) {
        item.text = rebasePath(directory, item.text);
      }
    }
  }
}

// Adds the entries of the `"options"` array of `child` to the one of `parent` (created when missing): an entry whose
// name is there already is dropped when its type and default agree and is a problem otherwise. Sorted by name at the end.
void mergeOptions(const std::string &directory, const JsonValue &child, JsonValue &parent) {
  const JsonValue *childOptions = child.find("options");
  if (childOptions == nullptr || childOptions->kind != JsonValue::Kind::Array ||
      childOptions->items.empty()) {
    return;
  }
  JsonValue *options = mutableField(parent, "options");
  if (options == nullptr) {
    JsonValue created;
    created.kind = JsonValue::Kind::Array;
    parent.members.emplace_back("options", std::move(created));
    options = mutableField(parent, "options");
  }
  if (options->kind != JsonValue::Kind::Array) {
    return;
  }
  for (const JsonValue &entry : childOptions->items) {
    if (entry.kind != JsonValue::Kind::Object) {
      continue;
    }
    bool found = false;
    for (const JsonValue &existing : options->items) {
      if (stringField(existing, "name") != stringField(entry, "name")) {
        continue;
      }
      found = true;
      if (stringField(existing, "type") != stringField(entry, "type") ||
          stringField(existing, "default") != stringField(entry, "default")) {
        JsonValue problem;
        problem.kind = JsonValue::Kind::String;
        problem.text = directory + ": option " + stringField(entry, "name") + " is declared as " +
                       stringField(entry, "type") + " with default " +
                       stringField(entry, "default") + ", but another script declares " +
                       stringField(existing, "type") + " with default " +
                       stringField(existing, "default");
        mutableField(parent, "problems")->items.push_back(std::move(problem));
      }
      break;
    }
    if (!found) {
      options->items.push_back(entry);
    }
  }
  std::stable_sort(options->items.begin(),
                   options->items.end(),
                   [](const JsonValue &left, const JsonValue &right) {
                     return stringField(left, "name") < stringField(right, "name");
                   });
}

} // namespace

bool mergeBuildGraphJson(const std::string &parentText,
                         const std::vector<BuildSubgraph> &subgraphs,
                         const std::string &projectDir,
                         std::string &merged,
                         std::string &error) {
  JsonValue parent;
  if (!JsonReader(parentText).parse(parent, error)) {
    error = "the graph is not valid JSON: " + error;
    return false;
  }
  JsonValue *nodes =
      parent.kind == JsonValue::Kind::Object ? mutableField(parent, "nodes") : nullptr;
  JsonValue *problems =
      parent.kind == JsonValue::Kind::Object ? mutableField(parent, "problems") : nullptr;
  if (nodes == nullptr || nodes->kind != JsonValue::Kind::Array || problems == nullptr ||
      problems->kind != JsonValue::Kind::Array) {
    error = "the graph has no \"nodes\" and \"problems\" arrays";
    return false;
  }
  parent.members.erase(
      std::remove_if(parent.members.begin(),
                     parent.members.end(),
                     [](const auto &member) { return member.first == "subprojects"; }),
      parent.members.end());
  nodes = mutableField(parent, "nodes");
  problems = mutableField(parent, "problems");
  for (const BuildSubgraph &subgraph : subgraphs) {
    JsonValue child;
    if (!JsonReader(subgraph.text).parse(child, error)) {
      error = subgraph.directory + ": the graph is not valid JSON: " + error;
      return false;
    }
    const JsonValue *childNodes =
        child.kind == JsonValue::Kind::Object ? child.find("nodes") : nullptr;
    if (childNodes == nullptr || childNodes->kind != JsonValue::Kind::Array) {
      error = subgraph.directory + ": the graph has no \"nodes\" array";
      return false;
    }
    for (JsonValue node : childNodes->items) {
      if (node.kind == JsonValue::Kind::Object) {
        moveNodeIntoParent(subgraph.directory, projectDir, node);
      }
      nodes->items.push_back(std::move(node));
    }
    mergeOptions(subgraph.directory, child, parent);
    // Adding the `"options"` member can move the members of `parent`.
    nodes = mutableField(parent, "nodes");
    problems = mutableField(parent, "problems");
    const JsonValue *childProblems = child.find("problems");
    if (childProblems != nullptr && childProblems->kind == JsonValue::Kind::Array) {
      for (JsonValue problem : childProblems->items) {
        if (problem.kind == JsonValue::Kind::String) {
          problem.text = subgraph.directory + ": " + problem.text;
        }
        problems->items.push_back(std::move(problem));
      }
    }
  }
  // The export order: by id, then by the text of the node.
  std::vector<std::pair<std::string, std::string>> keyed;
  for (const JsonValue &node : nodes->items) {
    std::string text;
    writeJson(node, text);
    const JsonValue *id = node.find("id");
    keyed.emplace_back(id != nullptr ? id->text : std::string(), text);
  }
  std::sort(keyed.begin(), keyed.end());
  std::string sortedNodes = "[";
  for (size_t i = 0; i < keyed.size(); ++i) {
    sortedNodes += (i == 0 ? "" : ",") + keyed[i].second;
  }
  sortedNodes += "]";
  JsonValue sorted;
  if (!JsonReader(sortedNodes).parse(sorted, error)) {
    return false;
  }
  *nodes = std::move(sorted);
  merged.clear();
  writeJson(parent, merged);
  merged += "\n";
  return true;
}

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
    if (kind != "compile" && kind != "executable" && kind != "copy") {
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
    if (kind == "copy") {
      // A copy step has no compiler: `source` is the file and `output` its place in the output directory.
      graph.steps.push_back(std::move(step));
      continue;
    }
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
  const JsonValue *options = root.find("options");
  if (options != nullptr && options->kind == JsonValue::Kind::Array) {
    for (const JsonValue &entry : options->items) {
      if (entry.kind != JsonValue::Kind::Object) {
        continue;
      }
      BuildOption option;
      option.name = stringField(entry, "name");
      option.type = stringField(entry, "type");
      option.defaultText = stringField(entry, "default");
      option.description = stringField(entry, "description");
      option.value = stringField(entry, "value");
      option.error = stringField(entry, "error");
      graph.options.push_back(std::move(option));
    }
  }
  const JsonValue *incomplete = root.find("incomplete");
  graph.incomplete =
      incomplete != nullptr && incomplete->kind == JsonValue::Kind::Bool && incomplete->boolean;
  graph.subprojects = stringList(root, "subprojects");
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
