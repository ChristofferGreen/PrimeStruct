#include "test_import_resolver_helpers.h"

#include "primec/CompilePipeline.h"

#include <algorithm>
#include <string_view>

namespace {
const primec::SourceUnit *findUnitByPath(const primec::ExpandedSource &source,
                                         const std::filesystem::path &path) {
  const std::string expected = std::filesystem::absolute(path).string();
  auto it = std::find_if(source.units.begin(), source.units.end(), [&](const primec::SourceUnit &unit) {
    return unit.displayPath == expected;
  });
  return it == source.units.end() ? nullptr : &*it;
}

bool segmentStartsBeforeOrAt(const primec::SourceSegment &left,
                             const primec::SourceSegment &right) {
  return left.flattenedStartLine < right.flattenedStartLine ||
         (left.flattenedStartLine == right.flattenedStartLine &&
          left.flattenedStartColumn <= right.flattenedStartColumn);
}

bool cursorBeforeOrAt(int leftLine, int leftColumn, int rightLine, int rightColumn) {
  return leftLine < rightLine || (leftLine == rightLine && leftColumn <= rightColumn);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.imports.resolver");

TEST_CASE("expands single import") {
  const std::string libPath = writeTemp("lib_a.prime", "[return<int>]\nhelper(){ return(3i32) }\n");
  const std::string srcPath = writeTemp("main_a.prime", "import<\"" + libPath + "\">\n[return<int>]\nmain(){ return(helper()) }\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("helper") != std::string::npos);
}

TEST_CASE("expanded source ledger records primary direct nested and directory imports") {
  auto baseDir = importResolverPath("source_ledger_imports");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  const std::string nestedPath = writeFile(baseDir / "nested.prime", "// LEDGER_NESTED\n");
  const std::string directPath = writeFile(baseDir / "direct.prime",
                                           "// LEDGER_DIRECT_BEFORE\n"
                                           "import<\"nested.prime\">\n"
                                           "// LEDGER_DIRECT_AFTER\n");
  const std::string dirAPath = writeFile(baseDir / "dir" / "a.prime", "// LEDGER_DIR_A\n");
  const std::string dirBPath = writeFile(baseDir / "dir" / "b.prime", "// LEDGER_DIR_B\n");
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "// LEDGER_PRIMARY_BEFORE\n"
                                        "import<\"direct.prime\"; \"dir\">\n"
                                        "// LEDGER_PRIMARY_AFTER\n");

  primec::ExpandedSource expanded;
  std::string error;
  primec::ImportResolver resolver;
  REQUIRE(resolver.expandImports(srcPath, expanded, error));
  CHECK(error.empty());
  CHECK(expanded.text.find("LEDGER_PRIMARY_BEFORE") != std::string::npos);
  CHECK(expanded.text.find("LEDGER_DIRECT_BEFORE") != std::string::npos);
  CHECK(expanded.text.find("LEDGER_NESTED") != std::string::npos);
  CHECK(expanded.text.find("LEDGER_DIR_A") != std::string::npos);
  CHECK(expanded.text.find("LEDGER_DIR_B") != std::string::npos);

  REQUIRE(expanded.units.size() == 5);
  CHECK(expanded.units[0].id == 0);
  CHECK(expanded.units[0].kind == primec::SourceUnitKind::Primary);
  CHECK(expanded.units[0].displayPath == std::filesystem::absolute(srcPath).string());
  CHECK(expanded.units[0].originalStartLine == 1);
  CHECK(expanded.units[0].originalStartColumn == 1);

  const auto *direct = findUnitByPath(expanded, directPath);
  const auto *nested = findUnitByPath(expanded, nestedPath);
  const auto *dirA = findUnitByPath(expanded, dirAPath);
  const auto *dirB = findUnitByPath(expanded, dirBPath);
  REQUIRE(direct != nullptr);
  REQUIRE(nested != nullptr);
  REQUIRE(dirA != nullptr);
  REQUIRE(dirB != nullptr);
  CHECK(direct->kind == primec::SourceUnitKind::Import);
  CHECK(nested->kind == primec::SourceUnitKind::Import);
  CHECK(dirA->kind == primec::SourceUnitKind::Import);
  CHECK(dirB->kind == primec::SourceUnitKind::Import);
  CHECK(direct->id < nested->id);
  CHECK(direct->id < dirA->id);
  CHECK(dirA->id < dirB->id);
  CHECK(primec::sourceUnitKindName(direct->kind) == std::string_view("import"));
  CHECK(std::count_if(expanded.segments.begin(),
                      expanded.segments.end(),
                      [&](const primec::SourceSegment &segment) {
                        return segment.unitId == direct->id;
                      }) == 2);
  CHECK(std::count_if(expanded.segments.begin(),
                      expanded.segments.end(),
                      [&](const primec::SourceSegment &segment) {
                        return segment.unitId == nested->id;
                      }) == 1);

  REQUIRE_FALSE(expanded.segments.empty());
  for (size_t i = 1; i < expanded.segments.size(); ++i) {
    CHECK(segmentStartsBeforeOrAt(expanded.segments[i - 1], expanded.segments[i]));
    CHECK(cursorBeforeOrAt(expanded.segments[i - 1].flattenedEndLine,
                           expanded.segments[i - 1].flattenedEndColumn,
                           expanded.segments[i].flattenedStartLine,
                           expanded.segments[i].flattenedStartColumn));
  }
}

TEST_CASE("expanded source ledger records generated import separators") {
  auto baseDir = importResolverPath("source_ledger_generated");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  const std::string libPath = writeFile(baseDir / "lib.prime", "// LEDGER_NO_TRAILING_NEWLINE");
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import<\"lib.prime\">\n"
                                        "// LEDGER_AFTER\n");

  primec::ExpandedSource expanded;
  std::string error;
  primec::ImportResolver resolver;
  REQUIRE(resolver.expandImports(srcPath, expanded, error));
  CHECK(error.empty());

  auto generated = std::find_if(expanded.units.begin(), expanded.units.end(), [](const primec::SourceUnit &unit) {
    return unit.kind == primec::SourceUnitKind::Generated &&
           unit.moduleKey == "<import-separator>";
  });
  REQUIRE(generated != expanded.units.end());
  CHECK(generated->originalStartLine == 0);
  CHECK(generated->originalStartColumn == 0);
  auto segment = std::find_if(expanded.segments.begin(), expanded.segments.end(), [&](const primec::SourceSegment &item) {
    return item.unitId == generated->id;
  });
  REQUIRE(segment != expanded.segments.end());
  CHECK(segment->originalStartLine == 0);
  CHECK(segment->originalStartColumn == 0);
  CHECK(expanded.text.find("LEDGER_NO_TRAILING_NEWLINE\n\n// LEDGER_AFTER") != std::string::npos);
}

TEST_CASE("compile pipeline exposes stdlib auto include source units") {
  auto baseDir = importResolverPath("source_ledger_stdlib");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import /std/maybe/Maybe\n"
                                        "[return<int>]\n"
                                        "main(){ return(0i32) }\n");

