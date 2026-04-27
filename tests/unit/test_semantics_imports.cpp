#include "test_semantics_helpers.h"

#include <algorithm>


TEST_SUITE_BEGIN("primestruct.semantics.imports");

TEST_CASE("import brings immediate children into root") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_CASE("import resolves definitions declared before import") {
  const std::string source = R"(
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
import /util
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("definition ast transform hook resolves local symbol metadata") {
  const std::string source = R"(
[ast return<void>]
trace_calls() {
}

[trace_calls int]
main() {
  return(1i32)
}
)";
  std::string error;
  primec::Program program;
  REQUIRE(validateProgramCapturingProgram(source, "/main", error, program));
  CHECK(error.empty());

  const auto mainIt = std::find_if(program.definitions.begin(), program.definitions.end(),
                                  [](const primec::Definition &def) {
                                    return def.fullPath == "/main";
                                  });
  REQUIRE(mainIt != program.definitions.end());
  REQUIRE_FALSE(mainIt->transforms.empty());
  CHECK(mainIt->transforms.front().name == "trace_calls");
  CHECK(mainIt->transforms.front().isAstTransformHook);
  CHECK(mainIt->transforms.front().resolvedPath == "/trace_calls");
  CHECK(mainIt->transforms.front().phase == primec::TransformPhase::Semantic);
}

