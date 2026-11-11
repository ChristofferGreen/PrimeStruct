#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

// The semantic validator infers the return types of the generic `/std/http` router by trying expressions that fail
// and are dropped (TODO-5936). The spans those failures captured stayed behind and became the primary span of the
// next real diagnostic: a mistake in `main` was reported at `stdlib/std/http/http.prime`.

namespace {

const char *const RouterPrelude = R"(
import /std/http/*
import /std/http/server/*
import /std/os/*
import /std/text/*
import /std/text/view/*

[struct]
Bench() {
  [i64 mut] requests{0i64}
}

[effects(heap_alloc)]
take_form([Reference<Bench>] bench, [Request] request, [Reference<Response>] response) {
  bench.requests = bench.requests + 1i64
}

[return<AppRouter<Bench>> effects(heap_alloc)]
bench_router() {
  [AppRouter<Bench> mut] router{AppRouter<Bench>{Bench{}}}
  router.post("/form", take_form)
  return(router)
}
)";

std::string diagnosticsOf(const std::string &name, const std::string &source, std::string &srcPathOut) {
  srcPathOut = writeTemp(name + ".prime", source);
  const std::string errPath = (testScratchPath("") / (name + "_err.json")).string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPathOut) + " --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  return readFile(errPath);
}

} // namespace

TEST_CASE("a diagnostic in main of a program that imports the http server is reported in main") {
  const std::string source = std::string(RouterPrelude) + R"(
[return<int> effects(heap_alloc)]
main() {
  [AppRouter<Bench>] router{bench_router()}
  print_line(nothing_here)
  return(0i32)
}
)";
  std::string srcPath;
  const std::string diagnostics = diagnosticsOf("stdlib_span_main_effect", source, srcPath);
  CHECK(diagnostics.find("\"message\":\"print_line requires io_out effect\"") != std::string::npos);
  CHECK(diagnostics.find("\"primary_span\":{\"file\":\"" + srcPath + "\"") != std::string::npos);
  CHECK(diagnostics.find("http.prime") == std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /main\"") != std::string::npos);
  CHECK(diagnostics.find("AppRouter") == std::string::npos);
}

TEST_CASE("a diagnostic in a user definition of a program that imports the http server keeps that definition") {
  const std::string source = std::string(RouterPrelude) + R"(
[return<int>]
helper() {
  print_line(nothing_here)
  return(0i32)
}

[return<int> effects(heap_alloc)]
main() {
  [AppRouter<Bench>] router{bench_router()}
  return(0i32)
}
)";
  std::string srcPath;
  const std::string diagnostics = diagnosticsOf("stdlib_span_helper_unknown", source, srcPath);
  CHECK(diagnostics.find("\"message\":\"unknown identifier: nothing_here\"") != std::string::npos);
  CHECK(diagnostics.find("\"primary_span\":{\"file\":\"" + srcPath + "\"") != std::string::npos);
  CHECK(diagnostics.find("http.prime") == std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /helper\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /main\"") == std::string::npos);
}

TEST_SUITE_END();