  primec::Options options;
  options.inputPath = srcPath;
  options.importPaths = {std::filesystem::absolute("stdlib").string()};
  options.dumpStage = "pre_ast";

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error));
  CHECK(error.empty());
  CHECK(output.hasDumpOutput);
  CHECK(output.expandedSource.text.find("import /std/maybe/Maybe") != std::string::npos);

  auto stdlibUnit = std::find_if(output.expandedSource.units.begin(),
                                 output.expandedSource.units.end(),
                                 [](const primec::SourceUnit &unit) {
                                   return unit.kind == primec::SourceUnitKind::Stdlib &&
                                          unit.moduleKey == "/std/maybe";
                                 });
  REQUIRE(stdlibUnit != output.expandedSource.units.end());
  CHECK(primec::sourceUnitKindName(stdlibUnit->kind) == std::string_view("stdlib"));
  CHECK(stdlibUnit->displayPath.find("stdlib/std/maybe/maybe.prime") != std::string::npos);
  CHECK(stdlibUnit->originalStartLine == 1);
  CHECK(stdlibUnit->originalStartColumn == 1);
  CHECK(stdlibUnit->flattenedStartLine > output.expandedSource.units.front().flattenedStartLine);
  CHECK(output.expandedSource.text.find("Maybe<T>") != std::string::npos);
}

