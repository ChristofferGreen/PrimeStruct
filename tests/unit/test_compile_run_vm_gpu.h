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
  return(plus(r0_out[0i32], plus(r1_out[0i32], r2_out[0i32])))
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
  return(plus(r0_out[0i32], plus(r1_out[0i32], r2_out[0i32])))
}
)";
  const std::string srcPath = writeTemp("vm_gpu_variadic_pointer_buffer_args.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 29);
}

TEST_SUITE_END();
