#pragma once

#include <cstdint>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/ir_lowerer/IrLowererLowerSetupStage.h"

namespace primec::ir_lowerer {

// `[spawn] f(...)` on the targets whose runtime runs tasks (the vm target, serialized bytecode and the optimized C++
// emitter, docs/spec/vm-design.md, Tasks): f becomes a real call that TaskSpawn starts when its parameters and
// result are scalars a real call takes. Otherwise `reasonOut` says why the spawned call runs inline, which is a
// legal schedule.
bool taskSpawnRunsOnScheduler(const Definition &callee,
                              const LowerSetupStageState &setupStage,
                              std::string &reasonOut);

// The reservation index of `callee` as a real call, reserving it first if no call site has: TaskSpawn needs the
// function even when every plain call of it is inlined. TaskSpawn takes `(1 << 32) | index` until the function table
// is final.
uint64_t reserveTaskSpawnTarget(LowerSetupStageState &setupStage, const Definition &callee);

} // namespace primec::ir_lowerer
