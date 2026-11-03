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

// A binding from another binding is a copy: the text is not shared and each String frees its own block. The type is
// the imported short name `String`, which the lowerer used to look up as a raw name and so missed the Copy helper
// (TODO-5639: both Strings then pointed at one block and the VM stopped on the second free).
TEST_CASE("a binding from another String binding owns its own text") {
  CHECK(exitOf("  [String mut] a{/std/text/stringFrom(\"abc\")}\n"
               "  [String mut] b{a}\n"
               "  b.append(\"def\")\n"
               "  a.pushByte(convert<u8>(120i32))\n"
               "  [bool] ok{a.equalsText(\"abcx\") && b.equalsText(\"abcdef\") && a.count() == 4i32 && b.count() == 6i32}\n"
               "  return(if(ok, then() { 1i32 }, else() { 0i32 }))") == 1);
}

TEST_CASE("bindings from bindings of every spelling copy and free once") {
  CHECK(exitOf("  [/std/text/String mut] a{/std/text/stringFrom(\"abc\")}\n"
               "  [String mut] b{a}\n"
               "  [/std/text/String] c{b}\n"
               "  [String] d{c}\n"
               "  b.append(\"!\")\n"
               "  [bool] ok{a.equalsText(\"abc\") && b.equalsText(\"abc!\") && c.equalsText(\"abc\") && d.equalsText(\"abc\")}\n"
               "  return(if(ok, then() { 1i32 }, else() { 0i32 }))") == 1);
}

