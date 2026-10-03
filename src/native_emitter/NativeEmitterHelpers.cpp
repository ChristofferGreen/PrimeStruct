#include "NativeEmitterInternals.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrOpcodeTable.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace primec::native_emitter {
namespace {

std::string cfgErrorMessage(const IrCfgError &cfgError) {
  const IrOpcodeInfo *info = irOpcodeInfo(cfgError.opcode);
  const std::string opcodeName = info != nullptr ? info->name : "Unknown";
  switch (cfgError.kind) {
    case IrCfgErrorKind::InvalidJumpTarget:
      return "native backend detected invalid jump target";
    case IrCfgErrorKind::InconsistentDepth:
      return "native backend detected inconsistent stack depth at instruction " +
             std::to_string(cfgError.instructionIndex) + " (" + opcodeName + ")";
    case IrCfgErrorKind::None:
    case IrCfgErrorKind::UnsupportedOpcode:
    case IrCfgErrorKind::StackUnderflow:
    case IrCfgErrorKind::InvalidDup:
      break;
  }
  return "native backend detected invalid stack usage at instruction " +
         std::to_string(cfgError.instructionIndex) + " (" + opcodeName + ")";
}

} // namespace

// The entry block starts at fn.parameterCount: a function reached through a
// real Call/CallVoid finds its arguments already on the shared value stack and
// its first instructions are StoreLocals that pop them (see the Call handling in
// NativeEmitterFunctionEmit.cpp). Block structure and depth propagation live in
// the shared CFG (primec/ir/IrCfg.h).
bool computeMaxStackDepth(const IrFunction &fn, const IrModule &module, int64_t &maxDepth, std::string &error) {
  if (fn.instructions.empty()) {
    error = "native backend requires at least one instruction";
    return false;
  }
  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(fn, module, cfg, cfgError)) {
    error = cfgErrorMessage(cfgError);
    return false;
  }
  maxDepth = cfg.maxStackDepth;
  return true;
}

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &data, std::string &error) {
  std::filesystem::path outputPath(path);
  std::filesystem::path parent = outputPath.parent_path();
  if (parent.empty()) {
    parent = ".";
  }

  // Write to a temporary file and rename into place. This avoids keeping a
  // potentially problematic inode alive across runs (e.g. when tests leave
  // behind executing processes).
  const long long pid =
#if defined(_WIN32)
      static_cast<long long>(::_getpid());
#else
      static_cast<long long>(::getpid());
#endif
  std::filesystem::path tempPath =
      parent / (outputPath.filename().string() + ".tmp." + std::to_string(pid));

  {
    std::error_code cleanupEc;
    std::filesystem::remove(tempPath, cleanupEc);
  }

  std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
  if (!file) {
    error = "failed to open output file";
    return false;
  }
  file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!file.good()) {
    std::error_code cleanupEc;
    std::filesystem::remove(tempPath, cleanupEc);
    error = "failed to write output file";
    return false;
  }
  file.close();

  std::error_code ec;
  std::filesystem::permissions(tempPath,
                               std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add,
                               ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    error = "failed to set executable permissions";
    return false;
  }

  std::filesystem::remove(outputPath, ec);
  ec.clear();
  std::filesystem::rename(tempPath, outputPath, ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    error = "failed to move output file into place";
    return false;
  }
  return true;
}

} // namespace primec::native_emitter
