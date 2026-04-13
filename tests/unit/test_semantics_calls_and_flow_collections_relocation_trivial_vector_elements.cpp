#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("push accepts non-relocation-trivial vector element types through imported stdlib helpers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Mover> mut] values{vector<Mover>()}
  push(values, Mover())
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector constructor rejects non-relocation-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Mover>] values{vector<Mover>(Mover(), Mover())}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "vector literal requires relocation-trivial vector element type until container move/reallocation "
            "semantics are implemented: Mover") != std::string::npos);
}

TEST_CASE("push allows relocation-trivial string vector elements") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<string> mut] values{vector<string>("left"raw_utf8)}
  push(values, "right"raw_utf8)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental vector ownership-sensitive helpers accept non-trivial elements") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<Owned> mut] values{vectorPair<Owned>(Owned(10i32), Owned(20i32))}
  vectorPush<Owned>(values, Owned(30i32))
  vectorReserve<Owned>(values, 6i32)
  [i32] picked{plus(vectorAt<Owned>(values, 0i32).value, vectorAtUnsafe<Owned>(values, 2i32).value)}
  vectorPop<Owned>(values)
  vectorRemoveAt<Owned>(values, 0i32)
  vectorRemoveSwap<Owned>(values, 0i32)
  vectorClear<Owned>(values)
  return(picked)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector helpers route experimental vector receivers onto stdlib vector storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }

  Destroy() {
  }
}

[effects(heap_alloc), return<Vector<Owned>>]
wrapValues() {
  return(vectorPair<Owned>(Owned(10i32), Owned(20i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<Owned> mut] values{wrapValues()}
  /std/collections/vectorPush<Owned>(values, Owned(30i32))
  /std/collections/vectorReserve<Owned>(values, 6i32)
  [i32 mut] total{/std/collections/vectorCount<Owned>(values)}
  assign(total, plus(total, /std/collections/vectorCapacity<Owned>(values)))
  assign(total, plus(total, /std/collections/vectorAt<Owned>(values, 0i32).value))
  assign(total, plus(total, /std/collections/vectorAtUnsafe<Owned>(values, 2i32).value))
  /std/collections/vector/push<Owned>(values, Owned(40i32))
  assign(total, plus(total, /std/collections/vector/count<Owned>(values)))
  assign(total, plus(total, values.at(3i32).value))
  /std/collections/vector/remove_at<Owned>(values, 1i32)
  /std/collections/vectorRemoveSwap<Owned>(values, 0i32)
  values.pop()
  /std/collections/vector/clear<Owned>(values)
  return(plus(total, values.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector indexed removal helpers accept ownership-sensitive explicit Vector bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[struct]
Owned() {
  [i32] value{0i32}

  Destroy() {
  }
}

[struct]
Wrapper() {
  [Owned] value{Owned()}
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<Owned> mut] removed{/std/collections/vectorPair<Owned>(Owned(4i32), Owned(9i32))}
  remove_at(removed, 0i32)
  [Vector<Wrapper> mut] swapped{/std/collections/vectorTriple<Wrapper>(
      Wrapper(Owned(1i32)),
      Wrapper(Owned(7i32)),
      Wrapper(Owned(11i32)))}
  remove_swap(swapped, 0i32)
  return(plus(
      plus(/std/collections/vector/count<Owned>(removed), /std/collections/vector/at<Owned>(removed, 0i32).value),
      plus(/std/collections/vector/count<Wrapper>(swapped),
           /std/collections/vector/at_unsafe<Wrapper>(swapped, 0i32).value.value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector helpers still require stdlib imports for experimental vector receivers") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{vectorPair<i32>(4i32, 5i32)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("push on mutable vector field access reports vector-binding diagnostics") {
  const std::string source = R"(
import /std/collections/*

[struct]
Buffer() {
  [vector<i32> mut] values{vector<i32>()}
}

namespace Buffer {
  [effects(heap_alloc), return<void>]
  append([Buffer mut] self, [i32] value) {
    push(self.values, value)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Buffer mut] buffer{Buffer()}
  buffer.append(2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires vector binding") != std::string::npos);
}

TEST_CASE("push validates on mutable soa_vector parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
touch([soa_vector<Particle> mut] values) {
  push(values, Particle(2i32))
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

TEST_CASE("collection syntax parity validates for call and method forms") {
  const std::string source = R"(
import /std/collections/*

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
      plus(viaCall[2i32], viaMethod[2i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection indexing syntax parity keeps integer-index diagnostics") {
  const auto checkInvalidIndex = [](const std::string &exprText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32>] values{array<i32>(1i32, 2i32)}\n"
        "  return(" +
        exprText +
        ")\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("at requires integer index") != std::string::npos);
  };

  checkInvalidIndex("at(values, \"oops\"utf8)");
  checkInvalidIndex("values.at(\"oops\"utf8)");
  checkInvalidIndex("values[\"oops\"utf8]");
}

TEST_CASE("bare vector push requires imported stdlib helper before template specialization") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push<i32>(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector push requires imported stdlib helper before arity validation") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("push call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector reserve alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector reserve alias requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  /vector/reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("reserve on array reports vector binding before effect requirement") {
  const auto checkInvalidReserve = [](const std::string &stmtText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("reserve requires vector binding") != std::string::npos);
  };

  checkInvalidReserve("reserve(values, 8i32)");
  checkInvalidReserve("values.reserve(8i32)");
}

TEST_CASE("vector reserve alias requires integer capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/reserve(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("reserve rejects bool capacity in call and method forms") {
  const auto checkInvalidReserve = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK_FALSE(error.empty());
  };

  checkInvalidReserve("/vector/reserve(values, true)");
  checkInvalidReserve("values.reserve(true)");
}

TEST_CASE("bare vector reserve requires imported stdlib helper before template specialization") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve<i32>(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector reserve requires imported stdlib helper before arity validation") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector reserve validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve accepts nested non-relocation-trivial vector element types through imported stdlib helpers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[struct]
Wrapper() {
  [Mover] value{Mover()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>()}
  reserve(values, 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve validates on mutable soa_vector parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
touch([soa_vector<Particle> mut] values) {
  reserve(values, 8i32)
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

TEST_CASE("pop still rejects soa_vector helper target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
touch([soa_vector<Particle> mut] values) {
  pop(values)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires vector binding") != std::string::npos);
}

TEST_CASE("reserve call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method keeps same-path vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector pop alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector pop validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop allows non-drop-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[struct]
Owned() {
  [i32] value{1i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<Owned> mut] values{/std/collections/vectorSingle<Owned>(Owned())}
  values.pop()
  return(/std/collections/vector/count<Owned>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop allows drop-trivial string vector elements") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<string> mut] values{vector<string>("left"raw_utf8, "right"raw_utf8)}
  pop(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
