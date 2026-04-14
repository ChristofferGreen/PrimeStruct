#pragma once

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
#define PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED 1
#else
#define PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED 0
#endif

[[maybe_unused]] inline void
expectNativeVectorCountCompatibilityTypeMismatchReject(const std::string &compileCmd) {
  const std::string errPath = (testScratchPath("") /
                               "primec_native_vector_count_compatibility_type_mismatch_reject_err.txt")
                                  .string();
  const std::string captureCmd = compileCmd + " > /dev/null 2> " + errPath;
  CHECK(runCommand(captureCmd) != 0);
  const std::string err = readFile(errPath);
  CHECK((err.find("argument type mismatch for /vector/count parameter marker") != std::string::npos ||
         err.find("Native lowering error: struct parameter type mismatch") != std::string::npos));
}
