TEST_CASE("runs vm with numeric array literals") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  print_line(values.count())
  print_line(values[1i64])
  print_line(values[2u64])
  return(values[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_array_literals.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_literals_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n7\n9\n");
}

TEST_CASE("runs vm with numeric vector literals") {
  const std::string source = R"(
[return<int> effects(io_out, heap_alloc)]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  print_line(values.count())
  print_line(values[1i64])
  print_line(values[2u64])
  return(values[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_vector_literals.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_literals_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n7\n9\n");
}

TEST_CASE("runs vm with collection bracket literals") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32>] list{vector<i32>[3i32, 4i32]}
  [map<i32, i32>] table{map<i32, i32>[5i32=6i32]}
  return(plus(plus(values.count(), list.count()), count(table)))
}
)";
  const std::string srcPath = writeTemp("vm_collection_brackets.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector literal count method") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(vector<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector method call") {
  const std::string source = R"(
[return<int>]
/vector/first([vector<i32>] items) {
  return(items[0i32])
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(values.first())
}
)";
  const std::string srcPath = writeTemp("vm_vector_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with vector literal unsafe access") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(vector<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with map at method helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(values.at(3i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with map at_unsafe method helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at_unsafe_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_array_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_vector_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with stdlib collection shim helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  [map<i32, i32>] pairs{mapSingle<i32, i32>(3i32, 9i32)}
  [i32 mut] total{plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))}
  [vector<i32>] emptyValues{vectorNew<i32>()}
  [map<i32, i32>] emptyPairs{mapNew<i32, i32>()}
  assign(total, plus(total, plus(vectorCount<i32>(emptyValues), mapCount<i32, i32>(emptyPairs))))
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shims.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim multi constructors") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(5i32, 7i32, 9i32)}
  [map<i32, i32>] pairs{mapDouble<i32, i32>(1i32, 11i32, 2i32, 22i32)}
  [i32] vectorTotal{plus(vectorAt<i32>(values, 0i32), vectorAt<i32>(values, 2i32))}
  [i32] mapTotal{plus(mapAt<i32, i32>(pairs, 1i32), mapAtUnsafe<i32, i32>(pairs, 2i32))}
  return(plus(plus(vectorTotal, mapTotal), plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_multi_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 52);
}

TEST_CASE("runs vm with stdlib collection shim extended constructors") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(2i32, 4i32, 6i32, 8i32)}
  [map<i32, i32>] pairs{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  [i32] vectorTotal{plus(vectorAt<i32>(values, 1i32), vectorAtUnsafe<i32>(values, 3i32))}
  [i32] mapTotal{plus(mapAt<i32, i32>(pairs, 1i32), mapAtUnsafe<i32, i32>(pairs, 3i32))}
  return(plus(plus(vectorTotal, mapTotal), plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_extended_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 59);
}

TEST_CASE("rejects vm stdlib collection shim extended constructor type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(1i32, 2i32, true, 4i32)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_extended_ctor_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim access helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  [map<i32, i32>] pairs{mapSingle<i32, i32>(3i32, 9i32)}
  [i32] a{vectorAt<i32>(values, 0i32)}
  [i32] b{vectorAtUnsafe<i32>(values, 0i32)}
  [i32] c{mapAt<i32, i32>(pairs, 3i32)}
  [i32] d{mapAtUnsafe<i32, i32>(pairs, 3i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_access.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 32);
}

TEST_CASE("runs vm with stdlib collection shim capacity helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorSingle<i32>(7i32)}
  reserve(values, 4i32)
  return(vectorCapacity<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_capacity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with stdlib collection shim vector mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorReserve<i32>(values, 4i32)
  vectorPush<i32>(values, 11i32)
  vectorPush<i32>(values, 22i32)
  vectorPush<i32>(values, 33i32)
  [i32 mut] snapshot{plus(vectorCount<i32>(values), vectorCapacity<i32>(values))}
  vectorPop<i32>(values)
  vectorRemoveAt<i32>(values, 0i32)
  vectorPush<i32>(values, 44i32)
  vectorRemoveSwap<i32>(values, 0i32)
  assign(snapshot, plus(snapshot, vectorCount<i32>(values)))
  vectorClear<i32>(values)
  return(plus(snapshot, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_mutators.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with vector capacity helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  [i32] a{capacity(values)}
  return(plus(a, values.capacity()))
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with vector capacity after pop") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_after_pop.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with user array count method shadow") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(99i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_user_array_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 99);
}

TEST_CASE("runs vm with user array count call shadow") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(98i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_count_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 98);
}

TEST_CASE("runs vm with user map count call shadow") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(96i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_count_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 96);
}

TEST_CASE("runs vm with user map count method shadow") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(93i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_user_map_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 93);
}

TEST_CASE("runs vm with user string count call shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(94i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(count(text))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_count_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 94);
}

TEST_CASE("runs vm with user string count method shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(95i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.count())
}
)";
  const std::string srcPath = writeTemp("vm_user_string_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 95);
}

