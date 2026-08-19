#include "../test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  // Residual TODO-4731 gap: bare ref with a user same-path shadow on call receivers routes to the stdlib wrapper instead of the shadow.
  CHECK(error.find("reference escapes via return") != std::string::npos);
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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

TEST_CASE("method-like canonical soa helper shadows coexist as type-differentiated overloads") {
  // Each concrete SoaVector<Particle> shadow has a parameter-type signature
  // distinct from the templated stdlib helper at the same path, so the family
  // is accepted as overloads instead of rejected as duplicate definitions.
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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


TEST_SUITE_END();
