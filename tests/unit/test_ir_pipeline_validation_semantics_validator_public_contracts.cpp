#include "test_ir_pipeline_validation_helpers.h"

#include "primec/StdlibSurfaceRegistry.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

template <typename Entry, typename Predicate>
const Entry *findSemanticValidatorEntry(const std::vector<const Entry *> &entries,
                                        const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) {
                                 return entry != nullptr && predicate(*entry);
                               });
  return it == entries.end() ? nullptr : *it;
}

} // namespace

TEST_CASE("semantic product publishes validator inference routing facts") {
  const std::string source = R"(
import /std/collections/*

[return<i32>]
id_i32([i32] value) {
  return(value)
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] selected{id_i32(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] viaMethod{values.count()}
  [i32] viaBridge{count(values)}
  return(plus(selected, plus(viaMethod, viaBridge)))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out", "io_err"}));
  CHECK(error.empty());

  const auto *directEntry = findSemanticValidatorEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "id_i32" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) == "/id_i32";
      });
  REQUIRE(directEntry != nullptr);

  const auto *methodEntry = findSemanticValidatorEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "count" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(methodEntry != nullptr);
  CHECK(methodEntry->receiverTypeText.find("vector<i32>") != std::string::npos);

  const auto *bridgeEntry = findSemanticValidatorEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(bridgeEntry != nullptr);

  const auto *summaryEntry = findSemanticValidatorEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) == "/main";
      });
  REQUIRE(summaryEntry != nullptr);
  CHECK_FALSE(summaryEntry->isExecution);
  CHECK(summaryEntry->returnKind == "i32");
  CHECK(summaryEntry->activeEffects == std::vector<std::string>{"heap_alloc"});
}

TEST_CASE("semantic product publishes stdlib helper routing ids") {
  const std::string source = R"(
[return<i32>]
/std/file/FileError/status([i32] err) {
  return(err)
}

[return<i32>]
/std/collections/ContainerError/status([i32] err) {
  return(err)
}

[return<i32>]
/std/gfx/GfxError/status([i32] err) {
  return(err)
}

[return<i32>]
route_status() {
  return(plus(/std/file/FileError/status(1i32),
              plus(/std/collections/ContainerError/status(2i32),
                   /std/gfx/GfxError/status(3i32))))
}

[return<i32>]
main() {
  return(route_status())
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out", "io_err"}));
  CHECK(error.empty());

  const auto *fileDirectEntry = findSemanticValidatorEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/route_status" &&
               entry.callName == "/std/file/FileError/status" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/file/FileError/status";
      });
  REQUIRE(fileDirectEntry != nullptr);
  REQUIRE(fileDirectEntry->stdlibSurfaceId.has_value());
  CHECK(*fileDirectEntry->stdlibSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  const auto *containerDirectEntry = findSemanticValidatorEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/route_status" &&
               entry.callName == "/std/collections/ContainerError/status" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/ContainerError/status";
      });
  REQUIRE(containerDirectEntry != nullptr);
  REQUIRE(containerDirectEntry->stdlibSurfaceId.has_value());
  CHECK(*containerDirectEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsContainerErrorHelpers);

  const auto *gfxDirectEntry = findSemanticValidatorEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/route_status" &&
               entry.callName == "/std/gfx/GfxError/status" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/gfx/GfxError/status";
      });
  REQUIRE(gfxDirectEntry != nullptr);
  REQUIRE(gfxDirectEntry->stdlibSurfaceId.has_value());
  CHECK(*gfxDirectEntry->stdlibSurfaceId == primec::StdlibSurfaceId::GfxErrorHelpers);
}

TEST_SUITE_END();
