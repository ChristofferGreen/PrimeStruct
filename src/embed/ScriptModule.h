#pragma once

#include "primec/embed/Script.h"
#include "primec/ir/Ir.h"

namespace primec::embed {

struct Script::Module {
  IrModule ir;
};

} // namespace primec::embed
