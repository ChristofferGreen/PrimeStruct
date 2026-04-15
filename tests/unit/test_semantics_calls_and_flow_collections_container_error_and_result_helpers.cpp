#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("container error contract shape validates through Result.why") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[return<ContainerError>]
containerMissingKey() {
  return(ContainerError(1i32))
}

[return<void>]
main() {
  [Result<ContainerError>] status{ containerMissingKey().code }
  [Result<ContainerError>] unknown{ 99i32 }
  [string] message{ Result.why(status) }
  [auto] fallback{ Result.why(unknown) }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("containerErrorResult helper validates for value-carrying container results") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

[return<ContainerError>]
containerMissingKey() {
  return(ContainerError(1i32))
}

[return<Result<T, ContainerError>>]
containerErrorResult<T>([ContainerError] err) {
  return(multiply(convert<i64>(err.code), 4294967296i64))
}

[return<Result<i32, ContainerError>>]
main() {
  return(containerErrorResult<i32>(containerMissingKey()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported i32 container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<i32, ContainerError>>]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 7i32)}
  return(mapTryAt<string, i32>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported bool container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<bool, ContainerError>>]
main() {
  [map<string, bool>] values{map<string, bool>("left"raw_utf8, true)}
  return(mapTryAt<string, bool>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported string container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<string, ContainerError>>]
main() {
  [map<string, string>] values{map<string, string>("left"raw_utf8, "alpha"utf8)}
  return(mapTryAt<string, string>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("bare vector count wrapper call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count wrapper call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("bare vector capacity wrapper call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector capacity wrapper call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("count helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count builtin validates on method calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method on vector binding accepts same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method on vector binding requires helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported count forms validate on builtin soa_vector binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [i32] direct{count(values)}
  [i32] method{values.count()}
  return(plus(direct, method))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa_vector count rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(/soa_vector/count(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa_vector count slash-method rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values./soa_vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("explicit soa_vector count forms reject non-soa target" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /soa_vector/count(values)
  values./soa_vector/count()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/count") != std::string::npos);
}

TEST_CASE("canonical soa_vector count helper validates through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  return(/std/collections/vector/count(/std/collections/soa_vector/to_aos<Particle>(holder.cloneValues())))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare count helper validates through experimental soa helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  return(count(holder.cloneValues()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates through experimental soa helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.cloneValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector count helper keeps same-path helper shadow through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[return<string>]
/soa_vector/count([SoaVector<Particle>] values) {
  return("shadow"utf8)
}

[return<string>]
main() {
  [Holder] holder{Holder()}
  return(holder.cloneValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector method count fallback keeps same-path helper shadow through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[return<string>]
/soa_vector/count([SoaVector<Particle>] values) {
  return("shadow"utf8)
}

[return<string>]
main() {
  [Holder] holder{Holder()}
  return(holder.cloneValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib helpers validate on builtin soa_vector binding") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(soaVectorCount<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector count helper validates on experimental wrapper bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(/std/collections/soa_vector/count<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector get helper validates on experimental wrapper bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(/std/collections/soa_vector/get<Particle>(values, 0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector get helper rejects template arguments on non-soa receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/soa_vector/get<i32>(values, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("get requires soa_vector target") != std::string::npos);
}

TEST_CASE("canonical soa_vector get slash-method validates on experimental wrapper") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  return(values./std/collections/soa_vector/get(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector to_aos slash-method validates on experimental wrapper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  [vector<Particle>] unpacked{values./std/collections/soa_vector/to_aos()}
  return(count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector ref helper validates on experimental wrapper bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(/std/collections/soa_vector/ref<Particle>(values, 0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector mutator helpers validate on experimental wrapper bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  /std/collections/soa_vector/reserve<Particle>(values, 2i32)
  /std/collections/soa_vector/push<Particle>(values, Particle(4i32))
  /std/collections/soa_vector/push<Particle>(values, Particle(9i32))
  return(plus(/std/collections/soa_vector/count<Particle>(values),
              /std/collections/soa_vector/get<Particle>(values, 1i32).x))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa_vector to_aos helper validates on experimental wrapper bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{/std/collections/soa_vector/to_aos<Particle>(values)}
  return(count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wildcard-imported canonical soa_vector helpers infer receiver-matched templates") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa_vector/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  reserve(values, 2i32)
  push(values, Particle(4i32))
  push(values, Particle(9i32))
  [Particle] first{get(values, 0i32)}
  [Reference<Particle>] second{ref(values, 1i32)}
  [vector<Particle>] unpacked{to_aos(values)}
  return(plus(plus(count(values), plus(first.x, second.x)), count(unpacked)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib helpers reject primitive element types") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[return<int>]
main() {
  [SoaVector<i32>] values{soaVectorNew<i32>()}
  return(soaVectorCount<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires struct type argument: i32") != std::string::npos);
}

TEST_CASE("experimental soa_vector stdlib helpers reject non-reflect struct element types") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(soaVectorCount<Particle>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires reflect-enabled struct type argument: /Particle") != std::string::npos);
}

TEST_CASE("experimental soa_vector stdlib non-empty helper validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(soaVectorCount<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib wide reflect-enabled structs validate on pending width boundary") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle17() {
  [i32] a0{0i32}
  [i32] a1{0i32}
  [i32] a2{0i32}
  [i32] a3{0i32}
  [i32] a4{0i32}
  [i32] a5{0i32}
  [i32] a6{0i32}
  [i32] a7{0i32}
  [i32] a8{0i32}
  [i32] a9{0i32}
  [i32] a10{0i32}
  [i32] a11{0i32}
  [i32] a12{0i32}
  [i32] a13{0i32}
  [i32] a14{0i32}
  [i32] a15{0i32}
  [i32] a16{0i32}
}

[return<int>]
main() {
  [SoaVector<Particle17>] values{soaVectorNew<Particle17>()}
  return(soaVectorCount<Particle17>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib from-aos helper validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle>] values{vector<Particle>(Particle(7i32))}
  [SoaVector<Particle>] packed{soaVectorFromAos<Particle>(values)}
  return(soaVectorCount<Particle>(packed))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib to-aos helper validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<vector<Particle>>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(soaVectorToAos<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib to-aos method validates on wrapper surface") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<vector<Particle>>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(values.to_aos())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("experimental soa_vector stdlib non-empty to-aos helper validates on wrapper state") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<vector<Particle>>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(soaVectorToAos<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib non-empty to-aos method validates on wrapper state") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<vector<Particle>>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(values.to_aos())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("experimental soa_vector borrowed parameter read-only methods validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
pick([Reference<SoaVector<Particle>>] borrowed) {
  [Particle] first{borrowed.get(0i32)}
  [Reference<Particle>] second{borrowed.ref(1i32)}
  [Particle] firstBare{get(borrowed, 1i32)}
  [Reference<Particle>] secondBare{ref(dereference(borrowed), 0i32)}
  [vector<Particle>] unpacked{borrowed.to_aos()}
  [vector<Particle>] unpackedBare{to_aos(borrowed)}
  [i32] countBare{count(borrowed)}
  return(plus(plus(first.x, second.x),
              plus(plus(firstBare.x, secondBare.x),
                   plus(count(unpacked),
                        plus(count(unpackedBare), countBare)))))
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(pick(borrowed))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector inline location read-only methods validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Particle] firstA{location(values).get(0i32)}
  [Reference<Particle>] secondA{location(values).ref(1i32)}
  [vector<Particle>] unpackedA{location(values).to_aos()}
  [i32] countA{location(values).count()}
  [Particle] firstB{dereference(location(values)).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(values)).ref(1i32)}
  [vector<Particle>] unpackedB{dereference(location(values)).to_aos()}
  [i32] countB{dereference(location(values)).count()}
  return(plus(plus(firstA.x, secondA.x),
              plus(count(unpackedA),
                   plus(countA,
                        plus(plus(firstB.x, secondB.x),
                             plus(count(unpackedB), countB))))))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector inline location borrowed helper-return helper surfaces validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Particle] firstA{location(pickBorrowed(location(values))).get(0i32)}
  [Reference<Particle>] secondA{location(pickBorrowed(location(values))).ref(1i32)}
  [Particle] firstC{get(location(pickBorrowed(location(values))), 1i32)}
  [Reference<Particle>] secondC{ref(location(pickBorrowed(location(values))), 0i32)}
  [vector<Particle>] unpackedA{location(pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(pickBorrowed(location(values)))).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(pickBorrowed(location(values)))).ref(1i32)}
  [Particle] firstD{get(dereference(location(pickBorrowed(location(values)))), 0i32)}
  [Reference<Particle>] secondD{ref(dereference(location(pickBorrowed(location(values)))), 1i32)}
  [vector<Particle>] unpackedB{dereference(location(pickBorrowed(location(values)))).to_aos()}
  [i32] countB{dereference(location(pickBorrowed(location(values)))).count()}
  return(plus(plus(firstA.x, secondA.x),
              plus(plus(firstC.x, secondC.y),
                   plus(count(unpackedA),
                        plus(countA,
                             plus(plus(firstB.x, secondB.x),
                                  plus(plus(firstD.x, secondD.y),
                                       plus(count(unpackedB), countB))))))))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector borrowed helper-return helper surfaces validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Particle] first{pickBorrowed(location(values)).get(0i32)}
  [Reference<Particle>] second{pickBorrowed(location(values)).ref(1i32)}
  [Particle] firstBare{get(pickBorrowed(location(values)), 1i32)}
  [Reference<Particle>] secondBare{ref(dereference(pickBorrowed(location(values))), 0i32)}
  [vector<Particle>] unpacked{pickBorrowed(location(values)).to_aos()}
  [vector<Particle>] unpackedBare{to_aos(pickBorrowed(location(values)))}
  [i32] countBare{count(pickBorrowed(location(values)))}
  return(plus(plus(first.x, second.x),
              plus(plus(firstBare.x, secondBare.x),
                   plus(count(unpacked),
                        plus(count(unpackedBare), countBare)))))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector method-like borrowed helper-return helper surfaces validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  [Particle] first{holder.pickBorrowed(location(values)).get(0i32)}
  [Reference<Particle>] second{holder.pickBorrowed(location(values)).ref(1i32)}
  [Particle] firstBare{get(holder.pickBorrowed(location(values)), 1i32)}
  [Reference<Particle>] secondBare{ref(dereference(holder.pickBorrowed(location(values))), 0i32)}
  [i32] countBare{count(holder.pickBorrowed(location(values)))}
  [i32] fieldBareGet{get(holder.pickBorrowed(location(values)), 1i32).y}
  [i32] fieldBareRef{ref(holder.pickBorrowed(location(values)), 0i32).x}
  [i32] fieldMethodRef{holder.pickBorrowed(location(values)).ref(1i32).y}
  [i32] fieldMethod{holder.pickBorrowed(location(values)).y()[1i32]}
  [i32] fieldCall{y(holder.pickBorrowed(location(values)))[0i32]}
  return(plus(plus(first.x, second.x),
              plus(plus(firstBare.x, secondBare.x),
                   plus(countBare,
                        plus(plus(plus(fieldBareGet, fieldBareRef), fieldMethodRef),
                             plus(fieldMethod, fieldCall))))))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector direct return method-like borrowed helper-return reads validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  return(
    plus(count(holder.pickBorrowed(location(values))),
         plus(count(holder.pickBorrowed(location(values)).to_aos()),
              plus(holder.pickBorrowed(location(values)).get(0i32).x,
                   plus(ref(holder.pickBorrowed(location(values)), 1i32).y,
                        plus(get(holder.pickBorrowed(location(values)), 1i32).y,
                             plus(holder.pickBorrowed(location(values)).y()[1i32],
                                  y(holder.pickBorrowed(location(values)))[0i32]))))))
  )
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector inline location method-like borrowed helper-return helper surfaces validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  [Particle] firstA{location(holder.pickBorrowed(location(values))).get(0i32)}
  [Reference<Particle>] secondA{location(holder.pickBorrowed(location(values))).ref(1i32)}
  [Particle] firstC{get(location(holder.pickBorrowed(location(values))), 1i32)}
  [Reference<Particle>] secondC{ref(location(holder.pickBorrowed(location(values))), 0i32)}
  [vector<Particle>] unpackedA{location(holder.pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(holder.pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(holder.pickBorrowed(location(values)))).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(holder.pickBorrowed(location(values)))).ref(1i32)}
  [Particle] firstD{get(dereference(location(holder.pickBorrowed(location(values)))), 0i32)}
  [Reference<Particle>] secondD{ref(dereference(location(holder.pickBorrowed(location(values)))), 1i32)}
  [vector<Particle>] unpackedB{dereference(location(holder.pickBorrowed(location(values)))).to_aos()}
  [i32] countB{dereference(location(holder.pickBorrowed(location(values)))).count()}
  [i32] fieldBareGet{get(location(holder.pickBorrowed(location(values))), 1i32).y}
  [i32] fieldBareRef{ref(dereference(location(holder.pickBorrowed(location(values)))), 0i32).x}
  [i32] fieldMethodRef{location(holder.pickBorrowed(location(values))).ref(1i32).y}
  [int] helpersA{plus(plus(firstA.x, secondA.x), plus(firstC.x, secondC.y))}
  [int] unpackedCountsA{plus(count(unpackedA), countA)}
  [int] helpersB{plus(plus(firstB.x, secondB.x), plus(firstD.x, secondD.y))}
  [int] unpackedCountsB{plus(count(unpackedB), countB)}
  [int] fieldTotals{
    plus(location(holder.pickBorrowed(location(values))).y()[0i32],
         plus(dereference(location(holder.pickBorrowed(location(values)))).y()[1i32],
              plus(y(location(holder.pickBorrowed(location(values))))[0i32],
                   y(dereference(location(holder.pickBorrowed(location(values)))))[1i32])))
  }
  [int] total{
    plus(helpersA,
         plus(unpackedCountsA,
              plus(helpersB,
                   plus(unpackedCountsB,
                        plus(plus(plus(fieldBareGet, fieldBareRef), fieldMethodRef), fieldTotals)))))
  }
  return(total)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector direct return inline location method-like borrowed helper-return reads validate on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  return(
    plus(location(holder.pickBorrowed(location(values))).count(),
         plus(count(location(holder.pickBorrowed(location(values))).to_aos()),
              plus(dereference(location(holder.pickBorrowed(location(values)))).get(1i32).x,
                   plus(ref(dereference(location(holder.pickBorrowed(location(values)))), 0i32).x,
                        plus(get(location(holder.pickBorrowed(location(values))), 1i32).y,
                             plus(location(holder.pickBorrowed(location(values))).y()[0i32],
                                  y(dereference(location(holder.pickBorrowed(location(values)))))[1i32]))))))
  )
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib get helper validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<Particle>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(soaVectorGet<Particle>(values, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib get method validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<Particle>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(values.get(0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib ref helper validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{soaVectorRef<Particle>(values, 0i32)}
  return(value.x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib ref method validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<Particle>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(dereference(values.ref(0i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib ref method preserves reference pass-through and return") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<Particle>>]
pass([Reference<Particle>] value) {
  return(value)
}

[return<Reference<Particle>>]
pick([SoaVector<Particle>] values) {
  return(pass(values.ref(0i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pick(values)}
  return(value.x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector standalone ref method expires before later push when unused") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{values.ref(0i32)}
  values.push(Particle(9i32))
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector standalone ref method blocks later push while live") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{values.ref(0i32)}
  values.push(Particle(9i32))
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa_vector standalone ref helper blocks later reserve while live") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{soaVectorRef<Particle>(values, 0i32)}
  soaVectorReserve<Particle>(values, 3i32)
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa_vector helper-return ref method expires before later push when unused") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<Particle>>]
pass([Reference<Particle>] value) {
  return(value)
}

[return<Reference<Particle>>]
pick([SoaVector<Particle>] values) {
  return(pass(values.ref(0i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pick(values)}
  values.push(Particle(9i32))
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector helper-return ref method blocks later push while live") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<Particle>>]
pass([Reference<Particle>] value) {
  return(value)
}

[return<Reference<Particle>>]
pick([SoaVector<Particle>] values) {
  return(pass(values.ref(0i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pick(values)}
  values.push(Particle(9i32))
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa_vector borrowed helper-return ref method blocks later reserve while live") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Reference<Particle>] value{pickBorrowed(location(values)).ref(1i32)}
  soaVectorReserve<Particle>(values, 3i32)
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa_vector inline location borrowed helper-return ref blocks later push while live") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Reference<Particle>] value{location(pickBorrowed(location(values))).ref(1i32)}
  values.push(Particle(11i32))
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa_vector stdlib push and reserve helpers validate on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  soaVectorReserve<Particle>(values, 2i32)
  soaVectorPush<Particle>(values, Particle(4i32))
  soaVectorPush<Particle>(values, Particle(9i32))
  return(soaVectorGet<Particle>(values, 1i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib push and reserve methods validate on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.reserve(2i32)
  values.push(Particle(4i32))
  values.push(Particle(9i32))
  return(values.get(1i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib single-field index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
ScalarBox() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<ScalarBox> mut] values{soaVectorNew<ScalarBox>()}
  values.push(ScalarBox(4i32))
  values.push(ScalarBox(9i32))
  return(values.x()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib field-view method reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(values.x())
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector borrowed local field-view method reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(borrowed.x())
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector borrowed local field-view call-form reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  x(borrowed)
  x(dereference(borrowed))
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector inline location field-view methods report pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  location(values).x()
  x(location(values))
  x(dereference(location(values)))
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector inline location borrowed helper-return field-view methods report pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  location(pickBorrowed(location(values))).x()
  x(location(pickBorrowed(location(values))))
  x(dereference(location(pickBorrowed(location(values)))))
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector borrowed helper-return field-view call-form reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  x(pickBorrowed(location(values)))
  x(dereference(pickBorrowed(location(values))))
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector method-like borrowed helper-return field-view methods report pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Holder] holder{Holder()}
  holder.pickBorrowed(location(values)).x()
  x(holder.pickBorrowed(location(values)))
  location(holder.pickBorrowed(location(values))).x()
  x(location(holder.pickBorrowed(location(values))))
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector mutating field-view index validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  assign(values.y()[1i32], 17i32)
  return(values.y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector mutating field-view call-form index validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  assign(y(values)[1i32], 17i32)
  return(y(values)[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector mutating field-view method reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  assign(values.x(), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector mutating field-view call-form reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  assign(x(values), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector mutating dereferenced borrowed helper-return field-view index validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  assign(dereference(pickBorrowed(location(values))).y()[1i32], 17i32)
  return(dereference(pickBorrowed(location(values))).y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector mutating dereferenced borrowed helper-return field-view method reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  assign(dereference(pickBorrowed(location(values))).x(), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector mutating inline location borrowed helper-return field-view indexes validate") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  assign(location(pickBorrowed(location(values))).y()[0i32], 17i32)
  assign(y(location(pickBorrowed(location(values))))[0i32], 17i32)
  return(plus(location(pickBorrowed(location(values))).y()[0i32],
              y(location(pickBorrowed(location(values))))[0i32]))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector mutating method-like borrowed helper-return field-view indexes validate") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  assign(holder.pickBorrowed(location(values)).y()[1i32], 17i32)
  assign(y(holder.pickBorrowed(location(values)))[0i32], 19i32)
  assign(location(holder.pickBorrowed(location(values))).y()[0i32], 23i32)
  assign(y(dereference(location(holder.pickBorrowed(location(values)))))[1i32], 29i32)
  return(
    plus(holder.pickBorrowed(location(values)).y()[0i32],
         plus(y(holder.pickBorrowed(location(values)))[1i32],
              plus(location(holder.pickBorrowed(location(values))).y()[0i32],
                   y(dereference(location(holder.pickBorrowed(location(values)))))[1i32])))
  )
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector mutating inline location borrowed helper-return field-view methods report pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  assign(location(pickBorrowed(location(values))).x(), 17i32)
  assign(dereference(location(pickBorrowed(location(values)))).x(), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector mutating method-like borrowed helper-return field-view methods report pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Holder] holder{Holder()}
  assign(holder.pickBorrowed(location(values)).x(), 17i32)
  assign(x(holder.pickBorrowed(location(values))), 17i32)
  assign(location(holder.pickBorrowed(location(values))).x(), 17i32)
  assign(x(location(holder.pickBorrowed(location(values)))), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/field_view/x") != std::string::npos);
}

TEST_CASE("experimental soa_vector mutating ref field access validates") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  assign(ref(values, 0i32).y, 19i32)
  assign(values.ref(1i32).y, 17i32)
  return(plus(ref(values, 0i32).y, values.ref(1i32).y))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector bare get and ref field access validates") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(plus(ref(values, 0i32).y, get(values, 1i32).y))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected multi-field index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(values.y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected call-form index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  [int] total{
    plus(
      y(values)[0i32],
      plus(
        y(dereference(borrowed))[1i32],
        plus(
          y(pickBorrowed(location(values)))[1i32],
          y(dereference(pickBorrowed(location(values))))[0i32]
        )
      )
    )
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected inline location borrow index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [int] total{
    plus(
      location(values).y()[0i32],
      plus(
        dereference(location(values)).y()[1i32],
        plus(
          y(location(values))[0i32],
          y(dereference(location(values)))[1i32]
        )
      )
    )
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected borrowed dereference index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(dereference(borrowed).y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected borrowed local index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(borrowed.y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected borrowed helper-return index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(pickBorrowed(location(values)).y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected dereferenced borrowed helper-return index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(dereference(pickBorrowed(location(values))).y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib direct return borrowed helper-return reads validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(
    plus(count(pickBorrowed(location(values))),
         plus(count(pickBorrowed(location(values)).to_aos()),
              plus(pickBorrowed(location(values)).get(0i32).x,
                   plus(ref(pickBorrowed(location(values)), 1i32).y,
                        plus(get(pickBorrowed(location(values)), 1i32).y,
                             plus(pickBorrowed(location(values)).y()[1i32],
                                  y(pickBorrowed(location(values)))[0i32]))))))
  )
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected method-like borrowed helper-return index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  [int] total{
    plus(
      holder.pickBorrowed(location(values)).y()[1i32],
      y(holder.pickBorrowed(location(values)))[0i32]
    )
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib reflected inline location borrowed helper-return index syntax validates") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [int] total{
    plus(
      location(pickBorrowed(location(values))).y()[0i32],
      plus(
        dereference(location(pickBorrowed(location(values)))).y()[1i32],
        plus(
          y(location(pickBorrowed(location(values))))[0i32],
          y(dereference(location(pickBorrowed(location(values)))))[1i32]
        )
      )
    )
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib direct return inline location borrowed helper-return reads validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(
    plus(location(pickBorrowed(location(values))).count(),
         plus(count(location(pickBorrowed(location(values))).to_aos()),
              plus(dereference(location(pickBorrowed(location(values)))).get(1i32).x,
                   plus(ref(dereference(location(pickBorrowed(location(values)))), 0i32).x,
                        plus(get(location(pickBorrowed(location(values))), 1i32).y,
                             plus(location(pickBorrowed(location(values))).y()[0i32],
                                  y(dereference(location(pickBorrowed(location(values)))))[1i32]))))))
  )
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnReserve<i32>(values, 3i32)
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  soaColumnWrite<i32>(values, 1i32, 7i32)
  soaColumnClear<i32>(values)
  return(soaColumnCount<i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<Mover> mut] values{soaColumnNew<Mover>()}
  soaColumnPush<Mover>(values, Mover(3i32))
  soaColumnWrite<Mover>(values, 0i32, Mover(8i32))
  soaColumnClear<Mover>(values)
  return(soaColumnCount<Mover>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa storage borrowed ref helper validates on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  [Reference<i32>] borrowed{soaColumnRef<i32>(values, 1i32)}
  return(plus(dereference(borrowed), soaColumnCount<i32>(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa storage borrowed view helper validates on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  [SoaColumn<i32>] view{soaColumnBorrowedView<i32>(values)}
  return(plus(soaColumnRead<i32>(view, 1i32), soaColumnCount<i32>(view)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa storage borrowed view helper validates shared writes") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  [SoaColumn<i32> mut] view{soaColumnBorrowedView<i32>(values)}
  soaColumnWrite<i32>(view, 1i32, 7i32)
  return(soaColumnRead<i32>(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa storage borrow-slot helper validates reference return") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[return<Reference<i32>>]
borrow_second([SoaColumn<i32>] values) {
  return(soaColumnBorrowSlot<i32>(values, 1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  return(dereference(borrow_second(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental vector borrow-slot helper validates reference return") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

[return<Reference<i32>>]
borrow_second([Vector<i32>] values) {
  return(vectorBorrowSlot<i32>(values, 1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{vectorNew<i32>()}
  vectorPush<i32>(values, 2i32)
  vectorPush<i32>(values, 5i32)
  return(dereference(borrow_second(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental two-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns2<i32, i32> mut] values{soaColumns2New<i32, i32>()}
  soaColumns2Reserve<i32, i32>(values, 3i32)
  soaColumns2Push<i32, i32>(values, 2i32, 5i32)
  soaColumns2Push<i32, i32>(values, 7i32, 11i32)
  soaColumns2Write<i32, i32>(values, 1i32, 13i32, 17i32)
  soaColumns2Clear<i32, i32>(values)
  return(soaColumns2Count<i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental two-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns2<Mover, i32> mut] values{soaColumns2New<Mover, i32>()}
  soaColumns2Push<Mover, i32>(values, Mover(3i32), 5i32)
  soaColumns2Write<Mover, i32>(values, 0i32, Mover(8i32), 9i32)
  soaColumns2Clear<Mover, i32>(values)
  return(soaColumns2Count<Mover, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental three-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns3<i32, i32, i32> mut] values{soaColumns3New<i32, i32, i32>()}
  soaColumns3Reserve<i32, i32, i32>(values, 4i32)
  soaColumns3Push<i32, i32, i32>(values, 2i32, 5i32, 7i32)
  soaColumns3Push<i32, i32, i32>(values, 11i32, 13i32, 17i32)
  soaColumns3Write<i32, i32, i32>(values, 1i32, 19i32, 23i32, 29i32)
  soaColumns3Clear<i32, i32, i32>(values)
  return(soaColumns3Count<i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental three-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns3<Mover, i32, i32> mut] values{soaColumns3New<Mover, i32, i32>()}
  soaColumns3Push<Mover, i32, i32>(values, Mover(3i32), 5i32, 7i32)
  soaColumns3Write<Mover, i32, i32>(values, 0i32, Mover(8i32), 9i32, 11i32)
  soaColumns3Clear<Mover, i32, i32>(values)
  return(soaColumns3Count<Mover, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental four-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns4<i32, i32, i32, i32> mut] values{soaColumns4New<i32, i32, i32, i32>()}
  soaColumns4Reserve<i32, i32, i32, i32>(values, 4i32)
  soaColumns4Push<i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32)
  soaColumns4Push<i32, i32, i32, i32>(values, 11i32, 13i32, 17i32, 19i32)
  soaColumns4Write<i32, i32, i32, i32>(values, 1i32, 23i32, 29i32, 31i32, 37i32)
  soaColumns4Clear<i32, i32, i32, i32>(values)
  return(soaColumns4Count<i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental four-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns4<Mover, i32, i32, i32> mut] values{soaColumns4New<Mover, i32, i32, i32>()}
  soaColumns4Push<Mover, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32)
  soaColumns4Write<Mover, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 13i32, 17i32)
  soaColumns4Clear<Mover, i32, i32, i32>(values)
  return(soaColumns4Count<Mover, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental five-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns5<i32, i32, i32, i32, i32> mut] values{soaColumns5New<i32, i32, i32, i32, i32>()}
  soaColumns5Reserve<i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns5Push<i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32)
  soaColumns5Push<i32, i32, i32, i32, i32>(values, 13i32, 17i32, 19i32, 23i32, 29i32)
  soaColumns5Write<i32, i32, i32, i32, i32>(values, 1i32, 31i32, 37i32, 41i32, 43i32, 47i32)
  soaColumns5Clear<i32, i32, i32, i32, i32>(values)
  return(soaColumns5Count<i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental five-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns5<Mover, i32, i32, i32, i32> mut] values{soaColumns5New<Mover, i32, i32, i32, i32>()}
  soaColumns5Push<Mover, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32)
  soaColumns5Write<Mover, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 17i32, 19i32, 23i32)
  soaColumns5Clear<Mover, i32, i32, i32, i32>(values)
  return(soaColumns5Count<Mover, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental six-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns6<i32, i32, i32, i32, i32, i32> mut] values{soaColumns6New<i32, i32, i32, i32, i32, i32>()}
  soaColumns6Reserve<i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns6Push<i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32)
  soaColumns6Push<i32, i32, i32, i32, i32, i32>(values, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32)
  soaColumns6Write<i32, i32, i32, i32, i32, i32>(values, 1i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32)
  soaColumns6Clear<i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns6Count<i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental six-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns6<Mover, i32, i32, i32, i32, i32> mut] values{soaColumns6New<Mover, i32, i32, i32, i32, i32>()}
  soaColumns6Push<Mover, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32)
  soaColumns6Write<Mover, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 19i32, 23i32, 29i32, 31i32)
  soaColumns6Clear<Mover, i32, i32, i32, i32, i32>(values)
  return(soaColumns6Count<Mover, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental seven-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns7<i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns7New<i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns7Reserve<i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns7Push<i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32)
  soaColumns7Push<i32, i32, i32, i32, i32, i32, i32>(values, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns7Write<i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns7Clear<i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns7Count<i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental seven-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns7<Mover, i32, i32, i32, i32, i32, i32> mut] values{soaColumns7New<Mover, i32, i32, i32, i32, i32, i32>()}
  soaColumns7Push<Mover, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32)
  soaColumns7Write<Mover, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 23i32, 29i32, 31i32, 37i32, 41i32)
  soaColumns7Clear<Mover, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns7Count<Mover, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental eight-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns8<i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns8New<i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns8Reserve<i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns8Push<i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32)
  soaColumns8Push<i32, i32, i32, i32, i32, i32, i32, i32>(values, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns8Write<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns8Clear<i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns8Count<i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental eight-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns8<Mover, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns8New<Mover, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns8Push<Mover, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32)
  soaColumns8Write<Mover, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32)
  soaColumns8Clear<Mover, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns8Count<Mover, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental nine-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns9<i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns9New<i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns9Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns9Push<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32)
  soaColumns9Push<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32)
  soaColumns9Write<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 6i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32)
  soaColumns9Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns9Count<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental nine-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns9<Mover, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns9New<Mover, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns9Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32)
  soaColumns9Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32)
  soaColumns9Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns9Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental ten-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns10<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns10New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns10Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns10Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32)
  soaColumns10Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32)
  soaColumns10Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 6i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32)
  soaColumns10Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns10Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental ten-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns10<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns10New<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns10Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32)
  soaColumns10Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32)
  soaColumns10Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns10Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental eleven-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns11<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns11New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns11Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns11Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32)
  soaColumns11Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32)
  soaColumns11Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 6i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32)
  soaColumns11Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns11Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental eleven-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns11<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns11New<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns11Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32)
  soaColumns11Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32)
  soaColumns11Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns11Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental twelve-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns12<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns12New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns12Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns12Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32)
  soaColumns12Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32)
  soaColumns12Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32)
  soaColumns12Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns12Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental twelve-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns12<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns12New<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns12Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32)
  soaColumns12Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32)
  soaColumns12Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns12Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental thirteen-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns13<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns13New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns13Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns13Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32)
  soaColumns13Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32)
  soaColumns13Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32)
  soaColumns13Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns13Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental thirteen-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns13<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns13New<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns13Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns13Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32)
  soaColumns13Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns13Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental fourteen-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns14<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns14New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns14Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns14Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns14Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32)
  soaColumns14Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32)
  soaColumns14Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns14Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental fourteen-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns14<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns14New<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns14Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32)
  soaColumns14Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32)
  soaColumns14Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns14Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental fifteen-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns15<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns15New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns15Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns15Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32)
  soaColumns15Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32, 113i32)
  soaColumns15Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32, 53i32)
  soaColumns15Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns15Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental fifteen-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns15<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns15New<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns15Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32)
  soaColumns15Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32)
  soaColumns15Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns15Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_CASE("experimental sixteen-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns16<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns16New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns16Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns16Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32)
  soaColumns16Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32, 113i32, 127i32, 131i32)
  soaColumns16Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32, 53i32, 137i32)
  soaColumns16Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns16Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental sixteen-column soa storage helpers validate ownership-sensitive elements") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

Mover() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns16<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns16New<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns16Push<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, Mover(3i32), 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32)
  soaColumns16Write<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32, Mover(8i32), 9i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32, 113i32, 127i32)
  soaColumns16Clear<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(soaColumns16Count<Mover, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.get(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported get forms validate on builtin soa_vector binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [Particle] direct{get(values, 0i32)}
  [Particle] method{values.get(0i32)}
  return(plus(direct.x, method.x))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa_vector get rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /soa_vector/get(values, 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa_vector get slash-method rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values./soa_vector/get(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/get") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface soa_vector get_ref rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /soa_vector/get_ref(location(values), 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa_vector get_ref slash-method rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  location(values)./soa_vector/get_ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/get_ref") !=
        std::string::npos);
}

TEST_CASE("get root forms reject vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  get(values, 0i32)
  values.get(0i32)
  /soa_vector/get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("get requires soa_vector target") != std::string::npos);
}

TEST_CASE("canonical get helper validates through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(/std/collections/soa_vector/get(holder.cloneValues(), 0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare get helper validates through experimental soa helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(get(holder.cloneValues(), 0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method validates through experimental soa helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.cloneValues().get(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method fallback keeps same-path helper shadow through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<Particle>]
/soa_vector/get([SoaVector<Particle>] values, [int] index) {
  return(Particle(7i32))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.cloneValues().get(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported ref local bindings reject builtin soa_vector persistence") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [Particle] direct{ref(values, 0i32)}
  [Particle] method{values.ref(0i32)}
  return(plus(direct.x, method.x))
}
)";
  std::string error;
  const bool valid = validateProgram(source, "/main", error);
  if (!valid) {
    CHECK(error.find("unknown method: /std/collections/soa_vector/ref") !=
          std::string::npos);
  } else {
    CHECK(error.empty());
  }
}

TEST_CASE("explicit old-surface soa_vector ref rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /soa_vector/ref(values, 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa_vector ref slash-method rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values./soa_vector/ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/ref") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface soa_vector ref_ref rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /soa_vector/ref_ref(values, 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa_vector ref_ref slash-method rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values./soa_vector/ref_ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/ref_ref") !=
        std::string::npos);
}

TEST_CASE("ref root forms reject vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  ref(values, 0i32)
  values.ref(0i32)
  /soa_vector/ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("canonical ref helper validates through struct helper return receivers" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  /std/collections/soa_vector/ref(holder.cloneValues(), 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare ref helper validates through experimental soa helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  ref(holder.cloneValues(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method validates through experimental soa helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  holder.cloneValues().ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method fallback keeps same-path helper shadow through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<Particle>]
/soa_vector/ref([SoaVector<Particle>] values, [int] index) {
  return(Particle(7i32))
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.cloneValues().ref(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method fallback keeps same-path helper shadow for auto inference through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/ref([SoaVector<Particle>] values, [int] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] item{holder.cloneValues().ref(0i32)}
  return(item)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref call fallback keeps same-path helper shadow for auto inference through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/ref([SoaVector<Particle>] values, [int] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] item{ref(holder.cloneValues(), 0i32)}
  return(item)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref call fallback keeps same-path helper shadow for direct returns through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/ref([SoaVector<Particle>] values, [int] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(ref(holder.cloneValues(), 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get call fallback keeps same-path helper shadow for auto inference through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/get([SoaVector<Particle>] values, [int] index) {
  return(9i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] item{get(holder.cloneValues(), 0i32)}
  return(item)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get call fallback keeps same-path helper shadow for direct returns through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/get([SoaVector<Particle>] values, [int] index) {
  return(9i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(get(holder.cloneValues(), 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call fallback keeps same-path helper shadow for auto inference through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/count([SoaVector<Particle>] values) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] item{count(holder.cloneValues())}
  return(item)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call fallback keeps same-path helper shadow for direct returns through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/count([SoaVector<Particle>] values) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(count(holder.cloneValues()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push helper shadow works for helper-return bare and method expressions") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [Particle] value{Particle(13i32)}
  return(plus(push(holder.cloneValues(), value), holder.cloneValues().push(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve helper shadow works for helper-return bare and method expressions") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa_vector/reserve([SoaVector<Particle>] values, [int] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(reserve(holder.cloneValues(), 7i32), holder.cloneValues().reserve(10i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push and reserve bare and method forms validate on experimental soa_vector bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  reserve(values, 2i32)
  push(values, Particle(4i32))
  values.reserve(3i32)
  values.push(Particle(9i32))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit soa_vector mutators validate on builtin soa_vector binding") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  /soa_vector/reserve(values, 4i32)
  /soa_vector/push(values, Particle(12i32))
  values./soa_vector/push(Particle(14i32))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported soa_vector mutators validate on builtin soa_vector binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  reserve(values, 4i32)
  push(values, Particle(12i32))
  values.reserve(6i32)
  values.push(Particle(14i32))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit soa_vector reserve keeps canonical reject on vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /soa_vector/reserve(values, 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/std/collections/soa_vector/reserve") != std::string::npos);
}

TEST_CASE("explicit soa_vector push slash-method keeps canonical reject on vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  values./soa_vector/push(12i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/std/collections/soa_vector/push") != std::string::npos);
}

TEST_CASE("push helper call-form falls back to user helper on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/push([soa_vector<Particle> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  push(values, Particle(4i32))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /push parameter value") != std::string::npos);
}

TEST_CASE("to_soa helper validates on vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa method validates on vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  values.to_soa()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit to_soa slash-method validates on vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  values./to_soa()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit to_aos direct call validates on soa_vector binding" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported to_aos root forms validate on soa_vector binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
  [vector<Particle>] unpackedC{values.to_aos()}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit to_aos slash-method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values./to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos validates on borrowed indexed soa_vector receiver") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
/score([args<Reference<soa_vector<Particle>>>] values) {
  return(count(to_aos(dereference(values[0i32]))))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa and to_aos helpers compose for explicit conversion") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(count(to_aos(to_soa(values))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("to_soa helper rejects non-vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("to_soa requires vector target") != std::string::npos);
}

TEST_CASE("to_soa helper call-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_soa([soa_vector<Particle>] values) {
  return(5i32)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(to_soa(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa helper method-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_soa([vector<Particle>] values) {
  return(5i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(values.to_soa())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos root forms keep canonical reject on vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_aos(values)
  /to_aos(values)
  values.to_aos()
  values./to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/std/collections/soa_vector/to_aos") != std::string::npos);
}

TEST_CASE("explicit old-surface to_aos rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /to_aos(values)
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface to_aos slash-method rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values./to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/to_aos") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface to_aos_ref rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /to_aos_ref(location(values))
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface to_aos_ref slash-method rejects without same-path helper" * doctest::skip(true)) {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  location(values)./to_aos_ref()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/to_aos_ref") !=
        std::string::npos);
}

TEST_CASE("to_aos method fallback validates through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorNew<Particle>())
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  [vector<Particle>] unpacked{holder.cloneValues().to_aos()}
  return(/std/collections/vector/count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper call-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([vector<Particle>] values) {
  return(6i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(to_aos(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos method fallback validates with same-path helper present on struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorNew<Particle>())
}

[return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>] shadowed{vector<Particle>()}
  return(shadowed)
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  [vector<Particle>] unpacked{holder.cloneValues().to_aos()}
  return(/std/collections/vector/count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos method fallback keeps same-path helper shadow for auto inference through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorNew<Particle>())
}

[return<int>]
/to_aos([SoaVector<Particle>] values) {
  return(7i32)
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] item{holder.cloneValues().to_aos()}
  return(item)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper method-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([soa_vector<Particle>] values) {
  return(6i32)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.to_aos())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper-return builtin soa_vector forms keep same-path helper shadow") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa_vector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa_vector<Particle>())
}

[return<int>]
/to_aos([soa_vector<Particle>] values) {
  return(9i32)
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] itemA{to_aos(holder.cloneValues())}
  [auto] itemB{/to_aos(holder.cloneValues())}
  [auto] itemC{holder.cloneValues().to_aos()}
  return(plus(plus(itemA, itemB), itemC))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin soa_vector global helper-return read helpers validate without same-path shadow") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa_vector<Particle>>]
cloneValues() {
  [soa_vector<Particle>, mut] values{soa_vector<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] countBare{count(cloneValues())}
  [i32] countMethod{cloneValues().count()}
  [i32] getBare{get(cloneValues(), 0i32).x}
  [i32] getMethod{cloneValues().get(0i32).x}
  [i32] refBare{ref(cloneValues(), 0i32).x}
  [i32] refMethod{cloneValues().ref(0i32).x}
  return(plus(countBare,
              plus(countMethod,
                   plus(getBare,
                        plus(getMethod,
                             plus(refBare, refMethod))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin soa_vector method-like helper-return read helpers validate without same-path shadow") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

Holder() {}

[effects(heap_alloc), return<soa_vector<Particle>>]
/Holder/cloneValues([Holder] self) {
  [soa_vector<Particle>, mut] values{soa_vector<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [i32] countBare{count(holder.cloneValues())}
  [i32] countMethod{holder.cloneValues().count()}
  [i32] getBare{get(holder.cloneValues(), 0i32).x}
  [i32] getMethod{holder.cloneValues().get(0i32).x}
  [i32] refBare{ref(holder.cloneValues(), 0i32).x}
  [i32] refMethod{holder.cloneValues().ref(0i32).x}
  return(plus(countBare,
              plus(countMethod,
                   plus(getBare,
                        plus(getMethod,
                             plus(refBare, refMethod))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("aos and soa containers do not implicitly convert") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/consumeSoa([soa_vector<Particle>] values) {
  return(0i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(consumeSoa(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/consumeSoa") != std::string::npos);
}

TEST_CASE("ecs style soa_vector update loop validates with deferred structural phase") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa_vector<Particle> mut] particles, [vector<Particle> mut] spawnQueue) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }

  [soa_vector<Particle>] stagedSpawns{to_soa(spawnQueue)}
  reserve(particles, plus(count(particles), count(stagedSpawns)))
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] particles{soa_vector<Particle>()}
  [vector<Particle> mut] spawnQueue{vector<Particle>()}
  simulateStep(particles, spawnQueue)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ecs style update loop still requires explicit aos to soa conversion") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa_vector<Particle> mut] particles) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle> mut] particles{vector<Particle>()}
  simulateStep(particles)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/simulateStep") != std::string::npos);
}

TEST_SUITE_END();
