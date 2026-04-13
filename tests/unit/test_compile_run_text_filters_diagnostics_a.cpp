#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primec collect-diagnostics emits stable multi-parse payload") {
  const std::string source = R"(
import bad
import /std/math
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_parse.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_collect_diagnostics_parse_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1003\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import path must be a slash path\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"") !=
        std::string::npos);

  size_t parseCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1003\"", scan)) != std::string::npos) {
    ++parseCount;
    scan += 16;
  }
  CHECK(parseCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"import path must be a slash path\"");
  const size_t secondMessage = diagnostics.find(
      "\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-parse payload") {
  const std::string source = R"(
import bad
import /std/math
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_parse.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_collect_diagnostics_parse_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1003\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import path must be a slash path\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"") !=
        std::string::npos);

  size_t parseCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1003\"", scan)) != std::string::npos) {
    ++parseCount;
    scan += 16;
  }
  CHECK(parseCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"import path must be a slash path\"");
  const size_t secondMessage = diagnostics.find(
      "\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics keeps first duplicate-definition payload") {
  const std::string source = R"(
[return<int>]
dup() {
  return(1i32)
}
[return<int>]
dup() {
  return(2i32)
}
[return<int>]
other() {
  return(3i32)
}
[return<int>]
other() {
  return(4i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_dupes.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_collect_diagnostics_semantic_dupes_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /dup\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /other\"") == std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate definition: /dup\"");
  REQUIRE(firstMessage != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps first duplicate-definition payload") {
  const std::string source = R"(
[return<int>]
dup() {
  return(1i32)
}
[return<int>]
dup() {
  return(2i32)
}
[return<int>]
other() {
  return(3i32)
}
[return<int>]
other() {
  return(4i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_dupes.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_collect_diagnostics_semantic_dupes_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /dup\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /other\"") == std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate definition: /dup\"");
  REQUIRE(firstMessage != std::string::npos);
}

TEST_CASE("primec collect-diagnostics emits stable semantic import payload for unknown paths") {
  const std::string source = R"(
import /missing_alpha
import /missing_beta
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_imports.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_collect_diagnostics_semantic_imports_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown import path: /missing_alpha/*\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown import path: /missing_beta/*\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown import path: /missing_alpha/*\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown import path: /missing_beta/*\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable semantic import payload for unknown paths") {
  const std::string source = R"(
import /missing_alpha
import /missing_beta
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_imports.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_collect_diagnostics_semantic_imports_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown import path: /missing_alpha/*\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown import path: /missing_beta/*\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown import path: /missing_alpha/*\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown import path: /missing_beta/*\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for invalid transforms") {
  const std::string source = R"(
[static return<int>]
one() {
  return(1i32)
}
[effects(io_out) effects(io_out) return<int>]
two() {
  return(2i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_invalid_transforms.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_collect_diagnostics_semantic_invalid_transforms_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate effects transform on /two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /one\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /two\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"duplicate effects transform on /two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for invalid transforms") {
  const std::string source = R"(
[static return<int>]
one() {
  return(1i32)
}
[effects(io_out) effects(io_out) return<int>]
two() {
  return(2i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_invalid_transforms.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_collect_diagnostics_semantic_invalid_transforms_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate effects transform on /two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /one\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /two\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"duplicate effects transform on /two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for return-kind errors") {
  const std::string source = R"(
[return]
badNoType() {
  return(1i32)
}
[return<array>]
badArray() {
  return(array<i32>(1i32))
}
[return<MissingType>]
badUnknown() {
  return(3i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_return_kind.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_collect_diagnostics_semantic_return_kind_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badArray\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badUnknown\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 3);

  const size_t firstMessage = diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"");
  const size_t thirdMessage = diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  REQUIRE(thirdMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
  CHECK(secondMessage < thirdMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for return-kind errors") {
  const std::string source = R"(
[return]
badNoType() {
  return(1i32)
}
[return<array>]
badArray() {
  return(array<i32>(1i32))
}
[return<MissingType>]
badUnknown() {
  return(3i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_return_kind.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_collect_diagnostics_semantic_return_kind_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badArray\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badUnknown\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 3);

  const size_t firstMessage = diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"");
  const size_t thirdMessage = diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  REQUIRE(thirdMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
  CHECK(secondMessage < thirdMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for definition pass errors") {
  const std::string source = R"(
[return<int>]
first() {
  return(nope(1i32))
}
[return<int>]
second() {
  return(missing(2i32))
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_definitions.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_collect_diagnostics_semantic_definitions_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /first\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /second\"") != std::string::npos);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for definition pass errors") {
  const std::string source = R"(
[return<int>]
first() {
  return(nope(1i32))
}
[return<int>]
second() {
  return(missing(2i32))
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_definitions.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_collect_diagnostics_semantic_definitions_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /first\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /second\"") != std::string::npos);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-definition multi-semantic payload") {
  const std::string source = R"(
[return<int>]
bad() {
  nope(1i32)
  missing(2i32)
  return(0i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_definition.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_collect_diagnostics_semantic_intra_definition_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}


TEST_SUITE_END();
