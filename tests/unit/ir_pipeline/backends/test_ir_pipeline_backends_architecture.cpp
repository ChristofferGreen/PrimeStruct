#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/doctest.h"

#include "primec/pipeline/CompilePipeline.h"
#include "primec/support/ExternalTooling.h"
#include "primec/backend/IrBackends.h"
#include "primec/ir/IrPreparation.h"
#include "primec/backend/IrToCppEmitter.h"
#include "primec/backend/IrToGlslEmitter.h"
#include "primec/ir/IrValidation.h"
#include "primec/frontend/Lexer.h"
#include "primec/backend/NativeEmitter.h"
#include "primec/frontend/Parser.h"
#include "primec/support/ProcessRunner.h"
#include "primec/semantics/Semantics.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/testing/IrLowererHelpers.h"

#include "test_ir_pipeline_backends_helpers.h"
#include "test_ir_pipeline_backends_architecture.h"
