#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.language.slices");

namespace {

const std::string Prelude =
    "import /std/collections/*\n"
    "import /std/collections/slice/*\n";

struct Outcome {
  bool compiled = false;
  bool ok = false;
  int exitCode = -1;
  std::string diagnostics;
};

Outcome runBody(const std::string &body) {
  ScriptEngine engine;
  const std::string source = Prelude + "\n[return<int> effects(heap_alloc)]\nmain() {\n" + body + "\n}\n";
  const auto script = engine.compileSource("/slices.prime", source);
  Outcome outcome;
  outcome.compiled = script.valid();
  if (!outcome.compiled) {
    outcome.diagnostics = script.diagnostics();
    MESSAGE(outcome.diagnostics);
    return outcome;
  }
  const auto result = script.run();
  outcome.ok = result.ok;
  outcome.exitCode = result.exitCode;
  outcome.diagnostics = result.diagnostics;
  return outcome;
}

int exitOf(const std::string &body) {
  const Outcome outcome = runBody(body);
  CHECK(outcome.compiled);
  return outcome.exitCode;
}

// A vector<i32> `v` of 10, 20, 30, 40, 50 and its read view `all`.
const std::string Five =
    "  [vector<i32> mut] v{vector<i32>(10i32, 20i32, 30i32, 40i32, 50i32)}\n"
    "  [Slice<i32, Read>] all{/std/collections/vector/view<i32>(v)}\n";

const std::string Flag =
    "  return(if(ok, then() { 1i32 }, else() { 0i32 }))";

} // namespace

TEST_CASE("a view covers the whole vector without copying") {
  CHECK(exitOf(Five + "  [i32] n{all.count()}\n  [i32] last{all.at(4i32)}\n  return(n * 1000i32 + last)") ==
        5 * 1000 + 50);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(1i32)}\n"
               "  [Slice<i32, Read>] all{/std/collections/vector/view<i32>(v)}\n"
               "  [bool] ok{all.empty()}\n" +
               Flag) == 0);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>()}\n"
               "  [Slice<i32, Read>] all{/std/collections/vector/view<i32>(v)}\n"
               "  [bool] ok{all.empty()}\n" +
               Flag) == 1);
}

TEST_CASE("slice(start, end) narrows to a range of the same elements") {
  CHECK(exitOf(Five +
               "  [Slice<i32, Read>] mid{all.slice(1i32, 4i32)}\n"
               "  [i32] n{mid.count()}\n"
               "  return(n * 1000i32 + mid.at(0i32) + mid.at(2i32))") == 3 * 1000 + 20 + 40);
  CHECK(exitOf(Five +
               "  [Slice<i32, Read>] mid{/std/collections/vector/slice<i32>(v, 2i32, 5i32)}\n"
               "  [Slice<i32, Read>] inner{mid.slice(1i32, 2i32)}\n"
               "  return(inner.at(0i32))") == 40);
}

TEST_CASE("takeFirst, dropFirst, takeLast, dropLast, head and tail clamp and narrow") {
  CHECK(exitOf(Five + "  [Slice<i32, Read>] a{all.takeFirst(2i32)}\n  [Slice<i32, Read>] b{all.takeFirst(99i32)}\n"
                      "  [i32] n{a.count()}\n  return(n * 100i32 + b.count())") == 2 * 100 + 5);
  CHECK(exitOf(Five + "  [Slice<i32, Read>] a{all.dropFirst(2i32)}\n  return(a.at(0i32))") == 30);
  CHECK(exitOf(Five + "  [Slice<i32, Read>] a{all.takeLast(2i32)}\n  return(a.at(0i32))") == 40);
  CHECK(exitOf(Five + "  [Slice<i32, Read>] a{all.dropLast(2i32)}\n  return(a.count())") == 3);
  CHECK(exitOf(Five + "  [Slice<i32, Read>] a{all.head(1i32)}\n  [Slice<i32, Read>] b{all.tail(1i32)}\n"
                      "  [i32] n{a.count()}\n  return(n * 100i32 + b.count())") == 1 * 100 + 4);
}

