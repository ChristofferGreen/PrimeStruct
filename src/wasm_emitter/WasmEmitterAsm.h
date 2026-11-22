#pragma once

#include "WasmEmitterInternal.h"

namespace primec {

// A small assembler for the helper functions of the module (the heap, WasmEmitterHeap.cpp): one method per wasm
// instruction it needs, appending to a code body. Structured control is written as `block`/`loop`/`ifThen` and an
// explicit `end`, exactly as in the binary format, so a helper reads like its pseudo code.
class WasmAsm {
public:
  explicit WasmAsm(std::vector<uint8_t> &out) : out_(out) {
  }

  void localGet(uint32_t index) {
    op(0x20);
    leb(index);
  }
  void localSet(uint32_t index) {
    op(0x21);
    leb(index);
  }
  void localTee(uint32_t index) {
    op(0x22);
    leb(index);
  }
  void globalGet(uint32_t index) {
    op(0x23);
    leb(index);
  }
  void globalSet(uint32_t index) {
    op(0x24);
    leb(index);
  }
  void i32Const(int32_t value) {
    op(0x41);
    appendS32Leb(value, out_);
  }
  void i64Const(int64_t value) {
    op(0x42);
    appendS64Leb(value, out_);
  }
  void call(uint32_t function) {
    op(0x10);
    leb(function);
  }
  void drop() {
    op(0x1a);
  }
  void select() {
    op(0x1b);
  }
  void unreachable() {
    op(0x00);
  }
  void ret() {
    op(0x0f);
  }

  // Control: `ifThen` and `ifElse` consume an i32 condition; every opener needs its `end`.
  void block() {
    op(0x02);
    op(0x40);
  }
  void loop() {
    op(0x03);
    op(0x40);
  }
  void ifThen() {
    op(0x04);
    op(0x40);
  }
  void elseArm() {
    op(0x05);
  }
  void end() {
    op(0x0b);
  }
  void br(uint32_t depth) {
    op(0x0c);
    leb(depth);
  }
  void brIf(uint32_t depth) {
    op(0x0d);
    leb(depth);
  }

  // Memory: the offset is a constant added to the address on the stack.
  void i32Load(uint32_t offset) {
    op(0x28);
    leb(2);
    leb(offset);
  }
  void i64Load(uint32_t offset) {
    op(0x29);
    leb(3);
    leb(offset);
  }
  void i32Load16U(uint32_t offset) {
    op(0x2f);
    leb(1);
    leb(offset);
  }
  void i32Store(uint32_t offset) {
    op(0x36);
    leb(2);
    leb(offset);
  }
  void i64Store(uint32_t offset) {
    op(0x37);
    leb(3);
    leb(offset);
  }
  void i32Store16(uint32_t offset) {
    op(0x3b);
    leb(1);
    leb(offset);
  }
  // memory.copy (destination, source, byte count on the stack); overlapping ranges are handled.
  void memoryCopy() {
    op(0xfc);
    leb(10);
    op(0x00);
    op(0x00);
  }

  void i32Eqz() {
    op(0x45);
  }
  void i32Eq() {
    op(0x46);
  }
  void i32Ne() {
    op(0x47);
  }
  void i32LtU() {
    op(0x49);
  }
  void i32GtU() {
    op(0x4b);
  }
  void i32LeU() {
    op(0x4d);
  }
  void i32GeU() {
    op(0x4f);
  }
  void i64Eqz() {
    op(0x50);
  }
  void i64Eq() {
    op(0x51);
  }
  void i64Ne() {
    op(0x52);
  }
  void i64LtU() {
    op(0x54);
  }
  void i64GtU() {
    op(0x56);
  }
  void i64GeU() {
    op(0x5a);
  }
  void i32Add() {
    op(0x6a);
  }
  void i32Sub() {
    op(0x6b);
  }
  void i32And() {
    op(0x71);
  }
  void i32Or() {
    op(0x72);
  }
  void i32Shl() {
    op(0x74);
  }
  void i32ShrU() {
    op(0x76);
  }
  void i64Add() {
    op(0x7c);
  }
  void i64Sub() {
    op(0x7d);
  }
  void i64And() {
    op(0x83);
  }
  void i64Or() {
    op(0x84);
  }
  void i64Shl() {
    op(0x86);
  }
  void i64ShrU() {
    op(0x88);
  }
  void i32WrapI64() {
    op(0xa7);
  }
  void i64ExtendI32U() {
    op(0xad);
  }

  // `local.get a; local.get b; <compare>` style shorthands for the common operand pairs.
  void i32Increment(uint32_t local) {
    localGet(local);
    i32Const(1);
    i32Add();
    localSet(local);
  }

private:
  void op(uint8_t byte) {
    out_.push_back(byte);
  }
  void leb(uint32_t value) {
    appendU32Leb(value, out_);
  }
  std::vector<uint8_t> &out_;
};

} // namespace primec
