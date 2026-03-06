#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "third_party/doctest.h"

#include "primec/IrLowerer.h"
#include "primec/IrInliner.h"
#include "primec/IrSerializer.h"
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
#include "src/emitter/EmitterEmitSetupLifecycleHelperStep.h"
#include "src/emitter/EmitterEmitSetupMathImport.h"
#include "src/emitter/EmitterExprControlBoolLiteralStep.h"
#include "src/emitter/EmitterExprControlBodyWrapperStep.h"
#include "src/emitter/EmitterExprControlCallPathStep.h"
#include "src/emitter/EmitterExprControlCountRewriteStep.h"
#include "src/emitter/EmitterExprControlFieldAccessStep.h"
#include "src/emitter/EmitterExprControlFloatLiteralStep.h"
#include "src/emitter/EmitterExprControlIfEnvelopeStep.h"
#include "src/emitter/EmitterExprControlIntegerLiteralStep.h"
#include "src/emitter/EmitterExprControlMethodPathStep.h"
#include "src/emitter/EmitterExprControlStringLiteralStep.h"
#include "src/emitter/EmitterExprControlNameStep.h"
#include "src/ir_lowerer/IrLowererCallHelpers.h"
#include "src/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "src/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "src/ir_lowerer/IrLowererFileWriteHelpers.h"
#include "src/ir_lowerer/IrLowererFlowHelpers.h"
#include "src/ir_lowerer/IrLowererHelpers.h"
#include "src/ir_lowerer/IrLowererCountAccessHelpers.h"
#include "src/ir_lowerer/IrLowererIndexKindHelpers.h"
#include "src/ir_lowerer/IrLowererInlineCallContextHelpers.h"
#include "src/ir_lowerer/IrLowererInlineParamHelpers.h"
#include "src/ir_lowerer/IrLowererInlineStructArgHelpers.h"
#include "src/ir_lowerer/IrLowererLowerEffects.h"
#include "src/ir_lowerer/IrLowererLowerEntrySetup.h"
#include "src/ir_lowerer/IrLowererLowerExprEmitSetup.h"
#include "src/ir_lowerer/IrLowererLowerInferenceSetup.h"
#include "src/ir_lowerer/IrLowererLowerImportsStructsSetup.h"
#include "src/ir_lowerer/IrLowererLowerLocalsSetup.h"
#include "src/ir_lowerer/IrLowererLowerReturnCallsSetup.h"
#include "src/ir_lowerer/IrLowererOnErrorHelpers.h"
#include "src/ir_lowerer/IrLowererOperatorArithmeticHelpers.h"
#include "src/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.h"
#include "src/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "src/ir_lowerer/IrLowererOperatorComparisonHelpers.h"
#include "src/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.h"
#include "src/ir_lowerer/IrLowererOperatorPowAbsSignHelpers.h"
#include "src/ir_lowerer/IrLowererOperatorSaturateRoundingRootsHelpers.h"
#include "src/ir_lowerer/IrLowererReturnInferenceHelpers.h"
#include "src/ir_lowerer/IrLowererResultHelpers.h"
#include "src/ir_lowerer/IrLowererRuntimeErrorHelpers.h"
#include "src/ir_lowerer/IrLowererSetupInferenceHelpers.h"
#include "src/ir_lowerer/IrLowererSetupLocalsHelpers.h"
#include "src/ir_lowerer/IrLowererSetupMathHelpers.h"
#include "src/ir_lowerer/IrLowererStatementBindingHelpers.h"
#include "src/ir_lowerer/IrLowererStatementCallHelpers.h"
#include "src/ir_lowerer/IrLowererStructFieldBindingHelpers.h"
#include "src/ir_lowerer/IrLowererStructLayoutHelpers.h"
#include "src/ir_lowerer/IrLowererStructReturnPathHelpers.h"
#include "src/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "src/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "src/ir_lowerer/IrLowererUninitializedTypeHelpers.h"
#include "src/ir_lowerer/IrLowererStringLiteralHelpers.h"
#include "src/ir_lowerer/IrLowererSharedTypes.h"
#include "src/ir_lowerer/IrLowererStringCallHelpers.h"
#include "src/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "src/semantics/SemanticsValidatorExprCaptureSplitStep.h"
#include "src/semantics/SemanticsValidatorStatementLoopCountStep.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
bool parseAndValidate(const std::string &source,
                      primec::Program &program,
                      std::string &error,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  if (!parser.parse(program, error)) {
    return false;
  }
  primec::Semantics semantics;
  return semantics.validate(program, "/main", error, defaultEffects, entryDefaultEffects);
}

bool parseAndValidate(const std::string &source,
                      primec::Program &program,
                      std::string &error,
                      std::vector<std::string> defaultEffects = {}) {
  return parseAndValidate(source, program, error, defaultEffects, defaultEffects);
}

bool validateProgram(primec::Program &program,
                     const std::string &entry,
                     std::string &error,
                     std::vector<std::string> defaultEffects = {}) {
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaultEffects, defaultEffects);
}
} // namespace

#include "test_ir_pipeline_serialization.h"
#include "test_ir_pipeline_pointers.h"
#include "test_ir_pipeline_conversions.h"
#include "test_ir_pipeline_entry_args.h"
#include "test_ir_pipeline_validation.h"
#include "test_vm_debug_session.h"
#include "test_ir_pipeline_gpu.h"
#include "test_ir_pipeline_external_tooling.h"
#include "test_ir_pipeline_wasm.h"
