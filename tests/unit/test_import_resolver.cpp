#include "test_import_resolver_helpers.h"

#include "primec/CompilePipeline.h"
#include "primec/CliDriver.h"
#include "primec/SourceLocationMapper.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

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

std::string absolutePathText(const std::string &path) {
  return std::filesystem::absolute(path).string();
}

primec::Options diagnosticPipelineOptions(const std::string &inputPath) {
  primec::Options options;
  options.inputPath = inputPath;
  options.collectDiagnostics = true;
  options.skipSemanticProductForNonConsumingPath = true;
  return options;
}

std::filesystem::path repositoryStdlibPath() {
  return std::filesystem::exists("stdlib") ? std::filesystem::path("stdlib")
                                           : std::filesystem::path("..") / "stdlib";
}

primec::Options preAstPipelineOptions(const std::string &inputPath,
                                      const std::filesystem::path &stdlibPath) {
  primec::Options options;
  options.inputPath = inputPath;
  options.importPaths = {std::filesystem::absolute(stdlibPath).string()};
  options.dumpStage = "pre_ast";
  return options;
}

const primec::SourceUnit *findStdlibUnitByModuleKey(const primec::ExpandedSource &source,
                                                    std::string_view moduleKey) {
  auto it = std::find_if(source.units.begin(),
                         source.units.end(),
                         [&](const primec::SourceUnit &unit) {
                           return unit.kind == primec::SourceUnitKind::Stdlib &&
                                  unit.moduleKey == moduleKey;
                         });
  return it == source.units.end() ? nullptr : &*it;
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

TEST_CASE("expanded source diagnostic mapper indexes many segment lookups") {
  constexpr std::size_t SegmentCount = 384;
  primec::ExpandedSource expanded;
  expanded.units.reserve(SegmentCount);
  expanded.segments.reserve(SegmentCount);

  for (std::size_t index = 0; index < SegmentCount; ++index) {
    const int line = static_cast<int>(index) + 1;
    expanded.units.push_back(primec::SourceUnit{
        .id = index,
        .kind = primec::SourceUnitKind::Import,
        .displayPath = "segment_" + std::to_string(index) + ".prime",
        .moduleKey = "/segment/" + std::to_string(index),
        .flattenedStartLine = line,
        .flattenedStartColumn = 1,
        .flattenedEndLine = line,
        .flattenedEndColumn = 12,
        .originalStartLine = 1000 + line,
        .originalStartColumn = 3,
    });
    expanded.segments.push_back(primec::SourceSegment{
        .unitId = index,
        .flattenedStartLine = line,
        .flattenedStartColumn = 1,
        .flattenedEndLine = line,
        .flattenedEndColumn = 12,
        .originalStartLine = 1000 + line,
        .originalStartColumn = 3,
    });
  }

  primec::SourceLocationMapper mapper(expanded);
  const auto beforeStats = mapper.lookupStats();
  CHECK(beforeStats.indexedSegmentCount == SegmentCount);
  CHECK(beforeStats.lineBucketCount == SegmentCount);
  CHECK(beforeStats.indexedLookupCount == 0);

  std::vector<primec::DiagnosticSinkRecord> records;
  records.reserve(SegmentCount);
  for (std::size_t index = SegmentCount; index > 0; --index) {
    const int line = static_cast<int>(index);
    primec::DiagnosticSinkRecord record;
    record.message = "diagnostic " + std::to_string(index);
    record.primarySpan = primec::DiagnosticSpan{
        .file = "expanded.prime",
        .line = line,
        .column = 1,
        .endLine = line,
        .endColumn = 2,
    };
    record.hasPrimarySpan = true;
    records.push_back(std::move(record));
  }

  mapper.mapAndSortDiagnosticRecordsToSourceUnits(records);
  REQUIRE(records.size() == SegmentCount);
  for (std::size_t index = 0; index < SegmentCount; ++index) {
    const int originalLine = 1001 + static_cast<int>(index);
    CHECK(records[index].primarySpan.file ==
          "segment_" + std::to_string(index) + ".prime");
    CHECK(records[index].primarySpan.line == originalLine);
    CHECK(records[index].primarySpan.column == 3);
    CHECK(records[index].primarySpan.endLine == originalLine);
    CHECK(records[index].primarySpan.endColumn == 4);
  }

  const auto &afterStats = mapper.lookupStats();
  CHECK(afterStats.indexedLookupCount == SegmentCount * 3);
  CHECK(afterStats.segmentCandidateVisitCount == afterStats.indexedLookupCount);
  CHECK(afterStats.maxSegmentCandidatesVisitedPerLookup == 1);
  CHECK(afterStats.segmentCandidateVisitCount <
        SegmentCount * SegmentCount / 4);
}

TEST_CASE("expanded source diagnostic mapper preserves closed endpoint fallback") {
  primec::ExpandedSource expanded;
  expanded.units.push_back(primec::SourceUnit{
      .id = 0,
      .kind = primec::SourceUnitKind::Primary,
      .displayPath = "first.prime",
      .moduleKey = "/first",
      .flattenedStartLine = 1,
      .flattenedStartColumn = 1,
      .flattenedEndLine = 1,
      .flattenedEndColumn = 7,
      .originalStartLine = 20,
      .originalStartColumn = 1,
  });
  expanded.units.push_back(primec::SourceUnit{
      .id = 1,
      .kind = primec::SourceUnitKind::Import,
      .displayPath = "second.prime",
      .moduleKey = "/second",
      .flattenedStartLine = 1,
      .flattenedStartColumn = 7,
      .flattenedEndLine = 1,
      .flattenedEndColumn = 10,
      .originalStartLine = 40,
      .originalStartColumn = 1,
  });
  expanded.segments.push_back(primec::SourceSegment{
      .unitId = 0,
      .flattenedStartLine = 1,
      .flattenedStartColumn = 1,
      .flattenedEndLine = 1,
      .flattenedEndColumn = 7,
      .originalStartLine = 20,
      .originalStartColumn = 1,
  });
  expanded.segments.push_back(primec::SourceSegment{
      .unitId = 1,
      .flattenedStartLine = 1,
      .flattenedStartColumn = 7,
      .flattenedEndLine = 1,
      .flattenedEndColumn = 10,
      .originalStartLine = 40,
      .originalStartColumn = 1,
  });

  primec::SourceLocationMapper mapper(expanded);
  const auto mappedBoundary = mapper.mapExpandedSourceLocation(1, 7);
  REQUIRE(mappedBoundary.has_value());
  CHECK(mappedBoundary->file == "second.prime");
  CHECK(mappedBoundary->line == 40);
  CHECK(mappedBoundary->column == 1);

  const auto mappedClosedEnd = mapper.mapExpandedSourceLocation(1, 10);
  REQUIRE(mappedClosedEnd.has_value());
  CHECK(mappedClosedEnd->file == "second.prime");
  CHECK(mappedClosedEnd->line == 40);
  CHECK(mappedClosedEnd->column == 4);
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
  const std::filesystem::path stdlibPath = std::filesystem::exists("stdlib")
                                               ? std::filesystem::path("stdlib")
                                               : std::filesystem::path("..") / "stdlib";
  options.importPaths = {std::filesystem::absolute(stdlibPath).string()};
  options.dumpStage = "pre_ast";

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  INFO(error);
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

TEST_CASE("compile pipeline uses module manifest for direct gfx import") {
  auto baseDir = importResolverPath("stdlib_manifest_gfx_direct");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import /std/gfx\n"
                                        "[return<int>]\n"
                                        "main(){ return(0i32) }\n");

  primec::Options options = preAstPipelineOptions(srcPath, repositoryStdlibPath());
  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  INFO(error);
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error));
  CHECK(error.empty());
  CHECK(output.hasDumpOutput);

  const primec::SourceUnit *gfxUnit = findStdlibUnitByModuleKey(output.expandedSource, "/std/gfx");
  REQUIRE(gfxUnit != nullptr);
  CHECK(gfxUnit->displayPath.find("stdlib/std/gfx/gfx.prime") != std::string::npos);
  CHECK(output.expandedSource.text.find("namespace experimental") == std::string::npos);
  CHECK(output.expandedSource.text.find("/std/gfx/ignoreGfxError") != std::string::npos);
}

