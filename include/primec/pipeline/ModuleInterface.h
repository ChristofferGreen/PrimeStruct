#pragma once

// The interface of a module and its symbol table (`primec --emit=interface` and `--emit=symbols`,
// docs/ModulesAndLibrariesPlan.md section 5, docs/spec/integration-and-tooling.md).

#include "primec/ast/Ast.h"
#include "primec/frontend/ExpandedSource.h"
#include "primec/frontend/SemanticProduct.h"

#include <string>
#include <vector>

namespace primec {

// One public non-generic symbol of the module. Every hash is SHA-256 as 64 lowercase hex digits.
struct ModuleInterfaceSymbol {
  std::string path;
  // `struct`, `enum` or `function`.
  std::string kind;
  // The declaration without its summary: a struct in full, a function without body, with resolved types and effects.
  std::string signatureHash;
  // The whole definition, body included, summary included.
  std::string contentHash;
  // The content hashes of the symbol and of every definition it references, directly or not (private and stdlib
  // definitions included), as one hash.
  std::string closureHash;
};

struct ModuleInterface {
  // The interface in canonical PrimeStruct syntax.
  std::string text;
  // Sorted by path, then kind, then signature hash.
  std::vector<ModuleInterfaceSymbol> symbols;
  // The hash of the sorted `path kind signature` lines of the table: a change of a body or a summary leaves it alone.
  std::string interfaceHash;
};

// A module has no entry point, but semantic validation demands one. Adds `return<int> <entryPath>() { return(0i32) }` to
// `program` when it defines no `entryPath`, so a module validates; buildModuleInterface leaves `entryPath` out.
bool addInterfaceEntryStub(Program &program, const std::string &entryPath, std::string &error);

// Builds the interface of the definitions the module's own sources hold (not the stdlib's): the public structs, enums
// and non-generic functions of a validated program. `semanticProgram` supplies resolved return types and effects.
// `entryPath` is never part of the interface. A function is printed as a declaration `[..., imported(<libraryName>)]
// name(parameters) {}`, and a method nested in a struct inside the struct. Returns false with `error` set when the
// product lacks a fact.
bool buildModuleInterface(const Program &program,
                          const SemanticProgram &semanticProgram,
                          const ExpandedSource &source,
                          const std::string &entryPath,
                          const std::string &libraryName,
                          ModuleInterface &out,
                          std::string &error);

// The library name of a root file: the file name without its extension, with every character that is not a letter, a
// digit or `_` written as `_` (and `_` first when it starts with a digit), so `[imported(<name>)]` is one word.
std::string libraryNameOfRootFile(const std::string &path);

// The symbol table as JSON: `{"interfaceHash":...,"symbols":[{"path":...,"kind":...,"signature":...,"content":...,
// "closure":...}]}`, one symbol per line, ending in a newline.
std::string formatModuleSymbolTable(const ModuleInterface &moduleInterface);

} // namespace primec