TEST_CASE("runs vm with user vector count method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 97);
}

TEST_CASE("runs vm with user vector capacity method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_capacity_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 77);
}

TEST_CASE("runs vm with user vector count call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_count_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 97);
}

TEST_CASE("runs vm with user vector capacity call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_capacity_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 77);
}

TEST_CASE("runs vm with user array capacity call shadow") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(66i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_capacity_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 66);
}

TEST_CASE("runs vm with user array capacity method shadow") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(65i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("vm_user_array_capacity_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 65);
}

TEST_CASE("runs vm with user array at call shadow") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(61i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 61);
}

TEST_CASE("runs vm with user array at method shadow") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(63i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 63);
}

TEST_CASE("runs vm with user array at_unsafe call shadow") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(85i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_at_unsafe_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 85);
}

TEST_CASE("runs vm with user array at_unsafe method shadow") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(86i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 86);
}

TEST_CASE("runs vm with user map at_unsafe call shadow") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(62i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_unsafe_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 62);
}

TEST_CASE("runs vm with user map at_unsafe method shadow") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(64i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 64);
}

TEST_CASE("runs vm with user map at call shadow") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(67i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 67);
}

TEST_CASE("runs vm with user map at method shadow") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(65i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 65);
}

TEST_CASE("runs vm with user vector at call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(68i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 68);
}

TEST_CASE("runs vm with user vector at method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(69i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 69);
}

TEST_CASE("runs vm with user string at_unsafe call shadow") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(71i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at_unsafe(text, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_unsafe_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 71);
}

TEST_CASE("runs vm with user string at_unsafe method shadow") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(72i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 72);
}

TEST_CASE("runs vm with user vector at_unsafe call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_unsafe_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 81);
}

TEST_CASE("runs vm with user vector at_unsafe method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 82);
}

TEST_CASE("runs vm with user string at call shadow") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(83i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at(text, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 83);
}

TEST_CASE("runs vm with user string at method shadow") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(84i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 84);
}

TEST_CASE("runs vm with vector push helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  reserve(values, 3i32)
  push(values, 9i32)
  return(plus(values[2i32], capacity(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_push.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("runs vm with vector mutator method calls") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values.pop()
  values.reserve(3i32)
  values.push(9i32)
  values.remove_at(1i32)
  values.remove_swap(0i32)
  values.clear()
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_vector_mutator_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("runs vm with user push helper shadow") {
  const std::string source = R"(
[return<int>]
push([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  push(1i32, 2i32)
  return(push(4i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_push_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with user vector constructor shadow") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(vector([value] 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_constructor_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with user array constructor shadow") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(array([value] 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_constructor_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with user map constructor shadow") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  return(map([key] 4i32, [value] 6i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_constructor_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("rejects vm builtin vector constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(vector([value] 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_builtin_vector_constructor_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_builtin_vector_constructor_named_args_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects vm builtin array constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(array([value] 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_builtin_array_constructor_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_builtin_array_constructor_named_args_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects vm builtin map constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(map([key] 1i32, [value] 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_builtin_map_constructor_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_builtin_map_constructor_named_args_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("runs vm with user map constructor block shadow") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  map(4i32, 6i32) {
    assign(result, 7i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("vm_user_map_constructor_block_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with user vector constructor block shadow") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  vector(9i32) {
    assign(result, 4i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_constructor_block_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with user array constructor block shadow") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  array(9i32) {
    assign(result, 5i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("vm_user_array_constructor_block_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with user vector push call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_push_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector push method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.push(3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_push_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector pop call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  pop(values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_pop_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector pop method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.pop()
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_pop_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector reserve call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_reserve_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector reserve method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.reserve(3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_reserve_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector clear call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  clear(values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_clear_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector clear method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.clear()
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_clear_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_at call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_at(values, 1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_at method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_at(1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_swap call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_swap_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_swap method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_swap(1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_swap_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm vector reserve beyond capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_unsupported.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector reserve exceeds capacity\n");
}

TEST_CASE("rejects vm vector push beyond capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_push_full.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_push_full_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector capacity exceeded\n");
}

TEST_CASE("runs vm with vector shrink helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  pop(values)
  remove_at(values, 1i32)
  remove_swap(values, 0i32)
  last{at(values, 0i32)}
  size{count(values)}
  clear(values)
  return(plus(plus(last, size), count(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_shrink_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with vector literal count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(count(vector<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}
