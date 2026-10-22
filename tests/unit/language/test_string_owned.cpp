#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.language.string_owned");

namespace {

const std::string Prelude = "import /std/text/*\n";

struct Outcome {
  bool compiled = false;
  bool ok = false;
  int exitCode = -1;
  std::string diagnostics;
};

// Runs `body` as `main` (heap_alloc effect) with the String module imported.
Outcome runBody(const std::string &body, const std::string &definitions = "") {
  ScriptEngine engine;
  const std::string source =
      Prelude + definitions + "\n[return<int> effects(heap_alloc)]\nmain() {\n" + body + "\n}\n";
  const auto script = engine.compileSource("/string_owned.prime", source);
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

// The exit code of `body`, or -1 (and a failed check) when it does not compile.
int exitOf(const std::string &body, const std::string &definitions = "") {
  const Outcome outcome = runBody(body, definitions);
  CHECK(outcome.compiled);
  return outcome.exitCode;
}

} // namespace

TEST_CASE("an empty String allocates nothing") {
  CHECK(exitOf("  [String] s{String{}}\n"
               "  [i32] used{s.count() + s.capacity() + /std/text/stringHeapChunks()}\n"
               "  [bool] isEmpty{s.empty()}\n"
               "  return(if(isEmpty, then() { used }, else() { 99i32 }))") == 0);
}

TEST_CASE("String.from copies text into a 32-byte block with a NUL terminator") {
  CHECK(exitOf("  [String] s{/std/text/stringFrom(\"hello\")}\n"
               "  return(s.count() * 1000i32 + s.capacity() * 10i32 + convert<i32>(s.terminator()))") ==
        5 * 1000 + 31 * 10 + 0);
  CHECK(exitOf("  [String] s{/std/text/stringFrom(\"hello\")}\n"
               "  return(if(s.equalsText(\"hello\"), then() { 1i32 }, else() { 0i32 }))") == 1);
  CHECK(exitOf("  [String] s{/std/text/stringFrom(\"hello\")}\n"
               "  return(if(s.equalsText(\"hellp\"), then() { 1i32 }, else() { 0i32 }))") == 0);
}

TEST_CASE("append grows by doubling and keeps the text") {
  const std::string grow =
      "  [String mut] s{String{}}\n"
      "  for([i32 mut] i{0i32}, i < 5000i32, i++) {\n"
      "    s.pushByte(convert<u8>(65i32 + (i - i / 26i32 * 26i32)))\n"
      "  }\n"
      "  [i32 mut] bad{0i32}\n"
      "  for([i32 mut] i{0i32}, i < 5000i32, i++) {\n"
      "    if(convert<i32>(s.byteAt(i)) != 65i32 + (i - i / 26i32 * 26i32)) {\n"
      "      bad = bad + 1i32\n"
      "    }\n"
      "  }\n"
      "  return(bad * 100000i32 + s.count() + convert<i32>(s.terminator()))";
  CHECK(exitOf(grow) == 5000);
}

TEST_CASE("insert, erase, replace and substr edit the text in place") {
  CHECK(exitOf("  [String mut] s{/std/text/stringFrom(\"hello, world\")}\n"
               "  s.insert(5i32, \"!!\")\n"
               "  [bool] a{s.equalsText(\"hello!!, world\")}\n"
               "  s.erase(5i32, 7i32)\n"
               "  [bool] b{s.equalsText(\"hello, world\")}\n"
               "  s.replace(0i32, 5i32, \"J\")\n"
               "  [bool] c{s.equalsText(\"J, world\")}\n"
               "  s.replace(0i32, 1i32, \"Goodbye\")\n"
               "  [bool] d{s.equalsText(\"Goodbye, world\")}\n"
               "  [String] part{s.substr(9i32, 14i32)}\n"
               "  [bool] e{part.equalsText(\"world\")}\n"
               "  [i32] nul{convert<i32>(s.terminator())}\n"
               "  [bool] all{a && b && c && d && e}\n"
               "  [i32] flags{if(all, then() { 7i32 }, else() { 0i32 })}\n"
               "  return(flags + nul)") == 7);
}

TEST_CASE("a copy parameter gives its own text") {
  const std::string duplicate =
      "[return</std/text/String> effects(heap_alloc)]\n"
      "duplicate([/std/text/String copy] value) {\n"
      "  return(value)\n"
      "}\n";
  CHECK(exitOf("  [String mut] a{/std/text/stringFrom(\"abc\")}\n"
               "  [String mut] b{duplicate(a)}\n"
               "  b.append(\"def\")\n"
               "  a.pushByte(convert<u8>(120i32))\n"
               "  [bool] ok{a.equalsText(\"abcx\") && b.equalsText(\"abcdef\")}\n"
               "  return(if(ok, then() { 1i32 }, else() { 0i32 }))", duplicate) == 1);
}

TEST_CASE("reserve gives the class that holds the request and shrinkToFit gives it back") {
  CHECK(exitOf("  [String mut] s{/std/text/stringFrom(\"hi\")}\n"
               "  s.reserve(100i32)\n"
               "  [i32] big{s.capacity()}\n"
               "  s.shrinkToFit()\n"
               "  [i32] small{s.capacity()}\n"
               "  [bool] kept{s.equalsText(\"hi\")}\n"
               "  [i32] keptFlag{if(kept, then() { 1i32 }, else() { 0i32 })}\n"
               "  return(big * 1000i32 + small * 10i32 + keptFlag)") == 111 * 1000 + 31 * 10 + 1);
}

TEST_CASE("dropping Strings in a loop keeps the string heap constant") {
  const std::string churn =
      "[return<i32> effects(heap_alloc)]\n"
      "churn() {\n"
      "  [String mut] s{/std/text/stringFrom(\"a string long enough to need a heap block of its own\")}\n"
      "  s.append(\" and a little more text\")\n"
      "  return(s.count())\n"
      "}\n";
  const std::string loop =
      "  [i32 mut] chunksAfterWarmup{0i32}\n"
      "  for([i32 mut] round{0i32}, round < 400i32, round++) {\n"
      "    churn()\n"
      "    if(round == 5i32) {\n"
      "      chunksAfterWarmup = /std/text/stringHeapChunks()\n"
      "    }\n"
      "  }\n"
      "  [bool] steady{/std/text/stringHeapChunks() == chunksAfterWarmup}\n"
      "  [bool] drained{/std/text/stringHeapBytesInUse() == 0u64}\n"
      "  return(if(steady && drained, then() { 1i32 }, else() { 0i32 }))";
  CHECK(exitOf(loop, churn) == 1);
}

TEST_CASE("a String returned from a function owns its text") {
  const std::string make =
      "[return</std/text/String> effects(heap_alloc)]\n"
      "greeting([i32] times) {\n"
      "  [String mut] out{String{}}\n"
      "  for([i32 mut] i{0i32}, i < times, i++) {\n"
      "    out.append(\"hey \")\n"
      "  }\n"
      "  return(out)\n"
      "}\n";
  CHECK(exitOf("  [String] s{greeting(3i32)}\n  return(s.count())", make) == 12);
}

TEST_CASE("text over 64 KiB gets its own allocation and returns it when freed") {
  const std::string probe =
      "[return<i32> effects(heap_alloc)]\n"
      "largeWhileLive() {\n"
      "  [String mut] s{String{}}\n"
      "  s.reserve(70000i32)\n"
      "  return(/std/text/stringHeapLargeBlocks())\n"
      "}\n";
  CHECK(exitOf("  [i32] live{largeWhileLive()}\n  return(live * 10i32 + /std/text/stringHeapLargeBlocks())", probe) ==
        10);
}

TEST_CASE("the string memory limit faults, and tryReserve reports it") {
  CHECK(exitOf("  /std/text/setStringMemoryLimit(300000u64)\n"
               "  [String mut] s{String{}}\n"
               "  [bool] first{s.tryReserve(200i32)}\n"
               "  [bool] second{s.tryReserve(100000i32)}\n"
               "  return(if(first && !second, then() { 1i32 }, else() { 0i32 }))") == 1);
  const Outcome faulting = runBody("  /std/text/setStringMemoryLimit(300000u64)\n"
                                   "  [String mut] a{String{}}\n"
                                   "  a.reserve(200i32)\n"
                                   "  [String mut] b{String{}}\n"
                                   "  b.reserve(100000i32)\n"
                                   "  return(0i32)");
  REQUIRE(faulting.compiled);
  CHECK((!faulting.ok || faulting.exitCode != 0));
}

TEST_CASE("out of range access faults") {
  const Outcome outcome = runBody("  [String] s{/std/text/stringFrom(\"abc\")}\n  return(convert<i32>(s.byteAt(3i32)))");
  REQUIRE(outcome.compiled);
  CHECK((!outcome.ok || outcome.exitCode != 0));
}
