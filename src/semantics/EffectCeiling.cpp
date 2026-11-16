#include "primec/semantics/EffectCeiling.h"

#include "SemanticsHelpers.h"

#include <algorithm>
#include <string_view>

namespace primec {
namespace {

bool is_stdlib_path(const std::string &path) {
  return path == "/std" || path.rfind("/std/", 0) == 0;
}

std::string join_list(const std::vector<std::string> &items) {
  if (items.empty()) {
    return "none";
  }
  std::string out;
  for (const auto &item : items) {
    if (!out.empty()) {
      out += ",";
    }
    out += item;
  }
  return out;
}

bool is_type_definition(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return semantics::isStructLikeDefinition(def);
}

struct CeilingInputs {
  const std::vector<std::string> &allowed;
  std::vector<EffectCeilingViolation> &out;
};

// Reports every effect of one declaration that is outside the ceiling. A declaration without an [effects]
// transform receives `defaults`; `skipDefaults` skips it then.
void check_declaration(const CeilingInputs &in,
                       std::string_view kind,
                       const std::string &path,
                       const std::vector<Transform> &transforms,
                       const std::vector<std::string> &defaults,
                       bool skipDefaults,
                       int line,
                       int column) {
  const std::vector<std::string> *effects = nullptr;
  for (const auto &transform : transforms) {
    if (transform.name == "effects") {
      effects = &transform.arguments;
    }
  }
  const bool declared = effects != nullptr;
  if (!declared) {
    if (skipDefaults) {
      return;
    }
    effects = &defaults;
  }
  for (const auto &effect : *effects) {
    if (std::find(in.allowed.begin(), in.allowed.end(), effect) != in.allowed.end()) {
      continue;
    }
    EffectCeilingViolation violation;
    violation.message = std::string(kind) + " " + path +
                        (declared ? " declares effect " : " receives default effect ") + effect +
                        ", outside the allowed effects: " + join_list(in.allowed);
    violation.line = line;
    violation.column = column;
    in.out.push_back(std::move(violation));
  }
}

} // namespace

std::vector<EffectCeilingViolation>
findEffectCeilingViolations(const Program &program,
                            const std::string &entryPath,
                            const std::vector<std::string> &allowedEffects,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects) {
  std::vector<EffectCeilingViolation> violations;
  const CeilingInputs inputs{allowedEffects, violations};
  for (const auto &def : program.definitions) {
    if (is_stdlib_path(def.fullPath)) {
      continue;
    }
    const bool isEntry = def.fullPath == entryPath;
    check_declaration(inputs,
                      "definition",
                      def.fullPath,
                      def.transforms,
                      isEntry ? entryDefaultEffects : defaultEffects,
                      !isEntry && is_type_definition(def),
                      def.sourceLine,
                      def.sourceColumn);
  }
  for (const auto &exec : program.executions) {
    if (is_stdlib_path(exec.fullPath)) {
      continue;
    }
    check_declaration(inputs,
                      "execution",
                      exec.fullPath,
                      exec.transforms,
                      defaultEffects,
                      false,
                      exec.sourceLine,
                      exec.sourceColumn);
  }
  return violations;
}

} // namespace primec
