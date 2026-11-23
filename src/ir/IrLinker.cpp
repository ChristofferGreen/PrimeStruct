#include "primec/ir/IrLinker.h"

#include "primec/ir/IrOpcodeTable.h"
#include "primec/ir/IrOperandTable.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_map>
#include <unordered_set>
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
  // Where each string of the library went in the merged table (`NotMerged` for one no merged function reads).
  std::vector<uint64_t> strings;
  uint32_t debugShift = 0;
};

constexpr size_t NotMerged = std::numeric_limits<size_t>::max();

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

// Host imports merge by name and signature; the map says where each of the library's went. `used` (when not null)
// says which of the library's imports a merged function calls; the others are left out.
bool mergeHostImports(IrModule &module,
                      const IrModule &library,
                      const std::vector<bool> *used,
                      LinkMaps &maps,
                      std::string &error) {
  std::unordered_map<std::string, size_t> byName;
  for (size_t index = 0; index < module.hostImports.size(); ++index) {
    byName.emplace(module.hostImports[index].name, index);
  }
  for (size_t at = 0; at < library.hostImports.size(); ++at) {
    const IrHostImport &import = library.hostImports[at];
    if (used != nullptr && !(*used)[at]) {
      maps.hostImports.push_back(NotMerged);
      continue;
    }
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

// Whether `name` is the name of a generic instantiation: a path segment `<template>__t<hex digits>` that the compiler
// derives from the template arguments, so two modules that instantiate a generic alike name it alike.
bool isInstantiationName(const std::string &name) {
  size_t at = name.find("__t");
  while (at != std::string::npos) {
    size_t end = at + 3;
    while (end < name.size() && std::isxdigit(static_cast<unsigned char>(name[end])) != 0) {
      ++end;
    }
    if (end - (at + 3) >= 8 && (end == name.size() || name[end] == '/')) {
      return true;
    }
    at = name.find("__t", at + 3);
  }
  return false;
}

const std::string *stringOf(const IrModule &module, uint64_t index) {
  return index < module.stringTable.size() ? &module.stringTable[static_cast<size_t>(index)]
                                           : nullptr;
}

bool sameStringOperand(const IrModule &a, uint64_t left, const IrModule &b, uint64_t right) {
  const std::string *first = stringOf(a, left);
  const std::string *second = stringOf(b, right);
  return first != nullptr && second != nullptr && *first == *second;
}

// Whether two instructions mean the same in their own modules: an operand that indexes a table of its module
// (functions, host imports, strings) is compared by what it names, any other operand as it is.
bool sameInstruction(const IrModule &moduleA,
                     const IrInstruction &a,
                     const IrModule &moduleB,
                     const IrInstruction &b) {
  if (a.op != b.op || a.isStringLiteral != b.isStringLiteral) {
    return false;
  }
  switch (irOperandClass(a.op)) {
  case IrOperandClass::Function:
    return a.imm < moduleA.functions.size() && b.imm < moduleB.functions.size() &&
           moduleA.functions[static_cast<size_t>(a.imm)].name ==
               moduleB.functions[static_cast<size_t>(b.imm)].name;
  case IrOperandClass::HostImport:
    return a.imm < moduleA.hostImports.size() && b.imm < moduleB.hostImports.size() &&
           moduleA.hostImports[static_cast<size_t>(a.imm)].name ==
               moduleB.hostImports[static_cast<size_t>(b.imm)].name;
  case IrOperandClass::String:
    return sameStringOperand(moduleA, a.imm, moduleB, b.imm);
  case IrOperandClass::PrintString:
    return decodePrintFlags(a.imm) == decodePrintFlags(b.imm) &&
           sameStringOperand(
               moduleA, decodePrintStringIndex(a.imm), moduleB, decodePrintStringIndex(b.imm));
  case IrOperandClass::Constant:
    return a.isStringLiteral ? sameStringOperand(moduleA, a.imm, moduleB, b.imm) : a.imm == b.imm;
  default:
    return a.imm == b.imm;
  }
}

// Whether two definitions of one name are the same code.
bool sameFunction(const IrModule &moduleA,
                  const IrFunction &a,
                  const IrModule &moduleB,
                  const IrFunction &b) {
  if (a.parameterCount != b.parameterCount || a.instructions.size() != b.instructions.size()) {
    return false;
  }
  for (size_t at = 0; at < a.instructions.size(); ++at) {
    if (!sameInstruction(moduleA, a.instructions[at], moduleB, b.instructions[at])) {
      return false;
    }
  }
  return true;
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
    if (options != nullptr && options->onlyFunctions != nullptr &&
        options->onlyFunctions->count(function.name) == 0) {
      maps.functions.push_back(NotMerged);
      maps.copyFunction.push_back(false);
      continue;
    }
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
      if (isInstantiationName(function.name)) {
        // Both modules instantiated one generic alike: one copy. A different body under one name is an error.
        if (!sameFunction(module, existing, library, function)) {
          error = "instantiation " + function.name + " differs between the modules";
          return false;
        }
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
    if (inst.imm >= maps.functions.size() ||
        maps.functions[static_cast<size_t>(inst.imm)] == NotMerged) {
      return failRelocation(function, "function index", error);
    }
    inst.imm = maps.functions[static_cast<size_t>(inst.imm)];
    return true;
  case IrOperandClass::HostImport:
    if (inst.imm >= maps.hostImports.size() ||
        maps.hostImports[static_cast<size_t>(inst.imm)] == NotMerged) {
      return failRelocation(function, "host import index", error);
    }
    inst.imm = maps.hostImports[static_cast<size_t>(inst.imm)];
    return true;
  case IrOperandClass::String:
    if (inst.imm >= library.stringTable.size()) {
      return failRelocation(function, "string index", error);
    }
    inst.imm = maps.strings[static_cast<size_t>(inst.imm)];
    return true;
  case IrOperandClass::PrintString:
    if (decodePrintStringIndex(inst.imm) >= library.stringTable.size()) {
      return failRelocation(function, "string index", error);
    }
    inst.imm =
        encodePrintStringImm(maps.strings[static_cast<size_t>(decodePrintStringIndex(inst.imm))],
                             decodePrintFlags(inst.imm));
    return true;
  case IrOperandClass::Constant:
    if (inst.isStringLiteral) {
      if (inst.imm >= library.stringTable.size()) {
        return failRelocation(function, "string index", error);
      }
      inst.imm = maps.strings[static_cast<size_t>(inst.imm)];
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

// What the functions that are copied read of the library: one flag per host import and string, and the debug ids.
struct LibraryUse {
  std::vector<bool> hostImports;
  std::vector<bool> strings;
  std::unordered_set<uint32_t> debugIds;
};

LibraryUse collectLibraryUse(const IrModule &library, const LinkMaps &maps) {
  LibraryUse use;
  use.hostImports.assign(library.hostImports.size(), false);
  use.strings.assign(library.stringTable.size(), false);
  const auto markString = [&](uint64_t index) {
    if (index < use.strings.size()) {
      use.strings[static_cast<size_t>(index)] = true;
    }
  };
  for (size_t index = 0; index < library.functions.size(); ++index) {
    if (!maps.copyFunction[index]) {
      continue;
    }
    for (const IrInstruction &inst : library.functions[index].instructions) {
      if (inst.debugId != 0) {
        use.debugIds.insert(inst.debugId);
      }
      switch (irOperandClass(inst.op)) {
      case IrOperandClass::HostImport:
        if (inst.imm < use.hostImports.size()) {
          use.hostImports[static_cast<size_t>(inst.imm)] = true;
        }
        break;
      case IrOperandClass::String:
        markString(inst.imm);
        break;
      case IrOperandClass::PrintString:
        markString(decodePrintStringIndex(inst.imm));
        break;
      case IrOperandClass::Constant:
        if (inst.isStringLiteral) {
          markString(inst.imm);
        }
        break;
      default:
        break;
      }
    }
  }
  return use;
}

} // namespace

std::unordered_set<std::string>
reachableLibraryFunctionNames(const IrModule &module,
                              const std::vector<const IrModule *> &libraries) {
  // A definition keeps the table of its library, since a call names its target by an index into that table.
  struct Definition {
    const IrModule *library;
    const IrFunction *function;
  };
  std::unordered_map<std::string, std::vector<Definition>> definitions;
  for (const IrModule *library : libraries) {
    for (const IrFunction &function : library->functions) {
      if (!function.isImport) {
        definitions[function.name].push_back({library, &function});
      }
    }
  }
  std::unordered_set<std::string> reached;
  std::vector<std::string> pending;
  const auto reach = [&](const std::string &name) {
    if (reached.insert(name).second) {
      pending.push_back(name);
    }
  };
  for (const IrFunction &function : module.functions) {
    if (function.isImport) {
      reach(function.name);
    }
  }
  while (!pending.empty()) {
    const std::string name = std::move(pending.back());
    pending.pop_back();
    const auto found = definitions.find(name);
    if (found == definitions.end()) {
      continue;
    }
    for (const Definition &definition : found->second) {
      const std::vector<IrFunction> &table = definition.library->functions;
      for (const IrInstruction &inst : definition.function->instructions) {
        if (irOperandClass(inst.op) == IrOperandClass::Function && inst.imm < table.size()) {
          reach(table[static_cast<size_t>(inst.imm)].name);
        }
      }
    }
  }
  return reached;
}

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
  maps.debugShift = maxDebugId(merged);
  if (maps.debugShift > std::numeric_limits<uint32_t>::max() - maxDebugId(library)) {
    error = "too many debug ids to link";
    return false;
  }
  const bool onlyUsed = options != nullptr && options->onlyFunctions != nullptr;
  if (!mergeStructLayouts(merged, library, error) ||
      !mergeFunctionSlots(merged, library, options, maps, error)) {
    return false;
  }
  // A whole link takes every host import, string and debug id of the library; a link of the reachable functions only
  // those the functions it copies read.
  const LibraryUse use = collectLibraryUse(library, maps);
  if (!mergeHostImports(merged, library, onlyUsed ? &use.hostImports : nullptr, maps, error)) {
    return false;
  }
  std::vector<std::string> addedStrings;
  for (size_t at = 0; at < library.stringTable.size(); ++at) {
    if (onlyUsed && !use.strings[at]) {
      maps.strings.push_back(0);
      continue;
    }
    maps.strings.push_back(merged.stringTable.size() + addedStrings.size());
    addedStrings.push_back(library.stringTable[at]);
  }
  for (size_t index = 0; index < library.functions.size(); ++index) {
    if (!maps.copyFunction[index]) {
      continue;
    }
    if (!relocateFunction(library,
                          library.functions[index],
                          maps,
                          merged.functions[maps.functions[index]],
                          error)) {
      return false;
    }
  }
  merged.stringTable.insert(merged.stringTable.end(), addedStrings.begin(), addedStrings.end());
  for (IrInstructionSourceMapEntry entry : library.instructionSourceMap) {
    if (onlyUsed && entry.debugId != 0 && use.debugIds.count(entry.debugId) == 0) {
      continue;
    }
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
