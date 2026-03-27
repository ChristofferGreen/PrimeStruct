#include "NativeEmitterInternals.h"

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
bool computeMaxStackDepth(const IrFunction &fn, int64_t &maxDepth, std::string &error) {
  if (fn.instructions.empty()) {
    error = "native backend requires at least one instruction";
    return false;
  }
  auto opcodeName = [](IrOpcode op) -> const char * {
    switch (op) {
      case IrOpcode::PushI32:
        return "PushI32";
      case IrOpcode::PushI64:
        return "PushI64";
      case IrOpcode::PushArgc:
        return "PushArgc";
      case IrOpcode::LoadLocal:
        return "LoadLocal";
      case IrOpcode::StoreLocal:
        return "StoreLocal";
      case IrOpcode::AddressOfLocal:
        return "AddressOfLocal";
      case IrOpcode::LoadIndirect:
        return "LoadIndirect";
      case IrOpcode::StoreIndirect:
        return "StoreIndirect";
      case IrOpcode::Dup:
        return "Dup";
      case IrOpcode::Pop:
        return "Pop";
      case IrOpcode::AddI32:
        return "AddI32";
      case IrOpcode::SubI32:
        return "SubI32";
      case IrOpcode::MulI32:
        return "MulI32";
      case IrOpcode::DivI32:
        return "DivI32";
      case IrOpcode::NegI32:
        return "NegI32";
      case IrOpcode::AddI64:
        return "AddI64";
      case IrOpcode::SubI64:
        return "SubI64";
      case IrOpcode::MulI64:
        return "MulI64";
      case IrOpcode::DivI64:
        return "DivI64";
      case IrOpcode::DivU64:
        return "DivU64";
      case IrOpcode::NegI64:
        return "NegI64";
      case IrOpcode::CmpEqI32:
        return "CmpEqI32";
      case IrOpcode::CmpNeI32:
        return "CmpNeI32";
      case IrOpcode::CmpLtI32:
        return "CmpLtI32";
      case IrOpcode::CmpLeI32:
        return "CmpLeI32";
      case IrOpcode::CmpGtI32:
        return "CmpGtI32";
      case IrOpcode::CmpGeI32:
        return "CmpGeI32";
      case IrOpcode::CmpEqI64:
        return "CmpEqI64";
      case IrOpcode::CmpNeI64:
        return "CmpNeI64";
      case IrOpcode::CmpLtI64:
        return "CmpLtI64";
      case IrOpcode::CmpLeI64:
        return "CmpLeI64";
      case IrOpcode::CmpGtI64:
        return "CmpGtI64";
      case IrOpcode::CmpGeI64:
        return "CmpGeI64";
      case IrOpcode::CmpLtU64:
        return "CmpLtU64";
      case IrOpcode::CmpLeU64:
        return "CmpLeU64";
      case IrOpcode::CmpGtU64:
        return "CmpGtU64";
      case IrOpcode::CmpGeU64:
        return "CmpGeU64";
      case IrOpcode::JumpIfZero:
        return "JumpIfZero";
      case IrOpcode::Jump:
        return "Jump";
      case IrOpcode::ReturnVoid:
        return "ReturnVoid";
      case IrOpcode::ReturnI32:
        return "ReturnI32";
      case IrOpcode::ReturnI64:
        return "ReturnI64";
      case IrOpcode::ReturnF32:
        return "ReturnF32";
      case IrOpcode::ReturnF64:
        return "ReturnF64";
      case IrOpcode::PrintI32:
        return "PrintI32";
      case IrOpcode::PrintI64:
        return "PrintI64";
      case IrOpcode::PrintU64:
        return "PrintU64";
      case IrOpcode::PrintString:
        return "PrintString";
      case IrOpcode::PrintStringDynamic:
        return "PrintStringDynamic";
      case IrOpcode::PrintArgv:
        return "PrintArgv";
      case IrOpcode::PrintArgvUnsafe:
        return "PrintArgvUnsafe";
      case IrOpcode::LoadStringByte:
        return "LoadStringByte";
      case IrOpcode::LoadStringLength:
        return "LoadStringLength";
      case IrOpcode::FileOpenRead:
        return "FileOpenRead";
      case IrOpcode::FileOpenWrite:
        return "FileOpenWrite";
      case IrOpcode::FileOpenAppend:
        return "FileOpenAppend";
      case IrOpcode::FileOpenReadDynamic:
        return "FileOpenReadDynamic";
      case IrOpcode::FileOpenWriteDynamic:
        return "FileOpenWriteDynamic";
      case IrOpcode::FileOpenAppendDynamic:
        return "FileOpenAppendDynamic";
      case IrOpcode::FileReadByte:
        return "FileReadByte";
      case IrOpcode::FileClose:
        return "FileClose";
      case IrOpcode::FileFlush:
        return "FileFlush";
      case IrOpcode::FileWriteI32:
        return "FileWriteI32";
      case IrOpcode::FileWriteI64:
        return "FileWriteI64";
      case IrOpcode::FileWriteU64:
        return "FileWriteU64";
      case IrOpcode::FileWriteString:
        return "FileWriteString";
      case IrOpcode::FileWriteStringDynamic:
        return "FileWriteStringDynamic";
      case IrOpcode::FileWriteByte:
        return "FileWriteByte";
      case IrOpcode::FileWriteNewline:
        return "FileWriteNewline";
      case IrOpcode::Call:
        return "Call";
      case IrOpcode::CallVoid:
        return "CallVoid";
      case IrOpcode::HeapAlloc:
        return "HeapAlloc";
      case IrOpcode::HeapFree:
        return "HeapFree";
      case IrOpcode::HeapRealloc:
        return "HeapRealloc";
      case IrOpcode::PushF32:
        return "PushF32";
      case IrOpcode::PushF64:
        return "PushF64";
      case IrOpcode::AddF32:
        return "AddF32";
      case IrOpcode::SubF32:
        return "SubF32";
      case IrOpcode::MulF32:
        return "MulF32";
      case IrOpcode::DivF32:
        return "DivF32";
      case IrOpcode::NegF32:
        return "NegF32";
      case IrOpcode::AddF64:
        return "AddF64";
      case IrOpcode::SubF64:
        return "SubF64";
      case IrOpcode::MulF64:
        return "MulF64";
      case IrOpcode::DivF64:
        return "DivF64";
      case IrOpcode::NegF64:
        return "NegF64";
      case IrOpcode::CmpEqF32:
        return "CmpEqF32";
      case IrOpcode::CmpNeF32:
        return "CmpNeF32";
      case IrOpcode::CmpLtF32:
        return "CmpLtF32";
      case IrOpcode::CmpLeF32:
        return "CmpLeF32";
      case IrOpcode::CmpGtF32:
        return "CmpGtF32";
      case IrOpcode::CmpGeF32:
        return "CmpGeF32";
      case IrOpcode::CmpEqF64:
        return "CmpEqF64";
      case IrOpcode::CmpNeF64:
        return "CmpNeF64";
      case IrOpcode::CmpLtF64:
        return "CmpLtF64";
      case IrOpcode::CmpLeF64:
        return "CmpLeF64";
      case IrOpcode::CmpGtF64:
        return "CmpGtF64";
      case IrOpcode::CmpGeF64:
        return "CmpGeF64";
      case IrOpcode::ConvertI32ToF32:
        return "ConvertI32ToF32";
      case IrOpcode::ConvertI32ToF64:
        return "ConvertI32ToF64";
      case IrOpcode::ConvertI64ToF32:
        return "ConvertI64ToF32";
      case IrOpcode::ConvertI64ToF64:
        return "ConvertI64ToF64";
      case IrOpcode::ConvertU64ToF32:
        return "ConvertU64ToF32";
      case IrOpcode::ConvertU64ToF64:
        return "ConvertU64ToF64";
      case IrOpcode::ConvertF32ToI32:
        return "ConvertF32ToI32";
      case IrOpcode::ConvertF32ToI64:
        return "ConvertF32ToI64";
      case IrOpcode::ConvertF32ToU64:
        return "ConvertF32ToU64";
      case IrOpcode::ConvertF64ToI32:
        return "ConvertF64ToI32";
      case IrOpcode::ConvertF64ToI64:
        return "ConvertF64ToI64";
      case IrOpcode::ConvertF64ToU64:
        return "ConvertF64ToU64";
      case IrOpcode::ConvertF32ToF64:
        return "ConvertF32ToF64";
      case IrOpcode::ConvertF64ToF32:
        return "ConvertF64ToF32";
      default:
        return "Unknown";
    }
  };
  auto stackDelta = [](IrOpcode op) -> int32_t {
    switch (op) {
      case IrOpcode::PushI32:
      case IrOpcode::PushI64:
      case IrOpcode::PushF32:
      case IrOpcode::PushF64:
      case IrOpcode::PushArgc:
      case IrOpcode::LoadLocal:
      case IrOpcode::AddressOfLocal:
        return 1;
      case IrOpcode::StoreLocal:
      case IrOpcode::Pop:
        return -1;
      case IrOpcode::LoadIndirect:
      case IrOpcode::HeapAlloc:
      case IrOpcode::HeapRealloc:
        return 0;
      case IrOpcode::HeapFree:
        return -1;
      case IrOpcode::StoreIndirect:
        return -1;
      case IrOpcode::Dup:
        return 1;
      case IrOpcode::AddI32:
      case IrOpcode::SubI32:
      case IrOpcode::MulI32:
      case IrOpcode::DivI32:
      case IrOpcode::AddI64:
      case IrOpcode::SubI64:
      case IrOpcode::MulI64:
      case IrOpcode::DivI64:
      case IrOpcode::DivU64:
      case IrOpcode::AddF32:
      case IrOpcode::SubF32:
      case IrOpcode::MulF32:
      case IrOpcode::DivF32:
      case IrOpcode::AddF64:
      case IrOpcode::SubF64:
      case IrOpcode::MulF64:
      case IrOpcode::DivF64:
      case IrOpcode::CmpEqI32:
      case IrOpcode::CmpNeI32:
      case IrOpcode::CmpLtI32:
      case IrOpcode::CmpLeI32:
      case IrOpcode::CmpGtI32:
      case IrOpcode::CmpGeI32:
      case IrOpcode::CmpEqI64:
      case IrOpcode::CmpNeI64:
      case IrOpcode::CmpLtI64:
      case IrOpcode::CmpLeI64:
      case IrOpcode::CmpGtI64:
      case IrOpcode::CmpGeI64:
      case IrOpcode::CmpLtU64:
      case IrOpcode::CmpLeU64:
      case IrOpcode::CmpGtU64:
      case IrOpcode::CmpGeU64:
      case IrOpcode::CmpEqF32:
      case IrOpcode::CmpNeF32:
      case IrOpcode::CmpLtF32:
      case IrOpcode::CmpLeF32:
      case IrOpcode::CmpGtF32:
      case IrOpcode::CmpGeF32:
      case IrOpcode::CmpEqF64:
      case IrOpcode::CmpNeF64:
      case IrOpcode::CmpLtF64:
      case IrOpcode::CmpLeF64:
      case IrOpcode::CmpGtF64:
      case IrOpcode::CmpGeF64:
        return -1;
      case IrOpcode::NegI32:
      case IrOpcode::NegI64:
      case IrOpcode::NegF32:
      case IrOpcode::NegF64:
      case IrOpcode::ConvertI32ToF32:
      case IrOpcode::ConvertI32ToF64:
      case IrOpcode::ConvertI64ToF32:
      case IrOpcode::ConvertI64ToF64:
      case IrOpcode::ConvertU64ToF32:
      case IrOpcode::ConvertU64ToF64:
      case IrOpcode::ConvertF32ToI32:
      case IrOpcode::ConvertF32ToI64:
      case IrOpcode::ConvertF32ToU64:
      case IrOpcode::ConvertF64ToI32:
      case IrOpcode::ConvertF64ToI64:
      case IrOpcode::ConvertF64ToU64:
      case IrOpcode::ConvertF32ToF64:
      case IrOpcode::ConvertF64ToF32:
        return 0;
      case IrOpcode::JumpIfZero:
        return -1;
      case IrOpcode::Jump:
        return 0;
      case IrOpcode::ReturnVoid:
        return 0;
      case IrOpcode::ReturnI32:
      case IrOpcode::ReturnI64:
      case IrOpcode::ReturnF32:
      case IrOpcode::ReturnF64:
        return -1;
      case IrOpcode::PrintI32:
      case IrOpcode::PrintI64:
      case IrOpcode::PrintU64:
        return -1;
      case IrOpcode::PrintString:
        return 0;
      case IrOpcode::PrintStringDynamic:
        return -1;
      case IrOpcode::PrintArgv:
      case IrOpcode::PrintArgvUnsafe:
        return -1;
      case IrOpcode::LoadStringByte:
      case IrOpcode::LoadStringLength:
        return 0;
      case IrOpcode::FileOpenRead:
      case IrOpcode::FileOpenWrite:
      case IrOpcode::FileOpenAppend:
        return 1;
      case IrOpcode::FileOpenReadDynamic:
      case IrOpcode::FileOpenWriteDynamic:
      case IrOpcode::FileOpenAppendDynamic:
        return 0;
      case IrOpcode::FileReadByte:
        return 0;
      case IrOpcode::FileClose:
      case IrOpcode::FileFlush:
        return 0;
      case IrOpcode::FileWriteString:
      case IrOpcode::FileWriteNewline:
        return 0;
      case IrOpcode::FileWriteI32:
      case IrOpcode::FileWriteI64:
      case IrOpcode::FileWriteU64:
      case IrOpcode::FileWriteStringDynamic:
      case IrOpcode::FileWriteByte:
        return -1;
      case IrOpcode::Call:
        return 1;
      case IrOpcode::CallVoid:
        return 0;
      default:
        return 0;
    }
  };
  const int64_t kUnset = std::numeric_limits<int64_t>::min();
  std::vector<int64_t> depth(fn.instructions.size(), kUnset);
  std::vector<size_t> worklist;
  depth[0] = 0;
  worklist.push_back(0);
  maxDepth = 0;

  while (!worklist.empty()) {
    size_t index = worklist.back();
    worklist.pop_back();
    int64_t currentDepth = depth[index];
    maxDepth = std::max(maxDepth, currentDepth);
    const auto &inst = fn.instructions[index];
    int64_t nextDepth = currentDepth + stackDelta(inst.op);
    if (nextDepth < 0) {
      error = "native backend detected invalid stack usage at instruction " + std::to_string(index) + " (" +
              opcodeName(inst.op) + ")";
      return false;
    }
    maxDepth = std::max(maxDepth, nextDepth);

    auto pushSuccessor = [&](size_t nextIndex) -> bool {
      if (nextIndex >= fn.instructions.size()) {
        return true;
      }
      if (depth[nextIndex] == kUnset) {
        depth[nextIndex] = nextDepth;
        worklist.push_back(nextIndex);
        return true;
      }
      if (depth[nextIndex] != nextDepth) {
        error = "native backend detected inconsistent stack depth at instruction " + std::to_string(nextIndex) + " (" +
                opcodeName(fn.instructions[nextIndex].op) + ")";
        return false;
      }
      return true;
    };

    if (inst.op == IrOpcode::ReturnVoid || inst.op == IrOpcode::ReturnI32 || inst.op == IrOpcode::ReturnI64 ||
        inst.op == IrOpcode::ReturnF32 || inst.op == IrOpcode::ReturnF64) {
      continue;
    }
    if (inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) {
      if (static_cast<size_t>(inst.imm) > fn.instructions.size()) {
        error = "native backend detected invalid jump target";
        return false;
      }
      if (!pushSuccessor(static_cast<size_t>(inst.imm))) {
        return false;
      }
      if (inst.op == IrOpcode::JumpIfZero) {
        if (!pushSuccessor(index + 1)) {
          return false;
        }
      }
      continue;
    }
    if (!pushSuccessor(index + 1)) {
      return false;
    }
  }
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
