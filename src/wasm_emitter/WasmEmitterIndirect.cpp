#include "WasmEmitterInternal.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace primec {

namespace {

constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpCall = 0x10;
constexpr uint8_t WasmOpI64Const = 0x42;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmTableFuncRef = 0x70;

} // namespace

std::vector<bool> wasmAddressTakenFunctions(const IrModule &module) {
  std::vector<bool> taken(module.functions.size(), false);
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (inst.op == IrOpcode::FunctionAddress && inst.imm < taken.size()) {
        taken[static_cast<size_t>(inst.imm)] = true;
      }
    }
  }
  return taken;
}

std::vector<uint32_t> wasmIndirectParameterCounts(const IrModule &module, const std::vector<bool> &addressTaken) {
  std::set<uint32_t> counts;
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (inst.op == IrOpcode::CallIndirect || inst.op == IrOpcode::CallIndirectVoid) {
        counts.insert(static_cast<uint32_t>(inst.imm));
      }
    }
  }
  for (size_t f = 0; f < module.functions.size() && f < addressTaken.size(); ++f) {
    if (addressTaken[f]) {
      counts.insert(module.functions[f].parameterCount);
    }
  }
  return std::vector<uint32_t>(counts.begin(), counts.end());
}

WasmFunctionType wasmIndirectSignature(uint32_t parameterCount) {
  WasmFunctionType type;
  type.params.assign(parameterCount, WasmTypeI64);
  type.results = {WasmTypeI64};
  return type;
}

// (i64 slot x n) -> i64: each slot becomes the parameter type of `callee`, the result a slot (0 for no result).
WasmCodeBody buildWasmIndirectThunkBody(const WasmFunctionType &callee, uint32_t calleeFunction) {
  WasmCodeBody body;
  appendU32Leb(0, body.localDecls);
  std::vector<uint8_t> &out = body.instructions;
  for (size_t k = 0; k < callee.params.size(); ++k) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(static_cast<uint32_t>(k), out);
    emitWasmSlotConversion(WasmTypeSlot, callee.params[k], out);
  }
  out.push_back(WasmOpCall);
  appendU32Leb(calleeFunction, out);
  if (callee.results.empty()) {
    out.push_back(WasmOpI64Const);
    appendS64Leb(0, out);
  } else {
    emitWasmSlotConversion(callee.results[0], WasmTypeSlot, out);
  }
  out.push_back(WasmOpEnd);
  return body;
}

bool encodeTableSection(uint32_t entries, std::vector<uint8_t> &payload, std::string &error) {
  (void)error;
  payload.clear();
  appendU32Leb(1, payload);
  payload.push_back(WasmTableFuncRef);
  payload.push_back(0x00); // limits: a minimum only
  appendU32Leb(entries, payload);
  return true;
}

bool encodeElementSection(const std::vector<std::pair<uint32_t, uint32_t>> &elements,
                          std::vector<uint8_t> &payload,
                          std::string &error) {
  (void)error;
  payload.clear();
  appendU32Leb(static_cast<uint32_t>(elements.size()), payload);
  for (const auto &[tableIndex, function] : elements) {
    payload.push_back(0x00); // active segment of table 0, function indexes
    payload.push_back(WasmOpI32Const);
    appendS32Leb(static_cast<int32_t>(tableIndex), payload);
    payload.push_back(WasmOpEnd);
    appendU32Leb(1, payload);
    appendU32Leb(function, payload);
  }
  return true;
}

} // namespace primec
