#include "primec/Ast.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRegistry.h"
#include "primec/TransformRules.h"

#include "third_party/doctest.h"

#include <string_view>
#include <vector>

TEST_SUITE_BEGIN("primestruct.text_filters.helpers");

TEST_CASE("transform registry registration preserves deterministic order") {
  primec::TransformRegistry registry;
  registry.registerTransform({"first", primec::TransformPhase::Text, "-", true, true});
  registry.registerTransform({"second", primec::TransformPhase::Semantic, "-", true, true});
  registry.registerTransform({"third", primec::TransformPhase::Text, "-", true, true});

  std::vector<std::string_view> names;
  for (const auto &transform : registry.list()) {
    names.push_back(transform.name);
  }

  REQUIRE(names.size() == 3);
  CHECK(names[0] == "first");
  CHECK(names[1] == "second");
  CHECK(names[2] == "third");
}

TEST_CASE("transform registry supports same name in multiple phases") {
  primec::TransformRegistry registry;
  registry.registerTransform({"dual", primec::TransformPhase::Text, "-", true, true});
  registry.registerTransform({"dual", primec::TransformPhase::Semantic, "-", true, true});

  CHECK(registry.contains("dual", primec::TransformPhase::Text));
  CHECK(registry.contains("dual", primec::TransformPhase::Semantic));
  CHECK(registry.contains("dual", primec::TransformPhase::Auto));
}

TEST_CASE("default transform lookups follow registry phase metadata") {
  const auto transforms = primec::listTransforms();
  auto hasPhase = [&](std::string_view name, primec::TransformPhase phase) {
    for (const auto &transform : transforms) {
      if (transform.name == name && transform.phase == phase) {
        return true;
      }
    }
    return false;
  };

  std::vector<std::string_view> uniqueNames;
  for (const auto &transform : transforms) {
    bool alreadySeen = false;
    for (const auto &name : uniqueNames) {
      if (name == transform.name) {
        alreadySeen = true;
        break;
      }
    }
    if (!alreadySeen) {
      uniqueNames.push_back(transform.name);
    }
  }

  for (const auto &name : uniqueNames) {
    CHECK(primec::isTextTransformName(name) == hasPhase(name, primec::TransformPhase::Text));
    CHECK(primec::isSemanticTransformName(name) == hasPhase(name, primec::TransformPhase::Semantic));
  }
}

TEST_CASE("transform rules reject non-absolute root wildcard match") {
  primec::TextTransformRule rule;
  rule.wildcard = true;
  rule.path.clear();
  CHECK_FALSE(primec::ruleMatchesPath(rule, "main"));
}

TEST_CASE("transform rules reject empty path for root wildcard match") {
  primec::TextTransformRule rule;
  rule.wildcard = true;
  rule.path.clear();
  CHECK_FALSE(primec::ruleMatchesPath(rule, ""));
}

TEST_CASE("apply semantic transform rules returns early when empty") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/main";
  program.definitions.push_back(def);

  primec::applySemanticTransformRules(program, {});
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].transforms.empty());
}

TEST_CASE("apply semantic transform rules handles executions") {
  primec::Program program;
  primec::Execution exec;
  exec.fullPath = "/main";
  program.executions.push_back(exec);

  primec::TextTransformRule rule;
  rule.path = "/main";
  rule.transforms = {"single_type_to_return"};
  primec::applySemanticTransformRules(program, {rule});

  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].transforms.size() == 1);
  CHECK(program.executions[0].transforms[0].name == "single_type_to_return");
  CHECK(program.executions[0].transforms[0].phase == primec::TransformPhase::Semantic);
}

TEST_CASE("apply semantic transform rules skips unmatched execution") {
  primec::Program program;
  primec::Execution exec;
  exec.fullPath = "/main";
  program.executions.push_back(exec);

  primec::TextTransformRule rule;
  rule.path = "/other";
  rule.transforms = {"single_type_to_return"};
  primec::applySemanticTransformRules(program, {rule});

  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].transforms.empty());
}

TEST_SUITE_END();
