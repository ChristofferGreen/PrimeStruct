TEST_CASE("compiles and runs native templated stdlib return wrapper temporaries in expressions") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temporaries.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temporaries_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs native experimental soa_vector stdlib helpers") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(plus(values.count(), soaVectorCount<Particle>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_vector_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_soa_vector_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native rejects experimental soa_vector stdlib to_aos helper before vector struct-return support") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  [vector<Particle>] valuesAos{soaVectorToAos<Particle>(values)}
  return(vectorCount<Particle>(valuesAos))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_vector_to_aos_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_vector_to_aos_helpers_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "native backend does not support return type on /std/collections/experimental_soa_vector/soaVectorToAos__") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native templated stdlib wrapper temporary call forms") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_call_forms.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_call_forms_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native canonical namespaced map helpers on experimental map values") {
  expectCanonicalMapNamespaceExperimentalValueConformance("native");
}

TEST_CASE("compiles and runs native wrapper map helpers on experimental map values") {
  expectWrapperMapHelperExperimentalValueConformance("native");
}

TEST_CASE("compiles and runs native ownership-sensitive experimental map value methods") {
  expectExperimentalMapOwnershipMethodConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped inferred experimental map returns") {
  expectWrappedInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped experimental map parameters") {
  expectWrappedExperimentalMapParameterConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped experimental map bindings") {
  expectWrappedExperimentalMapBindingConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped experimental map assignment RHS values") {
  expectWrappedExperimentalMapAssignConformance("native");
}

TEST_CASE("compiles and runs native canonical namespaced map constructors on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalConstructorConformance("native");
}

TEST_CASE("compiles and runs native canonical namespaced map constructors through explicit experimental map returns") {
  expectCanonicalMapNamespaceExperimentalReturnConformance("native");
}

TEST_CASE("compiles and runs native canonical namespaced map constructors through explicit experimental map parameters") {
  expectCanonicalMapNamespaceExperimentalParameterConformance("native");
}

TEST_CASE("compiles and runs native wrapper map constructors on explicit experimental map bindings") {
  expectWrapperMapConstructorExperimentalBindingConformance("native");
}

TEST_CASE("compiles and runs native wrapper map constructors through explicit experimental map returns") {
  expectWrapperMapConstructorExperimentalReturnConformance("native");
}

TEST_CASE("compiles and runs native wrapper map constructors through explicit experimental map parameters") {
  expectWrapperMapConstructorExperimentalParameterConformance("native");
}

TEST_CASE("compiles and runs native experimental map variadic constructors") {
  expectExperimentalMapVariadicConstructorConformance("native");
}

TEST_CASE("rejects native experimental map variadic constructor type mismatch") {
  expectExperimentalMapVariadicConstructorMismatchReject("native");
}

TEST_CASE("compiles and runs native experimental map constructor assignments") {
  expectExperimentalMapAssignConformance("native");
}

TEST_CASE("compiles and runs native implicit map auto constructor inference") {
  expectImplicitMapAutoInferenceConformance("native");
}

