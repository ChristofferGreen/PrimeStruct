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
  CHECK(error.find("count is only supported as a statement") != std::string::npos);
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
  CHECK(error.find("count requires soa target") != std::string::npos);
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /std/collections/soa") !=
        std::string::npos);
}

TEST_CASE("experimental soa inline location read-only methods reject rooted helper path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /soa/get") != std::string::npos);
}

TEST_CASE("experimental soa inline location borrowed helper-return helper surfaces reject current ref_ref path") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /std/collections/soa") !=
        std::string::npos);
}

TEST_CASE("experimental soa borrowed helper-return helper surfaces reject current ref_ref path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa alias-only borrowed helper-return helpers reject current ref_ref path") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Particle] first{get(pickBorrowed(location(values)), 1i32)}
  [Reference<Particle>] second{ref(pickBorrowed(location(values)), 0i32)}
  [i32] countBare{count(pickBorrowed(location(values)))}
  return(plus(plus(first.x, second.x), countBare))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa method-like borrowed helper-return helper surfaces reject current ref_ref path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
  [Holder] holder{Holder{}}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /std/collections/soa") !=
        std::string::npos);
}

TEST_CASE("experimental soa direct return method-like borrowed helper-return reads reject retired helper path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
  [Holder] holder{Holder{}}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa direct helper shadows validate without duplicate reserve diagnostics") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(SoaVector<Particle>{})
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(count)
}

[return<Particle>]
/shadow/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/shadow/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/shadow/soa/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/shadow/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/shadow/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<Particle>]
/shadow/soa/SoaVector__Particle/get([SoaVector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/shadow/soa/SoaVector__Particle/ref([SoaVector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/shadow/soa/SoaVector__Particle/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<void>]
/shadow/soa/SoaVector__Particle/push([SoaVector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/shadow/soa/SoaVector__Particle/reserve([SoaVector<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<int>]
main() {
  [Particle] picked{/soa/get(cloneValues(), 1i32)}
  [Particle] pickedRef{/soa/ref(cloneValues(), 1i32)}
  [vector<Particle>] unpacked{/to_aos(cloneValues())}
  [i32] pushed{/soa/push(cloneValues(), Particle(7i32))}
  [i32] reserved{/soa/reserve(cloneValues(), 4i32)}
  return(plus(plus(picked.x, pickedRef.x), plus(pushed, reserved)))
}
  )";
  std::string error;
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa inline location method-like borrowed helper-return helper surfaces reject current ref_ref path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
  [Holder] holder{Holder{}}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("experimental soa direct return inline location method-like borrowed") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
  [Holder] holder{Holder{}}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib get helper accepts direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib get method reports current helper path") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /soa/get") != std::string::npos);
}

TEST_CASE("experimental soa stdlib ref helper accepts direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib ref method reports current dereference diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("dereference requires a pointer or reference") != std::string::npos);
}

TEST_CASE("experimental soa stdlib ref method pass-through reports current helper path") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /soa/ref") != std::string::npos);
}

TEST_CASE("experimental soa standalone ref method reports current binding diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("experimental soa standalone ref method push conflict reports current binding diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  INFO(error);
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("experimental soa standalone ref helper reserve conflict accepts direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa helper-return ref method validates direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /soa/ref") !=
        std::string::npos);
}

TEST_CASE("experimental soa helper-return ref method push conflict validates direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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
  INFO(error);
  CHECK(error.find("unknown method: /soa/ref") !=
        std::string::npos);
}

TEST_CASE("experimental soa borrowed helper-return ref method validates direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pickBorrowed(location(values)).ref(1i32)}
  soaVectorReserve<Particle>(values, 3i32)
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /std/collections/soa") !=
        std::string::npos);
}

