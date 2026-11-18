#include "primec/ir/IrLinker.h"

#include "primec/ir/IrOpcodeTable.h"
#include "primec/ir/IrOperandTable.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec {
namespace {

// Where each function, host import and string of the library went in the merged module.
struct LinkMaps {
  std::vector<size_t> functions;
  // Whether the body of the library's function i is copied into its slot: not when the slot already holds a
  // definition, or an import of the same name, and the library only declares it.
  std::vector<bool> copyFunction;
  std::vector<size_t> hostImports;
  uint64_t stringShift = 0;
  uint32_t debugShift = 0;
};

bool sameField(const IrStructField &a, const IrStructField &b) {
  return a.name == b.name && a.envelope == b.envelope && a.offsetBytes == b.offsetBytes &&
         a.sizeBytes == b.sizeBytes && a.alignmentBytes == b.alignmentBytes &&
         a.paddingKind == b.paddingKind && a.category == b.category &&
         a.visibility == b.visibility && a.isStatic == b.isStatic;
}

bool sameLayout(const IrStructLayout &a, const IrStructLayout &b) {
  return a.name == b.name && a.totalSizeBytes == b.totalSizeBytes &&
         a.alignmentBytes == b.alignmentBytes &&
         std::equal(a.fields.begin(), a.fields.end(), b.fields.begin(), b.fields.end(), sameField);
}

bool sameSignature(const IrHostImport &a, const IrHostImport &b) {
  return a.name == b.name && a.parameters == b.parameters && a.returnKind == b.returnKind;
}

// Struct layouts merge by name; equal layouts are one.
bool mergeStructLayouts(IrModule &module, const IrModule &library, std::string &error) {
  std::unordered_map<std::string, size_t> byName;
  for (size_t index = 0; index < module.structLayouts.size(); ++index) {
    byName.emplace(module.structLayouts[index].name, index);
  }
  for (const IrStructLayout &layout : library.structLayouts) {
    const auto found = byName.find(layout.name);
    if (found == byName.end()) {
      byName.emplace(layout.name, module.structLayouts.size());
      module.structLayouts.push_back(layout);
    } else if (!sameLayout(module.structLayouts[found->second], layout)) {
      error = "struct layout of " + layout.name + " differs between the modules";
      return false;
    }
  }
  return true;
}

// Host imports merge by name and signature; the map says where each of the library's went.
bool mergeHostImports(IrModule &module,
                      const IrModule &library,
                      LinkMaps &maps,
                      std::string &error) {
  std::unordered_map<std::string, size_t> byName;
  for (size_t index = 0; index < module.hostImports.size(); ++index) {
    byName.emplace(module.hostImports[index].name, index);
  }
  for (const IrHostImport &import : library.hostImports) {
    const auto found = byName.find(import.name);
    if (found == byName.end()) {
      byName.emplace(import.name, module.hostImports.size());
      maps.hostImports.push_back(module.hostImports.size());
      module.hostImports.push_back(import);
    } else if (!sameSignature(module.hostImports[found->second], import)) {
      error = "host import " + import.name + " has two signatures";
      return false;
    } else {
      maps.hostImports.push_back(found->second);
    }
  }
  return true;
}

// Whether `name` is under one of the shared roots of the options (`/std` holds `/std/math/abs`, not `/stdx`).
bool isSharedFunction(const IrLinkOptions *options, const std::string &name) {
  if (options == nullptr) {
    return false;
  }
  for (const std::string &root : options->sharedRoots) {
    if (name.size() > root.size() && name.compare(0, root.size(), root) == 0 &&
        name[root.size()] == '/') {
      return true;
    }
  }
  return false;
}

// Gives every function of the library a slot in the merged module: an import resolves to the definition of its name
// (which may be the other module's import, kept as one), a definition fills the slot of an import of its name, and
// two definitions of one name are an error. The bodies are copied later, once every index is known.
bool mergeFunctionSlots(IrModule &module,
                        const IrModule &library,
                        const IrLinkOptions *options,
                        LinkMaps &maps,
                        std::string &error) {
  std::unordered_map<std::string, size_t> byName;
  for (size_t index = 0; index < module.functions.size(); ++index) {
    byName.emplace(module.functions[index].name, index);
  }
  for (const IrFunction &function : library.functions) {
    const auto found = byName.find(function.name);
    if (found == byName.end()) {
      byName.emplace(function.name, module.functions.size());
      maps.functions.push_back(module.functions.size());
      maps.copyFunction.push_back(true);
      IrFunction stub;
      stub.name = function.name;
      stub.parameterCount = function.parameterCount;
      stub.isImport = function.isImport;
      stub.importLibrary = function.importLibrary;
      module.functions.push_back(std::move(stub));
      continue;
    }
    IrFunction &existing = module.functions[found->second];
    if (existing.parameterCount != function.parameterCount) {
      error = "function " + function.name + " takes " + std::to_string(existing.parameterCount) +
              " parameters in one module and " + std::to_string(function.parameterCount) +
              " in the other";
      return false;
    }
    if (!existing.isImport && !function.isImport) {
      if (isSharedFunction(options, function.name)) {
        maps.functions.push_back(found->second);
        maps.copyFunction.push_back(false);
        continue;
      }
      error = "duplicate definition of " + function.name;
      return false;
    }
    maps.functions.push_back(found->second);
    maps.copyFunction.push_back(existing.isImport && !function.isImport);
  }
  return true;
}

uint32_t maxDebugId(const IrModule &module) {
  uint32_t most = 0;
  for (const IrInstructionSourceMapEntry &entry : module.instructionSourceMap) {
    most = std::max(most, entry.debugId);
  }
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      most = std::max(most, inst.debugId);
    }
  }
  return most;
}

