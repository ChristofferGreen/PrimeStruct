#pragma once

#include "primec/Ir.h"

#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace primec {

struct VmDebugDapRunResult {
  bool disconnected = false;
  bool observedProgramExit = false;
  int programExitCode = 0;
};

bool runVmDebugDapSession(const IrModule &module,
                          const std::vector<std::string_view> &args,
                          std::string_view sourcePath,
                          std::istream &input,
                          std::ostream &output,
                          VmDebugDapRunResult &result,
                          std::string &error);

} // namespace primec