TEST_CASE("rejects single legacy include alias") {
  const std::string srcPath = writeTemp("main_legacy_alias.prime", "include<\"/std/io\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error == "legacy include<...> is no longer supported; use import<...>");
}

TEST_CASE("expands import with whitespace") {
  const std::string marker = "LIB_WS_MARKER";
  const std::string libPath = writeTemp("lib_ws.prime", "// " + marker + "\n");
  const std::string srcPath = writeTemp("main_ws_single.prime", "import<  \"" + libPath + "\"  >\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find(marker) != std::string::npos);
}

TEST_CASE("expands import with tight comment") {
  const std::string marker = "LIB_TIGHT_COMMENT";
  const std::string libPath = writeTemp("lib_tight_comment.prime", "// " + marker + "\n");
  const std::string srcPath = writeTemp("main_tight_comment.prime", "import/* note */<\"" + libPath + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find(marker) != std::string::npos);
}

TEST_CASE("expands import with bare slash path") {
  auto baseDir = importResolverPath("include_bare_path_base");
  auto includeRoot = importResolverPath("include_bare_path_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib" / "lib.prime", "// INCLUDE_BARE_PATH\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import</lib>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_BARE_PATH") != std::string::npos);
}

TEST_CASE("expands bare slash imports with semicolons") {
  auto baseDir = importResolverPath("include_bare_semicolon_base");
  auto includeRoot = importResolverPath("include_bare_semicolon_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib_a" / "lib.prime", "// INCLUDE_BARE_SEMI_A\n");
  writeFile(includeRoot / "lib_b" / "lib.prime", "// INCLUDE_BARE_SEMI_B\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import</lib_a;/lib_b>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_BARE_SEMI_A") != std::string::npos);
  CHECK(source.find("INCLUDE_BARE_SEMI_B") != std::string::npos);
}

TEST_CASE("expands bare slash imports with whitespace separators") {
  auto baseDir = importResolverPath("include_bare_ws_base");
  auto includeRoot = importResolverPath("include_bare_ws_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib_a" / "lib.prime", "// INCLUDE_BARE_WS_A\n");
  writeFile(includeRoot / "lib_b" / "lib.prime", "// INCLUDE_BARE_WS_B\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import</lib_a /lib_b>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_BARE_WS_A") != std::string::npos);
  CHECK(source.find("INCLUDE_BARE_WS_B") != std::string::npos);
}

TEST_CASE("bare slash import does not use absolute filesystem path") {
  auto absRoot = importResolverPath("include_bare_absolute");
  auto baseDir = importResolverPath("include_bare_absolute_base");
  std::filesystem::remove_all(absRoot);
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(absRoot);
  std::filesystem::create_directories(baseDir);

  writeFile(absRoot / "lib.prime", "// INCLUDE_BARE_ABS\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<" + absRoot.string() + ">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(!resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("resolves versioned import with single quotes") {
  auto baseDir = importResolverPath("include_single_quote_base");
  auto includeRoot = importResolverPath("include_single_quote_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_SINGLE_QUOTE_120\n");
  writeFile(includeRoot / "1.2.3" / "lib.prime", "// INCLUDE_SINGLE_QUOTE_123\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<'/lib.prime', version = '1.2'>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_SINGLE_QUOTE_123") != std::string::npos);
  CHECK(source.find("INCLUDE_SINGLE_QUOTE_120") == std::string::npos);
}