TEST_CASE("search finds elements and sub-slices") {
  CHECK(exitOf(Five + "  [i32] a{all.find(30i32)}\n  [i32] b{all.rfind(30i32)}\n  return(a * 100i32 + b + 10i32)") ==
        2 * 100 + 2 + 10);
  CHECK(exitOf(Five + "  return(all.find(99i32) + 1i32)") == 0);
  CHECK(exitOf(Five + "  [bool] ok{all.contains(40i32) && !all.contains(41i32)}\n" + Flag) == 1);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(1i32, 2i32, 1i32, 3i32, 1i32)}\n"
               "  [Slice<i32, Read>] all{/std/collections/vector/view<i32>(v)}\n"
               "  return(all.occurrences(1i32))") == 3);
  CHECK(exitOf(Five + "  [Slice<i32, Read>] needle{all.slice(2i32, 4i32)}\n"
                      "  return(all.findSlice(needle))") == 2);
}

TEST_CASE("equality, ordering, prefix and suffix compare element by element") {
  CHECK(exitOf(Five +
               "  [Slice<i32, Read>] a{all.slice(0i32, 3i32)}\n"
               "  [Slice<i32, Read>] b{all.slice(0i32, 3i32)}\n"
               "  [Slice<i32, Read>] c{all.slice(1i32, 4i32)}\n"
               "  [Slice<i32, Read>] tail{all.slice(3i32, 5i32)}\n"
               "  [bool] same{a.equals(b) && !a.equals(c)}\n"
               "  [i32] order{a.compare(c) + 1i32}\n"
               "  [bool] prefixes{all.startsWith(a) && all.endsWith(tail) && !all.startsWith(c)}\n"
               "  [bool] ok{same && prefixes}\n"
               "  [i32] flags{if(ok, then() { 10i32 }, else() { 0i32 })}\n"
               "  return(flags + order)") == 10 + 0);
}

TEST_CASE("splitPart cuts at a separator element") {
  const std::string csv =
      "  [vector<u8> mut] v{vector<u8>(97u8, 98u8, 44u8, 99u8, 44u8, 44u8, 100u8)}\n"
      "  [Slice<u8, Read>] all{/std/collections/vector/view<u8>(v)}\n";
  CHECK(exitOf(csv + "  return(all.splitCount(44u8))") == 4);
  CHECK(exitOf(csv +
               "  [Slice<u8, Read>] p0{all.splitPart(44u8, 0i32)}\n"
               "  [Slice<u8, Read>] p1{all.splitPart(44u8, 1i32)}\n"
               "  [Slice<u8, Read>] p2{all.splitPart(44u8, 2i32)}\n"
               "  [i32] a{p0.count()}\n  [i32] b{p1.count()}\n"
               "  return(a * 100i32 + b * 10i32 + p2.count())") == 2 * 100 + 1 * 10 + 0);
  CHECK(exitOf(csv + "  [Slice<u8, Read>] p3{all.splitPart(44u8, 3i32)}\n  return(convert<i32>(p3.at(0i32)))") == 100);
}

