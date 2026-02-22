TEST_SUITE_BEGIN("primestruct.compile.run.vm.gpu");

TEST_CASE("runs vm with gpu dispatch fallback") {
  const std::string source = R"(
[compute workgroup_size(64, 1, 1)]
/add_one(
  [Buffer<i32>] input,
  [Buffer<i32>] output,
  [i32] count
) {
  [i32] x{ /gpu/global_id_x() }
  if (x >= count) {
    return()
  }
  [i32] value{ buffer_load(input, x) }
  buffer_store(output, x, plus(value, 1i32))
}

[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32, 4i32)}
  [Buffer<i32>] input{ gpu_upload(values) }
  [Buffer<i32>] output{ gpu_buffer<i32>(4i32) }
  dispatch(/add_one, 4i32, 1i32, 1i32, input, output, 4i32)
  [array<i32>] result{ gpu_readback(output) }
  return(plus(plus(result[0i32], result[1i32]), plus(result[2i32], result[3i32])))
}
)";
  const std::string srcPath = writeTemp("vm_gpu_dispatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 14);
}

TEST_SUITE_END();
