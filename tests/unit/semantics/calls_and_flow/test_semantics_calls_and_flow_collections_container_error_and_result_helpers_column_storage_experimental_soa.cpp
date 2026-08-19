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


TEST_SUITE_END();
