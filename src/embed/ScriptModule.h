#pragma once

#include "primec/embed/Script.h"
#include "primec/ir/Ir.h"

#include <string>
#include <vector>

namespace primec::embed {

struct Script::Module {
  IrModule ir;
};

struct Script::ExportTable {
  struct Entry {
    std::string name;
    ExportSignature signature;
    std::shared_ptr<const Module> module;
  };
  std::vector<Entry> entries;
};

namespace detail {
// "i32", "i64", "u64", "f32", "f64", "bool", "string", "void".
const char *hostTypeSpelling(HostType type);
// Reserved host function names the engine-generated export wrappers use to
// receive arguments and report results (see ScriptEngine export wrappers).
constexpr const char *ExportArgPrefix = "__psarg_";
constexpr const char *ExportResultPrefix = "__psret_";
} // namespace detail

} // namespace primec::embed
