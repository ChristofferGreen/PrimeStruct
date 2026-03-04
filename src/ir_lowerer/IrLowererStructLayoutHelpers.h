#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

uint32_t alignTo(uint32_t value, uint32_t alignment);
bool parsePositiveInt(const std::string &text, int &valueOut);
bool extractAlignment(const std::vector<Transform> &transforms,
                      const std::string &context,
                      uint32_t &alignmentOut,
                      bool &hasAlignment,
                      std::string &error);
bool isLayoutQualifierName(const std::string &name);
IrStructFieldCategory fieldCategory(const Expr &expr);
IrStructVisibility fieldVisibility(const Expr &expr);
bool isStaticField(const Expr &expr);

} // namespace primec::ir_lowerer
