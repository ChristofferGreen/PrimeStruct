#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <string>

using primec::embed::HostText;
using primec::embed::HostTextView;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.language.host_text");

namespace {

const std::string Hosts = R"(
import /std/text/*

[host return<String>]
host_greet([String] name) {
}

[host return<String>]
host_empty() {
}

[host return<i32>]
host_measure([String] text) {
}

[host return<void>]
host_note([String] text) {
}
)";

primec::embed::Script compile(const std::string &mainBody) {
  ScriptEngine engine;
  const std::string source = Hosts + "\n[return<int> effects(heap_alloc)]\nmain() {\n" + mainBody + "\n}\n";
  primec::embed::Script script = engine.compileSource("/host_text.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  script.bind("host_greet", [](HostTextView name) { return HostText("hello " + std::string(name.value)); });
  script.bind("host_empty", []() { return HostText(); });
  script.bind("host_measure", [](HostTextView text) { return static_cast<int32_t>(text.value.size()); });
  script.bind("host_note", [](HostTextView) {});
  return script;
}

int exitOf(const std::string &mainBody) {
  const auto script = compile(mainBody);
  const auto result = script.run();
  CHECK_MESSAGE(result.ok, result.diagnostics);
  return result.exitCode;
}

} // namespace

TEST_CASE("a String argument reaches the host as its bytes and a String comes back") {
  CHECK(exitOf("  [String] who{/std/text/stringFrom(\"world\")}\n"
               "  [String] greeting{host_greet(who)}\n"
               "  return(greeting.count())") == 11);
  CHECK(exitOf("  [String] who{/std/text/stringFrom(\"world\")}\n"
               "  [String] greeting{host_greet(who)}\n"
               "  [bool] ok{greeting.equalsText(\"hello world\")}\n"
               "  return(if(ok, then() { 1i32 }, else() { 0i32 }))") == 1);
  CHECK(exitOf("  [String] who{/std/text/stringFrom(\"héllo\")}\n"
               "  return(host_measure(who))") == 6);
}

TEST_CASE("empty text crosses the boundary as no block") {
  CHECK(exitOf("  [String] nothing{String{}}\n  return(host_measure(nothing))") == 0);
  CHECK(exitOf("  [String] nothing{host_empty()}\n"
               "  [i32] used{nothing.count() + nothing.capacity() + /std/text/stringHeapChunks()}\n"
               "  return(used)") == 0);
}

TEST_CASE("a returned String owns its block and frees it at the end of its scope") {
  const std::string churn =
      "[return<int> effects(heap_alloc)]\n"
      "once() {\n"
      "  [String] who{/std/text/stringFrom(\"a name long enough to take a block\")}\n"
      "  [String] greeting{host_greet(who)}\n"
      "  return(greeting.count())\n"
      "}\n";
  ScriptEngine engine;
  const std::string source = Hosts + "\n" + churn +
                             "\n[return<int> effects(heap_alloc)]\nmain() {\n"
                             "  [i32 mut] checksum{0i32}\n"
                             "  for([i32 mut] i{0i32}, i < 10000i32, i++) {\n"
                             "    checksum = checksum + once()\n"
                             "  }\n"
                             "  [bool] steady{/std/text/stringHeapChunks() <= 2 && /std/text/stringHeapBytesInUse() == 0u64}\n"
                             "  return(if(steady && checksum == 10000i32 * 40i32, then() { 1i32 }, else() { 0i32 }))\n"
                             "}\n";
  auto script = engine.compileSource("/host_text_loop.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  script.bind("host_greet", [](HostTextView name) { return HostText("hello " + std::string(name.value)); });
  script.bind("host_empty", []() { return HostText(); });
  script.bind("host_measure", [](HostTextView text) { return static_cast<int32_t>(text.value.size()); });
  script.bind("host_note", [](HostTextView) {});
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 1);
}

TEST_CASE("a discarded String result is freed") {
  CHECK(exitOf("  [String] who{/std/text/stringFrom(\"a name long enough to take a block\")}\n"
               "  [u64] before{/std/text/stringHeapBytesInUse()}\n"
               "  host_greet(who)\n"
               "  host_greet(who)\n"
               "  [bool] drained{/std/text/stringHeapBytesInUse() == before}\n"
               "  return(if(drained, then() { 1i32 }, else() { 0i32 }))") == 1);
}

TEST_CASE("a host function that is not bound fails before running") {
  ScriptEngine engine;
  const auto script = engine.compileSource(
      "/host_text_unbound.prime",
      Hosts + "\n[return<int> effects(heap_alloc)]\nmain() {\n  [String] s{host_empty()}\n  return(s.count())\n}\n");
  REQUIRE(script.valid());
  const auto result = script.run();
  CHECK_FALSE(result.ok);
}