TEST_CASE("compiles and runs native inferred experimental map returns") {
  expectInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("compiles and runs native block inferred experimental map returns") {
  expectBlockInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("compiles and runs native auto block inferred experimental map returns") {
  expectAutoBlockInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("compiles and runs native inferred experimental map call receivers") {
  expectInferredExperimentalMapCallReceiverConformance("native");
}

TEST_CASE("compiles and runs native experimental map struct fields") {
  expectExperimentalMapStructFieldConformance("native");
}

TEST_CASE("compiles and runs native inferred experimental map struct fields") {
  expectInferredExperimentalMapStructFieldConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped inferred experimental map struct fields") {
  expectWrappedInferredExperimentalMapStructFieldConformance("native");
}

TEST_CASE("compiles and runs native experimental map method parameters") {
  expectExperimentalMapMethodParameterConformance("native");
}

TEST_CASE("compiles and runs native inferred experimental map parameters") {
  expectInferredExperimentalMapParameterConformance("native");
}

TEST_CASE("compiles and runs native inferred experimental map default parameters") {
  expectInferredExperimentalMapDefaultParameterConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped inferred experimental map default parameters") {
  expectWrappedInferredExperimentalMapDefaultParameterConformance("native");
}

TEST_CASE("compiles and runs native experimental map helper receivers") {
  expectExperimentalMapHelperReceiverConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped experimental map helper receivers") {
  expectWrappedExperimentalMapHelperReceiverConformance("native");
}

TEST_CASE("compiles and runs native experimental map method receivers") {
  expectExperimentalMapMethodReceiverConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped experimental map method receivers") {
  expectWrappedExperimentalMapMethodReceiverConformance("native");
}

TEST_CASE("compiles and runs native experimental map field assignments") {
  expectExperimentalMapFieldAssignConformance("native");
}

TEST_CASE("compiles and runs native dereferenced experimental map storage references") {
  expectExperimentalMapStorageReferenceConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultFieldAssignConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped dereferenced Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultDerefFieldAssignConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageFieldConformance("native");
}

TEST_CASE("compiles and runs native helper-wrapped dereferenced experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageDerefFieldConformance("native");
}

TEST_CASE("rejects native canonical namespaced map helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("native");
}

TEST_CASE("compiles and runs native canonical namespaced map _ref helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("native");
}

TEST_CASE("compiles and runs native experimental map methods") {
  expectExperimentalMapMethodConformance("native");
}

TEST_CASE("compiles and runs native borrowed experimental map helpers") {
  expectExperimentalMapReferenceHelperConformance("native");
}

TEST_CASE("compiles and runs native borrowed experimental map methods") {
  expectExperimentalMapReferenceMethodConformance("native");
}

TEST_CASE("compiles and runs native experimental map inserts") {
  expectExperimentalMapInsertConformance("native");
}

TEST_CASE("compiles and runs native experimental map ownership-sensitive values") {
  expectExperimentalMapOwnershipConformance("native");
}

TEST_CASE("compiles and runs native canonical namespaced map inserts on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalInsertConformance("native");
}

TEST_CASE("compiles and runs native builtin canonical map first-growth inserts") {
  expectBuiltinCanonicalMapInsertFirstGrowthConformance("native");
}

TEST_CASE("compiles and runs native builtin canonical map repeated-growth inserts") {
  expectBuiltinCanonicalMapInsertRepeatedGrowthConformance("native");
}

TEST_CASE("compiles and runs native builtin canonical map insert overwrites") {
  expectBuiltinCanonicalMapInsertOverwriteConformance("native");
}

TEST_CASE("rejects native canonical map constructor ownership growth") {
  expectCanonicalMapNamespaceOwnershipReject("native");
}

TEST_CASE("compiles and runs native experimental map bracket access") {
  expectExperimentalMapIndexConformance("native");
}

TEST_CASE("compiles and runs native experimental map custom comparable struct keys") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<bool>]
/Key/less_than([Key] left, [Key] right) {
  return(less_than(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapPair<Key, i32>(Key(2i32), 7i32, Key(5i32), 11i32)}
  [i32 mut] total{mapCount<Key, i32>(values)}
  assign(total, plus(total, mapAt<Key, i32>(values, Key(2i32))))
  assign(total, plus(total, mapAtUnsafe<Key, i32>(values, Key(5i32))))
  if(mapContains<Key, i32>(values, Key(2i32)),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_map_custom_comparable_key.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_map_custom_comparable_key.exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 21);
}

TEST_CASE("compiles and runs native shared vector conformance harness for stdlib and experimental helpers") {
  expectSharedVectorConformanceHarness("native");
}

TEST_CASE("compiles and runs native canonical namespaced vector helpers") {
  expectCanonicalVectorNamespaceConformance("native");
}

TEST_CASE("compiles and runs native canonical namespaced vector helpers on explicit Vector bindings") {
  expectCanonicalVectorNamespaceExplicitVectorBindingConformance("native");
}

TEST_CASE("compiles and runs native stdlib wrapper vector helpers on explicit Vector bindings") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingConformance("native");
}

TEST_CASE("rejects native stdlib wrapper vector helper explicit Vector mismatch") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject("native");
}

TEST_CASE("compiles and runs native stdlib wrapper vector constructors on explicit Vector bindings") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance("native");
}

