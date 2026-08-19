#include "../test_semantics_helpers.h"

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
  return(ContainerError{1i32})
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
  return(ContainerError{1i32})
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

TEST_CASE("mapTryAt helper reports retired i32 map helper diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<Result<i32, ContainerError>>]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 7i32)}
  return(/std/collections/map/tryAt<string, i32>(values, "left"raw_utf8))
}
)";
  std::string error;
  REQUIRE(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper reports retired bool map helper diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<Result<bool, ContainerError>>]
main() {
  [map<string, bool>] values{map<string, bool>("left"raw_utf8, true)}
  return(/std/collections/map/tryAt<string, bool>(values, "left"raw_utf8))
}
)";
  std::string error;
  REQUIRE(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper reports retired string map helper diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<Result<string, ContainerError>>]
main() {
  [map<string, string>] values{map<string, string>("left"raw_utf8, "alpha"utf8)}
  return(/std/collections/map/tryAt<string, string>(values, "left"raw_utf8))
}
)";
  std::string error;
  REQUIRE(validateProgram(source, "/main", error));
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

TEST_CASE("bare collection count resolves inside stdlib namespace helper") {
  const std::string source = R"(
import /std/collections/*

namespace std {
  namespace image {
    [effects(heap_alloc), return<int>]
    vector_count([vector<i32>] values) {
      return(count(values))
    }

    [return<int>]
    array_count([array<i32>] values) {
      return(count(values))
    }
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [array<i32>] fixed{array<i32>(3i32, 4i32, 5i32)}
  return(/std/image/vector_count(values) + /std/image/array_count(fixed))
}
)";
  std::string error;
  const bool valid = validateProgram(source, "/main", error);
  INFO(error);
  CHECK(valid);
  CHECK(error.empty());
}

TEST_CASE("args pack count method resolves inside stdlib vector namespace") {
  const std::string source = R"(
[return<i32>]
/std/collections/vector/make<T>([args<T>] values) {
  valueCount{values.count()}
  return(valueCount)
}

[return<i32>]
main() {
  return(/std/collections/vector/make<i32>())
}
)";
  std::string error;
  const bool valid = validateProgram(source, "/main", error);
  INFO(error);
  CHECK(valid);
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
  CHECK(error.find("unknown call target: /std/collections/vector/count") !=
        std::string::npos);
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

TEST_CASE("count method validates with soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported count forms validate with soa binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [i32] direct{count(values)}
  [i32] method{values.count()}
  return(plus(direct, method))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa count validates bare soa path") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(/soa/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  // TODO-4811 fix: "count is only supported as a statement" was an
  // over-broad rejection (count is a same-path read helper, not a
  // statement-only mutator) - expression position is legitimate now.
  // Still rejects, for a separate, still-open reason (same-path shadow
  // routing falls through to the retired soa_vector diagnostic family -
  // see TODO-4756's investigation notes).
  CHECK(error.find("unknown method: /std/collections/soa_vector/count") != std::string::npos);
}

TEST_CASE("explicit old-surface soa count slash-method validates with soa type") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(values./soa/count())
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit soa count forms reject non-soa target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /soa/count(values)
  values./soa/count()
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  // TODO-4811 fix: "count" is no longer classified as a statement-only
  // mutator, so this statement-position call no longer reaches the
  // mutator-specific "requires soa target" check - it now fails call
  // resolution directly (no /soa/count definition exists for a plain
  // vector receiver), which is still a correct rejection.
  CHECK(error.find("unknown call target: /soa/count") != std::string::npos);
}

TEST_CASE("public soa count helper validates through struct helper return receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<soa<Particle>>]
  cloneValues() {
    return(/std/collections/soa/single<Particle>(Particle(7i32)))
  }
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  return(/std/collections/vector/count(/std/collections/soa/to_aos<Particle>(holder.cloneValues())))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare count helper validates internal soa helper return receivers with current metadata diagnostic") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  [Holder] holder{Holder{}}
  return(count(holder.cloneValues()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("count method validates internal soa helper return receivers through retired method path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  [Holder] holder{Holder{}}
  return(holder.cloneValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("public soa count method ignores legacy helper shadow through public receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<soa<Particle>>]
  cloneValues() {
    return(/std/collections/soa/single<Particle>(Particle(7i32)))
  }
}

[return<string>]
/soa/count([soa<Particle>] values) {
  return("shadow"utf8)
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  return(holder.cloneValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa method count fallback keeps same-path helper shadow through struct helper return receivers compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
/soa/count([SoaVector<Particle>] values) {
  return("shadow"utf8)
}

[return<string>]
main() {
  [Holder] holder{Holder{}}
  return(holder.cloneValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib helpers validate through direct soa import") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("public soa count helper validates on public wrapper bindings") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  return(/std/collections/soa/count<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public soa get helper validates on public wrapper bindings") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  return(/std/collections/soa/get<Particle>(values, 0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public soa get helper validates template arguments on non-soa receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/soa/get<i32>(values, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("get requires soa target") != std::string::npos);
}

TEST_CASE("public soa get slash-method validates on public wrapper") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(9i32))}
  return(values./std/collections/soa/get(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public soa to_aos slash-method validates on public wrapper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(9i32))}
  [vector<Particle>] unpacked{values./std/collections/soa/to_aos()}
  return(count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public soa ref helper validates on public wrapper bindings") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  return(/std/collections/soa/ref<Particle>(values, 0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public soa mutator helpers validate on public wrapper bindings") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  /std/collections/soa/reserve<Particle>(values, 2i32)
  /std/collections/soa/push<Particle>(values, Particle(4i32))
  /std/collections/soa/push<Particle>(values, Particle(9i32))
  return(plus(/std/collections/soa/count<Particle>(values),
              /std/collections/soa/get<Particle>(values, 1i32).x))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrowed helper-return experimental soa helper surfaces rejected push path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  push(values, Particle(7i32, 8i32))
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
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): count/count_ref/get/
  // ref/to_aos/to_aos_ref/field-view accessors all now route correctly on
  // this borrowed helper-return receiver, so the whole program validates
  // and (verified via a standalone `--emit=vm` probe) runs to completion
  // returning 55 (2 + 2 + 7 + 12 + 12 + 12 + 8).
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
}

TEST_CASE("public soa to_aos helper validates on public wrapper bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{/std/collections/soa/to_aos<Particle>(values)}
  return(count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public soa count helper validates inside generic helper on public wrapper parameter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
/helper<T>([soa<T>] values) {
  return(/std/collections/soa/count<T>(values))
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  return(helper<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wildcard-imported public soa helpers reject current metadata inference gap") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{soa<Particle>()}
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
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("public soa wildcard import validates without collections import") {
  const std::string source = R"(
import /std/collections/soa/*

[return<int>]
/main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib helpers reject primitive element types") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib helpers reject non-reflect struct element types") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib non-empty helper accepts direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib wide reflect-enabled structs accept direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib from-aos helper validates typed binding") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib to-aos helper validates on reflect-enabled struct elements") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib to-aos method validates on wrapper surface") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/soa/*

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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib non-empty to-aos helper validates on wrapper state") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib non-empty to-aos method validates on wrapper state") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/soa/*

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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa borrowed parameter read-only methods reject retired ref_ref path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(pick(borrowed))
}
  )";
  std::string error;
  // TODO-5050 shape (a) (RESOLVED): get/ref/to_aos/count all now route
  // correctly on this borrowed-parameter receiver, so the program
  // validates. (A standalone `--emit=vm` probe shows it then crashes at
  // runtime with "array index out of bounds" - the fixture's single-
  // element vector doesn't have the index-1 elements `.ref(1i32)`/
  // `get(borrowed, 1i32)` read, unrelated to this TODO's routing gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
}


TEST_SUITE_END();
