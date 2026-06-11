#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");


TEST_CASE("no transforms disables single_type_to_return") {
  const std::string source = R"(
[i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_single_type_to_return_disabled.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_single_type_to_return_disabled_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null" +
                                 " --entry /main --no-transforms --transform-list=default,single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("explicit return transform") != std::string::npos);
}

TEST_CASE("per-envelope text transforms enable collections") {
  const std::string source = R"(
[collections return<int>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32}}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_per_env_collections.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_per_env_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main --text-transforms=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("per-envelope text transforms override operators") {
  const std::string source = R"(
[collections return<int>]
main() {
  return(1i32 + 2i32)
}
)";
  const std::string srcPath = writeTemp("compile_per_env_override.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_per_env_override_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("per-envelope text transforms apply to bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [operators i32] value{1i32+2i32}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_per_env_binding_ops.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_per_env_binding_ops_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main --text-transforms=none";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("per-envelope text transforms apply to executions in arguments") {
  const std::string source = R"(
[return<int>]
add([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  return([operators] add(1i32+2i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_per_env_exec_args.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_per_env_exec_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main --text-transforms=none";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("text transforms can append additional transforms") {
  const std::string source = R"(
[append_operators return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_append_operators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_append_operators_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=append_operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("transform list auto-deduces append_operators") {
  const std::string source = R"(
[append_operators return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_append_operators_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_append_operators_auto_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --transform-list=append_operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules apply to namespace paths") {
  const std::string source = R"(
namespace std {
  namespace math {
    [return<int>]
    add() {
      return(1i32+2i32)
    }
  }
}

[return<int>]
main() {
  return(/std/math/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_namespace.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_namespace_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/std/math/*=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules ignore unrelated wildcard path") {
  const std::string source = R"(
[return<int>]
mainly() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_unrelated_wildcard_path.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_unrelated_wildcard_path_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /mainly --text-transforms=none --text-transform-rules=/other/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules ignore unrelated exact path") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_unrelated_exact_path.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_unrelated_exact_path_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none --text-transform-rules=/other=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform wildcard does not match base path") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_wildcard_base_path.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_wildcard_base_path_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none --text-transform-rules=/main/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform wildcard does not match sibling prefix") {
  const std::string source = R"(
[return<int>]
mainly() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_wildcard_sibling_prefix.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_wildcard_sibling_prefix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /mainly --text-transforms=none --text-transform-rules=/main/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules apply without transform lists") {
  const std::string source = R"(
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_no_list.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_no_list_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/main=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules ignore empty rule tokens") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_empty_tokens.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_empty_tokens_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules " +
      quoteShellArg(";;/main=operators;;");
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules none token without follow-up keeps rules empty") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_none_only.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_none_only_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none --text-transform-rules=none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules prefer later matching entry") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_order.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_order_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_text_rule_order_err.txt").string();

  const std::string disableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=operators\\;/main=none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(disableLastCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string enableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none "
      "--text-transform-rules=/main=none\\;/main=operators";
  CHECK(runCommand(enableLastCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules prefer later wildcard match") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_wildcard_order.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_wildcard_order_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_wildcard_order_err.txt").string();

  const std::string wildcardLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=operators\\;/*=none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(wildcardLastCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string exactLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none "
      "--text-transform-rules=/*=none\\;/main=operators";
  CHECK(runCommand(exactLastCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform root wildcard applies to top-level definitions") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_root_wildcard_top_level.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_root_wildcard_top_level_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/*=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules clear on none token") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_none_token.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_none_token_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_none_token_err.txt").string();

  const std::string clearedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=operators\\;none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(clearedCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string reappliedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=none\\;/main=operators";
  CHECK(runCommand(reappliedCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules reject missing equals") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_missing_equals.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_missing_equals_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule must include '=': /main") != std::string::npos);
}

TEST_CASE("text transform rules reject empty transform list") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_empty_list_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_empty_list_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main= 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule list cannot be empty") != std::string::npos);
}

TEST_CASE("text transform rules reject empty path") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_empty_path_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_empty_path_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules==operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path cannot be empty") != std::string::npos);
}

TEST_CASE("text transform rules reject path without slash prefix") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_missing_slash_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_missing_slash_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=main=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path must start with '/': main") != std::string::npos);
}

TEST_CASE("text transform rules reject non-trailing wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_bad_wildcard_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_bad_wildcard_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/ma*in=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule '*' is only allowed as trailing '/*'") !=
        std::string::npos);
}

TEST_CASE("text transform rules reject recurse without wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_bad_recurse_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_bad_recurse_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main:recurse=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule recursion requires a '/*' suffix") != std::string::npos);
}

TEST_CASE("text transform rules reject semantic-only transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_semantic_only_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_semantic_only_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unsupported text transform: single_type_to_return") != std::string::npos);
}

TEST_CASE("text transform rules reject unknown transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_unknown_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_unknown_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=not_a_transform 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unknown transform: not_a_transform") != std::string::npos);
}

TEST_CASE("text transform rules apply to nested definitions") {
  const std::string source = R"(
[return<int>]
main() {
  helper() {
    return(1i32+2i32)
  }
  return(/main/helper())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_nested_def.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_nested_def_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/main/*=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules ignore nested transform lists") {
  const std::string source = R"(
main() {
  [implicit-utf8 string] name{"ok"}
  print_line("nope")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_nested_list.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_nested_list_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none " +
      "--text-transform-rules=/main=operators 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules recurse when requested") {
  const std::string source = R"(
namespace std {
  namespace math {
    namespace ops {
      [return<int>]
      add() {
        return(1i32+2i32)
      }
    }
  }
}

[return<int>]
main() {
  return(/std/math/ops/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_recurse.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_recurse_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_recurse_err.txt").string();

  const std::string noRecurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/std/math/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(noRecurseCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string recurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/std/math/*:recurse=operators";
  CHECK(runCommand(recurseCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules accept recursive suffix alias") {
  const std::string source = R"(
namespace std {
  namespace math {
    namespace ops {
      [return<int>]
      add() {
        return(1i32+2i32)
      }
    }
  }
}

[return<int>]
main() {
  return(/std/math/ops/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_recursive_alias.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_recursive_alias_exe").string();

  const std::string recursiveAliasCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/std/math/*:recursive=operators";
  CHECK(runCommand(recursiveAliasCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("root wildcard transform rules only recurse when requested") {
  const std::string source = R"(
[return<int>]
main() {
  helper() {
    return(1i32+2i32)
  }
  return(/main/helper())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_root_wildcard_recurse.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_root_wildcard_recurse_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_root_wildcard_recurse_err.txt").string();

  const std::string noRecurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(noRecurseCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string recurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/*:recurse=operators";
  CHECK(runCommand(recurseCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_SUITE_END();
