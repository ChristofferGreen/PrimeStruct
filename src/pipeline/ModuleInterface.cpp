#include "primec/pipeline/ModuleInterface.h"

#include "DeclarationText.h"
#include "DefinitionReferences.h"

#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"
#include "primec/support/Json.h"
#include "primec/support/Sha256.h"
#include "primec/support/SourceLocationMapper.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {

bool hasTransform(const std::vector<Transform> &transforms, const char *name) {
  for (const Transform &transform : transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

const Transform *findTransform(const std::vector<Transform> &transforms, const char *name) {
  for (const Transform &transform : transforms) {
    if (transform.name == name) {
      return &transform;
    }
  }
  return nullptr;
}

std::string lastSegment(const std::string &path) {
  const size_t slash = path.rfind('/');
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  // The `__ov<N>` suffix of an overload is a compiler detail: the interface spells the overload as written.
  const size_t overload = name.find("__ov");
  if (overload != std::string::npos && overload > 0) {
    name.resize(overload);
  }
  return name;
}

// What the semantic product knows about a definition, by path.
struct DefinitionFacts {
  std::unordered_map<std::string, std::string> returnTypes;
  std::unordered_map<std::string, std::string> categories;
  std::unordered_map<std::string, std::string> summaries;
};

DefinitionFacts collectFacts(const SemanticProgram &semanticProgram) {
  DefinitionFacts facts;
  for (const SemanticProgramReturnFact &fact : semanticProgram.returnFacts) {
    const std::string path(semanticProgramReturnFactDefinitionPath(semanticProgram, fact));
    facts.returnTypes.emplace(
        path, fact.bindingTypeText.empty() ? fact.returnKind : fact.bindingTypeText);
  }
  for (const SemanticProgramTypeMetadata &type : semanticProgram.typeMetadata) {
    facts.categories.emplace(type.fullPath, type.category);
  }
  for (const SemanticProgramDefinition &definition : semanticProgram.definitions) {
    if (!definition.summary.empty()) {
      facts.summaries.emplace(definition.fullPath, definition.summary);
    }
  }
  return facts;
}

// A public symbol of the module with its text, ready to print and to hash.
struct Item {
  const Definition *definition = nullptr;
  std::string kind;
  // The namespace block the symbol is printed in: its parent path, or the nearest namespace above a struct.
  std::string section;
  std::string summary;
  // The declaration with the path as written in the block and the summary: what the interface prints.
  std::string displayText;
  // The declaration with the full path and no summary: what the signature hash covers.
  std::string signatureText;
  // A method nested in a struct: printed inside the struct, not on its own.
  bool nestedInStruct = false;
};

std::string joinWords(const std::vector<std::string> &words) {
  std::string out;
  for (size_t i = 0; i < words.size(); ++i) {
    out += i == 0 ? "" : ", ";
    out += words[i];
  }
  return out;
}

std::string bracketed(const std::vector<std::string> &words) {
  return words.empty() ? "" : "[" + joinWords(words) + "] ";
}

std::string parametersText(const Definition &definition) {
  std::string out;
  for (size_t i = 0; i < definition.parameters.size(); ++i) {
    out += i == 0 ? "" : ", ";
    out += declarationParameterText(definition.parameters[i]);
  }
  return out;
}

std::string effectsWord(const std::vector<std::string> &effects) {
  std::string out = "effects(";
  for (size_t i = 0; i < effects.size(); ++i) {
    out += i == 0 ? "" : ", ";
    out += effects[i];
  }
  return out + ")";
}

bool functionHeader(const Definition &definition,
                    const DefinitionFacts &facts,
                    std::vector<std::string> &words,
                    std::string &error) {
  if (hasTransform(definition.transforms, "public")) {
    words.push_back("public");
  }
  // `mut` and `static` decide how a method is called (a `mut` method needs a mutable receiver).
  for (const char *flag : {"static", "mut", "compute", "unsafe"}) {
    if (hasTransform(definition.transforms, flag)) {
      words.push_back(flag);
    }
  }
  // The effects a caller must have are the ones the definition declares: a function that declares none needs nothing
  // of its caller, whatever its body does under the default effects.
  std::vector<std::string> declared;
  for (const Transform &transform : definition.transforms) {
    if (transform.name == "effects" && transform.templateArgs.empty()) {
      declared.insert(declared.end(), transform.arguments.begin(), transform.arguments.end());
    }
  }
  std::sort(declared.begin(), declared.end());
  declared.erase(std::unique(declared.begin(), declared.end()), declared.end());
  if (!declared.empty()) {
    words.push_back(effectsWord(declared));
  }
  if (const Transform *capabilities = findTransform(definition.transforms, "capabilities")) {
    words.push_back(declarationTransformText(*capabilities));
  }
  const auto returnType = facts.returnTypes.find(definition.fullPath);
  if (returnType != facts.returnTypes.end()) {
    words.push_back("return<" + returnType->second + ">");
  } else if (const Transform *declared = findTransform(definition.transforms, "return");
             declared != nullptr && !declared->templateArgs.empty()) {
    words.push_back("return<" + declared->templateArgs.front() + ">");
  } else {
    error = "no resolved return type for " + definition.fullPath;
    return false;
  }
  return true;
}

bool typeHeader(const Definition &definition, std::vector<std::string> &words) {
  static const std::set<std::string> kept = {"pod",
                                             "handle",
                                             "gpu_lane",
                                             "no_padding",
                                             "platform_independent_padding",
                                             "align_bytes",
                                             "align_kbytes"};
  if (hasTransform(definition.transforms, "public")) {
    words.push_back("public");
  }
  words.push_back("struct");
  for (const Transform &transform : definition.transforms) {
    if (kept.count(transform.name) != 0) {
      words.push_back(declarationTransformText(transform));
    }
  }
  return true;
}

std::string summaryWord(const std::string &summary) {
  return "summary(" + declarationQuotedString(summary) + ")";
}

// A struct with its fields and, in the printed interface, the declarations of its methods (a helper nested in the
// struct has an implicit `this`, which only a declaration inside the struct body keeps).
std::string structText(const Definition &definition,
                       const std::vector<std::string> &header,
                       const std::string &name,
                       const std::vector<std::string> &methods = {}) {
  std::string out = bracketed(header) + name + "(" + parametersText(definition) + ") {\n";
  for (const Expr &statement : definition.statements) {
    if (statement.isBinding) {
      out += "  " + declarationBindingText(statement) + "\n";
    }
  }
  for (const std::string &method : methods) {
    out += "  " + method + "\n";
  }
  return out + "}";
}

const Transform *typeTransformOf(const Expr &binding) {
  for (const Transform &transform : binding.transforms) {
    if (transform.name != "public" && transform.name != "private" && transform.name != "static" &&
        transform.name != "mut") {
      return &transform;
    }
  }
  return nullptr;
}

// An enum in its surface form (`Red = 0i32`). Validation rewrites an enum to a struct with static entries
// (docs/spec/type-system.md) and keeps no mark of it, so an enum is recognized by its shape: a `value` field and static
// entries of the struct's own type built from an integer literal. The struct form would not read back the same,
// because `Color{0i32}` is not `Color{[value] 0i32}`. Returns false for any other shape; the caller prints a struct.
bool enumText(const Definition &definition,
              const std::string &summary,
              const std::string &name,
              std::string &out) {
  std::string backing;
  std::string entries;
  for (const Expr &statement : definition.statements) {
    if (!statement.isBinding) {
      continue;
    }
    if (!hasTransform(statement.transforms, "static")) {
      const Transform *type = typeTransformOf(statement);
      if (statement.name != "value" || type == nullptr || !backing.empty()) {
        return false;
      }
      backing = type->name;
      continue;
    }
    const Transform *entryType = typeTransformOf(statement);
    if (entryType == nullptr || entryType->name != definition.fullPath ||
        statement.args.size() != 1 || statement.args.front().name != definition.fullPath ||
        statement.args.front().args.size() != 1 ||
        statement.args.front().args.front().kind != Expr::Kind::Literal) {
      return false;
    }
    entries += "  " + statement.name + " = " +
               declarationExprText(statement.args.front().args.front()) + "\n";
  }
  if (backing.empty() || entries.empty()) {
    return false;
  }
  std::vector<std::string> header;
  if (hasTransform(definition.transforms, "public")) {
    header.push_back("public");
  }
  header.push_back(backing == "i32" ? "enum" : "enum<" + backing + ">");
  if (!summary.empty()) {
    header.push_back(summaryWord(summary));
  }
  out = bracketed(header) + name + " {\n" + entries + "}";
  return true;
}

std::string functionText(const Definition &definition,
                         const std::vector<std::string> &header,
                         const std::string &name) {
  // An empty body: the grammar has no declaration without one, and `host` functions are written the same way.
  return bracketed(header) + name + "(" + parametersText(definition) + ") {}";
}

// The text of a definition for the content hash: every transform, the parameters and the whole body.
std::string rawDefinitionText(const Definition &definition) {
  return declarationTransformListText(definition.transforms) + definition.fullPath + "(" +
         parametersText(definition) + ") {\n" + declarationBodyText(definition.statements) + "}\n";
}

bool inModuleSources(const ExpandedSource &source, const Definition &definition) {
  const std::optional<SourceUnitLocation> location =
      mapExpandedSourceLocation(source, definition.sourceLine, definition.sourceColumn);
  return location.has_value() && (location->unitKind == SourceUnitKind::Primary ||
                                  location->unitKind == SourceUnitKind::Import);
}

bool isPublic(const Definition &definition) {
  return hasTransform(definition.transforms, "public") &&
         !hasTransform(definition.transforms, "private");
}

// The nearest namespace path above `path`, skipping the structs whose members are printed beside them.
std::string sectionOf(const std::string &path, const std::set<std::string> &structPaths) {
  std::string section = parentDefinitionPath(path);
  while (!section.empty() && structPaths.count(section) != 0) {
    section = parentDefinitionPath(section);
  }
  return section;
}

// What the printer adds to a function declaration: the library it is imported from.
std::string importedWord(const std::string &libraryName) {
  return "imported(" + libraryName + ")";
}

bool buildItem(const Definition &definition,
               const DefinitionFacts &facts,
               const std::set<std::string> &structPaths,
               const std::map<std::string, std::vector<std::string>> &methodsByStruct,
               const std::string &libraryName,
               Item &item,
               std::string &error) {
  item.definition = &definition;
  item.section = sectionOf(definition.fullPath, structPaths);
  const auto summary = facts.summaries.find(definition.fullPath);
  item.summary = summary == facts.summaries.end() ? "" : summary->second;
  const std::string displayName = parentDefinitionPath(definition.fullPath) == item.section
                                      ? lastSegment(definition.fullPath)
                                      : definition.fullPath;
  std::vector<std::string> header;
  if (structPaths.count(definition.fullPath) != 0) {
    if (enumText(definition, "", definition.fullPath, item.signatureText) &&
        enumText(definition, item.summary, displayName, item.displayText)) {
      item.kind = "enum";
      return true;
    }
    item.kind = "struct";
    typeHeader(definition, header);
    item.signatureText = structText(definition, header, definition.fullPath);
    if (!item.summary.empty()) {
      header.push_back(summaryWord(item.summary));
    }
    const auto methods = methodsByStruct.find(definition.fullPath);
    item.displayText =
        structText(definition,
                   header,
                   displayName,
                   methods == methodsByStruct.end() ? std::vector<std::string>() : methods->second);
    return true;
  }
  item.kind = "function";
  if (!functionHeader(definition, facts, header, error)) {
    return false;
  }
  item.signatureText = functionText(definition, header, definition.fullPath);
  header.push_back(importedWord(libraryName));
  if (!item.summary.empty()) {
    header.push_back(summaryWord(item.summary));
  }
  item.nestedInStruct =
      definition.isNested && structPaths.count(parentDefinitionPath(definition.fullPath)) != 0;
  item.displayText = functionText(
      definition, header, item.nestedInStruct ? lastSegment(definition.fullPath) : displayName);
  return true;
}

// The block tree of the interface: one node per namespace, the symbols of a namespace and its child namespaces.
struct Node {
  std::vector<const Item *> items;
  std::map<std::string, Node> children;
};

void insertItem(Node &root, const Item &item) {
  Node *node = &root;
  size_t start = 1;
  while (start < item.section.size()) {
    const size_t slash = item.section.find('/', start);
    const size_t end = slash == std::string::npos ? item.section.size() : slash;
    node = &node->children[item.section.substr(start, end - start)];
    start = end + 1;
  }
  node->items.push_back(&item);
}

void appendIndented(std::string &out, const std::string &text, int depth) {
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    out.append(static_cast<size_t>(depth) * 2, ' ');
    out.append(text, start, end - start);
    out += "\n";
    start = end + 1;
  }
}

void printNode(std::string &out,
               const Node &node,
               const std::string &path,
               const DefinitionFacts &facts,
               int depth,
               bool &first) {
  for (const Item *item : node.items) {
    out += first ? "" : "\n";
    first = false;
    appendIndented(out, item->displayText, depth);
  }
  for (const auto &child : node.children) {
    const std::string childPath = path + "/" + child.first;
    const auto summary = facts.summaries.find(childPath);
    out += first ? "" : "\n";
    first = false;
    const std::string header = summary == facts.summaries.end()
                                   ? "[namespace] "
                                   : "[namespace " + summaryWord(summary->second) + "] ";
    appendIndented(out, header + child.first + " {", depth);
    bool innerFirst = true;
    printNode(out, child.second, childPath, facts, depth + 1, innerFirst);
    appendIndented(out, "}", depth);
  }
}

std::string contentHash(const Definition &definition,
                        std::unordered_map<std::string, std::string> &cache) {
  auto it = cache.find(definition.fullPath);
  if (it == cache.end()) {
    it = cache.emplace(definition.fullPath, sha256Hex(rawDefinitionText(definition))).first;
  }
  return it->second;
}

// One hash over the content hashes of `root` and of everything reachable from it.
std::string closureHash(const std::string &root,
                        const std::unordered_map<std::string, const Definition *> &definitions,
                        const std::unordered_set<std::string> &definedPaths,
                        std::unordered_map<std::string, std::string> &contentCache) {
  std::set<std::string> reached = {root};
  std::vector<std::string> pending = {root};
  while (!pending.empty()) {
    const std::string path = pending.back();
    pending.pop_back();
    const auto it = definitions.find(path);
    if (it == definitions.end()) {
      continue;
    }
    for (const std::string &reference : collectDefinitionReferences(*it->second, definedPaths)) {
      if (reached.insert(reference).second) {
        pending.push_back(reference);
      }
    }
  }
  std::string lines;
  for (const std::string &path : reached) {
    const auto it = definitions.find(path);
    if (it != definitions.end()) {
      lines += path + " " + contentHash(*it->second, contentCache) + "\n";
    }
  }
  return sha256Hex(lines);
}

} // namespace

