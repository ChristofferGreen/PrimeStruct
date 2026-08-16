#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "third_party/doctest.h"

#include "primec/pipeline/CompilePipeline.h"
#include "primec/ir/IrLowerer.h"
#include "primec/ir/IrInliner.h"
#include "primec/backend/IrBackends.h"
#include "primec/ir/IrPreparation.h"
#include "primec/ir/IrSerializer.h"
#include "primec/backend/IrToCppEmitter.h"
#include "primec/backend/IrToGlslEmitter.h"
#include "primec/ir/IrValidation.h"
#include "primec/ir/IrVirtualRegisterAllocator.h"
#include "primec/ir/IrVirtualRegisterLiveness.h"
#include "primec/ir/IrVirtualRegisterLowering.h"
#include "primec/ir/IrVirtualRegisterScheduler.h"
#include "primec/ir/IrVirtualRegisterSpillInsertion.h"
#include "primec/ir/IrVirtualRegisterVerifier.h"
#include "primec/frontend/Lexer.h"
#include "primec/backend/NativeEmitter.h"
#include "primec/frontend/Parser.h"
#include "primec/semantics/Semantics.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmDebugAdapter.h"
#include "primec/backend/WasmEmitter.h"
#include "primec/testing/EmitterHelpers.h"
#include "primec/testing/IrLowererHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"
#include "primec/testing/TestScratch.h"
#include "../test_ir_pipeline_helpers.h"
#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
} // namespace

#include "test_ir_pipeline_serialization_structs.h"
