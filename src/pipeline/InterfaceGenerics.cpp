#include "InterfaceGenerics.h"

#include "DeclarationText.h"
#include "DefinitionReferences.h"

#include <unordered_set>

namespace primec {
namespace {

bool hasTransform(const Definition &definition, const char *name) {
  for (const Transform &transform : definition.transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

std::string templateArgsText(const Definition &definition) {
  if (definition.templateArgs.empty()) {
    return "";
  }
  std::string out = "<";
  for (size_t i = 0; i < definition.templateArgs.size(); ++i) {
    out += i == 0 ? "" : ", ";
    out += definition.templateArgs[i];
    if (i < definition.templateArgIsPack.size() && definition.templateArgIsPack[i]) {
      out += "...";
    }
  }
  return out + ">";
}

std::string transformsText(const Definition &definition) {
  std::vector<Transform> kept;
  for (const Transform &transform : definition.transforms) {
    if (transform.name != "namespace") {
      kept.push_back(transform);
    }
  }
  return declarationTransformListText(kept);
}

std::string parametersText(const Definition &definition) {
  std::string out;
  for (size_t i = 0; i < definition.parameters.size(); ++i) {
    out += i == 0 ? "" : ", ";
    out += declarationParameterText(definition.parameters[i]);
  }
  return out;
}

std::string headerText(const Definition &definition, const std::string &name) {
  return transformsText(definition) + name + templateArgsText(definition) + "(" +
         parametersText(definition) + ")";
}

void appendIndented(std::string &out, const std::string &text) {
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    out += "  ";
    out.append(text, start, end - start);
    out += "\n";
    start = end + 1;
  }
}

std::string lastSegment(const std::string &path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

bool isGenericDefinition(const Definition &definition) {
  return !definition.templateArgs.empty() && definition.instanceOfTemplate.empty();
}

bool isLifecycleHelper(const Definition &definition) {
  if (!definition.isNested) {
    return false;
  }
  const std::string leaf = lastSegment(definition.fullPath);
  return leaf == "Create" || leaf == "Destroy" || leaf == "Copy" || leaf == "Move";
}

std::string genericDefinitionText(const Definition &definition,
                                  const std::string &name,
                                  const std::vector<const Definition *> &methods) {
  std::string out = headerText(definition, name) + " {\n";
  if (hasTransform(definition, "struct") || !methods.empty()) {
    for (const Expr &statement : definition.statements) {
      if (statement.isBinding) {
        appendIndented(out, declarationBindingText(statement));
      }
    }
    for (const Definition *method : methods) {
      appendIndented(out, genericDefinitionText(*method, lastSegment(method->fullPath), {}));
    }
  } else {
    out += declarationBodyText(definition.statements);
  }
  return out + "}";
}

std::set<std::string>
reachedFromGenerics(const std::unordered_map<std::string, const Definition *> &definitions,
                    const std::vector<std::string> &seedPaths) {
  std::unordered_set<std::string> definedPaths;
  for (const auto &entry : definitions) {
    definedPaths.insert(entry.first);
  }
  std::set<std::string> reached;
  std::vector<std::string> pending = seedPaths;
  while (!pending.empty()) {
    const std::string path = pending.back();
    pending.pop_back();
    if (!reached.insert(path).second) {
      continue;
    }
    // The methods of a struct, and what is nested in a definition, belong to it.
    for (const auto &entry : definitions) {
      if (entry.first.size() > path.size() && entry.first.compare(0, path.size(), path) == 0 &&
          entry.first[path.size()] == '/' && entry.second->isNested) {
        pending.push_back(entry.first);
      }
    }
    const auto it = definitions.find(path);
    if (it == definitions.end()) {
      continue;
    }
    for (const std::string &reference : collectDefinitionReferences(*it->second, definedPaths)) {
      pending.push_back(reference);
    }
  }
  return reached;
}

} // namespace primec