TEST_CASE("copying a String binding in a loop leaves the string heap empty") {
  CHECK(exitOf("  [String mut] source{/std/text/stringFrom(\"a name long enough to need a heap block of its own\")}\n"
               "  [i32 mut] total{0i32}\n"
               "  for([i32 mut] i{0i32}, i < 300i32, i++) {\n"
               "    [String mut] copy{source}\n"
               "    copy.append(\"!\")\n"
               "    total = total + copy.count()\n"
               "  }\n"
               "  [bool] ok{source.count() == 50i32 && total == 300i32 * 51i32}\n"
               "  return(if(ok, then() { 1i32 }, else() { 0i32 }))") == 1);
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

namespace {
const std::string MoneyStruct =
    "[struct]\nMoney() {\n  [i32] cents{0i32}\n}\n\n"
    "[return<Money>]\n/Money/opPlus([Money] self, [Money] other) {\n  return(Money{self.cents + other.cents})\n}\n\n"
    "[return<bool>]\n/Money/opLessThan([Money] self, [Money] other) {\n  return(self.cents < other.cents)\n}\n";
} // namespace

TEST_CASE("a struct with an opPlus or opLessThan member works with the operators") {
  CHECK(exitOf("  [Money] a{Money{5i32}}\n  [Money] b{Money{7i32}}\n  [Money] c{a + b}\n"
               "  return(if(a < b, then() { c.cents }, else() { 0i32 }))",
               MoneyStruct) == 12);
}

TEST_CASE("an operator on a struct without the matching member stays a numeric-operand error") {
  const Outcome outcome = runBody("  [Money] a{Money{5i32}}\n  [Money] b{Money{7i32}}\n"
                                  "  return(if(a == b, then() { 1i32 }, else() { 0i32 }))",
                                  MoneyStruct);
  CHECK_FALSE(outcome.compiled);
  CHECK(outcome.diagnostics.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("a comparison after a method chain with arguments compares the chain") {
  const std::string chain =
      "[struct]\nBox() {\n  [i32] v{0i32}\n}\n\n"
      "[return<Box>]\n/Box/same([Box] self) {\n  return(self)\n}\n\n"
      "[return<i32>]\n/Box/cmp([Box] self, [Box] other) {\n  return(self.v - other.v)\n}\n";
  CHECK(exitOf("  [Box] a{Box{3i32}}\n  [Box] b{Box{3i32}}\n"
               "  return(if(a.same().cmp(b.same()) == 0i32, then() { 1i32 }, else() { 0i32 }))",
               chain) == 1);
  CHECK(exitOf("  [Box] a{Box{3i32}}\n  [Box] b{Box{5i32}}\n"
               "  return(if(a.same().cmp(b.same()) < 0i32, then() { 1i32 }, else() { 0i32 }))",
               chain) == 1);
}

TEST_CASE("a host function cannot return a string: run-time text requires String") {
  const Outcome outcome = runBody("  [string] name{host_name()}\n  return(0i32)", "[host return<string>]\nhost_name() {\n}\n");
  CHECK_FALSE(outcome.compiled);
  CHECK(outcome.diagnostics.find("run-time text requires String") != std::string::npos);
}

TEST_CASE("a string field holds static text: literals and other strings are the only sources") {
  const std::string named =
      "[struct]\nNamed() {\n  [string mut] label{\"first\"}\n}\n\n"
      "[return<int>]\nrename([string] text) {\n  [Named mut] n{Named{}}\n  n.label = text\n  return(n.label.count())\n}\n";
  CHECK(exitOf("  return(rename(\"second\"))", named) == 6);
}

namespace {
const std::string TextViews = "import /std/text/view/*\nimport /std/collections/vector/*\n";

Outcome runViews(const std::string &body, const std::string &definitions = "") {
  return runBody(body, TextViews + definitions);
}
} // namespace

TEST_CASE("a view is a loan on its owner until its last use") {
  const Outcome conflict = runViews("  [String mut] s{stringFromI32(12345i32)}\n  [StringView] v{s.view()}\n"
                                    "  s.append(\"x\")\n  return(v.count())");
  CHECK_FALSE(conflict.compiled);
  CHECK(conflict.diagnostics.find("borrowed binding: s (root: s, sink: v)") != std::string::npos);

  const Outcome viaNarrowed = runViews("  [String mut] s{stringFromI32(12345i32)}\n  [StringView] v{s.view()}\n"
                                       "  [StringView] w{v.slice(1i32, 3i32)}\n  s.clear()\n  return(w.count())");
  CHECK_FALSE(viaNarrowed.compiled);
  CHECK(viaNarrowed.diagnostics.find("borrowed binding: s") != std::string::npos);

  const Outcome assigned = runViews("  [String mut] s{stringFromI32(12345i32)}\n  [StringView] v{s.view()}\n"
                                    "  s = stringFromI32(7i32)\n  return(v.count())");
  CHECK_FALSE(assigned.compiled);
  CHECK(assigned.diagnostics.find("borrowed binding: s") != std::string::npos);
}

TEST_CASE("views may overlap, narrow, and the owner changes again after the last use") {
  CHECK(exitOf("  [String mut] s{stringFromI32(12345i32)}\n  [StringView] a{s.view()}\n  [StringView] b{s.view()}\n"
               "  [StringView] c{a.slice(1i32, 3i32)}\n  [i32] n{a.count() + b.count() + c.count()}\n"
               "  s.append(\"x\")\n  s.clear()\n  return(n)",
               TextViews) == 12);
  CHECK(exitOf("  [String mut] s{stringFromI32(12345i32)}\n  [StringView] v{s.view()}\n"
               "  [i32] n{v.count()}\n  s.append(\"xyz\")\n  return(n + s.count())",
               TextViews) == 13);
}

TEST_CASE("a view cannot be stored in a field, a container, or returned from a local owner") {
  const std::string holder = "[struct]\nHolder() {\n  [StringView mut] kept{textOf(\"x\")}\n}\n";
  const Outcome field = runViews("  [String] s{stringFromI32(12345i32)}\n  [StringView] v{s.view()}\n  [Holder mut] h{Holder{}}\n"
                                 "  h.kept = v\n  return(0i32)",
                                 holder);
  CHECK_FALSE(field.compiled);
  CHECK(field.diagnostics.find("slice escapes via field kept (root: s)") != std::string::npos);

  const Outcome container = runViews("  [String] s{stringFromI32(12345i32)}\n  [StringView] v{s.view()}\n"
                                     "  [vector<StringView> mut] views{vector<StringView>()}\n  views.push(v)\n  return(0i32)");
  CHECK_FALSE(container.compiled);
  CHECK(container.diagnostics.find("slice escapes via container (root: s)") != std::string::npos);

  const std::string leaking = "[return<StringView> effects(heap_alloc)]\nleak() {\n  [String] s{stringFromI32(1i32)}\n"
                              "  [StringView] v{s.view()}\n  return(v)\n}\n";
  const Outcome returned = runViews("  [StringView] v{leak()}\n  return(v.count())", leaking);
  CHECK_FALSE(returned.compiled);
  CHECK(returned.diagnostics.find("slice escapes via return (root: s)") != std::string::npos);
}

TEST_CASE("a definition may return a view of one of its parameters") {
  const std::string borrowing = "[return</std/text/view/StringView>]\nfirstHalf([/std/text/view/StringView] text) {\n"
                                "  return(text.slice(0i32, text.count() / 2i32))\n}\n";
  CHECK(exitOf("  [String] s{stringFromI32(12345i32)}\n  [StringView] half{firstHalf(s.view())}\n  return(half.count())",
               TextViews + borrowing) == 2);

  const std::string ambiguous = "[return<StringView>]\npickOne([StringView] a, [StringView] b, [bool] first) {\n"
                                "  [StringView] left{a.slice(0i32, 1i32)}\n  [StringView] right{b.slice(0i32, 1i32)}\n"
                                "  if(first) { return(left) }\n  return(right)\n}\n";
  const Outcome rejected = runViews("  return(0i32)", ambiguous);
  CHECK_FALSE(rejected.compiled);
  CHECK(rejected.diagnostics.find("returned view could borrow from more than one parameter") != std::string::npos);

  const std::string annotated = "[return</std/text/view/StringView> returns_borrow<a>]\npickOne([/std/text/view/StringView] a, [/std/text/view/StringView] b) {\n"
                                "  return(a.slice(0i32, 1i32))\n}\n";
  CHECK(exitOf("  return(pickOne(textOf(\"ab\"), textOf(\"cd\")).count())", TextViews + annotated) == 1);
}

namespace {
const std::string RequestParser =
    "import /std/text/view/*\nimport /std/collections/vector/*\n\n"
    "[struct]\nRequest() {\n  [/std/text/view/StringView] method{textOf(\"\")}\n  [/std/text/view/StringView] path{textOf(\"\")}\n}\n\n"
    "[return<Request>]\nparseRequest([/std/text/view/StringView] line) {\n"
    "  [i32] firstSpace{line.indexOf(textOf(\" \"))}\n"
    "  [/std/text/view/StringView] rest{line.slice(firstSpace + 1i32, line.count())}\n"
    "  [i32] secondSpace{rest.indexOf(textOf(\" \"))}\n"
    "  return(Request{[method] line.slice(0i32, firstSpace), [path] rest.slice(0i32, secondSpace)})\n}\n";
}

TEST_CASE("a struct of views is a loan on the buffers its views borrow") {
  CHECK(exitOf("  [String] buffer{stringFrom(\"GET /index.html HTTP/1.1\")}\n  [Request] req{parseRequest(buffer.view())}\n"
               "  return(req.method.count() * 100i32 + req.path.count())",
               RequestParser) == 311);

  const Outcome mutated = runBody("  [String mut] buffer{stringFrom(\"GET /a HTTP/1.1\")}\n  [Request] req{parseRequest(buffer.view())}\n"
                                  "  buffer.append(\"x\")\n  return(req.path.count())",
                                  RequestParser);
  CHECK_FALSE(mutated.compiled);
  CHECK(mutated.diagnostics.find("borrowed binding: buffer (root: buffer, sink: req)") != std::string::npos);

  const std::string leaking = "[return<Request> effects(heap_alloc)]\nreadLine() {\n  [String] buffer{stringFrom(\"GET /a HTTP/1.1\")}\n"
                              "  return(parseRequest(buffer.view()))\n}\n";
  const Outcome outlived = runBody("  [Request] req{readLine()}\n  return(req.path.count())", RequestParser + leaking);
  CHECK_FALSE(outlived.compiled);
  CHECK(outlived.diagnostics.find("slice escapes via return (root: buffer)") != std::string::npos);

  const Outcome stored = runBody("  [String] buffer{stringFrom(\"GET /a HTTP/1.1\")}\n  [Request] req{parseRequest(buffer.view())}\n"
                                 "  [vector<Request> mut] all{vector<Request>()}\n  all.push(req)\n  return(0i32)",
                                 RequestParser);
  CHECK_FALSE(stored.compiled);
  CHECK(stored.diagnostics.find("slice escapes via container (root: buffer)") != std::string::npos);

  const Outcome outer = runBody("  [Request mut] kept{Request{}}\n  if(true) {\n    [String] inner{stringFrom(\"GET /a HTTP/1.1\")}\n"
                                "    kept = parseRequest(inner.view())\n  }\n  return(kept.path.count())",
                                RequestParser);
  CHECK_FALSE(outer.compiled);
  CHECK(outer.diagnostics.find("slice escapes via outer binding kept (root: inner)") != std::string::npos);
}

TEST_CASE("text made in a string arena is rejected where it would outlive the arena") {
  const Outcome returned = runViews("  return(0i32)",
                                    "[return<String> effects(heap_alloc)]\nleak() {\n  [StringArena] arena{beginStringArena()}\n"
                                    "  [String] made{stringFromI32(7i32)}\n  return(made)\n}\n");
  CHECK_FALSE(returned.compiled);
  CHECK(returned.diagnostics.find("string escapes its arena via return (arena: arena)") != std::string::npos);

  const Outcome outer = runViews("  [String mut] kept{String{}}\n  if(true) {\n    [StringArena] arena{beginStringArena()}\n"
                                 "    [String] made{stringFromI32(7i32)}\n    kept = made\n  }\n  return(kept.count())");
  CHECK_FALSE(outer.compiled);
  CHECK(outer.diagnostics.find("string escapes its arena via outer binding kept (arena: arena)") != std::string::npos);

  const Outcome container = runViews("  [vector<String> mut] all{vector<String>()}\n"
                                     "  if(true) {\n    [StringArena] arena{beginStringArena()}\n"
                                     "    [String] made{stringFromI32(7i32)}\n    all.push(made)\n  }\n  return(0i32)");
  CHECK_FALSE(container.compiled);
  CHECK(container.diagnostics.find("string escapes its arena via container all (arena: arena)") != std::string::npos);
}

TEST_CASE("text that outlives its arena through a call faults when it is released") {
  const Outcome outlived = runViews("  [String] after{leak()}\n  return(after.count())",
                                    "[return<String> effects(heap_alloc)]\npassThrough([String] text) {\n  return(text.view().toOwned())\n}\n\n"
                                    "[return<String> effects(heap_alloc)]\nleak() {\n  [StringArena] arena{beginStringArena()}\n"
                                    "  [String] made{stringFromI32(777i32)}\n  return(passThrough(made))\n}\n");
  REQUIRE(outlived.compiled);
  CHECK_FALSE(outlived.ok);
  CHECK(outlived.diagnostics.find("invalid string heap free address") != std::string::npos);
}

TEST_CASE("a named String pushed into a vector is moved into it and keeps its text") {
  CHECK(exitOf("  [vector<String> mut] parts{vector<String>()}\n"
               "  for([i32 mut] k{0i32}, k < 4i32, k++) {\n"
               "    [String mut] part{stringFromI32(1000i32 + k)}\n    part.append(\"/s\")\n    parts.push(part)\n  }\n"
               "  [i32 mut] total{0i32}\n"
               "  for([i32 mut] k{0i32}, k < 4i32, k++) {\n    total = total + parts[k].count() * 100i32 + convert<i32>(parts[k].byteAt(3i32))\n  }\n"
               "  return(total - 2400i32)",
               "import /std/collections/vector/*\n") == (48 + 49 + 50 + 51));
}

TEST_CASE("the result of a returns_borrow call is a loan on the named argument only") {
  const std::string pick = "[return</std/text/view/StringView> returns_borrow<a>]\npickFirst([/std/text/view/StringView] a, [/std/text/view/StringView] b) {\n"
                           "  return(a.slice(0i32, 1i32))\n}\n";
  // `first` borrows `left`, not `right`: changing `right` is fine, changing `left` is not.
  const Outcome ok = runViews("  [String mut] left{stringFromI32(12i32)}\n  [String mut] right{stringFromI32(34i32)}\n"
                              "  [StringView] first{pickFirst(left.view(), right.view())}\n  right.append(\"x\")\n  return(first.count())",
                              pick);
  REQUIRE_MESSAGE(ok.compiled, ok.diagnostics);
  CHECK(ok.exitCode == 1);
  const Outcome bad = runViews("  [String mut] left{stringFromI32(12i32)}\n  [String mut] right{stringFromI32(34i32)}\n"
                               "  [StringView] first{pickFirst(left.view(), right.view())}\n  left.append(\"x\")\n  return(first.count())",
                               pick);
  CHECK_FALSE(bad.compiled);
  CHECK(bad.diagnostics.find("borrowed binding: left (root: left, sink: first)") != std::string::npos);
}

TEST_CASE("an imported struct type can be written with its bare name in parameters, returns, fields and locals") {
  const std::string parser =
      "import /std/text/view/*\n\n"
      "[struct]\nRequest() {\n  [StringView] method{textOf(\"\")}\n  [StringView] path{textOf(\"\")}\n}\n\n"
      "[return<Request>]\nparseRequest([StringView] line) {\n"
      "  [i32] firstSpace{line.indexOf(textOf(\" \"))}\n"
      "  [StringView] rest{line.slice(firstSpace + 1i32, line.count())}\n"
      "  [i32] secondSpace{rest.indexOf(textOf(\" \"))}\n"
      "  return(Request{[method] line.slice(0i32, firstSpace), [path] rest.slice(0i32, secondSpace)})\n}\n\n"
      "[return<StringView>]\nfirstWord([StringView] text) {\n  return(text.slice(0i32, text.indexOf(textOf(\" \"))))\n}\n";
  CHECK(exitOf("  [String] buffer{stringFrom(\"GET /index.html HTTP/1.1\")}\n  [Request] req{parseRequest(buffer.view())}\n"
               "  [StringView] word{firstWord(buffer.view())}\n  return(req.method.count() * 100i32 + req.path.count() + word.count())",
               parser) == (300 + 11 + 3));
}

TEST_CASE("a member path with a specialization suffix does not resolve through its struct's first member") {
  // Inserting into a StringMap<i32> infers the type of `less_than` calls inside the specialized collections; a
  // candidate path stripped of everything after `__t` named the bare struct, whose first specialization answered.
  CHECK(exitOf("  [StringMap<i32> mut] m{StringMap<i32>{}}\n  [String mut] key{stringWithCapacity(0i32)}\n"
               "  key.append(\"a\")\n  m.insert(key.view(), 3i32)\n  key.append(\"b\")\n  m.insert(key.view(), 4i32)\n"
               "  return(m.count() * 10i32 + m.getOr(key.view(), 0i32))",
               "import /std/text/string_map/*\n") == 24);
}

TEST_CASE("a mut method and a struct parameter work as real calls and write the receiver back") {
  const std::string counter =
      "[struct]\nCounter() {\n  [i32 mut] total{0i32}\n  [i32 mut] hits{0i32}\n\n"
      "  [mut return<void>]\n  bump([i32] delta) {\n    this.total = this.total + delta\n    this.hits = this.hits + 1i32\n  }\n\n"
      "  [mut return<i32>]\n  bumpTwice([i32] delta) {\n    this.bump(delta)\n    this.bump(delta)\n    return(this.total)\n  }\n\n"
      "  [mut return<i32>]\n  absorb([Counter] other) {\n    this.total = this.total + other.total\n"
      "    this.hits = this.hits + other.hits\n    return(this.total)\n  }\n}\n";
  CHECK(exitOf("  [Counter mut] a{Counter{}}\n  [Counter mut] b{Counter{}}\n  a.bump(3i32)\n  a.bump(4i32)\n  b.bump(10i32)\n"
               "  b.absorb(a)\n  [i32] last{b.absorb(a)}\n  [i32] twice{a.bumpTwice(1i32)}\n"
               "  return(b.total * 1000i32 + b.hits * 100i32 + last + twice * 0i32 + a.hits)",
               counter) == (24 * 1000 + 5 * 100 + 24 + 4));
}