bool failRelocation(const IrFunction &function, const char *what, std::string &error) {
  error = std::string("invalid ") + what + " in " + function.name + " of the library";
  return false;
}

// Rewrites the immediate of one instruction from the library's numbering to the merged module's.
bool relocateInstruction(IrInstruction &inst,
                         const IrModule &library,
                         const IrFunction &function,
                         const LinkMaps &maps,
                         std::string &error) {
  switch (irOperandClass(inst.op)) {
  case IrOperandClass::Function:
    if (inst.imm >= maps.functions.size()) {
      return failRelocation(function, "function index", error);
    }
    inst.imm = maps.functions[static_cast<size_t>(inst.imm)];
    return true;
  case IrOperandClass::HostImport:
    if (inst.imm >= maps.hostImports.size()) {
      return failRelocation(function, "host import index", error);
    }
    inst.imm = maps.hostImports[static_cast<size_t>(inst.imm)];
    return true;
  case IrOperandClass::String:
    if (inst.imm >= library.stringTable.size()) {
      return failRelocation(function, "string index", error);
    }
    inst.imm += maps.stringShift;
    return true;
  case IrOperandClass::PrintString:
    if (decodePrintStringIndex(inst.imm) >= library.stringTable.size()) {
      return failRelocation(function, "string index", error);
    }
    inst.imm = encodePrintStringImm(decodePrintStringIndex(inst.imm) + maps.stringShift,
                                    decodePrintFlags(inst.imm));
    return true;
  case IrOperandClass::Constant:
    if (inst.isStringLiteral) {
      if (inst.imm >= library.stringTable.size()) {
        return failRelocation(function, "string index", error);
      }
      inst.imm += maps.stringShift;
      if (inst.op == IrOpcode::PushI32 &&
          inst.imm > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        error = "string index of " + function.name + " does not fit a PushI32 after the link";
        return false;
      }
    }
    return true;
  case IrOperandClass::None:
  case IrOperandClass::Local:
  case IrOperandClass::Jump:
  case IrOperandClass::PrintFlags:
  case IrOperandClass::Width:
  case IrOperandClass::ExitCode:
    return true;
  case IrOperandClass::Unclassified:
    break;
  }
  error = "unclassified opcode in " + function.name + " of the library";
  return false;
}

// The library's function as it reads in the merged module.
bool relocateFunction(const IrModule &library,
                      const IrFunction &function,
                      const LinkMaps &maps,
                      IrFunction &out,
                      std::string &error) {
  out = function;
  if (function.isImport) {
    return true;
  }
  for (size_t at = 0; at < out.instructions.size(); ++at) {
    IrInstruction &inst = out.instructions[at];
    if (!relocateInstruction(inst, library, function, maps, error)) {
      return false;
    }
    if (inst.debugId != 0) {
      inst.debugId += maps.debugShift;
    }
  }
  return true;
}

} // namespace

bool linkIrModule(IrModule &module,
                  const IrModule &library,
                  std::string &error,
                  const IrLinkOptions *options) {
  error.clear();
  // The strings of a library move by the size of the program's table, and which pushes hold a string index is known
  // only for a module that marks them (IrInstruction::isStringLiteral, PSIR v39).
  if (!library.stringPushesMarked) {
    error = "the library was written before IR schema v39 and does not mark its string literals; "
            "rebuild it";
    return false;
  }
  IrModule merged = module;
  LinkMaps maps;
  maps.stringShift = merged.stringTable.size();
  maps.debugShift = maxDebugId(merged);
  if (maps.debugShift > std::numeric_limits<uint32_t>::max() - maxDebugId(library)) {
    error = "too many debug ids to link";
    return false;
  }
  if (!mergeStructLayouts(merged, library, error) ||
      !mergeHostImports(merged, library, maps, error) ||
      !mergeFunctionSlots(merged, library, options, maps, error)) {
    return false;
  }
  for (size_t index = 0; index < library.functions.size(); ++index) {
    const IrFunction &function = library.functions[index];
    if (!maps.copyFunction[index]) {
      continue;
    }
    if (!relocateFunction(
            library, function, maps, merged.functions[maps.functions[index]], error)) {
      return false;
    }
  }
  merged.stringTable.insert(
      merged.stringTable.end(), library.stringTable.begin(), library.stringTable.end());
  for (IrInstructionSourceMapEntry entry : library.instructionSourceMap) {
    if (entry.debugId != 0) {
      entry.debugId += maps.debugShift;
    }
    merged.instructionSourceMap.push_back(std::move(entry));
  }
  merged.schemaVersion = IrSchemaVersion;
  merged.stringPushesMarked = module.stringPushesMarked && library.stringPushesMarked;
  module = std::move(merged);
  return true;
}

bool checkIrImportsResolved(const IrModule &module, std::string &error) {
  for (const IrFunction &function : module.functions) {
    if (function.isImport) {
      error = "undefined symbol " + function.name + " (library " + function.importLibrary + ")";
      return false;
    }
  }
  return true;
}

} // namespace primec
