#pragma once

#include "primec/embed/Script.h"

#include <cstdint>
#include <string>
#include <vector>

// Source programs whose precompiled bytecode is pinned in
// embed_fixture_bytecode.h. Index i here matches index i there.
struct EmbedProgram {
  std::string name;
  std::string source;
  std::vector<std::string> args;
  int expectedExit;
};

inline const std::vector<EmbedProgram> &embedPrograms() {
  static const std::vector<EmbedProgram> programs = {
      {"return_eleven", "[return<int>]\nmain() {\n  return(11i32)\n}\n", {}, 11},
      {"return_zero", "[return<int>]\nmain() {\n  return(0i32)\n}\n", {}, 0},
      {"negative_result", "[return<int>]\nmain() {\n  return(0i32 - 5i32)\n}\n", {}, -5},
      {"arithmetic", "[return<int>]\nmain() {\n  return(6i32 * 7i32 + 1i32)\n}\n", {}, 43},
      {"loop_sum",
       "[return<int>]\nmain() {\n  [mut] sum{0i32}\n  [mut] i{1i32}\n  while(i <= 10i32) {\n"
       "    sum = sum + i\n    i = i + 1i32\n  }\n  return(sum)\n}\n",
       {},
       55},
      {"args_count", "[return<int>]\nmain([array<string>] args) {\n  return(args.count())\n}\n", {"a", "b"}, 3},
      {"struct_method",
       "[struct]\nCounter {\n  [i32] value{41i32}\n\n  [i32]\n  next_value() {\n    return(this.value + 1i32)\n  }\n}\n\n"
       "[i32]\nmain() {\n  [Counter] c{Counter{}}\n  return(c.next_value())\n}\n",
       {},
       42},
      {"stdlib_math",
       "import /std/math/*\n\n[return<int>]\nmain() {\n  return(convert<i32>(abs(-4.0f)))\n}\n",
       {},
       4},
      // Needs the host function from bindEmbedFixtureHosts: 40 + 2 + 1.
      {"host_call",
       "[host return<int>]\nhost_add([i32] a, [i32] b) {\n}\n\n[return<int>]\nmain() {\n  return(host_add(40i32, 2i32) + 1i32)\n}\n",
       {},
       43},
  };
  return programs;
}

// Binds the host functions the fixture programs declare. Safe to call on any
// fixture: bindings a script does not declare are ignored.
inline void bindEmbedFixtureHosts(primec::embed::Script &script) {
  script.bind("host_add", [](int32_t a, int32_t b) { return a + b; });
}

// Library with exports for the bundle fixture: add(i32, i32) -> i32,
// scale(i32, f64) -> f64, count_chars(string) -> i32, plus a main that returns 7.
inline const char *embedBundleSource() {
  return "[return<int>]\nadd([i32] a, [i32] b) {\n  return(a + b)\n}\n\n"
         "[return<int>]\ncount_chars([string] text) {\n  return(text.count())\n}\n\n"
         "[return<f64>]\nscale([i32] count, [f64] factor) {\n  return(convert<f64>(count) * factor)\n}\n\n"
         "[return<int>]\nmain() {\n  return(7i32)\n}\n";
}
