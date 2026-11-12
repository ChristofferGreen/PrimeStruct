#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.text_views");

// A `string` converts to a StringView where one is expected (docs/spec/strings-and-views.md, Current surface of the
// text view). StringView runs on the VM and the optimized C++ emitters, so these programs run on the VM configs and
// optexe, the backend web servers are built with.

namespace {

void runTextViewProgram(const std::string &name, const std::string &source, int exitCode) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = exitCode;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("a string literal and a string value pass where a StringView is expected") {
  runTextViewProgram("text_view_expected_type_sites", R"(
import /std/text/*
import /std/text/view/*

[struct]
Label {
  [StringView] text{StringView{}}
}

[return<i32>]
width([StringView] text) {
  return(text.count())
}

[return<i32>]
widths([StringView] first, [args<StringView>] rest) {
  [i32 mut] total{first.count()}
  [i32 mut] i{0i32}
  while(i < rest.count()) {
    total = total + rest[i].count()
    i = i + 1i32
  }
  return(total)
}

[return<StringView>]
fallback() {
  return("none")
}

[return<i32>]
describe([string] text) {
  return(1i32)
}

[return<i32>]
describe([StringView] text) {
  return(10i32)
}

[return<int> effects(heap_alloc)]
main() {
  [string] word{"four"}
  [StringView] bound{"seven"}
  [StringView mut] later{StringView{}}
  later = "abc"
  [Label] label{Label{[text] word}}
  [StringView] view{"view"}
  [String] formatted{formatText("{}-{}", "abc", "de")}
  // counts: 5 + 4 + 5 + 3 + 4 + 4 = 25; more: 5 (widths) + 6 ("abc-de") + 1 (the literal) + 10 (the view) = 22
  [i32] counts{width("hello") + width(word) + bound.count() + later.count() + label.text.count() + fallback().count()}
  [i32] more{widths("ab", "c", "de") + formatted.count() + describe("text") + describe(view)}
  if(!bound.startsWith("sev") || bound.indexOf("ven") != 2i32) {
    return(1i32)
  }
  return(counts + more)
}
)",
                     47);
}

TEST_CASE(
    "a string parameter is a real call: literals and string values reach count and the byte move") {
  // `String.append` and `weigh` take a `string` parameter that a body reads only as `count()` and through
  // `__pstext_literal`; the call sites pass the module-table index (a literal's, or a string binding's) and the
  // callee reads it as a runtime string. The program runs on every backend; the VM and optcpp take real calls.
  program_matrix::ProgramCase program;
  program.name = "string_parameter_real_call";
  program.source = R"(
import /std/text/*

[return<i32>]
weigh([string] text, [i32] times) {
  [i32 mut] i{0i32}
  [i32 mut] total{0i32}
  while(i < times) {
    total = total + text.count()
    i = i + 1i32
  }
  return(total)
}

[return<int> effects(heap_alloc)]
main() {
  [String mut] s{String{}}
  s.append("alpha")
  s.append("-")
  [string] word{"beta"}
  s.append(word)
  s.append("")
  [string] again{word}
  s.append(again)
  [i32] weighed{weigh("xyz", 3i32) + weigh(word, 2i32) + weigh("", 4i32) + weigh(again, 1i32)}
  // 5 + 1 + 4 + 0 + 4 = 14; 9 + 8 + 0 + 4 = 21
  if(s.count() != 14i32 || weighed != 21i32) {
    return(1i32)
  }
  if(!s.equalsText("alpha-betabeta")) {
    return(2i32)
  }
  return(s.count() + weighed)
}
)";
  program.exitCode = 35;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a string converts at a StringView field of a generic struct") {
  runTextViewProgram("text_view_generic_struct_field",
                     R"(
import /std/text/*
import /std/text/view/*

[struct]
Route<T>() {
  [StringView] method{StringView{}}
  [T] state{}
}

[return<int> effects(heap_alloc)]
main() {
  [string] name{"GET"utf8}
  [Route<i32>] byValue{Route<i32>{[method] name, [state] 3i32}}
  [Route<i32>] byLiteral{Route<i32>{"POST"utf8, 4i32}}
  return(byValue.state + byLiteral.state + byValue.method.count() + byLiteral.method.count())
}
)",
                     14);
}

TEST_CASE("http handlers take literal header names and bodies") {
  runTextViewProgram("text_view_http_literals", R"(
import /std/http/*
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc)]
main() {
  [String] raw{stringFrom("GET /hello HTTP/1.1\r\nHost: example\r\n\r\n")}
  [Request] request{parseRequest(raw.view(), 1024i32)}
  [Router mut] router{Router{}}
  router.get("/hello", []([Request] request, [Reference<Response>] response) {
    response.text("hello", "text/plain")
    response.header("X-Host", request.header("Host"))
  })
  [Response mut] response{Response{}}
  router.call(request, location(response))
  [bool] hasHost{request.hasHeader("host")}
  [StringView] host{request.header("Host")}
  [bool] hostMatches{host.equals("example")}
  if(!hasHost || !hostMatches) {
    return(1i32)
  }
  [String] wire{response.serialize()}
  [StringView] text{wire.view()}
  [i32] contentType{text.indexOf("Content-Type: text/plain\r\n")}
  [i32] forwarded{text.indexOf("X-Host: example\r\n")}
  if(contentType < 0i32 || forwarded < 0i32) {
    return(2i32)
  }
  // 200 + "hello"
  return(response.status / 10i32 + response.body.count())
}
)",
                     25);
}

namespace {

const std::string ElementViewDefinitions = R"(
import /std/text/*
import /std/text/view/*
import /std/collections/*

[struct]
Greeter() {
  [vector<String> mut] lines{vector<String>{}}
}

[return<String> effects(heap_alloc)]
make_line([i32] size) {
  [String mut] out{String{}}
  [i32 mut] i{0i32}
  while(i < size) {
    out.append("x")
    i = i + 1i32
  }
  return(out)
}

[return<i32>]
view_size([StringView] text) {
  return(text.count())
}
)";

} // namespace

TEST_CASE("a view of a vector element passes where a StringView is expected") {
  runTextViewProgram("text_view_element_argument",
                     ElementViewDefinitions + R"(
[return<int> effects(heap_alloc)]
main() {
  [vector<String> mut] lines{vector<String>{}}
  lines.push(make_line(2i32))
  lines.push(make_line(5i32))
  [Greeter mut] greeter{Greeter{}}
  greeter.lines.push(make_line(7i32))
  greeter.lines.push(make_line(9i32))
  return(view_size(lines[1i32].view()) + view_size(greeter.lines[1i32].view()) * 10i32)
}
)",
                     95);
}

TEST_CASE("a method call on a vector element of the wrong type still mismatches a StringView") {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/text_view_element_mismatch/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path sourcePath = dir / "text_view_element_mismatch.prime";
  {
    std::ofstream file(sourcePath, std::ios::binary);
    file << ElementViewDefinitions << R"(
[return<int> effects(heap_alloc)]
main() {
  [vector<String> mut] lines{vector<String>{}}
  lines.push(make_line(2i32))
  return(view_size(lines[0i32].count()))
}
)";
  }
  const program_matrix::Outcome outcome = program_matrix::runCaptured(
      "./primec --emit=ir " + program_matrix::quote(sourcePath.string()) + " -o " +
          program_matrix::quote((dir / "text_view_element_mismatch.psir").string()),
      dir / "text_view_element_mismatch.ir");
  CHECK(outcome.exitCode != 0);
  CHECK_MESSAGE(outcome.err.find("argument type mismatch for /view_size parameter text: expected "
                                 "/std/text/view/StringView got") != std::string::npos,
                outcome.err);
}