TEST_CASE("experimental soa inline location borrowed helper-return ref validates direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

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
  [SoaVector<Particle> mut] values{
      soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{location(pickBorrowed(location(values))).ref(1i32)}
  values.push(Particle(11i32))
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /std/collections/soa") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib push and reserve helpers validate through soa import") {
  const std::string source = R"(
import /std/collections/soa/*

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

TEST_CASE("experimental soa stdlib push and reserve methods reject retired method paths") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /soa/reserve") != std::string::npos);
}

TEST_CASE("experimental soa stdlib single-field index syntax reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
ScalarBox() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<ScalarBox> mut] values{soaVectorNew<ScalarBox>()}
  push(values, ScalarBox(4i32))
  push(values, ScalarBox(9i32))
  return(values.x()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:ScalarBox") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib field-view method reports escape diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK(error.find("field-view escapes via return") != std::string::npos);
}

TEST_CASE("experimental soa borrowed local field-view method reports escape diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK(error.find("field-view escapes via return") != std::string::npos);
}

TEST_CASE("experimental soa borrowed local field-view call-form validates") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa inline location field-view methods report field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  push(values, Particle(7i32, 8i32))
  location(values).x()
  x(location(values))
  x(dereference(location(values)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa inline location borrowed helper-return field-view methods report field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  push(values, Particle(7i32))
  location(pickBorrowed(location(values))).x()
  x(location(pickBorrowed(location(values))))
  x(dereference(location(pickBorrowed(location(values)))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa borrowed helper-return field-view call-form validates") {
  const std::string source = R"(
import /std/collections/soa/*

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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa method-like borrowed helper-return field-view methods validate") {
  const std::string source = R"(
import /std/collections/soa/*

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
  [Holder] holder{Holder{}}
  holder.pickBorrowed(location(values)).x()
  x(holder.pickBorrowed(location(values)))
  location(holder.pickBorrowed(location(values))).x()
  x(location(holder.pickBorrowed(location(values))))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa mutating field-view index reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  assign(values.y()[1i32], 17i32)
  return(values.y()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa mutating field-view call-form index reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  assign(y(values)[1i32], 17i32)
  return(y(values)[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa mutating field-view method reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
}

TEST_CASE("experimental soa mutating field-view call-form reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
}

TEST_CASE("experimental soa mutating dereferenced borrowed helper-return field-view index reports field_count diagnostic") {
  const std::string source = R"(
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
  push(values, Particle(9i32, 12i32))
  assign(dereference(pickBorrowed(location(values))).y()[1i32], 17i32)
  return(dereference(pickBorrowed(location(values))).y()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa mutating dereferenced borrowed helper-return field-view method reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
}

TEST_CASE("experimental soa mutating inline location borrowed helper-return field-view indexes validate") {
  const std::string source = R"(
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
  assign(location(pickBorrowed(location(values))).y()[0i32], 17i32)
  assign(y(location(pickBorrowed(location(values))))[0i32], 17i32)
  return(plus(location(pickBorrowed(location(values))).y()[0i32],
              y(location(pickBorrowed(location(values))))[0i32]))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa mutating method-like borrowed helper-return field-view indexes validate") {
  const std::string source = R"(
import /std/collections/soa/*

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
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa mutating inline location borrowed helper-return field-view methods report pending diagnostic") {
  const std::string source = R"(
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
  assign(location(pickBorrowed(location(values))).x(), 17i32)
  assign(dereference(location(pickBorrowed(location(values)))).x(), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
}

TEST_CASE("experimental soa mutating method-like borrowed helper-return field-view methods report pending diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  [Holder] holder{Holder{}}
  assign(holder.pickBorrowed(location(values)).x(), 17i32)
  assign(x(holder.pickBorrowed(location(values))), 17i32)
  assign(location(holder.pickBorrowed(location(values))).x(), 17i32)
  assign(x(location(holder.pickBorrowed(location(values)))), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa mutating ref field access validates internal metadata validation") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa bare get and ref field access validates internal metadata validation") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected multi-field index syntax reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  return(values.y()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected call-form index syntax reports field_count diagnostic") {
  const std::string source = R"(
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
  push(values, Particle(9i32, 12i32))
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected inline location borrow index syntax reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected borrowed dereference index syntax reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(dereference(borrowed).y()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected borrowed local index syntax reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(borrowed.y()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected borrowed helper-return index syntax reports field_count diagnostic") {
  const std::string source = R"(
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
  push(values, Particle(9i32, 12i32))
  return(pickBorrowed(location(values)).y()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected dereferenced borrowed helper-return index syntax reports field_count diagnostic") {
  const std::string source = R"(
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
  push(values, Particle(9i32, 12i32))
  return(dereference(pickBorrowed(location(values))).y()[1i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib direct return borrowed helper-return reads reject retired count ref") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa stdlib reflected method-like borrowed helper-return index syntax reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
  [Holder] holder{Holder{}}
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
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib reflected inline location borrowed helper-return") {
  const std::string source = R"(
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib direct return inline location borrowed helper-return reads reject retired count ref") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/vector/*

[return<Reference<i32>>]
borrow_second([Vector<i32>] values) {
  return(vectorBorrowSlot<i32>(values, 1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>()}
  /std/collections/vector/push<i32>(values, 2i32)
  /std/collections/vector/push<i32>(values, 5i32)
  return(dereference(borrow_second(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental two-column soa storage helpers validate on explicit column bindings") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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
import /std/collections/soa_storage/*

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

TEST_CASE("get helper validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values.get(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported get forms validate with soa binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [Particle] direct{get(values, 0i32)}
  [Particle] method{values.get(0i32)}
  return(plus(direct.x, method.x))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa get validates retired binding spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  /soa/get(values, 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface soa get slash-method validates retired binding spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values./soa/get(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface soa get_ref validates retired soa binding spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  /soa/get_ref(location(values), 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface soa get_ref slash-method validates retired soa binding spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  location(values)./soa/get_ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("get root forms reject vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  get(values, 0i32)
  values.get(0i32)
  /soa/get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("get requires soa target") != std::string::npos);
}

TEST_CASE("canonical get helper reports metadata limitation through struct helper return receivers") {
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

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(/std/collections/soa/get(holder.cloneValues(), 0i32).x)
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("bare get helper through experimental soa helper return receivers validates internal metadata validation") {
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
    return(soaVectorNew<Particle>())
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(get(holder.cloneValues(), 0i32).x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument") !=
        std::string::npos);
}

TEST_CASE("get method validates through experimental soa helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(holder.cloneValues().get(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method fallback keeps same-path helper shadow through struct helper return receivers compatibility") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [int] index) {
  return(Particle(7i32))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(holder.cloneValues().get(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref helper validates with soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported ref local bindings reject retired builtin soa binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [Particle] direct{ref(values, 0i32)}
  [Particle] method{values.ref(0i32)}
  return(plus(direct.x, method.x))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface soa ref slash-method validates retired soa binding spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values./soa/ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface soa ref_ref validates retired soa binding spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  /soa/ref_ref(values, 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface soa ref_ref slash-method validates retired soa binding spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values./soa/ref_ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("ref root forms reject vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  ref(values, 0i32)
  values.ref(0i32)
  /soa/ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("canonical ref helper through struct helper return receivers validates retired path") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  /std/collections/soa/ref(holder.cloneValues(), 0i32)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("bare ref helper through experimental soa helper return receivers validates internal metadata validation") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  ref(holder.cloneValues(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("ref method validates through experimental soa helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  holder.cloneValues().ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /soa/ref") != std::string::npos);
}

TEST_CASE("ref method fallback keeps same-path helper shadow through struct helper return receivers compatibility") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [int] index) {
  return(Particle(7i32))
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  return(holder.cloneValues().ref(0i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method fallback ignores retired same-path helper shadow for auto inference") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/ref([SoaVector<Particle>] values, [int] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [auto] item{holder.cloneValues().ref(0i32)}
  return(item)
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("ref call fallback auto inference validates internal metadata validation through struct helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/ref([SoaVector<Particle>] values, [int] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [auto] item{ref(holder.cloneValues(), 0i32)}
  return(item)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("ref call fallback direct returns reject internal metadata validation through struct helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/ref([SoaVector<Particle>] values, [int] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(ref(holder.cloneValues(), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("ref_ref keeps same-path helper shadow through borrowed soa vector returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<Reference<SoaVector<Particle>>>]
  borrowValues([Reference<SoaVector<Particle>>] values) {
    return(values)
  }
}

[return<i32>]
/soa/ref_ref([Reference<SoaVector<Particle>>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Holder] holder{Holder{}}
  [i32] direct{ref_ref(holder.borrowValues(location(values)), 0i32)}
  return(direct)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get call fallback auto inference validates internal metadata validation through struct helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/get([SoaVector<Particle>] values, [int] index) {
  return(9i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [auto] item{get(holder.cloneValues(), 0i32)}
  return(item)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("get call fallback direct returns reject internal metadata validation through struct helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/get([SoaVector<Particle>] values, [int] index) {
  return(9i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(get(holder.cloneValues(), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("count call fallback auto inference validates internal metadata validation through struct helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/count([SoaVector<Particle>] values) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [auto] item{count(holder.cloneValues())}
  return(item)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("count call fallback direct returns reject internal metadata validation through struct helper return receivers") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/count([SoaVector<Particle>] values) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(count(holder.cloneValues()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("push helper shadow through helper-return expressions validates internal metadata validation") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [Particle] value{Particle(13i32)}
  return(plus(push(holder.cloneValues(), value), holder.cloneValues().push(value)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("reserve helper shadow through helper-return expressions validates internal metadata validation") {
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
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
/soa/reserve([SoaVector<Particle>] values, [int] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  return(plus(reserve(holder.cloneValues(), 7i32), holder.cloneValues().reserve(10i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("explicit same-path soa helper shadows work for helper-return direct calls compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 10i32)))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 20i32)))
}

[effects(heap_alloc), return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>, mut] out{vector<Particle>()}
  out.push(Particle(5i32))
  return(out)
}

[return<int>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<int>]
/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [Particle] picked{/soa/get(holder.cloneValues(), 1i32)}
  [Particle] pickedRef{/soa/ref(holder.cloneValues(), 2i32)}
  [vector<Particle>] unpacked{/to_aos(holder.cloneValues())}
  return(plus(picked.x,
              plus(pickedRef.x,
                   plus(count(unpacked),
                        plus(/soa/push(holder.cloneValues(), Particle(13i32)),
                             /soa/reserve(holder.cloneValues(), 7i32))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("method-like canonical soa helper shadows reject duplicate definitions") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 10i32)))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 20i32)))
}

[effects(heap_alloc), return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>, mut] out{vector<Particle>()}
  out.push(Particle(5i32))
  return(out)
}

[return<int>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<int>]
/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(count)
}

[return<Particle>]
/std/collections/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[effects(heap_alloc), return<vector<Particle>>]
/std/collections/soa/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<int>]
/std/collections/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<int>]
/std/collections/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/get([SoaVector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/ref([SoaVector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[effects(heap_alloc), return<vector<Particle>>]
/std/collections/soa/SoaVector__Particle/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<void>]
/std/collections/soa/SoaVector__Particle/push([SoaVector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/soa/SoaVector__Particle/reserve([SoaVector<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [Particle] picked{holder.cloneValues().get(1i32)}
  [Particle] pickedRef{holder.cloneValues().ref(2i32)}
  [vector<Particle>] unpacked{holder.cloneValues().to_aos()}
  [i32] pushed{holder.cloneValues().push(Particle(13i32))}
  [i32] reserved{holder.cloneValues().reserve(7i32)}
  return(plus(picked.x,
              plus(pickedRef.x,
                   plus(count(unpacked),
                        plus(pushed, reserved)))))
  }
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /std/collections/soa/reserve") !=
        std::string::npos);
}

TEST_CASE("push and reserve bare and method forms reject internal metadata validation on experimental soa bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("explicit soa mutators reject retired builtin soa binding") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  /soa/reserve(values, 4i32)
  /soa/push(values, Particle(12i32))
  values./soa/push(Particle(14i32))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("imported soa mutators reject retired builtin soa binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  reserve(values, 4i32)
  push(values, Particle(12i32))
  values.reserve(6i32)
  values.push(Particle(14i32))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown") !=
        std::string::npos);
}

TEST_CASE("explicit soa reserve keeps canonical reject on vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /soa/reserve(values, 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve is only supported as a statement") !=
        std::string::npos);
}

TEST_CASE("explicit soa push slash-method keeps canonical reject on vector target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  values./soa/push(12i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") !=
        std::string::npos);
}

TEST_CASE("push helper call-form validates retired soa user-helper parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/push([soa<Particle> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  push(values, Particle(4i32))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("argument type mismatch") !=
        std::string::npos);
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

TEST_CASE("to_aos helper validates with soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface to_aos direct call validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  /to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("to_aos method validates with soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values.to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported non-root to_aos forms validate with soa binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{values.to_aos()}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector return accepts local builtin vector binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<vector<Particle>>]
copyValues() {
  [mut] out{vector<Particle>()}
  return(out)
}

[return<int>]
main() {
  [vector<Particle>] unpacked{copyValues()}
  return(count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface to_aos slash-method validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values./to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("to_aos validates borrowed indexed retired soa receiver") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
/score([args<Reference<soa<Particle>>>] values) {
  return(count(to_aos(dereference(values[0i32]))))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("to_soa and to_aos helpers reject removed canonical soa to_aos bridge") {
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("to_soa helper validates retired soa non-vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("to_soa requires vector target") !=
        std::string::npos);
}

TEST_CASE("to_soa helper call-form validates retired soa user-helper parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_soa([soa<Particle>] values) {
  return(5i32)
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(to_soa(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
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

TEST_CASE("non-root to_aos forms keep canonical reject on vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_aos(values)
  values.to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/std/collections/soa/to_aos") != std::string::npos);
}

TEST_CASE("explicit old-surface to_aos_ref direct call validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  /to_aos_ref(location(values))
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface to_aos_ref slash-method validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  location(values)./to_aos_ref()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("to_aos method fallback through struct helper return receivers validates removed canonical bridge") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  [Holder] holder{Holder{}}
  [vector<Particle>] unpacked{holder.cloneValues().to_aos()}
  return(/std/collections/vector/count(unpacked))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method: /std/collections/soa/SoaVector__") !=
        std::string::npos);
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

TEST_CASE("to_aos method fallback validates with same-path helper present on struct helper return receivers compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  [Holder] holder{Holder{}}
  [vector<Particle>] unpacked{holder.cloneValues().to_aos()}
  return(/std/collections/vector/count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos method fallback keeps same-path helper shadow for auto inference through") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  [Holder] holder{Holder{}}
  [auto] item{holder.cloneValues().to_aos()}
  return(item)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper method-form validates with soa user-helper parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([soa<Particle>] values) {
  return(6i32)
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(values.to_aos())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper-return builtin soa forms reject non-templated retired path") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa<Particle>())
}

[return<int>]
/to_aos([soa<Particle>] values) {
  return(9i32)
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  [auto] itemA{to_aos(holder.cloneValues())}
  [auto] itemB{/to_aos(holder.cloneValues())}
  [auto] itemC{holder.cloneValues().to_aos()}
  [auto] itemD{holder.cloneValues()./to_aos()}
  return(plus(plus(itemA, itemB), plus(itemC, itemD)))
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected array") !=
        std::string::npos);
}

TEST_CASE("builtin soa global helper-return read helpers reject non-templated retired path") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa<Particle>>]
cloneValues() {
  [soa<Particle>] values{soa<Particle>(Particle(7i32))}
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
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: get") !=
        std::string::npos);
}

TEST_CASE("builtin soa method-like helper-return read helpers reject primitive metadata first") {
  const std::string source = R"(
Holder() {}

[effects(heap_alloc), return<soa<i32>>]
/Holder/cloneValues([Holder] self) {
  [soa<i32>, mut] values{soa<i32>()}
  values.push(7i32)
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [i32] countBare{count(holder.cloneValues())}
  [i32] countMethod{holder.cloneValues().count()}
  [i32] getBare{get(holder.cloneValues(), 0i32)}
  [i32] getMethod{holder.cloneValues().get(0i32)}
  [Reference<i32>] refBare{ref(holder.cloneValues(), 0i32)}
  [Reference<i32>] refMethod{holder.cloneValues().ref(0i32)}
  return(plus(countBare,
              plus(countMethod,
                   plus(getBare,
                        plus(getMethod,
                             plus(dereference(refBare),
                                  dereference(refMethod)))))))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa return type requires struct element type") !=
        std::string::npos);
}

TEST_CASE("builtin soa method-like helper-return read helpers reject non-reflect Particle metadata first") {
  const std::string source = R"(
import /std/collections/soa/*

Particle() {
  [i32] x{1i32}
}

Holder() {}

[effects(heap_alloc), return<soa<Particle>>]
/Holder/cloneValues([Holder] self) {
  [soa<Particle>, mut] values{soa<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [soa<Particle>] cloned{holder.cloneValues()}
  return(count(cloned))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires reflect-enabled struct type argument: /Particle") !=
        std::string::npos);
}

TEST_CASE("aos and soa containers validate soa parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/consumeSoa([soa<Particle>] values) {
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
  INFO(error);
  CHECK(error.find("argument type mismatch") !=
        std::string::npos);
}

TEST_CASE("ecs style soa update loop validates retired parameter spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa<Particle> mut] particles, [vector<Particle> mut] spawnQueue) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }

  [soa<Particle>] stagedSpawns{to_soa(spawnQueue)}
  reserve(particles, plus(count(particles), count(stagedSpawns)))
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] particles{soa<Particle>()}
  [vector<Particle> mut] spawnQueue{vector<Particle>()}
  simulateStep(particles, spawnQueue)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown call target") !=
        std::string::npos);
}

TEST_CASE("ecs style update loop validates retired soa parameter before conversion mismatch") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa<Particle> mut] particles) {
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
  INFO(error);
  CHECK(error.find("argument type mismatch") !=
        std::string::npos);
}

TEST_SUITE_END();
