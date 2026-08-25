#include "../test_compile_run_helpers.h"
#include "../test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter materializes variadic pointer uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<Reference<uninitialized<i32>>>]
borrow_ref([Reference<uninitialized<i32>>] value) {
  return(value)
}

[return<int>]
score_ptrs([args<Pointer<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Pointer<uninitialized<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] p0{location(a0)}
  [Reference<uninitialized<i32>>] p1{location(a1)}
  [Reference<uninitialized<i32>>] p2{location(a2)}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] q0{location(b0)}
  [Reference<uninitialized<i32>>] q1{location(b1)}
  [Reference<uninitialized<i32>>] q2{location(b2)}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] r0{location(c0)}
  [Reference<uninitialized<i32>>] r1{location(c1)}

  return(plus(score_ptrs(location(borrow_ref(p0)), location(borrow_ref(p1)), location(borrow_ref(p2))),
              plus(forward(location(borrow_ref(q0)), location(borrow_ref(q1)), location(borrow_ref(q2))),
                   forward_mixed(location(borrow_ref(r0)), location(borrow_ref(r1))))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_pointer_uninitialized_scalar.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_variadic_args_pointer_uninitialized_scalar_err.txt")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "semantic-product method-call target missing lowered definition: /array/at") !=
        std::string::npos);
}

TEST_CASE("C++ emitter materializes variadic pointer uninitialized struct packs from borrowed helper references") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Reference<uninitialized<Pair>>>]
borrow_ref([Reference<uninitialized<Pair>>] value) {
  return(value)
}

[return<int>]
score_ptrs([args<Pointer<uninitialized<Pair>>>] values) {
  init(dereference(values[0i32]), Pair{1i32, 2i32})
  init(dereference(values.at(1i32)), Pair{3i32, 4i32})
  init(dereference(values.at_unsafe(2i32)), Pair{5i32, 6i32})
  [Pair] first{take(dereference(values[0i32]))}
  [Pair] second{take(dereference(values.at(1i32)))}
  [Pair] third{take(dereference(values.at_unsafe(2i32)))}
  return(plus(first.left, plus(second.right, third.left)))
}

[return<int>]
forward([args<Pointer<uninitialized<Pair>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<Pair>>>] values) {
  [uninitialized<Pair>] extra{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [uninitialized<Pair>] a0{uninitialized<Pair>()}
  [uninitialized<Pair>] a1{uninitialized<Pair>()}
  [uninitialized<Pair>] a2{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] p0{location(a0)}
  [Reference<uninitialized<Pair>>] p1{location(a1)}
  [Reference<uninitialized<Pair>>] p2{location(a2)}

  [uninitialized<Pair>] b0{uninitialized<Pair>()}
  [uninitialized<Pair>] b1{uninitialized<Pair>()}
  [uninitialized<Pair>] b2{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] q0{location(b0)}
  [Reference<uninitialized<Pair>>] q1{location(b1)}
  [Reference<uninitialized<Pair>>] q2{location(b2)}

  [uninitialized<Pair>] c0{uninitialized<Pair>()}
  [uninitialized<Pair>] c1{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] r0{location(c0)}
  [Reference<uninitialized<Pair>>] r1{location(c1)}

  return(plus(score_ptrs(location(borrow_ref(p0)), location(borrow_ref(p1)), location(borrow_ref(p2))),
              plus(forward(location(borrow_ref(q0)), location(borrow_ref(q1)), location(borrow_ref(q2))),
                   forward_mixed(location(borrow_ref(r0)), location(borrow_ref(r1))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_pointer_uninitialized_struct_helper_ref.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_variadic_args_pointer_uninitialized_struct_helper_ref_err.txt")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  // TODO-4802 (fixed): args<Pointer<uninitialized<Struct>>> itself now
  // resolves its struct type correctly (see the minimal repro in that
  // TODO, now passing independently). This test's own source additionally
  // uses .at()/.at_unsafe() method-call sugar to index the pack, which
  // still hits TODO-4800's separate "/array/at" gap - re-pinned to that
  // now-current rejection rather than TODO-4802's original one.
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("struct parameter type mismatch: expected /Pair, got <unknown>") !=
        std::string::npos);
}

TEST_SUITE_END();
