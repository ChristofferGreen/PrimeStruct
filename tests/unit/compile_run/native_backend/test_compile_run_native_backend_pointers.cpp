#include "../test_compile_run_helpers.h"

#if (defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))) || (defined(__linux__) && defined(__x86_64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.pointers");

TEST_CASE("native hello world example") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::string srcPath = (repoRoot / "examples" / "0.Concrete" / "hello_world.prime").string();
  const std::string exePath = (testScratchPath("") / "primec_native_hello_world_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_hello_world_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "Hello, world!\n");
}

TEST_CASE("native pointer plus offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(first), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_offset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ptr_plus_offset_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native pointer plus on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  [Reference<i32>] ref{location(first)}
  return(dereference(plus(location(ref), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_ref.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ptr_plus_ref_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native pointer minus offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_minus_offset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ptr_minus_offset_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("native assign() writes through a Reference<T>") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{4i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 7i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_native_ref.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ref_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("native location on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{8i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_native_ref_location.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ref_location_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("native reference arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{4i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, plus(ref, 3i32))
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_native_ref_arith.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ref_arith_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

namespace {
// The peak memory footprint `/usr/bin/time -l` reports for a run of `exePath` (0 when it reports none).
uint64_t nativePeakFootprint(const std::string &exePath, const std::string &outPath) {
  const std::string timePath = outPath + ".time";
  if (runCommand("/usr/bin/time -l " + exePath + " > " + outPath + " 2> " + timePath) != 0) {
    return 0;
  }
  std::istringstream lines(readFile(timePath));
  std::string line;
  while (std::getline(lines, line)) {
    if (line.find("peak memory footprint") != std::string::npos) {
      return std::stoull(line);
    }
  }
  return 0;
}

// The page reclaims (minor faults) `/usr/bin/time -l` reports for a run of `exePath` (0 when it reports none).
uint64_t nativePageReclaims(const std::string &exePath, const std::string &outPath) {
  const std::string timePath = outPath + ".time";
  if (runCommand("/usr/bin/time -l " + exePath + " > " + outPath + " 2> " + timePath) != 0) {
    return 0;
  }
  std::istringstream lines(readFile(timePath));
  std::string line;
  while (std::getline(lines, line)) {
    if (line.find("page reclaims") != std::string::npos) {
      return std::stoull(line);
    }
  }
  return 0;
}
} // namespace

TEST_CASE("native heap blocks share pages and freed blocks are reused") {
  // 20000 small blocks live at once, then 100000 allocations each freed again. On arm64 they come from
  // the pooled heap: a block per page would need 320 MB for the list alone.
  const std::string source = R"(
Node() {
  [i32 mut] value{0i32}
  [Pointer<Node> mut] next{}
}

[unsafe return<int> effects(heap_alloc, io_out)]
main() {
  [Pointer<Node> mut] head{/std/intrinsics/memory/alloc<Node>(1i32)}
  [i32 mut] i{1i32}
  while(i < 20000i32) {
    [Pointer<Node> mut] node{/std/intrinsics/memory/alloc<Node>(1i32)}
    assign(dereference(node).value, i)
    assign(dereference(node).next, head)
    head = node
    i = i + 1i32
  }
  [i64 mut] sum{0i64}
  i = 0i32
  while(i < 20000i32) {
    sum = sum + convert<i64>(dereference(head).value)
    [Pointer<Node>] next{dereference(head).next}
    /std/intrinsics/memory/free(head)
    head = next
    i = i + 1i32
  }
  print_line(sum)
  [i32 mut] zeroed{0i32}
  i = 0i32
  while(i < 100000i32) {
    [mut] box{/std/intrinsics/memory/alloc<i32>(3i32)}
    zeroed = zeroed + dereference(plus(box, 32i32))
    assign(dereference(plus(box, 32i32)), 5i32)
    /std/intrinsics/memory/free(box)
    i = i + 1i32
  }
  print_line(zeroed)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_heap_pool.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_heap_pool_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_heap_pool_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "199990000\n0\n");
#if defined(__APPLE__)
  const uint64_t peak = nativePeakFootprint(exePath, outPath);
  CHECK(peak > 0);
  CHECK(peak < 16ull * 1024 * 1024);
#endif
}

TEST_CASE("native text blocks and 16 KiB blocks freed in a loop are reused") {
  // 30000 times: a String grows by two appends and is dropped, and a 1025-slot block (what a vector literal
  // allocates) is allocated and freed. Neither returns a mapping to the system each time: one page reclaim per
  // iteration would be 30000 or more, and the pooled heap needs a few hundred for the whole run.
  const std::string source = R"(
import /std/text/*
[unsafe return<int> effects(heap_alloc, io_out)]
main() {
  [i64 mut] i{0i64}
  [i64 mut] total{0i64}
  while(i < 30000i64) {
    [String mut] s{String{}}
    s.append("hello")
    s.append(" world, a text of more than one size class")
    total = total + convert<i64>(s.count())
    [mut] box{/std/intrinsics/memory/alloc<i64>(1024i32)}
    assign(dereference(plus(box, 16i32)), 3i64)
    total = total + dereference(plus(box, 16i32))
    /std/intrinsics/memory/free(box)
    i = i + 1i64
  }
  print_line(total)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_heap_reuse_loop.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_heap_reuse_loop_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_heap_reuse_loop_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "1500000\n");
#if defined(__APPLE__)
  const uint64_t reclaims = nativePageReclaims(exePath, outPath);
  CHECK(reclaims > 0);
  CHECK(reclaims < 3000);
#endif
}

TEST_CASE("native text capacities are the string heap's block sizes") {
  // The pooled heap rounds a block up to a size class, but capacity() stays the VM's block size less the NUL byte.
  const std::string source = R"(
import /std/text/*
[return<int> effects(io_out, heap_alloc)]
main() {
  [String mut] a{String{}}
  a.reserve(40i32)
  print_line(a.capacity())
  [String mut] b{String{}}
  b.reserve(100i32)
  b.append("xyz")
  print_line(b.capacity())
  print_line(b.count())
  [String mut] c{String{}}
  c.reserve(9000i32)
  print_line(c.capacity())
  [String mut] d{String{}}
  d.reserve(70000i32)
  print_line(d.capacity())
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_text_capacity.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_text_capacity_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_text_capacity_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "47\n111\n3\n10239\n70000\n");
}

TEST_CASE("native heap realloc keeps the leading slots across pooled and mapped blocks") {
  // 1 slot (pooled) grows to 3000 (a mapping of its own) and shrinks back to 2 (pooled): the leading values
  // move with the block, and slots past the old size start zeroed.
  const std::string source = R"(
[unsafe return<int> effects(heap_alloc, io_out)]
main() {
  [mut] block{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(block), 7i32)
  block = /std/intrinsics/memory/realloc(block, 3000i32)
  print_line(dereference(plus(block, 16i32)))
  assign(dereference(plus(block, 16i32)), 8i32)
  assign(dereference(plus(block, 47984i32)), 5i32)
  block = /std/intrinsics/memory/realloc(block, 2i32)
  print_line(dereference(block))
  print_line(dereference(plus(block, 16i32)))
  block = /std/intrinsics/memory/realloc(block, 4i32)
  print_line(dereference(plus(block, 48i32)))
  /std/intrinsics/memory/free(block)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_heap_realloc_pool.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_heap_realloc_pool_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_heap_realloc_pool_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "0\n7\n8\n0\n");
}

TEST_CASE("native heap faults on a double free and on a realloc of a freed block") {
  const std::string doubleFree = R"(
[unsafe return<int> effects(heap_alloc, io_out)]
main() {
  [mut] block{/std/intrinsics/memory/alloc<i32>(2i32)}
  /std/intrinsics/memory/free(block)
  /std/intrinsics/memory/free(block)
  print_line(9i32)
  return(0i32)
}
)";
  const std::string reallocFreed = R"(
[unsafe return<int> effects(heap_alloc, io_out)]
main() {
  [mut] block{/std/intrinsics/memory/alloc<i32>(2i32)}
  /std/intrinsics/memory/free(block)
  [mut] grown{/std/intrinsics/memory/realloc(block, 4i32)}
  print_line(9i32)
  return(0i32)
}
)";
  struct Case {
    std::string name;
    std::string source;
    std::string message;
  };
  for (const Case &item :
       {Case{"double_free", doubleFree, "invalid heap free address: the block is already free\n"},
        Case{"realloc_freed", reallocFreed, "invalid heap realloc address: the block is free\n"}}) {
    CAPTURE(item.name);
    const std::string srcPath =
        writeTemp("compile_native_heap_" + item.name + ".prime", item.source);
    const std::string exePath =
        (testScratchPath("") / ("primec_native_heap_" + item.name + "_exe")).string();
    const std::string outPath =
        (testScratchPath("") / ("primec_native_heap_" + item.name + "_out.txt")).string();
    const std::string errPath =
        (testScratchPath("") / ("primec_native_heap_" + item.name + "_err.txt")).string();
    const std::string compileCmd =
        "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    const int exitCode = runCommand(exePath + " > " + outPath + " 2> " + errPath);
    CHECK(exitCode != 0);
    CHECK(readFile(outPath).empty());
#if defined(__APPLE__)
    CHECK(exitCode == 3);
    CHECK(readFile(errPath) == item.message);
#endif
  }
}

TEST_SUITE_END();
#endif