TEST_CASE("rejects native stdlib wrapper vector constructor explicit Vector mismatch") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchReject("native");
}

TEST_CASE("compiles and runs native stdlib wrapper vector constructors on inferred auto bindings") {
  expectStdlibWrapperVectorConstructorAutoInferenceConformance("native");
}

TEST_CASE("rejects native stdlib wrapper vector constructor auto inference mismatch") {
  expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject("native");
}

TEST_CASE("rejects native stdlib wrapper vector constructor receivers") {
  expectStdlibWrapperVectorConstructorReceiverConformance("native");
}

TEST_CASE("rejects native stdlib wrapper vector helper receiver mismatch") {
  expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject("native");
}

TEST_CASE("rejects native stdlib wrapper vector method receiver mismatch") {
  expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject("native");
}

TEST_CASE("rejects native canonical namespaced vector constructor temporaries") {
  expectCanonicalVectorNamespaceTemporaryReceiverConformance("native");
}

TEST_CASE("rejects native canonical namespaced vector explicit builtin bindings") {
  expectCanonicalVectorNamespaceExplicitBindingReject("native");
}

TEST_CASE("rejects native canonical namespaced vector named-argument temporaries") {
  expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance("native");
}

TEST_CASE("rejects native canonical namespaced vector named-argument explicit builtin bindings") {
  expectCanonicalVectorNamespaceNamedArgsExplicitBindingReject("native");
}

TEST_CASE("rejects native canonical namespaced vector mutators without imported helpers") {
  expectCanonicalVectorClearImportRequirement("native");
  expectCanonicalVectorRemoveAtImportRequirement("native");
  expectCanonicalVectorRemoveSwapImportRequirement("native");
}

TEST_CASE("compiles and runs native experimental vector helper runtime contracts") {
  expectExperimentalVectorRuntimeContracts("native");
}

TEST_CASE("compiles and runs native experimental vector ownership-sensitive helpers") {
  expectExperimentalVectorOwnershipContracts("native");
}

TEST_CASE("compiles and runs native canonical vector helpers on experimental vector receivers") {
  expectExperimentalVectorCanonicalHelperRoutingConformance("native");
}
TEST_CASE("compiles and runs native vector pop empty runtime contract") {
  SUBCASE("call") {
    expectVectorPopEmptyRuntimeContract("native", false);
  }

  SUBCASE("method") {
    expectVectorPopEmptyRuntimeContract("native", true);
  }
}

TEST_CASE("compiles and runs native vector index runtime contract") {
  expectVectorIndexRuntimeContract("native", "access_call");
  expectVectorIndexRuntimeContract("native", "access_method");
  expectVectorIndexRuntimeContract("native", "access_bracket");
  expectVectorIndexRuntimeContract("native", "remove_at_call");
  expectVectorIndexRuntimeContract("native", "remove_at_method");
  expectVectorIndexRuntimeContract("native", "remove_swap_call");
  expectVectorIndexRuntimeContract("native", "remove_swap_method");
}

TEST_CASE("compiles and runs native imported container error contract conformance") {
  expectContainerErrorConformance("native");
}

TEST_CASE("compiles and runs native checked pointer conformance harness for imported .prime helpers") {
  expectCheckedPointerHelperSurfaceConformance("native");
  expectCheckedPointerGrowthConformance("native");
  expectCheckedPointerOutOfBoundsConformance("native");
}

TEST_CASE("compiles and runs native templated stdlib vector wrapper temporary call forms") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_call_forms.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_vector_temp_call_forms_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native templated stdlib vector wrapper temporary methods in expressions") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_methods.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_vector_temp_methods_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native templated stdlib wrapper temporary index forms") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_index_forms.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_index_forms_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native templated stdlib wrapper temporary syntax parity") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_syntax_parity.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_syntax_parity_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("compiles and runs native templated stdlib wrapper temporary unsafe parity") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_parity.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_unsafe_parity_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 18);
}

TEST_CASE("compiles and runs native templated stdlib wrapper temporary count capacity parity") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_count_capacity_parity.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_count_capacity_parity_exe")
                                  .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}
