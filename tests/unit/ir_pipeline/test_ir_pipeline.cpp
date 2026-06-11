#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "third_party/doctest.h"

#include "primec/CompilePipeline.h"
#include "primec/IrLowerer.h"
#include "primec/IrInliner.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/IrSerializer.h"
#include "primec/IrToCppEmitter.h"
#include "primec/IrToGlslEmitter.h"
#include "primec/IrValidation.h"
#include "primec/IrVirtualRegisterAllocator.h"
#include "primec/IrVirtualRegisterLiveness.h"
#include "primec/IrVirtualRegisterLowering.h"
#include "primec/IrVirtualRegisterScheduler.h"
#include "primec/IrVirtualRegisterSpillInsertion.h"
#include "primec/IrVirtualRegisterVerifier.h"
#include "primec/Lexer.h"
#include "primec/NativeEmitter.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/Vm.h"
#include "primec/VmDebugAdapter.h"
#include "primec/WasmEmitter.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"
#include "primec/testing/EmitterHelpers.h"
#include "primec/testing/IrLowererHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"
#include "primec/testing/TestScratch.h"
#include "test_ir_pipeline_helpers.h"
#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
} // namespace

TEST_CASE("ir preparation stops unresolved generic semantic facts before lowering") {
  const std::string unsatisfiedRequirement = R"(
[return<i32> require(N > 0)]
positive_index<N>() {
  return(1i32)
}

[return<i32>]
main() {
  return(positive_index<0>())
}
)";

  primec::testing::detail::PreparedCompilePipelineIrState rejected;
  std::string error;
  CHECK_FALSE(primec::testing::prepareCompilePipelineIr(
      unsatisfiedRequirement, "/main", "vm", rejected, error));
  CHECK(rejected.errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error.find("requirement predicate not satisfied: "
                   "/std/meta/value_greater") != std::string::npos);
  CHECK_FALSE(rejected.output.hasSemanticProgram);
  CHECK(rejected.ir.functions.empty());

  const std::string escapingBranchType = R"(
[return<auto>]
pick([i32] value) {
  ct_if(type_equals<typeof<value>, i32>()) {
    [type] ValueT { typeof<value> }
    [struct] PairT {
      [ValueT] first{0i32}
    }
    [PairT] pair{PairT{value}}
    return(pair)
  } else {
    return(value)
  }
}

[return<i32>]
main() {
  return(pick(7i32))
}
)";

  primec::testing::detail::PreparedCompilePipelineIrState rejectedTypeFact;
  error.clear();
  CHECK_FALSE(primec::testing::prepareCompilePipelineIr(
      escapingBranchType, "/main", "vm", rejectedTypeFact, error));
  CHECK(rejectedTypeFact.errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error.find("local generated struct cannot escape return type: "
                   "/pick/PairT__ct_if_then_") != std::string::npos);
  CHECK_FALSE(rejectedTypeFact.output.hasSemanticProgram);
  CHECK(rejectedTypeFact.ir.functions.empty());

  const std::string selectedBranchFacts = R"(
[return<i32> require(N > 0)]
pick<N>([i32] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, i32>()) {
    assign(result, value)
  } else {
    missing_in_discarded_branch(value)
  }
  return(result)
}

[return<i32>]
main() {
  return(pick<4>(7i32))
}
)";

  primec::testing::detail::PreparedCompilePipelineIrState accepted;
  error.clear();
  REQUIRE(primec::testing::prepareCompilePipelineIr(
      selectedBranchFacts, "/main", "vm", accepted, error));
  CHECK(error.empty());
  REQUIRE(accepted.output.hasSemanticProgram);
  CHECK_FALSE(accepted.output.semanticProgram.requirementPredicateFacts.empty());
  CHECK_FALSE(accepted.ir.functions.empty());
}
