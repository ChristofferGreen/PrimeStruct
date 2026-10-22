#include "../test_compile_run_helpers.h"

// Slice<T, Capability> is a borrowed view of the stdlib's slice module: Read is a ReadSlice<T> with no
// write operations, Write (or ReadWrite) a WriteSlice<T>. Views of a vector come from
// /std/collections/vector/view, slice and sliceMut (docs/spec/strings-and-views.md, Slices).

TEST_SUITE_BEGIN("primestruct.compile.run.vm.slice_capability");

namespace {

int compileErrorOf(const std::string &name, const std::string &source, std::string &errText) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string errPath = (testScratchPath("") / ("primec_" + name + "_err.txt")).string();
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  const int code = runCommand(compileCmd);
  errText = readFile(errPath);
  return code;
}

} // namespace

TEST_CASE("runs vm with Read-capability slice parameter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int>]
sum_window([Slice<i32, Read>] window) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < window.count()) {
    total = total + window.at(i)
    i = i + 1i32
  }
  return(total)
}

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32, 11i32)}
  [Slice<i32, Read>] window{/std/collections/vector/slice<i32>(values, 1i32, 3i32)}
  return(sum_window(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_read.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);
}

TEST_CASE("runs vm with ReadWrite-capability mut slice parameter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int>]
overwrite_first([Slice<i32, ReadWrite> mut] window) {
  window.set(0i32, 99i32)
  return(window.at(0i32))
}

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32)}
  [Slice<i32, ReadWrite> mut] window{/std/collections/vector/sliceMut<i32>(values, 0i32, 2i32)}
  [i32] first{overwrite_first(window)}
  return(first + values.at(0i32))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_readwrite.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // The write went through the view into the vector itself.
  CHECK(runCommand(runVmCmd) == 198);
}

TEST_CASE("rejects mutation attempted through a Read-capability slice parameter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int>]
f([Slice<i32, Read>] window) {
  window.set(0i32, 99i32)
  return(window.at(0i32))
}

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32)}
  [Slice<i32, Read>] window{/std/collections/vector/view<i32>(values)}
  return(f(window))
}
)";
  std::string err;
  CHECK(compileErrorOf("vm_slice_capability_read_assign_rejected", source, err) == 2);
  CHECK(err.find("unknown call target: set") != std::string::npos);
}

TEST_CASE("rejects a Write slice where a Read slice is expected") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int>]
f([Slice<i32, Read>] window) {
  return(window.at(0i32))
}

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32)}
  [Slice<i32, Write> mut] window{/std/collections/vector/sliceMut<i32>(values, 0i32, 2i32)}
  return(f(window))
}
)";
  std::string err;
  CHECK(compileErrorOf("vm_slice_capability_write_for_read_rejected", source, err) == 2);
  CHECK(err.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("readOnly turns a Write slice into a Read slice") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int>]
f([Slice<i32, Read>] window) {
  return(window.at(1i32))
}

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32)}
  [Slice<i32, Write> mut] window{/std/collections/vector/sliceMut<i32>(values, 0i32, 2i32)}
  [Slice<i32, Read>] frozen{window.readOnly()}
  return(f(frozen))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_read_only.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("rejects unknown capability name on a slice parameter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32)}
  [Slice<i32, Bogus>] window{/std/collections/vector/view<i32>(values)}
  return(window.count())
}
)";
  std::string err;
  CHECK(compileErrorOf("vm_slice_capability_unknown_name_rejected", source, err) == 2);
  CHECK_FALSE(err.empty());
}

TEST_CASE("rejects Slice with only one template argument") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32)}
  [Slice<i32>] window{/std/collections/vector/view<i32>(values)}
  return(window.count())
}
)";
  std::string err;
  CHECK(compileErrorOf("vm_slice_capability_single_arg_rejected", source, err) == 2);
  CHECK_FALSE(err.empty());
}

TEST_CASE("runs vm with Read-capability slice local binding") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32, 11i32)}
  [Slice<i32, Read>] window{/std/collections/vector/slice<i32>(values, 1i32, 3i32)}
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < window.count()) {
    total = total + window.at(i)
    i = i + 1i32
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_local_binding_read.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 16);
}

TEST_CASE("runs vm with ReadWrite-capability mut slice local binding") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32)}
  [Slice<i32, ReadWrite> mut] window{/std/collections/vector/sliceMut<i32>(values, 0i32, 2i32)}
  window.set(0i32, 99i32)
  return(window.at(0i32))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_local_binding_readwrite.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 99);
}

TEST_CASE("rejects mutation through a Read-capability slice local binding") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/slice/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32)}
  [Slice<i32, Read>] window{/std/collections/vector/view<i32>(values)}
  window.set(0i32, 99i32)
  return(window.at(0i32))
}
)";
  std::string err;
  CHECK(compileErrorOf("vm_slice_capability_local_binding_read_assign_rejected", source, err) == 2);
  CHECK(err.find("unknown call target: set") != std::string::npos);
}

TEST_CASE("plain array parameters and locals are unaffected by Slice recognition") {
  const std::string source = R"(
[return<int>]
sum_all([array<i32>] values) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < count(values)) {
    total = total + values[i]
    i = i + 1i32
  }
  return(total)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32, 4i32)}
  return(sum_all(values))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_plain_array_unaffected.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_SUITE_END();