TEST_CASE("rejects versioned legacy include alias") {
  const std::string srcPath = writeTemp("main_legacy_version_alias.prime", "include<'/lib.prime', version='1.2'>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error == "legacy include<...> is no longer supported; use import<...>");
}

TEST_CASE("rejects version-first legacy include alias") {
  const std::string srcPath = writeTemp("main_legacy_version_first_alias.prime", "include<version='1.2', '/lib.prime'>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error == "legacy include<...> is no longer supported; use import<...>");
}

TEST_CASE("ignores duplicate imports") {
  const std::string marker = "LIB_B_MARKER";
  const std::string libPath = writeTemp("lib_b.prime",
                                        "// " + marker + "\n"
                                        "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string srcPath = writeTemp("main_b.prime",
                                        "import<\"" + libPath + "\">\n"
                                        "import<\"" + libPath + "\">\n"
                                        "[return<int>]\nmain(){ return(helper()) }\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find(marker);
  const auto second = source.find(marker, first == std::string::npos ? 0 : first + 1);
  CHECK(first != std::string::npos);
  CHECK(second == std::string::npos);
}

TEST_CASE("ignores duplicate imports with equivalent relative paths") {
  auto baseDir = importResolverPath("include_duplicate_relative");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "lib.prime", "// INCLUDE_DUPLICATE_RELATIVE\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime",
                "import<\"lib.prime\">\n"
                "import<\"./lib.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find("INCLUDE_DUPLICATE_RELATIVE");
  const auto second = source.find("INCLUDE_DUPLICATE_RELATIVE", first == std::string::npos ? 0 : first + 1);
  CHECK(first != std::string::npos);
  CHECK(second == std::string::npos);
}

TEST_CASE("missing import fails") {
  const std::string srcPath = writeTemp("main_c.prime", "import<\"/tmp/does_not_exist.prime\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("failed to read import") != std::string::npos);
}

TEST_CASE("ignores import directives inside string literals") {
  const std::string srcPath =
      writeTemp("include_in_string.prime",
                "[return<int>]\n"
                "main() {\n"
                "  print_line(\"before import<\\\"/tmp/does_not_exist.prime\\\"> after\"utf8)\n"
                "  return(0i32)\n"
                "}\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("import<\\\"/tmp/does_not_exist.prime\\\">") != std::string::npos);
}

TEST_CASE("ignores import directives inside comments") {
  const std::string srcPath =
      writeTemp("include_in_comment.prime",
                "// import<\"/tmp/does_not_exist.prime\">\n"
                "[return<int>]\n"
                "main(){ return(0i32) }\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("import<\"/tmp/does_not_exist.prime\">") != std::string::npos);
}

TEST_CASE("ignores import keyword within identifiers") {
  const std::string srcPath =
      writeTemp("include_identifier.prime",
                "ximport<\"/tmp/does_not_exist.prime\">\n"
                "importX<\"/tmp/does_not_exist.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("ximport<\"/tmp/does_not_exist.prime\">") != std::string::npos);
  CHECK(source.find("importX<\"/tmp/does_not_exist.prime\">") != std::string::npos);
}

TEST_CASE("ignores import keyword after path separator") {
  const std::string srcPath =
      writeTemp("include_after_slash.prime",
                "path/import<\"/tmp/does_not_exist.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("path/import<\"/tmp/does_not_exist.prime\">") != std::string::npos);
}

TEST_CASE("resolves import from import path") {
  auto baseDir = importResolverPath("include_search_base");
  auto includeRoot = importResolverPath("include_search_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib.prime", "// INCLUDE_ROOT_MARKER\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_ROOT_MARKER") != std::string::npos);
}

TEST_CASE("resolves relative import from import path") {
  auto baseDir = importResolverPath("include_relative_base");
  auto includeRoot = importResolverPath("include_relative_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib.prime", "// INCLUDE_RELATIVE_MARKER\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"lib.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_RELATIVE_MARKER") != std::string::npos);
}

TEST_CASE("supports semicolon separated imports") {
  const std::string markerA = "SEMICOLON_A";
  const std::string markerB = "SEMICOLON_B";
  const std::string libA = writeTemp("lib_semicolon_a.prime", "// " + markerA + "\n");
  const std::string libB = writeTemp("lib_semicolon_b.prime", "// " + markerB + "\n");
  const std::string srcPath =
      writeTemp("main_semicolon.prime", "import<\"" + libA + "\"; \"" + libB + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find(markerA) != std::string::npos);
  CHECK(source.find(markerB) != std::string::npos);
}
TEST_SUITE_END();
