#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

void expectVmVectorCountCompatibilityTypeMismatchReject(const std::string &runCmd) {
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_vector_count_compatibility_type_mismatch_reject_out.txt")
                                  .string();
  const std::string captureCmd = runCmd + " > " + outPath + " 2>&1";
  CHECK(runCommand(captureCmd) != 0);
  CHECK(readFile(outPath).find("mismatch") != std::string::npos);
}
