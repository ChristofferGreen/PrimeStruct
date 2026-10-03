#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec {

struct StdlibSurfaceMetadata;

// State owned by one compilation (TODO-5359; design in
// docs/CompilerArenaAllocator.md, "TODO-5358: compiler global state inventory
// and CompileContext design"). Compilation entry points install a context with
// CompileContext::Scope; helpers that cannot take a context parameter reach it
// through CompileContext::current().
//
// When no context is installed, current() returns a per-thread default context,
// so helpers keep working for callers that never install one (unit tests,
// embedding hosts calling single stages). The default is removed at the end of
// the migration.
class CompileContext {
public:
  // Memoized pure functions of their string argument (binding type names).
  // Entries are always allocated from the system heap (see the insert helpers in
  // SemanticsBindingTypeHelpers.cpp) so they never point into compile-arena
  // memory.
  struct TypeNameCaches {
    std::unordered_map<std::string, std::string> normalizeBindingTypeName;
    std::unordered_map<std::string, std::tuple<bool, std::string, std::string>> splitTemplateTypeName;
    std::unordered_map<std::string, std::pair<bool, std::vector<std::string>>> splitTopLevelTemplateArgs;
  };

  TypeNameCaches typeNames;

  // Memo of findStdlibSurfaceMetadataByResolvedPath: resolved path -> metadata of
  // the (process-wide, immutable) stdlib surface registry. Inserted under a
  // SystemHeapScope.
  std::unordered_map<std::string, const StdlibSurfaceMetadata *> resolvedStdlibSurfacePaths;

  // Benchmark option: skip allocator pressure relief between semantic phases
  // (ScopedSemanticAllocatorReliefDisable).
  bool disableSemanticAllocatorRelief = false;

  CompileContext() = default;
  CompileContext(const CompileContext &) = delete;
  CompileContext &operator=(const CompileContext &) = delete;

  // The context installed on this thread, or the thread's default context.
  static CompileContext &current();

  // Installs `context` as the current context of this thread for the scope's
  // lifetime and restores the previous one on exit.
  class Scope {
  public:
    explicit Scope(CompileContext &context);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

  private:
    CompileContext *previous_;
  };
};

} // namespace primec
