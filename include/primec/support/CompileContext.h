#pragma once

#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "primec/support/CallNames.h"
#include "primec/support/CompileArena.h"

namespace primec {

struct StdlibSurfaceMetadata;

struct TransparentStringHash {
  using is_transparent = void;
  size_t operator()(std::string_view text) const noexcept {
    return std::hash<std::string_view>{}(text);
  }
};

// Resolutions of call spellings (see resolveSimpleCallName), keyed by the spelling joined with whatever else the
// resolution depends on, NUL separated. One entry answers the match test for every target name.
using SimpleCallResolutionMemo =
    std::unordered_map<std::string, SimpleCallResolution, TransparentStringHash, std::equal_to<>>;

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

  // Memos of pure name classifiers that run once per expression in the front end
  // (isSimpleCallName in semantics and lowering, isRootBuiltinName,
  // getBuiltinArrayAccessName). Keys join every input with NUL separators; inserts
  // run under a SystemHeapScope like the other caches.
  struct NameClassifierCaches {
    SimpleCallResolutionMemo simpleCallNames;
    std::unordered_map<std::string, bool> rootBuiltinNames;
    std::unordered_map<std::string, std::pair<bool, std::string>> builtinArrayAccessNames;
  };

  NameClassifierCaches nameClassifiers;

  // Looks up `parts` joined by NUL in the memo without allocating (the key is built in a stack buffer when it fits);
  // on a miss stores `compute()` under a SystemHeapScope so no entry points into compile-arena memory.
  template <typename Compute>
  static const SimpleCallResolution &
  memoizedSimpleCall(SimpleCallResolutionMemo &memo,
                     std::initializer_list<std::string_view> parts,
                     Compute &&compute);

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

template <typename Compute>
const SimpleCallResolution &
CompileContext::memoizedSimpleCall(SimpleCallResolutionMemo &memo,
                                   std::initializer_list<std::string_view> parts,
                                   Compute &&compute) {
  char buffer[256];
  size_t length = 0;
  bool fits = true;
  for (const std::string_view part : parts) {
    if (length + part.size() + 1 > sizeof(buffer)) {
      fits = false;
      break;
    }
    if (!part.empty()) {
      std::memcpy(buffer + length, part.data(), part.size());
    }
    length += part.size();
    buffer[length++] = '\0';
  }
  std::string longKey;
  if (!fits) {
    for (const std::string_view part : parts) {
      longKey.append(part).push_back('\0');
    }
  }
  const std::string_view key = fits ? std::string_view(buffer, length) : std::string_view(longKey);
  if (const auto it = memo.find(key); it != memo.end()) {
    return it->second;
  }
  SimpleCallResolution resolution = compute();
  SystemHeapScope systemHeapGuard;
  SimpleCallResolution stored{resolution.hasSimpleName, std::string(resolution.simpleName)};
  return memo.emplace(std::string(key), std::move(stored)).first->second;
}

} // namespace primec
