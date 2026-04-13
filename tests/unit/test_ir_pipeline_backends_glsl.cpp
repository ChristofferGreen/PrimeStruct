#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/doctest.h"

#include "primec/CompilePipeline.h"
#include "primec/ExternalTooling.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/IrToCppEmitter.h"
#include "primec/IrToGlslEmitter.h"
#include "primec/IrValidation.h"
#include "primec/Lexer.h"
#include "primec/NativeEmitter.h"
#include "primec/Parser.h"
#include "primec/ProcessRunner.h"
#include "primec/Semantics.h"
#include "primec/testing/IrLowererHelpers.h"

#include "test_ir_pipeline_backends_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.glsl");

#include "test_ir_pipeline_backends_glsl_a.h"
#include "test_ir_pipeline_backends_glsl_b.h"
#include "test_ir_pipeline_backends_glsl_c.h"
