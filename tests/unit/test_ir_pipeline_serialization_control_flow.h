TEST_SUITE_BEGIN("primestruct.ir.pipeline.serialization");

namespace {
std::filesystem::path irSerializationControlFlowPath(const std::string &relativePath) {
  return primec::testing::testScratchPath("ir_pipeline_serialization/" + relativePath);
}
} // namespace

#include "test_ir_pipeline_serialization_control_flow_core.h"
#include "test_ir_pipeline_serialization_control_flow_vregs.h"
#include "test_ir_pipeline_serialization_control_flow_spills.h"
#include "test_ir_pipeline_serialization_control_flow_scheduler.h"
#include "test_ir_pipeline_serialization_control_flow_verifier.h"
#include "test_ir_pipeline_serialization_control_flow_native_backend.h"
#include "test_ir_pipeline_serialization_control_flow_debug_dump.h"
#include "test_ir_pipeline_serialization_control_flow_metadata.h"

TEST_SUITE_END();
