#pragma once

#include <bit>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace primec::embed {

// Primitive types that can cross the host boundary.
enum class HostType : uint8_t { Void, I32, I64, U64, F32, F64, Bool };

namespace detail {
template <class T> struct HostTypeOf;
template <> struct HostTypeOf<void> { static constexpr HostType value = HostType::Void; };
template <> struct HostTypeOf<int32_t> { static constexpr HostType value = HostType::I32; };
template <> struct HostTypeOf<int64_t> { static constexpr HostType value = HostType::I64; };
template <> struct HostTypeOf<uint64_t> { static constexpr HostType value = HostType::U64; };
template <> struct HostTypeOf<float> { static constexpr HostType value = HostType::F32; };
template <> struct HostTypeOf<double> { static constexpr HostType value = HostType::F64; };
template <> struct HostTypeOf<bool> { static constexpr HostType value = HostType::Bool; };

// Raw VM slot <-> C++ value (i32 is sign-extended, floats are bit patterns).
template <class T> T fromSlot(uint64_t slot) {
  if constexpr (std::is_same_v<T, int32_t>) {
    return static_cast<int32_t>(slot);
  } else if constexpr (std::is_same_v<T, int64_t>) {
    return static_cast<int64_t>(slot);
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    return slot;
  } else if constexpr (std::is_same_v<T, float>) {
    return std::bit_cast<float>(static_cast<uint32_t>(slot));
  } else if constexpr (std::is_same_v<T, double>) {
    return std::bit_cast<double>(slot);
  } else {
    return slot != 0;
  }
}

template <class T> uint64_t toSlot(T value) {
  if constexpr (std::is_same_v<T, int32_t>) {
    return static_cast<uint64_t>(static_cast<int64_t>(value));
  } else if constexpr (std::is_same_v<T, int64_t>) {
    return static_cast<uint64_t>(value);
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    return value;
  } else if constexpr (std::is_same_v<T, float>) {
    return std::bit_cast<uint32_t>(value);
  } else if constexpr (std::is_same_v<T, double>) {
    return std::bit_cast<uint64_t>(value);
  } else {
    return value ? 1u : 0u;
  }
}

template <class F> struct CallableTraits : CallableTraits<decltype(&F::operator())> {};
template <class C, class R, class... A> struct CallableTraits<R (C::*)(A...) const> {
  using Result = R;
  using Args = std::tuple<A...>;
};
template <class C, class R, class... A> struct CallableTraits<R (C::*)(A...)> {
  using Result = R;
  using Args = std::tuple<A...>;
};
template <class R, class... A> struct CallableTraits<R (*)(A...)> {
  using Result = R;
  using Args = std::tuple<A...>;
};
} // namespace detail

// Host functions a script may call, keyed by name. Signatures are deduced from
// the C++ callable (int32_t, int64_t, uint64_t, float, double, bool; void or
// one of those as the result) and must match what the script declares.
class HostBindings {
public:
  using RawInvoke = std::function<bool(const uint64_t *args, uint64_t &result, std::string &error)>;

  struct Entry {
    std::string name;
    std::vector<HostType> parameters;
    HostType returnType = HostType::Void;
    RawInvoke invoke;
  };

  template <class F> void bind(std::string name, F callable) {
    using Traits = detail::CallableTraits<F>;
    bindTyped(std::move(name), std::move(callable), static_cast<typename Traits::Args *>(nullptr),
              static_cast<typename Traits::Result *>(nullptr));
  }

  void bindRaw(std::string name, std::vector<HostType> parameters, HostType returnType, RawInvoke invoke);
  const std::vector<Entry> &entries() const { return entries_; }

private:
  template <class F, class... A, class R>
  void bindTyped(std::string name, F callable, std::tuple<A...> *, R *) {
    RawInvoke invoke = [callable = std::move(callable)](const uint64_t *args, uint64_t &result, std::string &) mutable {
      return call<R, A...>(callable, args, result, std::index_sequence_for<A...>{});
    };
    bindRaw(std::move(name), {detail::HostTypeOf<std::remove_cvref_t<A>>::value...}, detail::HostTypeOf<R>::value,
            std::move(invoke));
  }

  template <class R, class... A, class F, size_t... I>
  static bool call(F &callable, const uint64_t *args, uint64_t &result, std::index_sequence<I...>) {
    if constexpr (std::is_void_v<R>) {
      callable(detail::fromSlot<std::remove_cvref_t<A>>(args[I])...);
      result = 0;
    } else {
      result = detail::toSlot<R>(callable(detail::fromSlot<std::remove_cvref_t<A>>(args[I])...));
    }
    return true;
  }

  std::vector<Entry> entries_;
};

// Outcome of compiling or running a script. Failures are reported as data:
// the embed API never exits the process or writes to stdout/stderr itself.
struct ScriptResult {
  bool ok = false;
  // Return value of `main` truncated to 32 bits, like the primevm exit code.
  int exitCode = 0;
  // Diagnostic text in the same plain format the CLI prints on failure.
  std::string diagnostics;
};

// A compiled script. Cheap to copy (shares the compiled module); `run` may be
// called repeatedly without recompiling.
class Script {
public:
  Script() = default;

  // False when compilation failed; `diagnostics()` then says why.
  bool valid() const { return module_ != nullptr; }
  const std::string &diagnostics() const { return diagnostics_; }

  // Runs the entry definition. `args` become the script's argv after the
  // leading program name.
  ScriptResult run(const std::vector<std::string> &args = {}) const;

  // Registers a host function for this script (replacing any earlier binding of
  // the same name). Every host function the script declares must be bound with
  // a matching signature before `run`; otherwise `run` fails with a diagnostic
  // and executes nothing.
  template <class F> void bind(std::string name, F callable) { hostBindings_.bind(std::move(name), std::move(callable)); }
  const HostBindings &hostBindings() const { return hostBindings_; }

  // Names the host functions this script declares, with their signatures
  // rendered like "host_add(i32, i32) -> i32".
  std::vector<std::string> requiredHostFunctions() const;

  // Checks that every required host function is bound with a matching
  // signature. `run` performs the same check.
  bool checkHostBindings(std::string &error) const;

  // Serializes the compiled module to portable bytecode. Returns false with
  // `error` set when the script is not valid. Ship the bytes to a host that
  // links only the runtime-only library (for example iOS, where in-process
  // code generation is unavailable) and load them with `loadBytecode`.
  bool saveBytecode(std::vector<uint8_t> &out, std::string &error) const;

  // Loads bytecode produced by `saveBytecode`. The module is validated for
  // the VM; corrupt, truncated, or incompatible bytes yield an invalid Script
  // whose `diagnostics()` explains why.
  static Script loadBytecode(const std::vector<uint8_t> &bytes, const std::string &name = "script");

private:
  friend class ScriptEngine;
  struct Module;
  std::shared_ptr<const Module> module_;
  std::string name_;
  std::string diagnostics_;
  HostBindings hostBindings_;
};

} // namespace primec::embed
