#include "primec/TransformRules.h"

#include "primec/Ast.h"
#include "primec/TextFilterPipeline.h"

namespace primec {

bool ruleMatchesPath(const TextTransformRule &rule, const std::string &path) {
  if (rule.wildcard) {
    if (rule.path.empty()) {
      if (path.empty() || path[0] != '/') {
        return false;
      }
      if (rule.recursive) {
        return true;
      }
      return path.find('/', 1) == std::string::npos;
    }
    if (path.size() <= rule.path.size()) {
      return false;
    }
    if (path.compare(0, rule.path.size(), rule.path) != 0) {
      return false;
    }
    if (path[rule.path.size()] != '/') {
      return false;
    }
    if (rule.recursive) {
      return true;
    }
    return path.find('/', rule.path.size() + 1) == std::string::npos;
  }
  return path == rule.path;
}

const std::vector<std::string> *selectRuleTransforms(const std::vector<TextTransformRule> &rules,
                                                     const std::string &path) {
  const std::vector<std::string> *match = nullptr;
  for (const auto &rule : rules) {
    if (ruleMatchesPath(rule, path)) {
      match = &rule.transforms;
    }
  }
  return match;
}

void applySemanticTransformRules(Program &program, const std::vector<TextTransformRule> &rules) {
  if (rules.empty()) {
    return;
  }
  auto applyTo = [&](auto &node) {
    const auto *ruleTransforms = selectRuleTransforms(rules, node.fullPath);
    if (ruleTransforms == nullptr) {
      return;
    }
    for (const auto &name : *ruleTransforms) {
      Transform transform;
      transform.name = name;
      transform.phase = TransformPhase::Semantic;
      node.transforms.push_back(std::move(transform));
    }
  };
  for (auto &def : program.definitions) {
    applyTo(def);
  }
  for (auto &exec : program.executions) {
    applyTo(exec);
  }
}

} // namespace primec
