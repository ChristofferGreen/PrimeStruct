#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.gpu");

TEST_CASE("runs vm with gpu dispatch fallback") {
  const std::string source = R"(
[compute workgroup_size(64, 1, 1)]
/add_one(
  [Buffer<i32>] input,
  [Buffer<i32>] output,
  [i32] count
) {
  [i32] x{ /std/gpu/global_id_x() }
  if (x >= count) {
    return()
  }
  [i32] value{ /std/gpu/buffer_load(input, x) }
  /std/gpu/buffer_store(output, x, plus(value, 1i32))
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32, 4i32)}
  [Buffer<i32>] input{ /std/gpu/upload(values) }
  [Buffer<i32>] output{ /std/gpu/buffer<i32>(4i32) }
  /std/gpu/dispatch(/add_one, 4i32, 1i32, 1i32, input, output, 4i32)
  [array<i32>] result{ /std/gpu/readback(output) }
  return(plus(plus(result[0i32], result[1i32]), plus(result[2i32], result[3i32])))
}
)";
  const std::string srcPath = writeTemp("vm_gpu_dispatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 14);
}

TEST_CASE("runs vm with canonical stdlib Buffer compute access helpers") {
  const std::string source = R"(
import /std/gfx/*

[compute workgroup_size(1, 1, 1)]
/copy_values([Buffer<i32>] input, [Buffer<i32>] output) {
  [i32] viaMethod{input.load(0i32)}
  [i32] viaDirect{/std/gfx/Buffer/load(input, 0i32)}
  output.store(0i32, viaMethod)
  /std/gfx/Buffer/store(output, 1i32, viaDirect)
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  [Buffer<i32>] input{/std/gfx/Buffer/upload(values)}
  [Buffer<i32>] output{/std/gfx/Buffer/allocate<i32>(2i32)}
  /std/gpu/dispatch(/copy_values, 1i32, 1i32, 1i32, input, output)
  [array<i32>] result{output.readback()}
  return(plus(result[0i32], result[1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_gfx_buffer_compute_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with experimental stdlib Buffer compute access helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[compute workgroup_size(1, 1, 1)]
/copy_values([Buffer<i32>] input, [Buffer<i32>] output) {
  [i32] viaMethod{input.load(0i32)}
  [i32] viaDirect{/std/gfx/experimental/Buffer/load(input, 0i32)}
  output.store(0i32, viaMethod)
  /std/gfx/experimental/Buffer/store(output, 1i32, viaDirect)
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] values{array<i32>(5i32)}
  [Buffer<i32>] input{/std/gfx/experimental/Buffer/upload(values)}
  [Buffer<i32>] output{/std/gfx/experimental/Buffer/allocate<i32>(2i32)}
  /std/gpu/dispatch(/copy_values, 1i32, 1i32, 1i32, input, output)
  [array<i32>] result{output.readback()}
  return(plus(result[0i32], result[1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_compute_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("runs vm with gpu dispatch fallback and variadic Buffer packs") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/score_direct([args<Buffer<i32>>] values) {
  [i32] x{ /std/gpu/global_id_x() }
  if (x >= 1i32) {
    return()
  }
  [i32] total{
    plus(/std/gpu/buffer_load(values[0i32], 0i32),
         /std/gpu/buffer_load(values[minus(count(values), 1i32)], 0i32))
  }
  /std/gpu/buffer_store(values[0i32], 0i32, total)
}

[compute workgroup_size(1, 1, 1)]
/forward([args<Buffer<i32>>] values) {
  /score_direct([spread] values)
}

[compute workgroup_size(1, 1, 1)]
/forward_mixed([Buffer<i32>] extra, [args<Buffer<i32>>] values) {
  /score_direct(extra, [spread] values)
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32)}
  [Buffer<i32>] b0{ /std/gpu/upload(a0) }
  [Buffer<i32>] b1{ /std/gpu/upload(a1) }
  [Buffer<i32>] b2{ /std/gpu/upload(a2) }

  [array<i32>] c0{array<i32>(5i32)}
  [array<i32>] c1{array<i32>(6i32)}
  [array<i32>] c2{array<i32>(8i32)}
  [Buffer<i32>] d0{ /std/gpu/upload(c0) }
  [Buffer<i32>] d1{ /std/gpu/upload(c1) }
  [Buffer<i32>] d2{ /std/gpu/upload(c2) }

  [array<i32>] e0{array<i32>(7i32)}
  [array<i32>] e1{array<i32>(9i32)}
  [array<i32>] extra_values{array<i32>(2i32)}
  [Buffer<i32>] f0{ /std/gpu/upload(e0) }
  [Buffer<i32>] f1{ /std/gpu/upload(e1) }
  [Buffer<i32>] extra{ /std/gpu/upload(extra_values) }

  /std/gpu/dispatch(/score_direct, 1i32, 1i32, 1i32, b0, b1, b2)
  /std/gpu/dispatch(/forward, 1i32, 1i32, 1i32, d0, d1, d2)
  /std/gpu/dispatch(/forward_mixed, 1i32, 1i32, 1i32, extra, f0, f1)

  [array<i32>] r0{ /std/gpu/readback(b0) }
  [array<i32>] r1{ /std/gpu/readback(d0) }
  [array<i32>] r2{ /std/gpu/readback(extra) }
  return(plus(r0[0i32], plus(r1[0i32], r2[0i32])))
}
)";
  const std::string srcPath = writeTemp("vm_gpu_variadic_buffer_args.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 29);
}

TEST_CASE("runs vm variadic Reference<Buffer> packs through location/dereference") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(gpu_dispatch)]
score_buffers_reference([args<Reference<Buffer<i32>>>] values) {
  [Buffer<i32>] headValue{dereference(values[0i32])}
  [Buffer<i32>] tailValue{dereference(values[minus(count(values), 1i32)])}
  return(plus(headValue.count(), plus(tailValue.count(), count(values))))
}

[return<int> effects(gpu_dispatch)]
forward_reference([args<Reference<Buffer<i32>>>] values) {
  return(score_buffers_reference([spread] values))
}

[return<int> effects(gpu_dispatch)]
forward_reference_mixed([args<Reference<Buffer<i32>>>] values) {
  [Buffer<i32>] extra{/std/gfx/Buffer/allocate<i32>(10i32)}
  return(score_buffers_reference(location(extra), [spread] values))
}

[return<int> effects(gpu_dispatch)]
main() {
  [Buffer<i32>] d0Value{/std/gfx/Buffer/allocate<i32>(3i32)}
  [Buffer<i32>] d1Value{/std/gfx/Buffer/allocate<i32>(1i32)}
  [Buffer<i32>] d2Value{/std/gfx/Buffer/allocate<i32>(2i32)}
  [Buffer<i32>] f0Value{/std/gfx/Buffer/allocate<i32>(4i32)}
  [Buffer<i32>] f1Value{/std/gfx/Buffer/allocate<i32>(1i32)}
  [Buffer<i32>] f2Value{/std/gfx/Buffer/allocate<i32>(5i32)}
  [Buffer<i32>] m0Value{/std/gfx/Buffer/allocate<i32>(6i32)}
  [Buffer<i32>] m1Value{/std/gfx/Buffer/allocate<i32>(2i32)}
  [Reference<Buffer<i32>>] d0{location(d0Value)}
  [Reference<Buffer<i32>>] d1{location(d1Value)}
  [Reference<Buffer<i32>>] d2{location(d2Value)}
  [Reference<Buffer<i32>>] f0{location(f0Value)}
  [Reference<Buffer<i32>>] f1{location(f1Value)}
  [Reference<Buffer<i32>>] f2{location(f2Value)}
  [Reference<Buffer<i32>>] m0{location(m0Value)}
  [Reference<Buffer<i32>>] m1{location(m1Value)}
  [int] direct{score_buffers_reference(d0, d1, d2)}
  [int] forwarded{forward_reference(f0, f1, f2)}
  [int] mixed{forward_reference_mixed(m0, m1)}
  return(plus(direct, plus(forwarded, mixed)))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_buffer_reference.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 35);
}

TEST_CASE("runs vm variadic Pointer<Buffer> packs with dereference helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(gpu_dispatch)]
score_buffers_pointer([args<Pointer<Buffer<i32>>>] values) {
  [Pointer<Buffer<i32>>] head{at(values, 0i32)}
  [Pointer<Buffer<i32>>] tail{at(values, minus(count(values), 1i32))}
  [Buffer<i32>] headValue{dereference(head)}
  [Buffer<i32>] tailValue{dereference(tail)}
  return(plus(headValue.count(), plus(tailValue.count(), count(values))))
}

[return<int> effects(gpu_dispatch)]
forward_pointer([args<Pointer<Buffer<i32>>>] values) {
  return(score_buffers_pointer([spread] values))
}

[return<int> effects(gpu_dispatch)]
forward_pointer_mixed([args<Pointer<Buffer<i32>>>] values) {
  [Buffer<i32>] extra{/std/gfx/Buffer/allocate<i32>(10i32)}
  return(score_buffers_pointer(location(extra), [spread] values))
}

[return<int> effects(gpu_dispatch)]
main() {
  [Buffer<i32>] d0Value{/std/gfx/Buffer/allocate<i32>(3i32)}
  [Buffer<i32>] d1Value{/std/gfx/Buffer/allocate<i32>(1i32)}
  [Buffer<i32>] d2Value{/std/gfx/Buffer/allocate<i32>(2i32)}
  [Buffer<i32>] f0Value{/std/gfx/Buffer/allocate<i32>(4i32)}
  [Buffer<i32>] f1Value{/std/gfx/Buffer/allocate<i32>(1i32)}
  [Buffer<i32>] f2Value{/std/gfx/Buffer/allocate<i32>(5i32)}
  [Buffer<i32>] m0Value{/std/gfx/Buffer/allocate<i32>(6i32)}
  [Buffer<i32>] m1Value{/std/gfx/Buffer/allocate<i32>(2i32)}
  [Pointer<Buffer<i32>>] d0{location(d0Value)}
  [Pointer<Buffer<i32>>] d1{location(d1Value)}
  [Pointer<Buffer<i32>>] d2{location(d2Value)}
  [Pointer<Buffer<i32>>] f0{location(f0Value)}
  [Pointer<Buffer<i32>>] f1{location(f1Value)}
  [Pointer<Buffer<i32>>] f2{location(f2Value)}
  [Pointer<Buffer<i32>>] m0{location(m0Value)}
  [Pointer<Buffer<i32>>] m1{location(m1Value)}
  [int] direct{score_buffers_pointer(d0, d1, d2)}
  [int] forwarded{forward_pointer(f0, f1, f2)}
  [int] mixed{forward_pointer_mixed(m0, m1)}
  return(plus(direct, plus(forwarded, mixed)))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_buffer_pointer.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 35);
}


TEST_CASE("runs vm with gpu dispatch fallback and borrowed variadic Buffer packs") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/score_direct([args<Reference<Buffer<i32>>>] values) {
  [i32] x{ /std/gpu/global_id_x() }
  if (x >= 1i32) {
    return()
  }
  [i32] total{
    plus(/std/gpu/buffer_load(dereference(values[0i32]), 0i32),
         /std/gpu/buffer_load(dereference(values[minus(count(values), 1i32)]), 0i32))
  }
  /std/gpu/buffer_store(dereference(values[0i32]), 0i32, total)
}

[compute workgroup_size(1, 1, 1)]
/forward([args<Reference<Buffer<i32>>>] values) {
  /score_direct([spread] values)
}

[compute workgroup_size(1, 1, 1)]
/forward_mixed([Reference<Buffer<i32>>] extra, [args<Reference<Buffer<i32>>>] values) {
  /score_direct(extra, [spread] values)
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32)}
  [Buffer<i32>] b0{ /std/gpu/upload(a0) }
  [Buffer<i32>] b1{ /std/gpu/upload(a1) }
  [Buffer<i32>] b2{ /std/gpu/upload(a2) }
  [Reference<Buffer<i32>>] r0{location(b0)}
  [Reference<Buffer<i32>>] r1{location(b1)}
  [Reference<Buffer<i32>>] r2{location(b2)}

  [array<i32>] c0{array<i32>(5i32)}
  [array<i32>] c1{array<i32>(6i32)}
  [array<i32>] c2{array<i32>(8i32)}
  [Buffer<i32>] d0{ /std/gpu/upload(c0) }
  [Buffer<i32>] d1{ /std/gpu/upload(c1) }
  [Buffer<i32>] d2{ /std/gpu/upload(c2) }
  [Reference<Buffer<i32>>] s0{location(d0)}
  [Reference<Buffer<i32>>] s1{location(d1)}
  [Reference<Buffer<i32>>] s2{location(d2)}

  [array<i32>] e0{array<i32>(7i32)}
  [array<i32>] e1{array<i32>(9i32)}
  [array<i32>] extra_values{array<i32>(2i32)}
  [Buffer<i32>] f0{ /std/gpu/upload(e0) }
  [Buffer<i32>] f1{ /std/gpu/upload(e1) }
  [Buffer<i32>] extra{ /std/gpu/upload(extra_values) }
  [Reference<Buffer<i32>>] t0{location(f0)}
  [Reference<Buffer<i32>>] t1{location(f1)}
  [Reference<Buffer<i32>>] extra_ref{location(extra)}

  /std/gpu/dispatch(/score_direct, 1i32, 1i32, 1i32, r0, r1, r2)
  /std/gpu/dispatch(/forward, 1i32, 1i32, 1i32, s0, s1, s2)
  /std/gpu/dispatch(/forward_mixed, 1i32, 1i32, 1i32, extra_ref, t0, t1)

  [array<i32>] r0_out{ /std/gpu/readback(b0) }
  [array<i32>] r1_out{ /std/gpu/readback(d0) }
  [array<i32>] r2_out{ /std/gpu/readback(extra) }
  [i32] total{plus(r0_out[0i32], plus(r1_out[0i32], r2_out[0i32]))}
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_gpu_variadic_borrowed_buffer_args.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 29);
}

TEST_CASE("runs vm with gpu dispatch fallback and pointer variadic Buffer packs") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/score_direct([args<Pointer<Buffer<i32>>>] values) {
  [i32] x{ /std/gpu/global_id_x() }
  if (x >= 1i32) {
    return()
  }
  [i32] total{
    plus(/std/gpu/buffer_load(dereference(values[0i32]), 0i32),
         /std/gpu/buffer_load(dereference(values[minus(count(values), 1i32)]), 0i32))
  }
  /std/gpu/buffer_store(dereference(values[0i32]), 0i32, total)
}

[compute workgroup_size(1, 1, 1)]
/forward([args<Pointer<Buffer<i32>>>] values) {
  /score_direct([spread] values)
}

[compute workgroup_size(1, 1, 1)]
/forward_mixed([Pointer<Buffer<i32>>] extra, [args<Pointer<Buffer<i32>>>] values) {
  /score_direct(extra, [spread] values)
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32)}
  [Buffer<i32>] b0{ /std/gpu/upload(a0) }
  [Buffer<i32>] b1{ /std/gpu/upload(a1) }
  [Buffer<i32>] b2{ /std/gpu/upload(a2) }
  [Pointer<Buffer<i32>>] p0{location(b0)}
  [Pointer<Buffer<i32>>] p1{location(b1)}
  [Pointer<Buffer<i32>>] p2{location(b2)}

  [array<i32>] c0{array<i32>(5i32)}
  [array<i32>] c1{array<i32>(6i32)}
  [array<i32>] c2{array<i32>(8i32)}
  [Buffer<i32>] d0{ /std/gpu/upload(c0) }
  [Buffer<i32>] d1{ /std/gpu/upload(c1) }
  [Buffer<i32>] d2{ /std/gpu/upload(c2) }
  [Pointer<Buffer<i32>>] q0{location(d0)}
  [Pointer<Buffer<i32>>] q1{location(d1)}
  [Pointer<Buffer<i32>>] q2{location(d2)}

  [array<i32>] e0{array<i32>(7i32)}
  [array<i32>] e1{array<i32>(9i32)}
  [array<i32>] extra_values{array<i32>(2i32)}
  [Buffer<i32>] f0{ /std/gpu/upload(e0) }
  [Buffer<i32>] f1{ /std/gpu/upload(e1) }
  [Buffer<i32>] extra{ /std/gpu/upload(extra_values) }
  [Pointer<Buffer<i32>>] m0{location(f0)}
  [Pointer<Buffer<i32>>] m1{location(f1)}
  [Pointer<Buffer<i32>>] extra_ptr{location(extra)}

  /std/gpu/dispatch(/score_direct, 1i32, 1i32, 1i32, p0, p1, p2)
  /std/gpu/dispatch(/forward, 1i32, 1i32, 1i32, q0, q1, q2)
  /std/gpu/dispatch(/forward_mixed, 1i32, 1i32, 1i32, extra_ptr, m0, m1)

  [array<i32>] r0_out{ /std/gpu/readback(b0) }
  [array<i32>] r1_out{ /std/gpu/readback(d0) }
  [array<i32>] r2_out{ /std/gpu/readback(extra) }
  [i32] total{plus(r0_out[0i32], plus(r1_out[0i32], r2_out[0i32]))}
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_gpu_variadic_pointer_buffer_args.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 29);
}

TEST_CASE("runs vm with canonical stdlib Buffer arg-pack method receivers") {
  const std::string source = R"(
import /std/gfx/*

[compute workgroup_size(1, 1, 1)]
/score_values([args<Buffer<i32>>] values) {
  [i32] head{values.at(0i32).load(0i32)}
  [i32] tail{values.at_unsafe(minus(count(values), 1i32)).load(0i32)}
  values[0i32].store(0i32, plus(head, tail))
}

[compute workgroup_size(1, 1, 1)]
/score_refs([args<Reference<Buffer<i32>>>] values) {
  [i32] head{dereference(values.at(0i32)).load(0i32)}
  [i32] tail{dereference(values.at_unsafe(minus(count(values), 1i32))).load(0i32)}
  dereference(values[0i32]).store(0i32, plus(head, tail))
}

[compute workgroup_size(1, 1, 1)]
/score_ptrs([args<Pointer<Buffer<i32>>>] values) {
  [i32] head{dereference(values.at(0i32)).load(0i32)}
  [i32] tail{dereference(values.at_unsafe(minus(count(values), 1i32))).load(0i32)}
  dereference(values[0i32]).store(0i32, plus(head, tail))
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32)}
  [Buffer<i32>] b0{/std/gfx/Buffer/upload(a0)}
  [Buffer<i32>] b1{/std/gfx/Buffer/upload(a1)}
  [Buffer<i32>] b2{/std/gfx/Buffer/upload(a2)}

  [array<i32>] c0{array<i32>(5i32)}
  [array<i32>] c1{array<i32>(6i32)}
  [array<i32>] c2{array<i32>(8i32)}
  [Buffer<i32>] d0{/std/gfx/Buffer/upload(c0)}
  [Buffer<i32>] d1{/std/gfx/Buffer/upload(c1)}
  [Buffer<i32>] d2{/std/gfx/Buffer/upload(c2)}
  [Reference<Buffer<i32>>] r0{location(d0)}
  [Reference<Buffer<i32>>] r1{location(d1)}
  [Reference<Buffer<i32>>] r2{location(d2)}

  [array<i32>] e0{array<i32>(7i32)}
  [array<i32>] e1{array<i32>(9i32)}
  [array<i32>] e2{array<i32>(2i32)}
  [Buffer<i32>] f0{/std/gfx/Buffer/upload(e0)}
  [Buffer<i32>] f1{/std/gfx/Buffer/upload(e1)}
  [Buffer<i32>] f2{/std/gfx/Buffer/upload(e2)}
  [Pointer<Buffer<i32>>] p0{location(f0)}
  [Pointer<Buffer<i32>>] p1{location(f1)}
  [Pointer<Buffer<i32>>] p2{location(f2)}

  /std/gpu/dispatch(/score_values, 1i32, 1i32, 1i32, b0, b1, b2)
  /std/gpu/dispatch(/score_refs, 1i32, 1i32, 1i32, r0, r1, r2)
  /std/gpu/dispatch(/score_ptrs, 1i32, 1i32, 1i32, p0, p1, p2)

  [array<i32>] out0{b0.readback()}
  [array<i32>] out1{d0.readback()}
  [array<i32>] out2{f0.readback()}
  return(plus(out0[0i32], plus(out1[0i32], out2[0i32])))
}
)";
  const std::string srcPath = writeTemp("vm_gfx_buffer_arg_pack_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_SUITE_END();
