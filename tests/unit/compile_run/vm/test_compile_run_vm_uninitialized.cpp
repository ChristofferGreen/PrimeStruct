#include "../test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.uninitialized");

TEST_CASE("runs vm with uninitialized string storage") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [uninitialized<string>] storage{uninitialized<string>()}
  init(storage, "hello"utf8)
  print_line(take(storage))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_string.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_uninitialized_string_out.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_vm_uninitialized_string_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string errorText = readFile(errPath);
  CHECK(errorText.find("return requires uninitialized storage to be dropped") !=
        std::string::npos);
}

TEST_CASE("names the way out of a map or soa slot allocation") {
  const std::string source = R"(
import /std/collections/*

[reflect struct]
Particle() {
  [i32] x{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<soa<Particle>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<soa<Particle>>>(1i32)}
  [Pointer<uninitialized<map<i32, i32>>> mut] table{/std/intrinsics/memory/alloc<uninitialized<map<i32, i32>>>(1i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_map_soa_slot.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_uninitialized_map_soa_slot_err.txt").string();
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath) ==
        2);
  const std::string errorText = readFile(errPath);
  CHECK(errorText.find("does not support alloc target type: soa<Particle> (a slot of a map or soa "
                       "value does not "
                       "exist yet; keep the value in a local binding or a struct field") !=
        std::string::npos);

  const std::string mapSource = R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<map<i32, i32>>> mut] table{/std/intrinsics/memory/alloc<uninitialized<map<i32, i32>>>(1i32)}
  return(0i32)
}
)";
  const std::string mapPath = writeTemp("vm_uninitialized_map_slot.prime", mapSource);
  CHECK(runCommand("./primec --emit=vm " + mapPath + " --entry /main > /dev/null 2> " + errPath) ==
        2);
  CHECK(readFile(errPath).find(
            "alloc target type: map<i32, i32> (a slot of a map or soa value does not exist yet") !=
        std::string::npos);
}

TEST_CASE("runs vm with uninitialized borrow") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 9i32)
  [Reference<i32>] ref{borrow(storage)}
  [i32] out{dereference(ref)}
  drop(storage)
  return(out)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_borrow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with dereferenced uninitialized borrow") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] storage_ref{location(storage)}
  init(dereference(storage_ref), 9i32)
  [Reference<i32>] ref{borrow(dereference(storage_ref))}
  [i32] out{dereference(ref)}
  drop(dereference(storage_ref))
  return(out)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_deref_borrow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with uninitialized struct field") {
  const std::string source = R"(
[struct]
Box() {
  [uninitialized<i32>] value{uninitialized<i32>()}

  [public]
  Create() {
  }
}

[return<int>]
main() {
  [Box mut] box{Box{}}
  [Reference<Box> mut] ref{location(box)}
  init(ref.value, 7i32)
  [i32] out{take(ref.value)}
  return(out)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_struct.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_uninitialized_struct_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_SUITE_END();