TEST_CASE("a Write slice edits the vector's elements in place") {
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(1i32, 2i32, 3i32, 4i32)}\n"
               "  [Slice<i32, Write> mut] w{/std/collections/vector/sliceMut<i32>(v, 1i32, 3i32)}\n"
               "  w.set(0i32, 20i32)\n"
               "  return(v.at(1i32) * 10i32 + v.at(0i32))") == 20 * 10 + 1);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(1i32, 2i32, 3i32, 4i32)}\n"
               "  [Slice<i32, Write> mut] w{/std/collections/vector/sliceMut<i32>(v, 0i32, 4i32)}\n"
               "  w.fill(7i32)\n"
               "  return(v.at(0i32) + v.at(3i32))") == 14);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(5i32, 6i32, 7i32, 8i32)}\n"
               "  [Slice<i32, Write> mut] w{/std/collections/vector/sliceMut<i32>(v, 0i32, 4i32)}\n"
               "  w.reverse()\n"
               "  return(v.at(0i32) * 10i32 + v.at(3i32))") == 8 * 10 + 5);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(5i32, 1i32, 4i32, 2i32, 3i32)}\n"
               "  [Slice<i32, Write> mut] w{/std/collections/vector/sliceMut<i32>(v, 0i32, 5i32)}\n"
               "  w.sort()\n"
               "  return(v.at(0i32) * 10000i32 + v.at(1i32) * 1000i32 + v.at(2i32) * 100i32 + v.at(3i32) * 10i32 + "
               "v.at(4i32))") == 12345);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(1i32, 2i32, 3i32, 4i32, 5i32)}\n"
               "  [Slice<i32, Write> mut] dst{/std/collections/vector/sliceMut<i32>(v, 1i32, 5i32)}\n"
               "  [Slice<i32, Read>] src{/std/collections/vector/slice<i32>(v, 0i32, 4i32)}\n"
               "  dst.copyFrom(src)\n"
               "  return(v.at(0i32) * 10000i32 + v.at(1i32) * 1000i32 + v.at(2i32) * 100i32 + v.at(3i32) * 10i32 + "
               "v.at(4i32))") == 11234);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>(1i32, 2i32, 3i32)}\n"
               "  [Slice<i32, Write> mut] w{/std/collections/vector/sliceMut<i32>(v, 0i32, 3i32)}\n"
               "  [Slice<i32, Write> mut] tail{w.slice(1i32, 3i32)}\n"
               "  tail.fill(9i32)\n"
               "  [Slice<i32, Read>] ro{tail.readOnly()}\n"
               "  return(v.at(0i32) * 100i32 + v.at(2i32) * 10i32 + ro.count())") == 1 * 100 + 9 * 10 + 2);
}

TEST_CASE("slices of vector<u8> step one byte") {
  CHECK(exitOf("  [vector<u8> mut] v{vector<u8>(5u8, 4u8, 3u8, 2u8, 1u8)}\n"
               "  [Slice<u8, Write> mut] w{/std/collections/vector/sliceMut<u8>(v, 0i32, 5i32)}\n"
               "  w.sort()\n"
               "  [Slice<u8, Write> mut] tail{w.slice(2i32, 5i32)}\n"
               "  return(convert<i32>(tail.at(0i32)) * 100i32 + convert<i32>(v.at(4i32)) * 10i32 + tail.count())") ==
        3 * 100 + 5 * 10 + 3);
}

TEST_CASE("first and last are Maybe values") {
  CHECK(exitOf(Five + "  [Maybe<i32>] f{all.first()}\n  [bool] ok{f.isSome()}\n" + Flag) == 1);
  CHECK(exitOf("  [vector<i32> mut] v{vector<i32>()}\n"
               "  [Slice<i32, Read>] none{/std/collections/vector/view<i32>(v)}\n"
               "  [Maybe<i32>] l{none.last()}\n  [bool] ok{l.isEmpty()}\n" +
               Flag) == 1);
}

TEST_CASE("range and index faults end the program") {
  const Outcome range = runBody(Five + "  [Slice<i32, Read>] bad{all.slice(2i32, 9i32)}\n  return(bad.count())");
  REQUIRE(range.compiled);
  CHECK((!range.ok || range.exitCode != 0));
  const Outcome index = runBody(Five + "  return(all.at(5i32))");
  REQUIRE(index.compiled);
  CHECK((!index.ok || index.exitCode != 50));
}

TEST_CASE("a read slice has no write operations") {
  const Outcome write = runBody(Five + "  all.set(0i32, 1i32)\n  return(0i32)");
  CHECK_FALSE(write.compiled);
  const Outcome fill = runBody(Five + "  all.fill(1i32)\n  return(0i32)");
  CHECK_FALSE(fill.compiled);
}