TEST_CASE("definition ast transform hook resolves imported public symbol metadata") {
  const std::string source = R"(
import /tools

namespace tools {
  [public ast return<void>]
  trace_calls() {
  }
}

[trace_calls return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  primec::Program program;
  REQUIRE(validateProgramCapturingProgram(source, "/main", error, program));
  CHECK(error.empty());

  const auto mainIt = std::find_if(program.definitions.begin(), program.definitions.end(),
                                  [](const primec::Definition &def) {
                                    return def.fullPath == "/main";
                                  });
  REQUIRE(mainIt != program.definitions.end());
  REQUIRE_FALSE(mainIt->transforms.empty());
  CHECK(mainIt->transforms.front().isAstTransformHook);
  CHECK(mainIt->transforms.front().resolvedPath == "/tools/trace_calls");
}

TEST_CASE("definition ast transform hook rewrites local definition body") {
  const std::string source = R"(
[ast return<FunctionAst>]
make_seven([FunctionAst] fn) {
  return(replace_body_with_return_i32(fn, 7i32))
}

[make_seven return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  primec::Program program;
  REQUIRE(validateProgramCapturingProgram(source, "/main", error, program));
  CHECK(error.empty());

  const auto mainIt = std::find_if(program.definitions.begin(), program.definitions.end(),
                                  [](const primec::Definition &def) {
                                    return def.fullPath == "/main";
                                  });
  REQUIRE(mainIt != program.definitions.end());
  REQUIRE(mainIt->returnExpr.has_value());
  CHECK(mainIt->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(mainIt->returnExpr->literalValue == 7);
  REQUIRE_FALSE(mainIt->statements.empty());
  CHECK(mainIt->statements.front().kind == primec::Expr::Kind::Call);
  CHECK(mainIt->statements.front().name == "return");
  CHECK(std::none_of(program.definitions.begin(), program.definitions.end(),
                    [](const primec::Definition &def) {
                      return def.fullPath == "/make_seven";
                    }));
}

TEST_CASE("definition ast transform hook rewrites imported public definition body") {
  const std::string source = R"(
import /tools

namespace tools {
  [public ast return<FunctionAst>]
  make_nine([FunctionAst] fn) {
    return(replace_body_with_return_i32(fn, 9i32))
  }
}

[make_nine return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  primec::Program program;
  REQUIRE(validateProgramCapturingProgram(source, "/main", error, program));
  CHECK(error.empty());

  const auto mainIt = std::find_if(program.definitions.begin(), program.definitions.end(),
                                  [](const primec::Definition &def) {
                                    return def.fullPath == "/main";
                                  });
  REQUIRE(mainIt != program.definitions.end());
  REQUIRE(mainIt->returnExpr.has_value());
  CHECK(mainIt->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(mainIt->returnExpr->literalValue == 9);
}

TEST_CASE("definition ast transform hook rejects unsupported FunctionAst result") {
  const std::string source = R"(
[ast return<FunctionAst>]
identity([FunctionAst] fn) {
  return(fn)
}

[identity return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ast transform hook returned unsupported FunctionAst shape on /main via /identity") !=
        std::string::npos);
}

TEST_CASE("definition ast transform ct-eval adapter rejects unknown helper targets") {
  const std::string source = R"(
[ast return<FunctionAst>]
unknown_rewrite([FunctionAst] fn) {
  return(unknown_function_ast_helper(fn))
}

[unknown_rewrite return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ct-eval ast-transform adapter: unresolved FunctionAst helper target "
                   "unknown_function_ast_helper") != std::string::npos);
}

TEST_CASE("definition ast transform ct-eval adapter rejects contradictory helper input") {
  const std::string source = R"(
[ast return<FunctionAst>]
bad_rewrite([FunctionAst] fn) {
  return(replace_body_with_return_i32(other, 7i32))
}

[bad_rewrite return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ct-eval ast-transform adapter: first FunctionAst helper argument "
                   "must be the hook parameter fn") != std::string::npos);
}

TEST_CASE("definition ast transform hook rejects imported private symbol") {
  const std::string source = R"(
import /tools

namespace tools {
  [private ast return<void>]
  trace_calls() {
  }
}

[trace_calls return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ast transform hook is not visible on /main: trace_calls") != std::string::npos);
}

TEST_CASE("definition ast transform hook rejects unsupported signature") {
  const std::string source = R"(
[ast return<int>]
trace_calls() {
  return(1i32)
}

[trace_calls return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ast transform hook must use return<void> or return<FunctionAst>: /trace_calls") !=
        std::string::npos);
}

TEST_CASE("definition ast transform hook rejects text phase attachment") {
  const std::string source = R"(
[ast return<void>]
trace_calls() {
}

[text(trace_calls) return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("text(...) group requires text transforms on /main: trace_calls") !=
        std::string::npos);
}

TEST_CASE("definition ast transform hook rejects ambiguous imported symbols") {
  const std::string source = R"(
import /a
import /b

namespace a {
  [public ast return<void>]
  trace_calls() {
  }
}

namespace b {
  [public ast return<void>]
  trace_calls() {
  }
}

[trace_calls return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ambiguous ast transform hook on /main: trace_calls") != std::string::npos);
  CHECK(error.find("/a/trace_calls") != std::string::npos);
  CHECK(error.find("/b/trace_calls") != std::string::npos);
}

TEST_CASE("import resolves struct types and constructors") {
  const std::string source = R"(
import /util
namespace util {
  [public struct]
  Widget() {
    [i32] value{1i32}
  }
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves methods on struct types") {
  const std::string source = R"(
import /util
[public struct]
/util/Widget() {
  [i32] value{1i32}
}
[public return<int>]
/util/Widget/get([Widget] self, [i32] value) {
  return(value)
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(item.get(5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("implicit auto inference crosses imported call graph") {
  const std::string source = R"(
import /util
import /bridge
namespace util {
  [public return<auto>]
  id([auto] value) {
    return(value)
  }
}
namespace bridge {
  [public return<auto>]
  forward([auto] value) {
    return(id(value))
  }
}
[return<int>]
main() {
  return(forward(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import aliases a single definition") {
  const std::string source = R"(
import /util/inc
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import does not alias nested definitions") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(inner())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: inner") != std::string::npos);
}

TEST_CASE("import aliases explicit nested definition path") {
  const std::string source = R"(
import /util/nested/inner
namespace util {
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(inner())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import does not alias namespace blocks") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(nested())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: nested") != std::string::npos);
}

TEST_CASE("import rejects namespace-only path") {
  const std::string source = R"(
import /util
namespace util {
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util/*") != std::string::npos);
}

TEST_CASE("import rejects /std/math without wildcard after semicolon") {
  const std::string source = R"(
import /util; /std/math
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("import /std/math is not supported; use import /std/math/* or /std/math/<name>") != std::string::npos);
}

TEST_CASE("import accepts whitespace-separated paths") {
  const std::string source = R"(
import /util /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(2i32), 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts semicolon-separated paths") {
  const std::string source = R"(
import /util; /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(2i32), 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts wildcard math and util paths") {
  const std::string source = R"(
import /std/math/* /util/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<f64>]
main() {
  [i32] value{inc(1i32)}
  return(sin(0.0f64))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts multiple explicit math paths") {
  const std::string source = R"(
import /std/math/sin /std/math/pi
[return<f64>]
main() {
  return(plus(pi, sin(0.0f64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts explicit math min and max") {
  const std::string source = R"(
import /std/math/min /std/math/max
[return<i32>]
main() {
  return(max(min(1i32, 4i32), 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import rejects unknown wildcard path") {
  const std::string source = R"(
import /missing/*
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /missing/*") != std::string::npos);
}

TEST_CASE("import rejects unknown single-segment path") {
  const std::string source = R"(
import /missing
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /missing/*") != std::string::npos);
}

TEST_CASE("import rejects missing definition") {
  const std::string source = R"(
import /util/missing
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util/missing") != std::string::npos);
}

TEST_CASE("import rejects private definition path") {
  const std::string source = R"(
import /util/inc
namespace util {
  [private return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import path refers to private definition: /util/inc") != std::string::npos);
}

TEST_CASE("import rejects wildcard with only private children") {
  const std::string source = R"(
import /util
namespace util {
  [private return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util/*") != std::string::npos);
}

TEST_CASE("import conflicts with existing root definitions") {
  const std::string source = R"(
import /util
[return<int>]
dup() {
  return(1i32)
}
namespace util {
  [public return<int>]
  dup() {
    return(2i32)
  }
}
[return<int>]
main() {
  return(dup())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: dup") != std::string::npos);
}

TEST_CASE("import conflicts with root builtin names") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  plus([i32] value) {
    return(value)
  }
}
[return<int>]
main() {
  return(plus(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: plus") != std::string::npos);
}

TEST_CASE("import rejects explicit root builtin alias") {
  const std::string source = R"(
import /util/assign
namespace util {
  [public return<void>]
  assign([i32] value) {
    return()
  }
}
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: assign") != std::string::npos);
}

TEST_CASE("import conflicts between namespaces") {
  const std::string source = R"(
import /util, /tools
namespace util {
  [public return<int>]
  dup() {
    return(1i32)
  }
}
namespace tools {
  [public return<int>]
  dup() {
    return(2i32)
  }
}
[return<int>]
main() {
  return(dup())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: dup") != std::string::npos);
}

TEST_CASE("import resolves execution targets") {
  const std::string source = R"(
import /util
namespace util {
  [public return<void>]
  run([i32] count) {
    return()
  }
}
[return<int>]
main() {
  return(1i32)
}
run(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts multiple paths in one statement") {
  const std::string source = R"(
import /util, /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(1i32), 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts comma-separated wildcards") {
  const std::string source = R"(
import /std/math/*, /util/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<f32>]
main() {
  return(sin(convert<f32>(inc(1i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("duplicate imports are ignored") {
  const std::string source = R"(
import /util, /util
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import works after definitions") {
  const std::string source = R"(
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(2i32))
}
import /util
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import ignores nested definitions") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  first() {
    return(1i32)
  }
  namespace nested {
    [public return<int>]
    second() {
      return(2i32)
    }
  }
}
[return<int>]
main() {
  return(first())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import rejects nested definitions in root") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [public return<int>]
    second() {
      return(2i32)
    }
  }
}
[return<int>]
main() {
  return(second())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: second") != std::string::npos);
}

TEST_CASE("exact file imports keep bare bridge aliases") {
  const std::string source = R"(
import /std/file/File
import /std/file/FileError

[effects(file_read), return<int>]
main() {
  [Result<File<Read>, FileError>] fileStatus{File<Read>("in.txt"utf8)}
  [FileError] err{FileError.eof()}
  [Result<FileError>] status{FileError.status(err)}
  return(plus(count(Result.why(fileStatus)), count(Result.why(status))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("exact collection imports keep bare bridge aliases") {
  const std::string source = R"(
import /std/collections/map
import /std/collections/ContainerError

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 7i32, 2i32, 11i32)}
  [ContainerError] err{ContainerError.missingKey()}
  return(plus(/std/collections/map/count(values),
      count(Result.why(ContainerError.status(err)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("exact map imports keep canonical wrapper access helpers visible") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 7i32, 2i32, 11i32)}
  if(/std/collections/map/contains(values, 1i32),
     then() { },
     else() { return(0i32) })
  return(plus(/std/collections/map/at(values, 1i32),
      /std/collections/map/at_unsafe(values, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("exact vector imports keep bare bridge aliases") {
  const std::string source = R"(
import /std/collections/vector

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 8i32, 15i32)}
  return(plus(/std/collections/vector/count(values),
      /std/collections/vector/at(values, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("exact vector and map imports keep bridge parity together") {
  const std::string source = R"(
import /std/collections/vector
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 8i32)}
  [map<i32, i32>] pairs{map<i32, i32>(1i32, 7i32, 2i32, 11i32)}
  return(plus(plus(count(values), at(values, 1i32)),
      plus(count(pairs), at(pairs, 2i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wildcard collection imports keep bare vector and map bridge aliases") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 8i32)}
  [map<i32, i32>] pairs{map<i32, i32>(1i32, 7i32)}
  return(plus(plus(count(values), at(values, 1i32)),
      plus(count(pairs), at(pairs, 1i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wildcard collection imports keep bare map count alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] pairs{map<i32, i32>(1i32, 7i32)}
  return(count(pairs))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("exact vector import keeps map bridge aliases unavailable") {
  const std::string source = R"(
import /std/collections/vector

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] pairs{map<i32, i32>(1i32, 7i32)}
  return(count(pairs))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("exact map import keeps vector bridge aliases unavailable") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 8i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib-owned definitions keep direct collection helper imports visible") {
  const std::string source = R"(
import /std/collections/*

namespace std {
  namespace demo {
  [public effects(heap_alloc), return<int>]
  probe() {
    [vector<i32>] values{vector<i32>(4i32, 8i32)}
    [map<i32, i32>] pairs{map<i32, i32>(1i32, 7i32)}
    return(plus(plus(count(values), at(values, 1i32)),
                plus(count(pairs), at(pairs, 1i32))))
  }
  }
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

TEST_CASE("stdlib-owned definitions keep exact map helper imports visible") {
  const std::string source = R"(
import /std/collections/map

namespace std {
  namespace demo {
  [public effects(heap_alloc), return<int>]
  probe() {
    [map<i32, i32>] pairs{map<i32, i32>(1i32, 7i32)}
    return(count(pairs))
  }
  }
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

TEST_CASE("stdlib-owned definitions keep direct collection shim imports visible") {
  const std::string source = R"(
import /std/collections/*

namespace std {
  namespace demo {
  [public effects(heap_alloc), return<int>]
  probe() {
    [vector<i32> mut] values{vectorSingle<i32>(7i32)}
    vectorPush<i32>(values, 9i32)
    return(plus(vectorAtUnsafe<i32>(values, 1i32), vectorCount<i32>(values)))
  }
  }
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

TEST_CASE("exact gfx imports keep bare bridge aliases") {
  const std::string source = R"(
import /std/gfx/Buffer
import /std/gfx/GfxError

[return<int>]
main() {
  [Buffer<i32>] buffer{Buffer<i32>([token] 2i32, [elementCount] 4i32)}
  [GfxError] err{GfxError.queue_submit_failed()}
  [string] whyText{GfxError.why(err)}
  return(plus(buffer.count(), count(whyText)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

#include "test_semantics_imports_gfx.h"

TEST_SUITE_END();
