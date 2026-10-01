#pragma once

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
  };
  return programs;
}
