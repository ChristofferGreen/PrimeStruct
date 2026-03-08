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

TEST_CASE("runs vm with templated stdlib collection return envelopes") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVector<i32>(9i32)}
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 4i32)}
  return(plus(plus(vectorCount<i32>(values), mapCount<string, i32>(pairs)), mapAt<string, i32>(pairs, "only"raw_utf8)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_returns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with templated stdlib return wrapper temporaries") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorTotal{wrapVector<i32>(9i32).count()}
  [i32] mapAtTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).at("only"raw_utf8)}
  [i32] mapUnsafeTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe("only"raw_utf8)}
  [i32] mapCountTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).count()}
  return(plus(plus(vectorTotal, mapAtTotal), plus(mapUnsafeTotal, mapCountTotal)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temporaries.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  [i32] a{mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] b{mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] c{mapCount<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32))}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with templated stdlib vector wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{vectorAt<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] b{vectorAtUnsafe<i32>(wrapVector<i32>(5i32), 0i32)}
  [i32] c{vectorCount<i32>(wrapVector<i32>(6i32))}
  [i32] d{vectorCapacity<i32>(wrapVector<i32>(7i32))}
  return(plus(plus(plus(a, b), c), d))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with templated stdlib vector wrapper temporary methods") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{wrapVector<i32>(4i32).at(0i32)}
  [i32] b{wrapVector<i32>(5i32).at_unsafe(0i32)}
  [i32] c{wrapVector<i32>(6i32).count()}
  [i32] d{wrapVector<i32>(7i32).capacity()}
  return(plus(plus(plus(a, b), c), d))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary index forms") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{wrapVector<i32>(4i32)[0i32]}
  [i32] b{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(a, b))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_index_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary syntax parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorCall{vectorAt<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at(0i32)}
  [i32] vectorIndex{wrapVector<i32>(4i32)[0i32]}
  [i32] mapCall{mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at("only"raw_utf8)}
  [i32] mapIndex{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(plus(plus(vectorCall, vectorMethod), vectorIndex), plus(plus(mapCall, mapMethod), mapIndex)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_syntax_parity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary unsafe parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorCall{vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at_unsafe(0i32)}
  [i32] mapCall{mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8)}
  return(plus(plus(vectorCall, vectorMethod), plus(mapCall, mapMethod)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 18);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary count capacity parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{mapCount<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32))}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).count()}
  [i32] vectorCountCall{vectorCount<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCountMethod{wrapVector<i32>(4i32).count()}
  [i32] vectorCapacityCall{vectorCapacity<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCapacityMethod{wrapVector<i32>(4i32).capacity()}
  return(plus(plus(plus(mapCall, mapMethod), plus(vectorCountCall, vectorCountMethod)),
              plus(vectorCapacityCall, vectorCapacityMethod)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_capacity_parity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with user wrapper temporary at_unsafe shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(at_unsafe(wrapMap(), 1i32), wrapMap().at_unsafe(1i32)),
              plus(at_unsafe(wrapVector(), 0i32), wrapVector().at_unsafe(0i32))))
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_at_unsafe_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 294);
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(at_unsafe(wrapMap(), true), wrapMap().at_unsafe(true)),
      plus(at_unsafe(wrapVector(), true), wrapVector().at_unsafe(true))))
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow value mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at_unsafe(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at_unsafe(1i32)}
  [i32] vectorCall{at_unsafe(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at_unsafe(0i32)}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow arity mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(at_unsafe(wrapMap(), 1i32, 2i32), wrapMap().at_unsafe(1i32, 2i32)),
      plus(at_unsafe(wrapVector(), 0i32, 1i32), wrapVector().at_unsafe(0i32, 1i32))))
}
)";
  const std::string srcPath =
      writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow missing arguments") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(at_unsafe(wrapMap()), wrapMap().at_unsafe()),
      plus(at_unsafe(wrapVector()), wrapVector().at_unsafe())))
}
)";
  const std::string srcPath =
      writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_missing_arguments.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user wrapper temporary at shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(75i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(76i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(at(wrapMap(), 1i32), wrapMap().at(1i32)),
              plus(at(wrapVector(), 0i32), wrapVector().at(0i32))))
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_at_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 302);
}

TEST_CASE("runs vm with user wrapper temporary count capacity shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(plus(count(wrapVector()), wrapVector().count()),
                   plus(capacity(wrapVector()), wrapVector().capacity()))))
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_count_capacity_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 468);
}

