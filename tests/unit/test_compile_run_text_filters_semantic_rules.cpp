#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");


TEST_CASE("text transforms accept whitespace separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1+2)
}
)";
  const std::string srcPath = writeTemp("compile_text_transforms_whitespace.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_transforms_whitespace_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=" + quoteShellArg("operators implicit-i32");
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("transform list enables single_type_to_return") {
  const std::string source = R"(
[i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_single_type_to_return_missing_return.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_single_type_to_return_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main --transform-list=default,single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}

TEST_CASE("per-definition single_type_to_return marker") {
  const std::string source = R"(
[single_type_to_return i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_single_type_to_return_marker.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_single_type_to_return_marker_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}

TEST_CASE("semantic transforms flag enables single_type_to_return") {
  const std::string source = R"(
[i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_semantic_single_type_to_return.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_single_type_to_return_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main --semantic-transforms=single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}

TEST_CASE("semantic transform rules apply per path") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rules.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rules_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rules_err.txt").string();

  const std::string okCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none";
  CHECK(runCommand(okCmd) == 0);

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none --semantic-transform-rules=/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(ruleCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules prefer later matching entry") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rule_order.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rule_order_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rule_order_err.txt").string();

  const std::string disableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=single_type_to_return\\;/main=none";
  CHECK(runCommand(disableLastCmd) == 0);

  const std::string enableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=none\\;/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(enableLastCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules clear on none token") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rules_none_token.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rules_none_token_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rules_none_token_err.txt").string();

  const std::string clearedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=single_type_to_return\\;none";
  CHECK(runCommand(clearedCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string reappliedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=none\\;/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(reappliedCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules ignore empty rule tokens") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_empty_tokens.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_empty_tokens_err.txt").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none --semantic-transform-rules " +
      quoteShellArg(";;/main=single_type_to_return;;") + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(ruleCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules prefer later wildcard match") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rule_wildcard_order.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rule_wildcard_order_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rule_wildcard_order_err.txt").string();

  const std::string wildcardLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=single_type_to_return\\;/*=none";
  CHECK(runCommand(wildcardLastCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string exactLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/*=none\\;/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(exactLastCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform root wildcard applies to top-level definitions") {
  const std::string source = R"(
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_root_wildcard_top_level.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_root_wildcard_top_level_err.txt")
          .string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/*=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(ruleCmd) == 2);
  CHECK(readFile(errPath).find("single_type_to_return requires a type transform on /main") !=
        std::string::npos);
}

TEST_CASE("semantic transform rules reject recurse without wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_bad_recurse_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_bad_recurse_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main:recurse=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule recursion requires a '/*' suffix") != std::string::npos);
}

TEST_CASE("semantic transform rules reject empty transform list") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_empty_list_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_empty_list_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main= 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule list cannot be empty") != std::string::npos);
}

TEST_CASE("semantic transform rules reject non-trailing wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_bad_wildcard_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_bad_wildcard_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/ma*in=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule '*' is only allowed as trailing '/*'") !=
        std::string::npos);
}

TEST_CASE("semantic transform rules reject path without slash prefix") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_missing_slash_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_missing_slash_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path must start with '/': main") != std::string::npos);
}

TEST_CASE("semantic transform rules reject missing equals") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_missing_equals_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_missing_equals_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule must include '=': /main") != std::string::npos);
}

TEST_CASE("semantic transform rules reject empty path") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_empty_path_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_empty_path_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules==single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path cannot be empty") != std::string::npos);
}

TEST_CASE("semantic transform rules reject text-only transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_text_only_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_text_only_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unsupported semantic transform: operators") != std::string::npos);
}

TEST_CASE("semantic transform rules reject unknown transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_unknown_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_unknown_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=not_a_transform 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unknown transform: not_a_transform") != std::string::npos);
}

TEST_CASE("semantic transform rules accept recursive suffix alias") {
  const std::string source = R"(
[i32]
main() {
  [i32]
  helper() {
    inner() {
      return(1u64)
    }
    return(0i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_recursive_alias.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_recursive_alias_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_recursive_alias_err.txt").string();

  const std::string nonRecursiveCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none --semantic-transform-rules=/main/*=single_type_to_return";
  CHECK(runCommand(nonRecursiveCmd) == 0);

  const std::string recursiveAliasCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main/*:recursive=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(recursiveAliasCmd) == 2);
  CHECK(readFile(errPath).find("single_type_to_return requires a type transform on /main/helper/inner") !=
        std::string::npos);
}

TEST_CASE("semantic transform wildcard does not match sibling prefix") {
  const std::string source = R"(
[i32]
mainly() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_wildcard_prefix.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_wildcard_prefix_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /mainly --semantic-transforms=none "
      "--semantic-transform-rules=/main/*=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic transform wildcard does not match base path") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_wildcard_base.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_wildcard_base_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main/*=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic transform rules ignore unrelated exact path") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_unrelated_exact_path.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_unrelated_exact_path_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/other=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic transform rules ignore unrelated wildcard path") {
  const std::string source = R"(
[i32]
mainly() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_unrelated_wildcard_path.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_unrelated_wildcard_path_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /mainly --semantic-transforms=none "
      "--semantic-transform-rules=/other/*=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic root wildcard only recurses when requested") {
  const std::string source = R"(
[i32]
main() {
  [i32]
  helper() {
    inner() {
      return(1u64)
    }
    return(0i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_root_wildcard_recurse.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_root_wildcard_recurse_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_root_wildcard_recurse_err.txt").string();

  const std::string noRecurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none --semantic-transform-rules=/*=single_type_to_return";
  CHECK(runCommand(noRecurseCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string recurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/*:recurse=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(recurseCmd) == 2);
  CHECK(readFile(errPath).find("single_type_to_return requires a type transform on /main/helper/inner") !=
        std::string::npos);
}

TEST_CASE("semantic transforms ignore text transforms") {
  const std::string source = R"(
[operators i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_semantic_single_type_to_return_text.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_single_type_to_return_text_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main --semantic-transforms=single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}


TEST_SUITE_END();