TEST_CASE("compile pipeline uses module manifest for gfx wildcard import") {
  auto baseDir = importResolverPath("stdlib_manifest_gfx_wildcard");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import /std/gfx/*\n"
                                        "[return<int>]\n"
                                        "main(){ return(0i32) }\n");

  primec::Options options = preAstPipelineOptions(srcPath, repositoryStdlibPath());
  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  INFO(error);
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error));
  CHECK(error.empty());
  CHECK(output.hasDumpOutput);

  const primec::SourceUnit *gfxUnit = findStdlibUnitByModuleKey(output.expandedSource, "/std/gfx");
  REQUIRE(gfxUnit != nullptr);
  CHECK(gfxUnit->displayPath.find("stdlib/std/gfx/gfx.prime") != std::string::npos);
  CHECK(output.expandedSource.text.find("namespace experimental") == std::string::npos);
  CHECK(output.expandedSource.text.find("/std/gfx/ignoreGfxError") != std::string::npos);
}

TEST_CASE("compile pipeline rejects malformed stdlib module manifest metadata") {
  auto runWithManifest = [](std::string_view scratchName,
                            const std::string &manifestContents,
                            std::string &error) {
    auto baseDir = importResolverPath(scratchName);
    std::filesystem::remove_all(baseDir);
    std::filesystem::create_directories(baseDir);
    const std::filesystem::path stdlibRoot = baseDir / "stdlib";
    writeFile(stdlibRoot / "std" / "modules.psmeta", manifestContents);
    writeFile(stdlibRoot / "std" / "gfx" / "gfx.prime",
              "namespace std { namespace gfx { [public return<void>] noop(){ return() } } }\n");
    const std::string srcPath = writeFile(baseDir / "main.prime",
                                          "import /std/gfx\n"
                                          "[return<int>]\n"
                                          "main(){ return(0i32) }\n");

    primec::Options options = preAstPipelineOptions(srcPath, stdlibRoot);
    primec::CompilePipelineOutput output;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    return primec::runCompilePipeline(options, output, errorStage, error);
  };

  SUBCASE("missing source_file") {
    std::string error;
    CHECK_FALSE(runWithManifest("stdlib_manifest_missing_source",
                                "[module]\n"
                                "root = /std/gfx\n",
                                error));
    CHECK(error.find("module entry missing source_file for /std/gfx") != std::string::npos);
  }

  SUBCASE("invalid source_file") {
    std::string error;
    CHECK_FALSE(runWithManifest("stdlib_manifest_invalid_source",
                                "[module]\n"
                                "root = /std/gfx\n"
                                "source_file = ../gfx.prime\n",
                                error));
    CHECK(error.find("source_file must be a relative stdlib path for /std/gfx") !=
          std::string::npos);
  }
}

TEST_CASE("compile pipeline maps parser diagnostics through source units") {
  auto baseDir = importResolverPath("source_diagnostics_parse");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  const std::string importedPath = writeFile(baseDir / "imported.prime",
                                             "import bad_imported\n"
                                             "[return<int>]\n"
                                             "imported(){ return(1i32) }\n");
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import<\"imported.prime\">\n"
                                        "import bad_primary\n"
                                        "[return<int>]\n"
                                        "main(){ return(0i32) }\n");

  primec::Options options = diagnosticPipelineOptions(srcPath);
  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnostics;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  CHECK_FALSE(primec::runCompilePipeline(options, output, errorStage, error, &diagnostics));
  CHECK(errorStage == primec::CompilePipelineErrorStage::Parse);

  REQUIRE(diagnostics.records.size() >= 2);
  const auto &primaryRecord = diagnostics.records[0];
  const auto &importedRecord = diagnostics.records[1];
  CHECK(primaryRecord.message == "import path must be a slash path");
  CHECK(importedRecord.message == "import path must be a slash path");
  REQUIRE(primaryRecord.hasPrimarySpan);
  REQUIRE(importedRecord.hasPrimarySpan);
  CHECK(primaryRecord.primarySpan.file == absolutePathText(srcPath));
  CHECK(primaryRecord.primarySpan.line == 3);
  CHECK(primaryRecord.primarySpan.column == 1);
  CHECK(importedRecord.primarySpan.file == absolutePathText(importedPath));
  CHECK(importedRecord.primarySpan.line == 2);
  CHECK(importedRecord.primarySpan.column == 1);
}

TEST_CASE("parser diagnostic stability contract exposes code notes and source-unit spans") {
  auto baseDir = importResolverPath("source_diagnostics_parse_stability");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import bad_primary\n"
                                        "[return<int>]\n"
                                        "main(){ return(0i32) }\n");

  primec::Options options = diagnosticPipelineOptions(srcPath);
  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnostics;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  CHECK_FALSE(primec::runCompilePipeline(options, output, errorStage, error, &diagnostics));
  CHECK(errorStage == primec::CompilePipelineErrorStage::Parse);
  REQUIRE(output.hasFailure);

  const primec::CliFailure failure = primec::describeCompilePipelineFailure(output);
  CHECK(failure.code == primec::DiagnosticCode::ParseError);
  CHECK(failure.notes == std::vector<std::string>{"stage: parse"});
  REQUIRE(failure.diagnosticInfo.has_value());
  REQUIRE(failure.diagnosticInfo->records.size() == 1);

  const primec::DiagnosticSinkRecord &record = failure.diagnosticInfo->records.front();
  CHECK(record.message == "import path must be a slash path");
  REQUIRE(record.hasPrimarySpan);
  CHECK(record.primarySpan.file == absolutePathText(srcPath));
  CHECK(record.primarySpan.line == 2);
  CHECK(record.primarySpan.column == 1);
  CHECK(record.primarySpan.endLine == 2);
  CHECK(record.primarySpan.endColumn == 1);

  const primec::DiagnosticRecord publicRecord =
      primec::makeDiagnosticRecord(failure.code,
                                   record.message,
                                   srcPath,
                                   failure.notes,
                                   &record.primarySpan,
                                   record.relatedSpans);
  CHECK(publicRecord.code == "PSC1003");
  CHECK(publicRecord.message == "import path must be a slash path");
  CHECK(publicRecord.notes == std::vector<std::string>{"stage: parse"});
  CHECK(publicRecord.primarySpan.file == absolutePathText(srcPath));
  CHECK(publicRecord.primarySpan.line == 2);
  CHECK(publicRecord.primarySpan.column == 1);
  CHECK(publicRecord.relatedSpans.empty());
}

TEST_CASE("compile pipeline maps imported semantic diagnostics through source units") {
  auto baseDir = importResolverPath("source_diagnostics_semantic");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  const std::string importedPath = writeFile(baseDir / "imported.prime",
                                             "[return<int>]\n"
                                             "imported() {\n"
                                             "  return(missing_imported(1i32))\n"
                                             "}\n");
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import<\"imported.prime\">\n"
                                        "[return<int>]\n"
                                        "main(){ return(0i32) }\n");

  primec::Options options = diagnosticPipelineOptions(srcPath);
  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnostics;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  CHECK_FALSE(primec::runCompilePipeline(options, output, errorStage, error, &diagnostics));
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);

  const auto recordIt = std::find_if(diagnostics.records.begin(),
                                     diagnostics.records.end(),
                                     [](const primec::DiagnosticSinkRecord &record) {
                                       return record.message ==
                                              "unknown call target: missing_imported";
                                     });
  REQUIRE(recordIt != diagnostics.records.end());
  REQUIRE(recordIt->hasPrimarySpan);
  CHECK(recordIt->primarySpan.file == absolutePathText(importedPath));
  CHECK(recordIt->primarySpan.line == 3);
  CHECK(recordIt->primarySpan.column == 10);
  REQUIRE(recordIt->relatedSpans.size() == 1);
  CHECK(recordIt->relatedSpans[0].label == "definition: /imported");
  CHECK(recordIt->relatedSpans[0].span.file == absolutePathText(importedPath));
  CHECK(recordIt->relatedSpans[0].span.line == 2);
  CHECK(recordIt->relatedSpans[0].span.column == 1);
}

TEST_CASE("semantic unknown-call stability contract exposes mapped notes") {
  auto baseDir = importResolverPath("source_diagnostics_semantic_stability");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  const std::string importedPath = writeFile(baseDir / "imported.prime",
                                             "[return<int>]\n"
                                             "imported() {\n"
                                             "  return(missing_imported(1i32))\n"
                                             "}\n");
  const std::string srcPath = writeFile(baseDir / "main.prime",
                                        "import<\"imported.prime\">\n"
                                        "[return<int>]\n"
                                        "main(){ return(0i32) }\n");

  primec::Options options = diagnosticPipelineOptions(srcPath);
  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnostics;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  CHECK_FALSE(primec::runCompilePipeline(options, output, errorStage, error, &diagnostics));
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  REQUIRE(output.hasFailure);

  const primec::CliFailure failure = primec::describeCompilePipelineFailure(output);
  CHECK(failure.code == primec::DiagnosticCode::SemanticError);
  REQUIRE(failure.diagnosticInfo.has_value());

  const auto recordIt = std::find_if(failure.diagnosticInfo->records.begin(),
                                     failure.diagnosticInfo->records.end(),
                                     [](const primec::DiagnosticSinkRecord &record) {
                                       return record.message ==
                                              "unknown call target: missing_imported";
                                     });
  REQUIRE(recordIt != failure.diagnosticInfo->records.end());
  REQUIRE(recordIt->hasPrimarySpan);
  REQUIRE(recordIt->relatedSpans.size() == 1);

  const primec::DiagnosticStabilityContract contract =
      primec::diagnosticStabilityContract(failure.code, recordIt->message);
  CHECK(contract.code == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.message == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.primarySpan == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.notes == primec::DiagnosticStabilityTier::Stable);

  const primec::DiagnosticRecord publicRecord =
      primec::makeDiagnosticRecord(failure.code,
                                   recordIt->message,
                                   srcPath,
                                   failure.notes,
                                   &recordIt->primarySpan,
                                   recordIt->relatedSpans);
  CHECK(publicRecord.code == "PSC1005");
  CHECK(publicRecord.message == "unknown call target: missing_imported");
  CHECK(std::find(publicRecord.notes.begin(), publicRecord.notes.end(), "stage: semantic") !=
        publicRecord.notes.end());
  CHECK(publicRecord.primarySpan.file == absolutePathText(importedPath));
  CHECK(publicRecord.primarySpan.line == 3);
  CHECK(publicRecord.primarySpan.column == 10);
  CHECK(publicRecord.primarySpan.endLine == 3);
  CHECK(publicRecord.primarySpan.endColumn == 10);
  REQUIRE(publicRecord.relatedSpans.size() == 1);
  CHECK(publicRecord.relatedSpans[0].label == "definition: /imported");
  CHECK(publicRecord.relatedSpans[0].span.file == absolutePathText(importedPath));
  CHECK(publicRecord.relatedSpans[0].span.line == 2);
  CHECK(publicRecord.relatedSpans[0].span.column == 1);
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
