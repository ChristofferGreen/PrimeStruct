#include "test_compile_run_helpers.h"
#include "test_compile_run_reflection_codegen_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.reflection_codegen");

TEST_CASE("reflection codegen helper runtime stays aligned across backends") {
  const std::string source = reflectionCodegenRuntimeSource();
  const std::string srcPath = writeTemp("compile_reflection_codegen_runtime.prime", source);
  const std::string expectedOut = "/Pair {\n  x\n  y\n}\n";

  const std::string vmOutPath = (testScratchPath("") / "primec_reflection_codegen_vm.out.txt").string();
  const std::string vmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(vmOutPath);
  CHECK(runCommand(vmCmd) == 7);
  CHECK(readFile(vmOutPath) == expectedOut);

  const std::string exePath = (testScratchPath("") / "primec_reflection_codegen_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  const std::string exeOutPath =
      (testScratchPath("") / "primec_reflection_codegen_exe.out.txt").string();
  const std::string exeRunCmd = quoteShellArg(exePath) + " > " + quoteShellArg(exeOutPath);
  CHECK(runCommand(exeRunCmd) == 7);
  CHECK(readFile(exeOutPath) == expectedOut);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_codegen_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  const std::string nativeOutPath =
      (testScratchPath("") / "primec_reflection_codegen_native.out.txt").string();
  const std::string nativeRunCmd = quoteShellArg(nativePath) + " > " + quoteShellArg(nativeOutPath);
  CHECK(runCommand(nativeRunCmd) == 7);
  CHECK(readFile(nativeOutPath) == expectedOut);
}

TEST_CASE("reflection compare helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<int>]
main() {
  [Pair] low{Pair([x] 1i32, [y] 2i32)}
  [Pair] mid{Pair([x] 1i32, [y] 5i32)}
  [Pair] high{Pair([x] 2i32, [y] 0i32)}
  [i32 mut] score{0i32}
  if(less_than(/Pair/Compare(low, mid), 0i32), then() { assign(score, plus(score, 1i32)) }, else() { })
  if(greater_than(/Pair/Compare(high, mid), 0i32), then() { assign(score, plus(score, 2i32)) }, else() { })
  if(equal(/Pair/Compare(low, low), 0i32), then() { assign(score, plus(score, 4i32)) }, else() { })
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_compare_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath = (testScratchPath("") / "primec_reflection_compare_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_compare_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_CASE("reflection hash64 helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Pair() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<int>]
main() {
  [Pair] first{Pair([x] 3i32, [y] 5i32)}
  [Pair] same{Pair([x] 3i32, [y] 5i32)}
  [Pair] swapped{Pair([x] 5i32, [y] 3i32)}
  [u64] firstHash{/Pair/Hash64(first)}
  [u64] sameHash{/Pair/Hash64(same)}
  [u64] swappedHash{/Pair/Hash64(swapped)}
  [bool] sameOk{equal(firstHash, sameHash)}
  [bool] orderOk{not_equal(firstHash, swappedHash)}
  return(if(and(sameOk, orderOk), then() { 7i32 }, else() { 3i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_hash64_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath = (testScratchPath("") / "primec_reflection_hash64_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_hash64_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_CASE("reflection clear helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Clear, Default, Compare)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair mut] value{Pair([x] 9i32, [y] 8i32)}
  /Pair/Clear(value)
  return(if(equal(/Pair/Compare(value, /Pair/Default()), 0i32), then() { 7i32 }, else() { 3i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_clear_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath = (testScratchPath("") / "primec_reflection_clear_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_clear_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_CASE("reflection validate helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Pair() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<int>]
main() {
  [Pair] value{Pair([x] 1i32, [y] 2i32)}
  return(if(Result.error(/Pair/Validate(value)), then() { 3i32 }, else() { 7i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_validate_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath = (testScratchPath("") / "primec_reflection_validate_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_validate_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_CASE("reflection serde helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Serialize, Deserialize, Equal)]
Pair() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<int>]
main() {
  [Pair] source{Pair([x] 4i32, [y] 9i32)}
  [array<u64>] encoded{/Pair/Serialize(source)}
  [Pair mut] decoded{Pair()}
  [bool] decodeFailed{Result.error(/Pair/Deserialize(decoded, encoded))}
  return(if(or(decodeFailed, not(/Pair/Equal(source, decoded))), then() { 3i32 }, else() { 7i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_serialize_deserialize.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath =
      (testScratchPath("") / "primec_reflection_serialize_deserialize_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath =
      (testScratchPath("") / "primec_reflection_serialize_deserialize_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_CASE("reflection SoaSchema helper runtime stays aligned across backends") {
  const std::string source = R"(
import /std/collections/internal_soa_storage/*

[struct reflect generate(SoaSchema)]
Pair() {
  [i32] x{0i32}
  [private bool] ok{true}
  [static i32] shared{9i32}
}

[return<int>]
main() {
  [i32 mut] score{0i32}
  [i32 mut] index{0i32}
  if(equal(/Pair/SoaSchemaFieldCount(), 2i32), then() { assign(score, plus(score, 1i32)) }, else() { })
  if(equal(/Pair/SoaSchemaFieldName(index).count(), 1i32), then() { assign(score, plus(score, 2i32)) }, else() { })
  if(equal(/Pair/SoaSchemaFieldOffset(index), 0i32), then() { assign(score, plus(score, 4i32)) }, else() { })
  increment(index)
  if(equal(/Pair/SoaSchemaFieldType(index).count(), 4i32), then() { assign(score, plus(score, 8i32)) }, else() { })
  if(equal(/Pair/SoaSchemaFieldVisibility(index).count(), 7i32),
     then() { assign(score, plus(score, 16i32)) },
     else() { })
  if(equal(/Pair/SoaSchemaFieldOffset(index), 4i32), then() { assign(score, plus(score, 32i32)) }, else() { })
  if(equal(/Pair/SoaSchemaElementStride(), 8i32), then() { assign(score, plus(score, 64i32)) }, else() { })
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_soa_schema_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 127);

  const std::string exePath = (testScratchPath("") / "primec_reflection_soa_schema_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 127);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_soa_schema_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 127);
}

TEST_CASE("reflection SoaSchema chunk helper runtime stays aligned across backends") {
  const std::string source = R"(
import /std/collections/internal_soa_storage/*

[struct reflect generate(SoaSchema)]
Wide() {
  [i32] f0{0i32}
  [i32] f1{0i32}
  [i32] f2{0i32}
  [i32] f3{0i32}
  [i32] f4{0i32}
  [i32] f5{0i32}
  [i32] f6{0i32}
  [i32] f7{0i32}
  [i32] f8{0i32}
  [i32] f9{0i32}
  [i32] f10{0i32}
  [i32] f11{0i32}
  [i32] f12{0i32}
  [i32] f13{0i32}
  [i32] f14{0i32}
  [i32] f15{0i32}
  [i32] f16{0i32}
}

[return<int>]
main() {
  [i32 mut] score{0i32}
  if(equal(/Wide/SoaSchemaChunkCount(), 2i32), then() { assign(score, plus(score, 1i32)) }, else() { })
  if(equal(/Wide/SoaSchemaChunkFieldStart(0i32), 0i32), then() { assign(score, plus(score, 2i32)) }, else() { })
  if(equal(/Wide/SoaSchemaChunkFieldCount(0i32), 16i32), then() { assign(score, plus(score, 4i32)) }, else() { })
  if(equal(/Wide/SoaSchemaChunkFieldStart(1i32), 16i32), then() { assign(score, plus(score, 8i32)) }, else() { })
  if(equal(/Wide/SoaSchemaChunkFieldCount(1i32), 1i32), then() { assign(score, plus(score, 16i32)) }, else() { })
  if(equal(/Wide/SoaSchemaChunkFieldCount(2i32), 0i32), then() { assign(score, plus(score, 32i32)) }, else() { })
  if(equal(/Wide/SoaSchemaElementStride(), 68i32), then() { assign(score, plus(score, 64i32)) }, else() { })
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_soa_schema_chunk_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 127);

  const std::string exePath = (testScratchPath("") / "primec_reflection_soa_schema_chunk_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 127);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_soa_schema_chunk_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 127);
}

TEST_CASE("reflection SoaSchema storage helper runtime stays aligned across backends") {
  const std::string source = R"(
import /std/collections/internal_soa_storage/*

[struct reflect generate(SoaSchema)]
Wide() {
  [i32] f0{0i32}
  [i32] f1{0i32}
  [i32] f2{0i32}
  [i32] f3{0i32}
  [i32] f4{0i32}
  [i32] f5{0i32}
  [i32] f6{0i32}
  [i32] f7{0i32}
  [i32] f8{0i32}
  [i32] f9{0i32}
  [i32] f10{0i32}
  [i32] f11{0i32}
  [i32] f12{0i32}
  [i32] f13{0i32}
  [i32] f14{0i32}
  [i32] f15{0i32}
  [i32] f16{0i32}
}

[return<int>]
main() {
  [/Wide/SoaSchemaStorage mut] storage{/Wide/SoaSchemaStorageNew()}
  [i32 mut] score{0i32}
  if(equal(/Wide/SoaSchemaStorageCount(storage), 0i32), then() { assign(score, plus(score, 1i32)) }, else() { })
  if(equal(/Wide/SoaSchemaStorageCapacity(storage), 0i32), then() { assign(score, plus(score, 2i32)) }, else() { })
  /Wide/SoaSchemaStorageReserve(storage, 5i32)
  if(equal(/Wide/SoaSchemaStorageCapacity(storage), 5i32), then() { assign(score, plus(score, 4i32)) }, else() { })
  if(equal(storage.chunk0.first.field_capacity(), 5i32), then() { assign(score, plus(score, 8i32)) }, else() { })
  if(equal(storage.chunk0.sixteenth.field_capacity(), 5i32), then() { assign(score, plus(score, 16i32)) }, else() { })
  if(equal(storage.chunk1.field_capacity(), 5i32), then() { assign(score, plus(score, 32i32)) }, else() { })
  /Wide/SoaSchemaStorageClear(storage)
  if(equal(/Wide/SoaSchemaStorageCount(storage), 0i32), then() { assign(score, plus(score, 64i32)) }, else() { })
  drop(storage)
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_soa_schema_storage_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 127);

  const std::string exePath = (testScratchPath("") / "primec_reflection_soa_schema_storage_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 127);

  const std::string nativePath = (testScratchPath("") / "primec_reflection_soa_schema_storage_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 127);
}

TEST_SUITE_END();
