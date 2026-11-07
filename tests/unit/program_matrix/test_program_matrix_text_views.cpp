#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.text_views");

// A `string` converts to a StringView where one is expected (docs/spec/strings-and-views.md, Current surface of the
// text view). StringView runs on the VM and the optimized C++ emitters; the native backends have no text heap yet
// (TODO-5586), so these programs run on the VM configs and optexe, the backend web servers are built with.

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