bool addInterfaceEntryStub(Program &program, const std::string &entryPath, std::string &error) {
  for (const Definition &definition : program.definitions) {
    if (definition.fullPath == entryPath) {
      return true;
    }
  }
  const std::string stubSource = "[return<int>]\n" + entryPath + "() {\n  return(0i32)\n}\n";
  Lexer lexer(stubSource);
  Parser parser(lexer.tokenize(), false);
  Program stub;
  if (!parser.parse(stub, error) || stub.definitions.empty()) {
    error = "cannot add the entry stub for " + entryPath + ": " + error;
    return false;
  }
  program.definitions.push_back(std::move(stub.definitions.front()));
  return true;
}

bool buildModuleInterface(const Program &program,
                          const SemanticProgram &semanticProgram,
                          const ExpandedSource &source,
                          const std::string &entryPath,
                          const std::string &libraryName,
                          ModuleInterface &out,
                          std::string &error) {
  out = {};
  const DefinitionFacts facts = collectFacts(semanticProgram);
  std::set<std::string> structPaths;
  for (const auto &category : facts.categories) {
    if (category.second == "struct" || category.second == "enum") {
      structPaths.insert(category.first);
    }
  }

  std::unordered_map<std::string, const Definition *> definitions;
  std::unordered_set<std::string> definedPaths;
  for (const Definition &definition : program.definitions) {
    definitions.emplace(definition.fullPath, &definition);
    definedPaths.insert(definition.fullPath);
  }

  std::vector<Item> items;
  for (const Definition &definition : program.definitions) {
    if (definition.fullPath == entryPath || !definition.templateArgs.empty() ||
        !definition.instanceOfTemplate.empty() ||
        hasTransform(definition.transforms, "namespace") || !isPublic(definition) ||
        !inModuleSources(source, definition)) {
      continue;
    }
    const auto category = facts.categories.find(definition.fullPath);
    if (category != facts.categories.end() && category->second != "struct" &&
        category->second != "enum") {
      continue;
    }
    Item item;
    if (!buildItem(definition, facts, structPaths, {}, libraryName, item, error)) {
      return false;
    }
    items.push_back(std::move(item));
  }
  // The methods nested in a struct are printed inside it: collect their declarations, in path order, and build the
  // struct items again with them.
  std::map<std::string, std::vector<std::string>> methodsByStruct;
  {
    std::vector<const Item *> methods;
    for (const Item &item : items) {
      if (item.nestedInStruct) {
        methods.push_back(&item);
      }
    }
    std::sort(methods.begin(), methods.end(), [](const Item *left, const Item *right) {
      return left->definition->fullPath < right->definition->fullPath;
    });
    for (const Item *method : methods) {
      methodsByStruct[parentDefinitionPath(method->definition->fullPath)].push_back(
          method->displayText);
    }
  }
  if (!methodsByStruct.empty()) {
    for (Item &item : items) {
      if (item.kind != "struct" || methodsByStruct.count(item.definition->fullPath) == 0) {
        continue;
      }
      Item rebuilt;
      if (!buildItem(
              *item.definition, facts, structPaths, methodsByStruct, libraryName, rebuilt, error)) {
        return false;
      }
      item = std::move(rebuilt);
    }
  }
  std::sort(items.begin(), items.end(), [](const Item &left, const Item &right) {
    if (left.definition->fullPath != right.definition->fullPath) {
      return left.definition->fullPath < right.definition->fullPath;
    }
    return left.signatureText < right.signatureText;
  });

  Node root;
  for (const Item &item : items) {
    if (!item.nestedInStruct) {
      insertItem(root, item);
    }
  }
  bool first = true;
  printNode(out.text, root, "", facts, 0, first);

  std::unordered_map<std::string, std::string> contentCache;
  for (const Item &item : items) {
    ModuleInterfaceSymbol symbol;
    symbol.path = item.definition->fullPath;
    symbol.kind = item.kind;
    symbol.signatureHash = sha256Hex(item.signatureText);
    symbol.contentHash = contentHash(*item.definition, contentCache);
    symbol.closureHash = closureHash(symbol.path, definitions, definedPaths, contentCache);
    out.symbols.push_back(std::move(symbol));
  }
  std::sort(out.symbols.begin(),
            out.symbols.end(),
            [](const ModuleInterfaceSymbol &left, const ModuleInterfaceSymbol &right) {
              if (left.path != right.path) {
                return left.path < right.path;
              }
              if (left.kind != right.kind) {
                return left.kind < right.kind;
              }
              return left.signatureHash < right.signatureHash;
            });
  std::string lines;
  for (const ModuleInterfaceSymbol &symbol : out.symbols) {
    lines += symbol.path + " " + symbol.kind + " " + symbol.signatureHash + "\n";
  }
  out.interfaceHash = sha256Hex(lines);
  return true;
}

std::string libraryNameOfRootFile(const std::string &path) {
  std::string name = std::filesystem::path(path).stem().string();
  for (char &c : name) {
    const bool word =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    c = word ? c : '_';
  }
  if (name.empty() || (name.front() >= '0' && name.front() <= '9')) {
    name.insert(name.begin(), '_');
  }
  return name;
}

std::string formatModuleSymbolTable(const ModuleInterface &moduleInterface) {
  std::string out = "{\"interfaceHash\":\"" + moduleInterface.interfaceHash + "\",\"symbols\":[";
  for (size_t i = 0; i < moduleInterface.symbols.size(); ++i) {
    const ModuleInterfaceSymbol &symbol = moduleInterface.symbols[i];
    out += i == 0 ? "\n" : ",\n";
    out += "{\"path\":\"" + jsonEscape(symbol.path) + "\",\"kind\":\"" + symbol.kind +
           "\",\"signature\":\"" + symbol.signatureHash + "\",\"content\":\"" + symbol.contentHash +
           "\",\"closure\":\"" + symbol.closureHash + "\"}";
  }
  out += moduleInterface.symbols.empty() ? "]}\n" : "\n]}\n";
  return out;
}

} // namespace primec
