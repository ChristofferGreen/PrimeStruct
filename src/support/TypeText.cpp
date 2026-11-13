#include "primec/support/TypeText.h"

#include <cctype>
#include <tuple>
#include <utility>

#include "primec/support/CompileArena.h"
#include "primec/support/CompileContext.h"

namespace primec {

namespace {

// The memo tables live in the compile context. Every insert runs under a SystemHeapScope so entries are never
// compile-arena memory: clear() keeps a table's bucket array, and a later insert can reallocate it (TODO-5235 in
// docs/CompilerArenaAllocator.md).
CompileContext::TypeNameCaches &typeNameCaches() {
  return CompileContext::current().typeNames;
}

bool splitTemplateTypeNameUncached(const std::string &text, std::string &base, std::string &arg) {
  base = text;
  arg.clear();
  const size_t open = text.find('<');
  if (open == std::string::npos || open == 0 || text.back() != '>') {
    return false;
  }
  int depth = 0;
  const size_t start = open + 1;
  for (size_t i = start; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c != '>') {
      continue;
    }
    if (depth > 0) {
      --depth;
      continue;
    }
    if (i + 1 != text.size()) {
      return false;
    }
    base = text.substr(0, open);
    arg = text.substr(start, i - start);
    return true;
  }
  return false;
}

bool splitTopLevelTemplateArgsUncached(std::string_view text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.emplace_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (c == ',' && depth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  for (const auto &segment : out) {
    if (segment.empty()) {
      return false;
    }
  }
  return !out.empty();
}

} // namespace

std::string trimTemplateTypeText(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

bool splitTopLevelTemplateArgs(std::string_view text, std::vector<std::string> &out) {
  auto &cache = typeNameCaches().splitTopLevelTemplateArgs;
  const std::string key(text);
  auto it = cache.find(key);
  if (it != cache.end()) {
    out = it->second.second;
    return it->second.first;
  }
  const bool ok = splitTopLevelTemplateArgsUncached(text, out);
  {
    SystemHeapScope systemHeapGuard;
    cache.emplace(key, std::make_pair(ok, out));
  }
  return ok;
}

bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg) {
  auto &cache = typeNameCaches().splitTemplateTypeName;
  auto it = cache.find(text);
  if (it != cache.end()) {
    base = std::get<1>(it->second);
    arg = std::get<2>(it->second);
    return std::get<0>(it->second);
  }
  const bool ok = splitTemplateTypeNameUncached(text, base, arg);
  {
    SystemHeapScope systemHeapGuard;
    cache.emplace(text, std::make_tuple(ok, base, arg));
  }
  return ok;
}

} // namespace primec
