#include "WasmEmitterAsm.h"
#include "WasmEmitterInternal.h"

#include <string>
#include <vector>

namespace primec {

namespace {

constexpr uint32_t FdLocal = 1;
constexpr int32_t StderrFd = 2;
constexpr int32_t RuntimeErrorExitCode = 3;

// The messages of `syncFault`: the first two are the VM's faults for a lock that is not held, the others are the
// blocking sites of the single-threaded model (docs/spec/vm-design.md, Tasks, Wasm).
const std::string SyncMessages[4] = {
    "wasm: a task would wait for a lock that is held (MutexLock): Wasm runs every task to its end at its spawn, so "
    "this program needs another thread\n",
    "VM error: mutex unlocked by a task that does not hold it\n",
    "VM error: condition wait by a task that does not hold the lock\n",
    "wasm: a task would wait for a condition (ConditionWait): Wasm runs every task to its end at its spawn, so this "
    "program needs another thread\n",
};

} // namespace

const std::string &wasmSyncMessage(size_t which) {
  return SyncMessages[which];
}

// (i32 which) -> (): writes message `which` to stderr and ends the program with exit code 3.
WasmCodeBody buildWasmSyncFaultHelperBody(const WasmRuntimeContext &runtime) {
  WasmCodeBody body;
  appendU32Leb(1, body.localDecls);
  appendU32Leb(1, body.localDecls);
  body.localDecls.push_back(WasmTypeI32);
  std::vector<uint8_t> &out = body.instructions;
  WasmAsm a(out);
  a.i32Const(StderrFd);
  a.localSet(FdLocal);
  for (uint32_t which = 0; which < 4; ++which) {
    a.localGet(0);
    a.i32Const(static_cast<int32_t>(which));
    a.i32Eq();
    a.ifThen();
    emitWasiWriteLiteralFromFdLocal(
        FdLocal, runtime.syncMessageAddr[which], runtime.syncMessageLength[which], runtime, out);
    a.drop();
    a.end();
  }
  a.i32Const(RuntimeErrorExitCode);
  a.call(runtime.procExitImportIndex);
  a.unreachable();
  a.end();
  return body;
}

// A lock cell holds 0 while free and 1 while held; with one thread a task that finds it held can never be woken, so
// that is a blocking site (message 0), as is every wait (message 3). A notify wakes nobody.
void emitWasmSyncOperation(IrOpcode op,
                           std::vector<uint8_t> &out,
                           const WasmRuntimeContext &runtime,
                           uint32_t first) {
  WasmAsm a(out);
  switch (op) {
  case IrOpcode::MutexLock:
    a.localGet(first);
    a.i64Load(0);
    a.i64Eqz();
    a.ifThen();
    a.localGet(first);
    a.i64Const(1);
    a.i64Store(0);
    a.elseArm();
    a.i32Const(0);
    a.call(runtime.syncFaultFunction);
    a.end();
    break;
  case IrOpcode::MutexUnlock:
    a.localGet(first);
    a.i64Load(0);
    a.i64Const(1);
    a.i64Ne();
    a.ifThen();
    a.i32Const(1);
    a.call(runtime.syncFaultFunction);
    a.end();
    a.localGet(first);
    a.i64Const(0);
    a.i64Store(0);
    break;
  case IrOpcode::ConditionWait:
    a.localGet(first);
    a.i64Load(0);
    a.i64Const(1);
    a.i64Ne();
    a.ifThen();
    a.i32Const(2);
    a.call(runtime.syncFaultFunction);
    a.end();
    a.i32Const(3);
    a.call(runtime.syncFaultFunction);
    break;
  default:
    break;
  }
}

} // namespace primec
