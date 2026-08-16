#pragma once

#include <string>
#include <vector>

namespace primec {

struct TextTransformRule;
struct Program;

bool ruleMatchesPath(const TextTransformRule &rule, const std::string &path);
const std::vector<std::string> *selectRuleTransforms(const std::vector<TextTransformRule> &rules,
                                                     const std::string &path);
void applySemanticTransformRules(Program &program, const std::vector<TextTransformRule> &rules);

} // namespace primec