TEST_CASE("rejects vm user wrapper temporary count capacity shadow value mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/count([map<i32, i32>] values) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/vector/capacity([vector<i32>] values) {
  return(true)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{count(wrapMap())}
  [i32] mapMethod{wrapMap().count()}
  [i32] vectorCountCall{count(wrapVector())}
  [i32] vectorCountMethod{wrapVector().count()}
  [i32] vectorCapacityCall{capacity(wrapVector())}
  [i32] vectorCapacityMethod{wrapVector().capacity()}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_count_capacity_shadow_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user wrapper temporary index shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(wrapMap()[1i32], wrapVector()[0i32]))
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_index_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 163);
}

TEST_CASE("runs vm with user wrapper temporary syntax parity shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(83i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(84i32)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at(1i32)}
  [i32] mapIndex{wrapMap()[1i32]}
  [i32] vectorCall{at(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at(0i32)}
  [i32] vectorIndex{wrapVector()[0i32]}
  return(plus(plus(plus(mapCall, mapMethod), mapIndex), plus(plus(vectorCall, vectorMethod), vectorIndex)))
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_syntax_parity_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 501);
}

TEST_CASE("rejects vm user wrapper temporary syntax parity shadow mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(83i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(84i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(plus(at(wrapMap(), true), wrapMap().at(true)), wrapMap()[true]),
      plus(plus(at(wrapVector(), true), wrapVector().at(true)), wrapVector()[true])))
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_syntax_parity_shadow_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary syntax parity shadow value mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at(1i32)}
  [i32] mapIndex{wrapMap()[1i32]}
  [i32] vectorCall{at(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at(0i32)}
  [i32] vectorIndex{wrapVector()[0i32]}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_syntax_parity_shadow_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib collection return envelope unsupported arg") {
  const std::string source = R"(
import /std/collections/*

[return<vector<Unknown>>]
wrapUnknown([i32] value) {
  return(vectorSingle<i32>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapUnknown(3i32)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_bad_arg.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_templated_return_bad_arg_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unsupported return type on /wrapUnknown") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary index key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32)[1i32])
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_index_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary syntax parity key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), 1i32),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at(1i32)),
      wrapMap<string, i32>("only"raw_utf8, 5i32)[1i32]))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_syntax_parity_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary syntax parity value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  [bool] mapCall{mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [bool] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at("only"raw_utf8)}
  [bool] mapIndex{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  [bool] vectorCall{vectorAt<i32>(wrapVector<i32>(4i32), 0i32)}
  [bool] vectorMethod{wrapVector<i32>(4i32).at(0i32)}
  [bool] vectorIndex{wrapVector<i32>(4i32)[0i32]}
  return(plus(plus(plus(mapCall, mapMethod), mapIndex), plus(plus(vectorCall, vectorMethod), vectorIndex)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_syntax_parity_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), 1i32),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe(1i32)),
      plus(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), true), wrapVector<i32>(4i32).at_unsafe(true))))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  [bool] mapCall{mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [bool] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8)}
  [bool] vectorCall{vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32)}
  [bool] vectorMethod{wrapVector<i32>(4i32).at_unsafe(0i32)}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8, 1i32),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8, 1i32)),
      plus(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32, 1i32),
           wrapVector<i32>(4i32).at_unsafe(0i32, 1i32))))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity missing arguments") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32)),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe()),
      plus(vectorAtUnsafe<i32>(wrapVector<i32>(4i32)), wrapVector<i32>(4i32).at_unsafe())))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_missing_arguments.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary count capacity parity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapCount<i32, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32)),
           wrapMap<string, i32>("only"raw_utf8, 5i32).count(1i32)),
      plus(vectorCount<bool>(wrapVector<i32>(4i32)),
           plus(vectorCapacity<bool>(wrapVector<i32>(4i32)), wrapVector<i32>(4i32).capacity(1i32)))))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_capacity_parity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary index value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [bool] bad{wrapMap<string, i32>("only"raw_utf8, 4i32)["only"raw_utf8]}
  return(plus(0i32, bad))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_index_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, bool>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, bool>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapCount<i32, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapCount<string, bool>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapCount<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).count(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at("only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary method missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at())
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_method_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe("only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe method missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe())
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_method_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<bool>(wrapVector<i32>(4i32), 0i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<i32>(wrapVector<i32>(4i32), true))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_index_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<i32>(wrapVector<i32>(4i32), 0i32, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<i32>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_missing_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<bool>(wrapVector<i32>(4i32), 0i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), true))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_index_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<i32>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_missing_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCount<bool>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_count_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary count call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCount<i32>(wrapVector<i32>(4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_count_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary count method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).count(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_count_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at(0i32, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary method missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at())
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_method_missing_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at_unsafe(0i32, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe method missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at_unsafe())
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_method_missing_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary capacity type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCapacity<bool>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_capacity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary capacity call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCapacity<i32>(wrapVector<i32>(4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_capacity_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary capacity method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).capacity(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_capacity_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary method index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at(true))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_method_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32)[true])
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_index_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary index value mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [bool] bad{wrapVector<i32>(4i32)[0i32]}
  return(plus(0i32, bad))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_index_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe method index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at_unsafe(true))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_method_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map return envelope unsupported key arg") {
  const std::string source = R"(
import /std/collections/*

[return<map<Unknown, i32>>]
wrapMapUnknownKey([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMapUnknownKey("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_bad_key.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_map_bad_key_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unsupported return type on /wrapMapUnknownKey") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map return envelope unsupported value arg") {
  const std::string source = R"(
import /std/collections/*

[return<map<string, Unknown>>]
wrapMapUnknownValue([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMapUnknownValue("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_bad_value.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_map_bad_value_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unsupported return type on /wrapMapUnknownValue") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib vector return envelope nested arg") {
  const std::string source = R"(
import /std/collections/*

[return<vector<array<i32>>>]
wrapVectorArray([i32] value) {
  return(vectorSingle<i32>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVectorArray(3i32)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_vector_nested_arg.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_vector_nested_arg_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unsupported return type on /wrapVectorArray") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map return envelope nested arg") {
  const std::string source = R"(
import /std/collections/*

[return<map<string, array<i32>>>]
wrapMapNestedValue([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMapNestedValue("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_nested_arg.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_map_nested_arg_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unsupported return type on /wrapMapNestedValue") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib vector return envelope wrong arity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<i32, i32>>]
wrapVector([i32] value) {
  return(vectorSingle<i32>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVector(3i32)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_vector_arity.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_templated_return_vector_arity_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector return type requires exactly one template argument on /wrapVector") !=
        std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map return envelope wrong arity") {
  const std::string source = R"(
import /std/collections/*

[return<map<string>>]
wrapMap([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMap("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_arity.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_templated_return_map_arity_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map return type requires exactly two template arguments on /wrapMap") !=
        std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector single") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  return(plus(plus(vectorAt<i32>(values, 0i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_single.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("runs vm with stdlib collection shim vector new") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorNew<i32>()}
  return(plus(vectorCount<i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_new.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm stdlib collection shim vector new type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorNew<bool>()}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_new_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim vector single type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_single_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector pair") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorPair<i32>(11i32, 13i32)}
  return(plus(plus(vectorAt<i32>(values, 1i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pair.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 26);
}

TEST_CASE("rejects vm stdlib collection shim vector pair type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorPair<i32>(1i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pair_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector triple") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(10i32, 20i32, 30i32)}
  return(plus(plus(vectorAt<i32>(values, 2i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_triple.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 43);
}

TEST_CASE("rejects vm stdlib collection shim vector triple type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(1i32, 2i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_triple_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector quad") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(3i32, 6i32, 9i32, 12i32)}
  return(plus(plus(vectorAt<i32>(values, 3i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quad.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 18);
}

TEST_CASE("rejects vm stdlib collection shim vector quad type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(1i32, 2i32, 3i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quad_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map single") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapSingle<string, i32>("only"raw_utf8, 21i32)}
  return(plus(mapAt<string, i32>(values, "only"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 22);
}

TEST_CASE("rejects vm stdlib collection shim map single type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>(1i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map single key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>("oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map new") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<i32, i32>()}
  return(plus(mapCount<i32, i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with stdlib collection shim map new string key envelope") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapNew<string, i32>()}
  return(plus(mapCount<string, i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new_string.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm stdlib collection shim map new type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<bool, i32>()}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map new string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<string, i32>()}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new_string_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map count") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapCount<i32, i32>(values), 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("runs vm with stdlib collection shim map count string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapCount<string, i32>(values), 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("rejects vm stdlib collection shim map count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapCount<bool, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map count string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map at") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapAt<i32, i32>(values, 2i32), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 23);
}

TEST_CASE("runs vm with stdlib collection shim map at string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAt<string, i32>(values, "right"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("rejects vm stdlib collection shim map at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapAt<bool, i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map at string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapAt<string, i32>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map at unsafe") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapAtUnsafe<i32, i32>(values, 3i32), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 33);
}

TEST_CASE("runs vm with stdlib collection shim map at unsafe string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAtUnsafe<string, i32>(values, "left"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("rejects vm stdlib collection shim map at unsafe type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapAtUnsafe<bool, i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map at unsafe string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapAtUnsafe<string, i32>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map method access string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(values.at("right"raw_utf8), values.at_unsafe("left"raw_utf8)), values.count()))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_method_access_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("rejects vm stdlib collection shim map method at string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_at_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map method at unsafe string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_at_unsafe_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map method call parity string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] viaCall{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  [map<string, i32>] viaMethod{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(
      plus(mapAt<string, i32>(viaCall, "right"raw_utf8), viaMethod.at("right"raw_utf8)),
      plus(mapAtUnsafe<string, i32>(viaCall, "left"raw_utf8),
          plus(viaMethod.at_unsafe("left"raw_utf8), plus(mapCount<string, i32>(viaCall), viaMethod.count())))))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_method_call_parity_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 54);
}

TEST_CASE("rejects vm stdlib collection shim map method call parity key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAt<string, i32>(values, 1i32), values.at(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_call_parity_key_type_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map method call parity unsafe key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAtUnsafe<string, i32>(values, 1i32), values.at_unsafe(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_call_parity_unsafe_key_type_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map single standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapSingle<string, i32>("only"raw_utf8, 21i32)}
  return(plus(plus(mapAt<string, i32>(values, "only"raw_utf8), mapAtUnsafe<string, i32>(values, "only"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 43);
}

TEST_CASE("rejects vm stdlib collection shim map single standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>("oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map pair standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 7i32, 2i32, 10i32)}
  return(plus(plus(mapAt<i32, i32>(values, 2i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 19);
}

TEST_CASE("rejects vm stdlib collection shim map pair standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, 3i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map pair standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapPair<string, i32>("alpha"raw_utf8, 12i32, "beta"raw_utf8, 20i32)}
  return(plus(plus(mapAt<string, i32>(values, "beta"raw_utf8), mapAtUnsafe<string, i32>(values, "alpha"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE("rejects vm stdlib collection shim map pair standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, "oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map double standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("rejects vm stdlib collection shim map double standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapDouble<i32, i32>(1i32, 2i32, "oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map triple standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapTriple<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32)}
  return(plus(plus(mapAt<string, i32>(values, "c"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("rejects vm stdlib collection shim map triple standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 2i32, 3i32, 4i32, "oops"raw_utf8, 6i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map quad standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32)}
  return(plus(plus(mapAt<i32, i32>(values, 4i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 18);
}

TEST_CASE("runs vm with stdlib collection shim map quad standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuad<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32)}
  return(plus(plus(mapAt<string, i32>(values, "d"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm stdlib collection shim map quad standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map quad standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, "oops"raw_utf8, 8i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map quint standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32)}
  return(plus(plus(mapAt<i32, i32>(values, 5i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 21);
}

TEST_CASE("runs vm with stdlib collection shim map quint standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuint<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32)}
  return(plus(plus(mapAt<string, i32>(values, "e"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("rejects vm stdlib collection shim map quint standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map quint standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, "oops"raw_utf8, 10i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map sext standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32)}
  return(plus(plus(mapAt<i32, i32>(values, 6i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 26);
}

TEST_CASE("runs vm with stdlib collection shim map sext standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSext<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32,
        "f"raw_utf8, 6i32)}
  return(plus(plus(mapAt<string, i32>(values, "f"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("rejects vm stdlib collection shim map sext standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map sext standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, "oops"raw_utf8, 12i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map sept standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32, 7i32, 19i32)}
  return(plus(plus(mapAt<i32, i32>(values, 7i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 29);
}

TEST_CASE("runs vm with stdlib collection shim map sept standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSept<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32,
        "f"raw_utf8, 6i32, "g"raw_utf8, 7i32)}
  return(plus(plus(mapAt<string, i32>(values, "g"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("rejects vm stdlib collection shim map sept standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map sept standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, "oops"raw_utf8, 14i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map oct standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32, 7i32, 19i32, 8i32,
        23i32)}
  return(plus(plus(mapAt<i32, i32>(values, 8i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE("runs vm with stdlib collection shim map oct standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapOct<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32, "h"raw_utf8, 8i32)}
  return(plus(plus(mapAt<string, i32>(values, "h"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("rejects vm stdlib collection shim map oct standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, 14i32, 15i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map oct standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, 14i32, "oops"raw_utf8,
        16i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map double") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("rejects vm stdlib collection shim map double type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapDouble<i32, i32>(1i32, 2i32, 3i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map triple") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapTriple<string, i32>("left"raw_utf8, 5i32, "center"raw_utf8, 7i32, "right"raw_utf8, 9i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("rejects vm stdlib collection shim map triple type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
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

TEST_CASE("runs vm with stdlib collection shim vector quint constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuint<i32>(1i32, 3i32, 5i32, 7i32, 9i32)}
  [i32] picked{plus(vectorAt<i32>(values, 4i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quint_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("rejects vm stdlib collection shim vector quint type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuint<i32>(1i32, 2i32, 3i32, 4i32, true)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quint_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector sext constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSext<i32>(2i32, 4i32, 6i32, 8i32, 10i32, 12i32)}
  [i32] picked{plus(vectorAt<i32>(values, 5i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sext_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 20);
}

TEST_CASE("rejects vm stdlib collection shim vector sext type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSext<i32>(1i32, 2i32, 3i32, 4i32, 5i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sext_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector sept constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSept<i32>(3i32, 6i32, 9i32, 12i32, 15i32, 18i32, 21i32)}
  [i32] picked{plus(vectorAt<i32>(values, 6i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sept_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 31);
}

TEST_CASE("rejects vm stdlib collection shim vector sept type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSept<i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sept_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector oct constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorOct<i32>(4i32, 8i32, 12i32, 16i32, 20i32, 24i32, 28i32, 32i32)}
  [i32] picked{plus(vectorAt<i32>(values, 7i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_oct_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 44);
}

TEST_CASE("rejects vm stdlib collection shim vector oct type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorOct<i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_oct_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map pair string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapPair<string, i32>("alpha"raw_utf8, 12i32, "beta"raw_utf8, 20i32)}
  [string] key{"beta"raw_utf8}
  return(plus(plus(mapAt<string, i32>(values, key), mapAtUnsafe<string, i32>(values, "alpha"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE("rejects vm stdlib collection shim map pair type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, 3i32, true)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map quad") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuad<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32)}
  return(plus(plus(mapAt<string, i32>(values, "d"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm stdlib collection shim map quad type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map quint") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuint<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32)}
  return(plus(plus(mapAt<string, i32>(values, "e"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("rejects vm stdlib collection shim map quint type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map sext") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSext<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32)}
  return(plus(plus(mapAt<string, i32>(values, "f"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("rejects vm stdlib collection shim map sext type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map sept") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSept<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32)}
  return(plus(plus(mapAt<string, i32>(values, "g"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("rejects vm stdlib collection shim map sept type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map oct") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapOct<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32, "h"raw_utf8, 8i32)}
  return(plus(plus(mapAt<string, i32>(values, "h"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("rejects vm stdlib collection shim map oct type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, 14i32, 15i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_mismatch.prime", source);
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

TEST_CASE("runs vm with stdlib collection shim vector capacity") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorSingle<i32>(9i32)}
  vectorReserve<i32>(values, 5i32)
  return(plus(vectorCapacity<i32>(values), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_capacity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm stdlib collection shim vector capacity type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(9i32)}
  return(vectorCapacity<bool>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_capacity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector count") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(plus(vectorCount<i32>(values), 10i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("rejects vm stdlib collection shim vector count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(vectorCount<bool>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_count_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector at") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("rejects vm stdlib collection shim vector at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(vectorAt<bool>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector at unsafe") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(plus(vectorAtUnsafe<i32>(values, 2i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm stdlib collection shim vector at unsafe type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(vectorAtUnsafe<bool>(values, 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at_unsafe_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector push") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorPush<i32>(values, 5i32)
  vectorPush<i32>(values, 8i32)
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_push.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("rejects vm stdlib collection shim vector push type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorPush<bool>(values, true)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_push_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector pop") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorPop<i32>(values)
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pop.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm stdlib collection shim vector pop type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorPop<bool>(values)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pop_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector reserve") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorReserve<i32>(values, 6i32)
  return(plus(vectorCapacity<i32>(values), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_reserve.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm stdlib collection shim vector reserve type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorReserve<bool>(values, 6i32)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_reserve_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector clear") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorClear<i32>(values)
  return(plus(vectorCount<i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_clear.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm stdlib collection shim vector clear type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorClear<bool>(values)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_clear_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector remove at") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveAt<i32>(values, 1i32)
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("rejects vm stdlib collection shim vector remove at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveAt<bool>(values, 1i32)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_at_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector remove swap") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveSwap<i32>(values, 0i32)
  return(plus(vectorAt<i32>(values, 0i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_swap.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("rejects vm stdlib collection shim vector remove swap type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveSwap<bool>(values, 0i32)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_swap_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
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

TEST_CASE("grows vm vector reserve beyond initial capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  push(values, 9i32)
  return(plus(count(values), capacity(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_grows.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("grows vm vector push beyond initial capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(plus(count(values), capacity(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_push_grows.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm vector literal at local dynamic limit") {
  auto buildVectorLiteralArgs = [](int count) {
    std::string args;
    args.reserve(static_cast<size_t>(count) * 6);
    for (int i = 0; i < count; ++i) {
      if (i > 0) {
        args += ", ";
      }
      args += "1i32";
    }
    return args;
  };

  const std::string source = std::string(
      "[effects(heap_alloc), return<int>]\n"
      "main() {\n"
      "  [vector<i32> mut] values{vector<i32>(") +
                             buildVectorLiteralArgs(256) +
                             ")}\n"
                             "  return(convert<i32>(and(equal(count(values), 256i32), equal(capacity(values), "
                             "256i32))))\n"
                             "}\n";
  const std::string srcPath = writeTemp("vm_vector_literal_local_limit.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm vector literal above local dynamic limit") {
  auto buildVectorLiteralArgs = [](int count) {
    std::string args;
    args.reserve(static_cast<size_t>(count) * 6);
    for (int i = 0; i < count; ++i) {
      if (i > 0) {
        args += ", ";
      }
      args += "1i32";
    }
    return args;
  };

  const std::string source = std::string(
      "[effects(heap_alloc), return<int>]\n"
      "main() {\n"
      "  [vector<i32> mut] values{vector<i32>(") +
                             buildVectorLiteralArgs(257) +
                             ")}\n"
                             "  return(count(values))\n"
                             "}\n";
  const std::string srcPath = writeTemp("vm_vector_literal_local_limit_overflow.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_literal_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector literal exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve beyond local dynamic limit") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, 257i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_local_limit.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve negative literal at lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, -1i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_negative_literal.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_negative_literal_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded expression beyond local dynamic limit") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(200i32, 57i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_limit.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded negative expression at lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, minus(1i32, 2i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_negative.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded signed overflow at lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(9223372036854775807i64, 1i64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_signed_overflow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_signed_overflow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded unsigned expression beyond local dynamic limit") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(200u64, 57u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_unsigned_limit.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_unsigned_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded unsigned wraparound at lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, minus(1u64, 2u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_unsigned_wrap.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_unsigned_wrap_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve dynamic value beyond local dynamic limit") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main([array<string>] args) {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(257i32, count(args)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_dynamic_limit.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_dynamic_limit_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector reserve exceeds local capacity limit (256)\n");
}

TEST_CASE("rejects vm vector push beyond local dynamic limit") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  repeat(257i32) {
    push(values, 1i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_push_local_limit.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_push_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector local capacity limit exceeded (256)\n");
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

TEST_CASE("runs vm with collection syntax parity for call and method forms") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] viaCall{vector<i32>(10i32, 20i32, 30i32)}
  [vector<i32> mut] viaMethod{vector<i32>(10i32, 20i32, 30i32)}
  pop(viaCall)
  viaMethod.pop()
  reserve(viaCall, 3i32)
  viaMethod.reserve(3i32)
  push(viaCall, 40i32)
  viaMethod.push(40i32)
  return(plus(
      plus(at(viaCall, 2i32), viaMethod.at(2i32)),
      plus(viaCall[2i32], plus(viaMethod[2i32], plus(count(viaCall), viaMethod.count())))))
}
)";
  const std::string srcPath = writeTemp("vm_collection_syntax_parity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 166);
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
