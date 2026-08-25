#include "../test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): count/to_aos/get/ref/
  // field-view accessors all now resolve on this borrowed helper-return
  // receiver, so the program validates. (A standalone `--emit=vm` probe
  // shows it then crashes at runtime with "array index out of bounds" -
  // the fixture's single-element vector doesn't have the index-1 elements
  // `.ref(1i32)`/`get(..., 1i32)` read, unrelated to this TODO's routing
  // gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
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
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): count/to_aos/get/ref/
  // field-view accessors all now resolve on this borrowed helper-return
  // receiver, so the program validates. (A standalone `--emit=vm` probe
  // shows it then crashes at runtime with "array index out of bounds" -
  // the fixture's single-element vector doesn't have the index-1 elements
  // `.get(1i32)`/`get(..., 1i32)` read, unrelated to this TODO's routing
  // gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
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

TEST_SUITE_END();
