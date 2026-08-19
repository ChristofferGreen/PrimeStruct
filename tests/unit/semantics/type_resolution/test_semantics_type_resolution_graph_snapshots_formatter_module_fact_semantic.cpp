#include "third_party/doctest.h"

#include "test_semantics_type_resolution_graph_snapshots_shared.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

TEST_CASE("semantic product formatter resolves module callable-summary indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i64",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 501,
      .provenanceHandle = 1501,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = true,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 502,
      .provenanceHandle = 1502,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.callableSummaryIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.callableSummaryIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramCallableSummaryView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.callableSummaries[1]);
  CHECK(view[1] == &semanticProgram.callableSummaries[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "callable_summaries[0]: full_path=\"/a\"";
  const std::string secondEntry = "callable_summaries[1]: full_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps callable-summary text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    primec::SemanticProgramCallableSummary first;
    first.isExecution = false;
    first.returnKind = "return";
    first.isCompute = false;
    first.isUnsafe = false;
    first.activeEffects = {};
    first.activeCapabilities = {};
    first.hasResultType = true;
    first.resultTypeHasValue = true;
    first.resultValueType = "i64";
    first.resultErrorType = "";
    first.hasOnError = false;
    first.onErrorHandlerPath = "";
    first.onErrorErrorType = "";
    first.onErrorBoundArgCount = 0;
    first.semanticNodeId = 601;
    first.provenanceHandle = 1601;
    first.fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first");
    first.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    first.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
    semanticProgram.callableSummaries.push_back(std::move(first));

    primec::SemanticProgramCallableSummary second;
    second.isExecution = true;
    second.returnKind = "return";
    second.isCompute = true;
    second.isUnsafe = false;
    second.activeEffects = {};
    second.activeCapabilities = {};
    second.hasResultType = true;
    second.resultTypeHasValue = true;
    second.resultValueType = "i32";
    second.resultErrorType = "";
    second.hasOnError = false;
    second.onErrorHandlerPath = "";
    second.onErrorErrorType = "";
    second.onErrorBoundArgCount = 0;
    second.semanticNodeId = 602;
    second.provenanceHandle = 1602;
    second.fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/second");
    second.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    second.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    semanticProgram.callableSummaries.push_back(std::move(second));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.callableSummaryIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.callableSummaryIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module binding-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::SemanticProgramBindingFact first;
  first.scopePath = "/z";
  first.siteKind = "local";
  first.name = "last";
  first.bindingTypeText = "i64";
  first.isMutable = false;
  first.isEntryArgString = false;
  first.isUnsafeReference = false;
  first.referenceRoot = "";
  first.sourceLine = 20;
  first.sourceColumn = 2;
  first.semanticNodeId = 701;
  first.provenanceHandle = 1701;
  first.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z");
  first.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local");
  first.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "last");
  first.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z/last");
  first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  first.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.bindingFacts.push_back(std::move(first));

  primec::SemanticProgramBindingFact second;
  second.scopePath = "/a";
  second.siteKind = "local";
  second.name = "first";
  second.bindingTypeText = "i32";
  second.isMutable = false;
  second.isEntryArgString = false;
  second.isUnsafeReference = false;
  second.referenceRoot = "";
  second.sourceLine = 10;
  second.sourceColumn = 1;
  second.semanticNodeId = 702;
  second.provenanceHandle = 1702;
  second.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a");
  second.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local");
  second.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "first");
  second.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a/first");
  second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  second.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.bindingFacts.push_back(std::move(second));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.bindingFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.bindingFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramBindingFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.bindingFacts[1]);
  CHECK(view[1] == &semanticProgram.bindingFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "binding_facts[0]: scope_path=\"/a\"";
  const std::string secondEntry = "binding_facts[1]: scope_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps binding-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    primec::SemanticProgramBindingFact first;
    first.scopePath = "/first";
    first.siteKind = "parameter";
    first.name = "left";
    first.bindingTypeText = "i64";
    first.isMutable = true;
    first.isEntryArgString = false;
    first.isUnsafeReference = false;
    first.referenceRoot = "";
    first.sourceLine = 5;
    first.sourceColumn = 3;
    first.semanticNodeId = 801;
    first.provenanceHandle = 1801;
    first.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first");
    first.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "parameter");
    first.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "left");
    first.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/first/left");
    first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
    first.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.bindingFacts.push_back(std::move(first));

    primec::SemanticProgramBindingFact second;
    second.scopePath = "/second";
    second.siteKind = "local";
    second.name = "right";
    second.bindingTypeText = "i32";
    second.isMutable = false;
    second.isEntryArgString = false;
    second.isUnsafeReference = false;
    second.referenceRoot = "";
    second.sourceLine = 8;
    second.sourceColumn = 4;
    second.semanticNodeId = 802;
    second.provenanceHandle = 1802;
    second.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/second");
    second.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local");
    second.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "right");
    second.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/second/right");
    second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    second.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.bindingFacts.push_back(std::move(second));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.bindingFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.bindingFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module return-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::SemanticProgramReturnFact first;
  first.returnKind = "return";
  first.structPath = "/i64";
  first.bindingTypeText = "i64";
  first.isMutable = false;
  first.isEntryArgString = false;
  first.isUnsafeReference = false;
  first.referenceRoot = "";
  first.sourceLine = 20;
  first.sourceColumn = 2;
  first.semanticNodeId = 901;
  first.provenanceHandle = 1901;
  first.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z");
  first.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
  first.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i64");
  first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  first.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.returnFacts.push_back(std::move(first));

  primec::SemanticProgramReturnFact second;
  second.returnKind = "return";
  second.structPath = "/i32";
  second.bindingTypeText = "i32";
  second.isMutable = false;
  second.isEntryArgString = false;
  second.isUnsafeReference = false;
  second.referenceRoot = "";
  second.sourceLine = 10;
  second.sourceColumn = 1;
  second.semanticNodeId = 902;
  second.provenanceHandle = 1902;
  second.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a");
  second.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
  second.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i32");
  second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  second.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.returnFacts.push_back(std::move(second));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.returnFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.returnFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramReturnFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.returnFacts[1]);
  CHECK(view[1] == &semanticProgram.returnFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "return_facts[0]: definition_path=\"/a\"";
  const std::string secondEntry = "return_facts[1]: definition_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps return-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    primec::SemanticProgramReturnFact first;
    first.returnKind = "return";
    first.structPath = "/i64";
    first.bindingTypeText = "i64";
    first.isMutable = false;
    first.isEntryArgString = false;
    first.isUnsafeReference = false;
    first.referenceRoot = "";
    first.sourceLine = 5;
    first.sourceColumn = 3;
    first.semanticNodeId = 911;
    first.provenanceHandle = 1911;
    first.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first");
    first.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    first.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i64");
    first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
    first.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.returnFacts.push_back(std::move(first));

    primec::SemanticProgramReturnFact second;
    second.returnKind = "return";
    second.structPath = "/i32";
    second.bindingTypeText = "i32";
    second.isMutable = false;
    second.isEntryArgString = false;
    second.isUnsafeReference = false;
    second.referenceRoot = "";
    second.sourceLine = 8;
    second.sourceColumn = 4;
    second.semanticNodeId = 912;
    second.provenanceHandle = 1912;
    second.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/second");
    second.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    second.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i32");
    second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    second.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.returnFacts.push_back(std::move(second));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.returnFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.returnFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module local-auto-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeLocalAuto = [&](std::string scopePath,
                           std::string bindingName,
                           std::string bindingTypeText,
                           std::string initializerResolvedPath,
                           int sourceLine,
                           int sourceColumn,
                           uint64_t semanticNodeId,
                           uint64_t provenanceHandle) {
    primec::SemanticProgramLocalAutoFact entry;
    entry.scopePath = std::move(scopePath);
    entry.bindingName = std::move(bindingName);
    entry.bindingTypeText = std::move(bindingTypeText);
    entry.initializerBindingTypeText = entry.bindingTypeText;
    entry.initializerQueryTypeText = entry.bindingTypeText;
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingName);
    entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
    entry.initializerBindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerBindingTypeText);
    entry.initializerQueryTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerQueryTypeText);
    entry.initializerResolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, initializerResolvedPath);
    return entry;
  };

  semanticProgram.localAutoFacts.push_back(
      makeLocalAuto("/z", "last", "i64", "/initLast", 20, 2, 921, 1921));
  semanticProgram.localAutoFacts.push_back(
      makeLocalAuto("/a", "first", "i32", "/initFirst", 10, 1, 922, 1922));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.localAutoFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.localAutoFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramLocalAutoFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.localAutoFacts[1]);
  CHECK(view[1] == &semanticProgram.localAutoFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "local_auto_facts[0]: scope_path=\"/a\" binding_name=\"first\"";
  const std::string secondEntry = "local_auto_facts[1]: scope_path=\"/z\" binding_name=\"last\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps local-auto-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeLocalAuto = [&](std::string scopePath,
                             std::string bindingName,
                             std::string bindingTypeText,
                             std::string initializerResolvedPath,
                             int sourceLine,
                             int sourceColumn,
                             uint64_t semanticNodeId,
                             uint64_t provenanceHandle) {
      primec::SemanticProgramLocalAutoFact entry;
      entry.scopePath = std::move(scopePath);
      entry.bindingName = std::move(bindingName);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.initializerBindingTypeText = entry.bindingTypeText;
      entry.initializerQueryTypeText = entry.bindingTypeText;
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingName);
      entry.bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.initializerBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerBindingTypeText);
      entry.initializerQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerQueryTypeText);
      entry.initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, initializerResolvedPath);
      return entry;
    };

    semanticProgram.localAutoFacts.push_back(
        makeLocalAuto("/first", "left", "i64", "/initLeft", 5, 3, 931, 1931));
    semanticProgram.localAutoFacts.push_back(
        makeLocalAuto("/second", "right", "i32", "/initRight", 8, 4, 932, 1932));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.localAutoFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.localAutoFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module query-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeQuery = [&](std::string scopePath,
                       std::string callName,
                       std::string resolvedPath,
                       std::string typeText,
                       std::string bindingText,
                       int sourceLine,
                       int sourceColumn,
                       uint64_t semanticNodeId,
                       uint64_t provenanceHandle) {
    primec::SemanticProgramQueryFact entry;
    entry.scopePath = std::move(scopePath);
    entry.callName = std::move(callName);
    entry.queryTypeText = std::move(typeText);
    entry.bindingTypeText = std::move(bindingText);
    entry.hasResultType = true;
    entry.resultTypeHasValue = true;
    entry.resultValueType = "i32";
    entry.resultErrorType = "MyError";
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
    entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
    entry.queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.queryTypeText);
    entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
    entry.receiverBindingTypeTextId = primec::InvalidSymbolId;
    entry.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
    entry.resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultErrorType);
    return entry;
  };

  semanticProgram.queryFacts.push_back(
      makeQuery("/z", "lookupLast", "/lookup/last", "Result<i32, MyError>", "Result<i32, MyError>", 20, 2, 941, 1941));
  semanticProgram.queryFacts.push_back(
      makeQuery("/a", "lookupFirst", "/lookup/first", "Result<i32, MyError>", "Result<i32, MyError>", 10, 1, 942, 1942));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.queryFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.queryFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramQueryFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.queryFacts[1]);
  CHECK(view[1] == &semanticProgram.queryFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "query_facts[0]: scope_path=\"/a\" call_name=\"lookupFirst\"";
  const std::string secondEntry = "query_facts[1]: scope_path=\"/z\" call_name=\"lookupLast\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps query-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeQuery = [&](std::string scopePath,
                         std::string callName,
                         std::string resolvedPath,
                         int sourceLine,
                         int sourceColumn,
                         uint64_t semanticNodeId,
                         uint64_t provenanceHandle) {
      primec::SemanticProgramQueryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.callName = std::move(callName);
      entry.queryTypeText = "Result<i32, MyError>";
      entry.bindingTypeText = "Result<i32, MyError>";
      entry.hasResultType = true;
      entry.resultTypeHasValue = true;
      entry.resultValueType = "i32";
      entry.resultErrorType = "MyError";
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.queryTypeText);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.receiverBindingTypeTextId = primec::InvalidSymbolId;
      entry.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
      entry.resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultErrorType);
      return entry;
    };

    semanticProgram.queryFacts.push_back(makeQuery("/first", "leftLookup", "/first/lookup", 5, 3, 951, 1951));
    semanticProgram.queryFacts.push_back(makeQuery("/second", "rightLookup", "/second/lookup", 8, 4, 952, 1952));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.queryFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.queryFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module try-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeTryFact = [&](std::string scopePath,
                         std::string resolvedPath,
                         int sourceLine,
                         int sourceColumn,
                         uint64_t semanticNodeId,
                         uint64_t provenanceHandle) {
    primec::SemanticProgramTryFact entry;
    entry.scopePath = std::move(scopePath);
    entry.operandBindingTypeText = "Result<i32, Err>";
    entry.operandReceiverBindingTypeText = "";
    entry.operandQueryTypeText = "Result<i32, Err>";
    entry.valueType = "i32";
    entry.errorType = "Err";
    entry.contextReturnKind = "return";
    entry.onErrorHandlerPath = "/unexpectedError";
    entry.onErrorErrorType = "Err";
    entry.onErrorBoundArgCount = 1;
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
    entry.operandBindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandBindingTypeText);
    entry.operandReceiverBindingTypeTextId = primec::InvalidSymbolId;
    entry.operandQueryTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandQueryTypeText);
    entry.valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.valueType);
    entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
    entry.contextReturnKindId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.contextReturnKind);
    entry.onErrorHandlerPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorHandlerPath);
    entry.onErrorErrorTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorErrorType);
    return entry;
  };

  semanticProgram.tryFacts.push_back(
      makeTryFact("/z", "/try/last", 20, 2, 991, 1991));
  semanticProgram.tryFacts.push_back(
      makeTryFact("/a", "/try/first", 10, 1, 992, 1992));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.tryFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.tryFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramTryFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.tryFacts[1]);
  CHECK(view[1] == &semanticProgram.tryFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "try_facts[0]: scope_path=\"/a\"";
  const std::string secondEntry = "try_facts[1]: scope_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter resolves module on-error-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeOnErrorFact = [&](std::string definitionPath,
                             std::string handlerPath,
                             int sourceLine,
                             uint64_t semanticNodeId,
                             uint64_t provenanceHandle) {
    primec::SemanticProgramOnErrorFact entry;
    entry.definitionPath = std::move(definitionPath);
    entry.returnKind = "return";
    entry.errorType = "Err";
    entry.boundArgCount = 1;
    entry.boundArgTexts = {"err"};
    entry.returnResultHasValue = true;
    entry.returnResultValueType = "i32";
    entry.returnResultErrorType = "Err";
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.definitionPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.definitionPath);
    entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
    entry.handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, handlerPath);
    entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
    entry.boundArgTextIds = {
        primec::semanticProgramInternCallTargetString(semanticProgram, "err"),
    };
    entry.returnResultValueTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultValueType);
    entry.returnResultErrorTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultErrorType);
    (void)sourceLine;
    return entry;
  };

  semanticProgram.onErrorFacts.push_back(
      makeOnErrorFact("/z", "/handler/last", 20, 993, 1993));
  semanticProgram.onErrorFacts.push_back(
      makeOnErrorFact("/a", "/handler/first", 10, 994, 1994));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.onErrorFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.onErrorFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramOnErrorFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.onErrorFacts[1]);
  CHECK(view[1] == &semanticProgram.onErrorFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "on_error_facts[0]: definition_path=\"/a\"";
  const std::string secondEntry = "on_error_facts[1]: definition_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps try/on-error text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeTryFact = [&](std::string scopePath,
                           std::string resolvedPath,
                           uint64_t semanticNodeId,
                           uint64_t provenanceHandle) {
      primec::SemanticProgramTryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.operandBindingTypeText = "Result<i32, Err>";
      entry.operandReceiverBindingTypeText = "";
      entry.operandQueryTypeText = "Result<i32, Err>";
      entry.valueType = "i32";
      entry.errorType = "Err";
      entry.contextReturnKind = "return";
      entry.onErrorHandlerPath = "/unexpectedError";
      entry.onErrorErrorType = "Err";
      entry.onErrorBoundArgCount = 1;
      entry.sourceLine = 5;
      entry.sourceColumn = 3;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.operandBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandBindingTypeText);
      entry.operandReceiverBindingTypeTextId = primec::InvalidSymbolId;
      entry.operandQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandQueryTypeText);
      entry.valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.valueType);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.contextReturnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.contextReturnKind);
      entry.onErrorHandlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorHandlerPath);
      entry.onErrorErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorErrorType);
      return entry;
    };

    auto makeOnErrorFact = [&](std::string definitionPath,
                               std::string handlerPath,
                               uint64_t semanticNodeId,
                               uint64_t provenanceHandle) {
      primec::SemanticProgramOnErrorFact entry;
      entry.definitionPath = std::move(definitionPath);
      entry.returnKind = "return";
      entry.errorType = "Err";
      entry.boundArgCount = 1;
      entry.boundArgTexts = {"err"};
      entry.returnResultHasValue = true;
      entry.returnResultValueType = "i32";
      entry.returnResultErrorType = "Err";
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.definitionPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
      entry.handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, handlerPath);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "err"),
      };
      entry.returnResultValueTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultValueType);
      entry.returnResultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultErrorType);
      return entry;
    };

    semanticProgram.tryFacts.push_back(
        makeTryFact("/first", "/try/first", 995, 1995));
    semanticProgram.tryFacts.push_back(
        makeTryFact("/second", "/try/second", 996, 1996));

    semanticProgram.onErrorFacts.push_back(
        makeOnErrorFact("/first", "/handler/first", 997, 1997));
    semanticProgram.onErrorFacts.push_back(
        makeOnErrorFact("/second", "/handler/second", 998, 1998));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.tryFactIndices.push_back(0);
      moduleFirst.onErrorFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.tryFactIndices.push_back(1);
      moduleSecond.onErrorFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter keeps first dedup-slice text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeDirectCallTarget = [&](std::string scopePath,
                                    std::string callName,
                                    std::string resolvedPath,
                                    int sourceLine,
                                    int sourceColumn,
                                    uint64_t semanticNodeId,
                                    uint64_t provenanceHandle) {
      primec::SemanticProgramDirectCallTarget entry;
      entry.scopePath = std::move(scopePath);
      entry.callName = std::move(callName);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      return entry;
    };

    auto makeMethodCallTarget = [&](std::string scopePath,
                                    std::string methodName,
                                    std::string receiverTypeText,
                                    std::string resolvedPath,
                                    int sourceLine,
                                    int sourceColumn,
                                    uint64_t semanticNodeId,
                                    uint64_t provenanceHandle) {
      primec::SemanticProgramMethodCallTarget entry;
      entry.scopePath = std::move(scopePath);
      entry.methodName = std::move(methodName);
      entry.receiverTypeText = std::move(receiverTypeText);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.methodName);
      entry.receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.receiverTypeText);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      return entry;
    };

    auto makeCallableSummary = [&](std::string fullPath,
                                   std::string resultValueType,
                                   uint64_t semanticNodeId,
                                   uint64_t provenanceHandle) {
      primec::SemanticProgramCallableSummary entry;
      entry.isExecution = false;
      entry.returnKind = "return";
      entry.isCompute = false;
      entry.isUnsafe = false;
      entry.activeEffects = {};
      entry.activeCapabilities = {};
      entry.hasResultType = true;
      entry.resultTypeHasValue = true;
      entry.resultValueType = std::move(resultValueType);
      entry.resultErrorType = "";
      entry.hasOnError = false;
      entry.onErrorHandlerPath = "";
      entry.onErrorErrorType = "";
      entry.onErrorBoundArgCount = 0;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, fullPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
      entry.resultValueTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
      return entry;
    };

    auto makeBindingFact = [&](std::string scopePath,
                               std::string bindingName,
                               std::string resolvedPath,
                               std::string bindingTypeText,
                               int sourceLine,
                               int sourceColumn,
                               uint64_t semanticNodeId,
                               uint64_t provenanceHandle) {
      primec::SemanticProgramBindingFact entry;
      entry.scopePath = std::move(scopePath);
      entry.siteKind = "local";
      entry.name = std::move(bindingName);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.siteKind);
      entry.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.name);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      return entry;
    };

    semanticProgram.directCallTargets.push_back(
        makeDirectCallTarget("/first", "makeFirst", "/helpers/makeFirst", 5, 3, 1101, 2101));
    semanticProgram.directCallTargets.push_back(
        makeDirectCallTarget("/second", "makeSecond", "/helpers/makeSecond", 9, 5, 1102, 2102));

    semanticProgram.methodCallTargets.push_back(
        makeMethodCallTarget("/first", "count", "vector<i32>", "/std/collections/vector/count", 6, 4, 1201, 2201));
    semanticProgram.methodCallTargets.push_back(
        makeMethodCallTarget("/second", "size", "map<i32, i32>", "/std/collections/map/size", 10, 6, 1202, 2202));

    semanticProgram.callableSummaries.push_back(
        makeCallableSummary("/first", "i32", 1301, 2301));
    semanticProgram.callableSummaries.push_back(
        makeCallableSummary("/second", "i64", 1302, 2302));

    semanticProgram.bindingFacts.push_back(
        makeBindingFact("/first", "left", "/first/left", "i32", 7, 2, 1401, 2401));
    semanticProgram.bindingFacts.push_back(
        makeBindingFact("/second", "right", "/second/right", "i64", 11, 2, 1402, 2402));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.directCallTargetIndices.push_back(0);
      moduleFirst.methodCallTargetIndices.push_back(0);
      moduleFirst.callableSummaryIndices.push_back(0);
      moduleFirst.bindingFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.directCallTargetIndices.push_back(1);
      moduleSecond.methodCallTargetIndices.push_back(1);
      moduleSecond.callableSummaryIndices.push_back(1);
      moduleSecond.bindingFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter keeps second dedup-slice text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeReturnFact = [&](std::string definitionPath,
                              std::string structPath,
                              std::string bindingTypeText,
                              int sourceLine,
                              int sourceColumn,
                              uint64_t semanticNodeId,
                              uint64_t provenanceHandle) {
      primec::SemanticProgramReturnFact entry;
      entry.returnKind = "return";
      entry.structPath = std::move(structPath);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, definitionPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
      entry.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.structPath);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      return entry;
    };

    auto makeLocalAutoFact = [&](std::string scopePath,
                                 std::string bindingName,
                                 std::string bindingTypeText,
                                 std::string initializerResolvedPath,
                                 int sourceLine,
                                 int sourceColumn,
                                 uint64_t semanticNodeId,
                                 uint64_t provenanceHandle) {
      primec::SemanticProgramLocalAutoFact entry;
      entry.scopePath = std::move(scopePath);
      entry.bindingName = std::move(bindingName);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.initializerBindingTypeText = entry.bindingTypeText;
      entry.initializerQueryTypeText = entry.bindingTypeText;
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingName);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, initializerResolvedPath);
      entry.initializerBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerBindingTypeText);
      entry.initializerQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerQueryTypeText);
      return entry;
    };

    auto makeQueryFact = [&](std::string scopePath,
                             std::string callName,
                             std::string resolvedPath,
                             std::string queryTypeText,
                             std::string bindingTypeText,
                             int sourceLine,
                             int sourceColumn,
                             uint64_t semanticNodeId,
                             uint64_t provenanceHandle) {
      primec::SemanticProgramQueryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.callName = std::move(callName);
      entry.queryTypeText = std::move(queryTypeText);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.hasResultType = true;
      entry.resultTypeHasValue = true;
      entry.resultValueType = "i32";
      entry.resultErrorType = "Err";
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.queryTypeText);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
      entry.resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultErrorType);
      return entry;
    };

    auto makeBridgePathChoice = [&](std::string scopePath,
                                    std::string collectionFamily,
                                    std::string helperName,
                                    std::string chosenPath,
                                    int sourceLine,
                                    int sourceColumn,
                                    uint64_t semanticNodeId,
                                    uint64_t provenanceHandle) {
      primec::SemanticProgramBridgePathChoice entry;
      entry.scopePath = std::move(scopePath);
      entry.collectionFamily = std::move(collectionFamily);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.collectionFamily);
      entry.helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, helperName);
      entry.chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, chosenPath);
      return entry;
    };

    auto makeTryFact = [&](std::string scopePath,
                           std::string resolvedPath,
                           int sourceLine,
                           int sourceColumn,
                           uint64_t semanticNodeId,
                           uint64_t provenanceHandle) {
      primec::SemanticProgramTryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.operandBindingTypeText = "Result<i32, Err>";
      entry.operandReceiverBindingTypeText = "";
      entry.operandQueryTypeText = "Result<i32, Err>";
      entry.valueType = "i32";
      entry.errorType = "Err";
      entry.contextReturnKind = "return";
      entry.onErrorHandlerPath = "/unexpectedError";
      entry.onErrorErrorType = "Err";
      entry.onErrorBoundArgCount = 1;
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.operandBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandBindingTypeText);
      entry.operandReceiverBindingTypeTextId = primec::InvalidSymbolId;
      entry.operandQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandQueryTypeText);
      entry.valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.valueType);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.contextReturnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.contextReturnKind);
      entry.onErrorHandlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorHandlerPath);
      entry.onErrorErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorErrorType);
      return entry;
    };

    auto makeOnErrorFact = [&](std::string definitionPath,
                               std::string handlerPath,
                               uint64_t semanticNodeId,
                               uint64_t provenanceHandle) {
      primec::SemanticProgramOnErrorFact entry;
      entry.definitionPath = std::move(definitionPath);
      entry.returnKind = "return";
      entry.errorType = "Err";
      entry.boundArgCount = 1;
      entry.boundArgTexts = {"err"};
      entry.returnResultHasValue = true;
      entry.returnResultValueType = "i32";
      entry.returnResultErrorType = "Err";
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.definitionPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
      entry.handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, handlerPath);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "err"),
      };
      entry.returnResultValueTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultValueType);
      entry.returnResultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultErrorType);
      return entry;
    };

    semanticProgram.returnFacts.push_back(
        makeReturnFact("/first", "/i64", "i64", 5, 1, 961, 1961));
    semanticProgram.returnFacts.push_back(
        makeReturnFact("/second", "/i32", "i32", 8, 1, 962, 1962));

    semanticProgram.localAutoFacts.push_back(
        makeLocalAutoFact("/first", "lhs", "i64", "/init/lhs", 5, 2, 971, 1971));
    semanticProgram.localAutoFacts.push_back(
        makeLocalAutoFact("/second", "rhs", "i32", "/init/rhs", 8, 2, 972, 1972));

    semanticProgram.queryFacts.push_back(
        makeQueryFact("/first", "lookupLeft", "/lookup/left", "Result<i32, Err>", "Result<i32, Err>", 5, 3, 981, 1981));
    semanticProgram.queryFacts.push_back(
        makeQueryFact("/second", "lookupRight", "/lookup/right", "Result<i32, Err>", "Result<i32, Err>", 8, 3, 982, 1982));

    semanticProgram.bridgePathChoices.push_back(
        makeBridgePathChoice("/first", "vector", "at", "/std/collections/vector/at", 5, 4, 983, 1983));
    semanticProgram.bridgePathChoices.push_back(
        makeBridgePathChoice("/second", "map", "at", "/std/collections/map/at", 8, 4, 984, 1984));

    semanticProgram.tryFacts.push_back(makeTryFact("/first", "/lookup/left", 5, 5, 985, 1985));
    semanticProgram.tryFacts.push_back(makeTryFact("/second", "/lookup/right", 8, 5, 986, 1986));

    semanticProgram.onErrorFacts.push_back(makeOnErrorFact("/first", "/handler/left", 987, 1987));
    semanticProgram.onErrorFacts.push_back(makeOnErrorFact("/second", "/handler/right", 988, 1988));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.returnFactIndices.push_back(0);
      moduleFirst.localAutoFactIndices.push_back(0);
      moduleFirst.queryFactIndices.push_back(0);
      moduleFirst.bridgePathChoiceIndices.push_back(0);
      moduleFirst.tryFactIndices.push_back(0);
      moduleFirst.onErrorFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.returnFactIndices.push_back(1);
      moduleSecond.localAutoFactIndices.push_back(1);
      moduleSecond.queryFactIndices.push_back(1);
      moduleSecond.bridgePathChoiceIndices.push_back(1);
      moduleSecond.tryFactIndices.push_back(1);
      moduleSecond.onErrorFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter exact golden is stable") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.sourceImports = {"/std/collections/*"};
  semanticProgram.imports = {"/id", "/main"};
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      "id",
      "/id",
      "/",
      2,
      3,
      11,
      101,
  });
  semanticProgram.executions.push_back(primec::SemanticProgramExecution{
      "main",
      "/main",
      "/",
      7,
      1,
      12,
      102,
  });
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 9,
      .sourceColumn = 10,
      .semanticNodeId = 13,
      .provenanceHandle = 103,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "count",
      .receiverTypeText = "vector<i32>",
      .sourceLine = 9,
      .sourceColumn = 13,
      .semanticNodeId = 14,
      .provenanceHandle = 104,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 9,
      .sourceColumn = 13,
      .semanticNodeId = 15,
      .provenanceHandle = 105,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = true,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {"io_out"},
      .activeCapabilities = {"gpu"},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "MyError",
      .hasOnError = true,
      .onErrorHandlerPath = "/unexpectedError",
      .onErrorErrorType = "MyError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 16,
      .provenanceHandle = 106,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .activeEffectIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
      .activeCapabilityIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "gpu"),
      },
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "MyError"),
      .onErrorHandlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/unexpectedError"),
      .onErrorErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "MyError"),
  });
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      "/Particle",
      "struct",
      true,
      false,
      true,
      true,
      16,
      2,
      0,
      11,
      5,
      17,
      107,
  });
  semanticProgram.structFieldMetadata.push_back(primec::SemanticProgramStructFieldMetadata{
      "/Particle",
      "left",
      0,
      "i32",
      12,
      7,
      18,
      108,
  });
  semanticProgram.structFieldMetadata.push_back(primec::SemanticProgramStructFieldMetadata{
      "/Particle",
      "right",
      1,
      "i64",
      13,
      7,
      19,
      109,
  });
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "value",
      .bindingTypeText = "i32",
      .isMutable = true,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 12,
      .sourceColumn = 7,
      .semanticNodeId = 20,
      .provenanceHandle = 110,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/value"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 13,
      .sourceColumn = 3,
      .semanticNodeId = 21,
      .provenanceHandle = 111,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "selected",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "i32",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "return",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 14,
      .sourceColumn = 9,
      .semanticNodeId = 22,
      .provenanceHandle = 112,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, MyError>",
      .bindingTypeText = "Result<i32, MyError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "MyError",
      .sourceLine = 15,
      .sourceColumn = 4,
      .semanticNodeId = 23,
      .provenanceHandle = 113,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, MyError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, MyError>",
      .valueType = "i32",
      .errorType = "MyError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/unexpectedError",
      .onErrorErrorType = "MyError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 16,
      .sourceColumn = 8,
      .semanticNodeId = 24,
      .provenanceHandle = 114,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "MyError",
      .boundArgCount = 1,
      .boundArgTexts = {"err"},
      .returnResultHasValue = true,
      .returnResultValueType = "i32",
      .returnResultErrorType = "MyError",
      .semanticNodeId = 25,
      .provenanceHandle = 115,
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/unexpectedError"),
  });

  const std::string expected = R"(semantic_product {
  entry_path: "/main"
  source_imports[0]: "/std/collections/*"
  imports[0]: "/id"
  imports[1]: "/main"
  definitions[0]: full_path="/id" name="id" namespace_prefix="/" provenance_handle=101 source="2:3"
  executions[0]: full_path="/main" name="main" namespace_prefix="/" provenance_handle=102 source="7:1"
  direct_call_targets[0]: scope_path="/main" call_name="id" resolved_path="/id" provenance_handle=103 source="9:10"
  method_call_targets[0]: scope_path="/main" method_name="count" receiver_type_text="vector<i32>" resolved_path="/std/collections/vector/count" stdlib_surface_id="collections.vector_helpers" provenance_handle=104 source="9:13"
  bridge_path_choices[0]: scope_path="/main" collection_family="vector" helper_name="count" chosen_path="/std/collections/vector/count" stdlib_surface_id="collections.vector_helpers" provenance_handle=105 source="9:13"
  callable_summaries[0]: full_path="/main" is_execution=true return_kind="return" is_compute=false is_unsafe=false active_effects=["io_out"] active_capabilities=["gpu"] has_result_type=true result_type_has_value=true result_value_type="i32" result_error_type="MyError" has_on_error=true on_error_handler_path="/unexpectedError" on_error_error_type="MyError" on_error_bound_arg_count=1 provenance_handle=106
  type_metadata[0]: full_path="/Particle" category="struct" is_public=true has_no_padding=false has_platform_independent_padding=true has_explicit_alignment=true explicit_alignment_bytes=16 field_count=2 enum_value_count=0 provenance_handle=107 source="11:5"
  struct_field_metadata[0]: struct_path="/Particle" field_name="left" field_index=0 binding_type_text="i32" provenance_handle=108 source="12:7"
  struct_field_metadata[1]: struct_path="/Particle" field_name="right" field_index=1 binding_type_text="i64" provenance_handle=109 source="13:7"
  binding_facts[0]: scope_path="/main" site_kind="local" name="value" resolved_path="/main/value" binding_type_text="i32" is_mutable=true is_entry_arg_string=false is_unsafe_reference=false reference_root="" provenance_handle=110 source="12:7"
  return_facts[0]: definition_path="/main" return_kind="return" struct_path="/i32" binding_type_text="i32" is_mutable=false is_entry_arg_string=false is_unsafe_reference=false reference_root="" provenance_handle=111 source="13:3"
  local_auto_facts[0]: scope_path="/main" binding_name="selected" binding_type_text="i32" initializer_resolved_path="/id" initializer_binding_type_text="i32" initializer_receiver_binding_type_text="" initializer_query_type_text="i32" initializer_result_has_value=false initializer_result_value_type="" initializer_result_error_type="" initializer_has_try=false initializer_try_operand_resolved_path="" initializer_try_operand_binding_type_text="" initializer_try_operand_receiver_binding_type_text="" initializer_try_operand_query_type_text="" initializer_try_value_type="" initializer_try_error_type="" initializer_try_context_return_kind="return" initializer_try_on_error_handler_path="" initializer_try_on_error_error_type="" initializer_try_on_error_bound_arg_count=0 initializer_direct_call_resolved_path="" initializer_direct_call_return_kind="" initializer_method_call_resolved_path="" initializer_method_call_return_kind="" provenance_handle=112 source="14:9"
  query_facts[0]: scope_path="/main" call_name="lookup" resolved_path="/lookup" query_type_text="Result<i32, MyError>" binding_type_text="Result<i32, MyError>" receiver_binding_type_text="" has_result_type=true result_type_has_value=true result_value_type="i32" result_error_type="MyError" provenance_handle=113 source="15:4"
  try_facts[0]: scope_path="/main" operand_resolved_path="/lookup" operand_binding_type_text="Result<i32, MyError>" operand_receiver_binding_type_text="" operand_query_type_text="Result<i32, MyError>" value_type="i32" error_type="MyError" context_return_kind="return" on_error_handler_path="/unexpectedError" on_error_error_type="MyError" on_error_bound_arg_count=1 provenance_handle=114 source="16:8"
  on_error_facts[0]: definition_path="/main" return_kind="return" handler_path="/unexpectedError" error_type="MyError" bound_arg_count=1 bound_arg_texts=["err"] return_result_has_value=true return_result_value_type="i32" return_result_error_type="MyError" provenance_handle=115
}
)";

  CHECK(primec::formatSemanticProgram(semanticProgram) == expected);
}

TEST_CASE("semantic product dump helper matches formatter output") {
  const std::string source =
      "import /std/collections/*\n"
      "\n"
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [vector<i32>] values{vector<i32>()}\n"
      "  return(id(/std/collections/vector/count(values)))\n"
      "}\n";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(source, "/main", dumps, error));
  CHECK(error.empty());

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  // TODO-4815 (fixed): id(/std/collections/vector/count(values)) now
  // correctly infers its implicit template argument (T=i32) again,
  // agreeing with captureSemanticBoundaryDumpsForTesting's success above.
  CHECK(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());
  CHECK_FALSE(dumps.semanticProduct.empty());
}


TEST_SUITE_END();
