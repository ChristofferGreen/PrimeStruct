#include "primec/support/DiagnosticNames.h"

#include <cctype>
#include <string_view>

namespace primec {
namespace {

bool isPathCharacter(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '/';
}

bool isHexDigit(char ch) {
  return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

// The end of the first `__t<hex digits>` in `token` at or after `from`, or npos.
size_t findGeneratedSuffixEnd(std::string_view token, size_t from) {
  size_t marker = token.find("__t", from);
  while (marker != std::string_view::npos) {
    size_t end = marker + 3;
    while (end < token.size() && isHexDigit(token[end])) {
      ++end;
    }
    if (end > marker + 3) {
      return end;
    }
    marker = token.find("__t", marker + 3);
  }
  return std::string_view::npos;
}

// `/pkg/Oops` is `Oops` and `Box</pkg/Oops>` is `Box<Oops>`: the namespaces of a type argument are dropped.
std::string dropNamespaces(const std::string &text) {
  std::string out;
  size_t index = 0;
  while (index < text.size()) {
    if (text[index] != '/') {
      out.push_back(text[index]);
      ++index;
      continue;
    }
    size_t end = index;
    while (end < text.size() && isPathCharacter(text[end])) {
      ++end;
    }
    const size_t lastSlash = text.rfind('/', end - 1);
    out.append(text, lastSlash + 1, end - lastSlash - 1);
    index = end;
  }
  return out;
}

std::string templateLeafWithoutArity(const std::string &templatePath) {
  const size_t slash = templatePath.find_last_of('/');
  std::string leaf = slash == std::string::npos ? templatePath : templatePath.substr(slash + 1);
  const size_t arity = leaf.rfind("__arity");
  if (arity != std::string::npos && arity + 7 < leaf.size()) {
    bool digits = true;
    for (size_t i = arity + 7; i < leaf.size(); ++i) {
      digits = digits && std::isdigit(static_cast<unsigned char>(leaf[i])) != 0;
    }
    if (digits) {
      leaf.erase(arity);
    }
  }
  return leaf;
}

// One path token of the text: its generated instances are spelled out, the rest stays.
std::string rewriteToken(const std::string &token, const TemplateInstanceLookup &lookup) {
  std::string out;
  size_t cursor = 0;
  while (cursor < token.size()) {
    const size_t end = findGeneratedSuffixEnd(token, cursor);
    if (end == std::string_view::npos) {
      break;
    }
    const std::string generatedPath = token.substr(0, end);
    const std::optional<TemplateInstanceName> instance = lookup(generatedPath);
    if (!instance.has_value()) {
      // Not an instance: keep the text up to the suffix and look for the next one.
      out.append(token, cursor, end - cursor);
      cursor = end;
      continue;
    }
    out = templateInstanceDisplayName(*instance, lookup);
    cursor = end;
    // The rest of the token (a member, a variant) is relative to the instance: `/Error`, `/push`.
    break;
  }
  out.append(token, cursor, std::string::npos);
  return out;
}

} // namespace

std::string templateInstanceDisplayName(const TemplateInstanceName &instance,
                                        const TemplateInstanceLookup &lookup) {
  std::string name = templateLeafWithoutArity(instance.templatePath);
  name += "<";
  for (size_t i = 0; i < instance.arguments.size(); ++i) {
    if (i != 0) {
      name += ", ";
    }
    name += dropNamespaces(userFacingDiagnosticText(instance.arguments[i], lookup));
  }
  name += ">";
  return name;
}

std::string userFacingDiagnosticText(const std::string &text,
                                     const TemplateInstanceLookup &lookup) {
  if (!lookup || text.find("__t") == std::string::npos) {
    return text;
  }
  std::string out;
  size_t index = 0;
  while (index < text.size()) {
    if (!isPathCharacter(text[index])) {
      out.push_back(text[index]);
      ++index;
      continue;
    }
    size_t end = index;
    while (end < text.size() && isPathCharacter(text[end])) {
      ++end;
    }
    out += rewriteToken(text.substr(index, end - index), lookup);
    index = end;
  }
  return out;
}

} // namespace primec
