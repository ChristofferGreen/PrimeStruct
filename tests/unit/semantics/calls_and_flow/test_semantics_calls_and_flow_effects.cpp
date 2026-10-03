#include "third_party/doctest.h"

#include "primec/frontend/SemanticProduct.h"
#include "../test_semantics_helpers.h"

#include <algorithm>

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.effects");

namespace {

bool validateProgramWithSemanticProduct(const std::string &source,
                                        primec::SemanticProgram &semanticProgram,
                                        std::string &error) {
  auto program = parseProgram(source);
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram);
}

std::string semanticProductText(const primec::SemanticProgram &semanticProgram,
                                primec::SymbolId textId,
                                const std::string &fallback) {
  if (textId == primec::InvalidSymbolId) {
    return fallback;
  }
  return std::string(
      primec::semanticProgramResolveCallTargetString(semanticProgram, textId));
}

bool hasBindingFact(const primec::SemanticProgram &semanticProgram,
                    const std::string &scopePath,
                    const std::string &name,
                    const std::string &bindingTypeText) {
  return std::any_of(semanticProgram.bindingFacts.begin(),
                     semanticProgram.bindingFacts.end(),
                     [&](const primec::SemanticProgramBindingFact &fact) {
    return semanticProductText(semanticProgram, fact.scopePathId, fact.scopePath) ==
               scopePath &&
           semanticProductText(semanticProgram, fact.nameId, fact.name) == name &&
           semanticProductText(semanticProgram,
                               fact.bindingTypeTextId,
                               fact.bindingTypeText) == bindingTypeText;
                     });
}

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool hasBindingFactWithTypePrefix(const primec::SemanticProgram &semanticProgram,
                                  const std::string &scopePath,
                                  const std::string &name,
                                  const std::string &bindingTypePrefix) {
  return std::any_of(semanticProgram.bindingFacts.begin(),
                     semanticProgram.bindingFacts.end(),
                     [&](const primec::SemanticProgramBindingFact &fact) {
    return semanticProductText(semanticProgram, fact.scopePathId, fact.scopePath) ==
               scopePath &&
           semanticProductText(semanticProgram, fact.nameId, fact.name) == name &&
           startsWith(semanticProductText(semanticProgram,
                                          fact.bindingTypeTextId,
                                          fact.bindingTypeText),
                      bindingTypePrefix);
  });
}

bool hasQueryFact(const primec::SemanticProgram &semanticProgram,
                  const std::string &scopePath,
                  const std::string &callName,
                  const std::string &queryTypeText,
                  const std::string &bindingTypeText) {
  return std::any_of(semanticProgram.queryFacts.begin(),
                     semanticProgram.queryFacts.end(),
                     [&](const primec::SemanticProgramQueryFact &fact) {
    return semanticProductText(semanticProgram, fact.scopePathId, fact.scopePath) ==
               scopePath &&
           semanticProductText(semanticProgram, fact.callNameId, fact.callName) ==
               callName &&
           semanticProductText(semanticProgram,
                               fact.queryTypeTextId,
                               fact.queryTypeText) == queryTypeText &&
           semanticProductText(semanticProgram,
                               fact.bindingTypeTextId,
                               fact.bindingTypeText) == bindingTypeText;
                     });
}

bool hasQueryFactWithTupleSpecialization(
    const primec::SemanticProgram &semanticProgram,
    const std::string &scopePath,
    const std::string &callNamePrefix) {
  return std::any_of(semanticProgram.queryFacts.begin(),
                     semanticProgram.queryFacts.end(),
                     [&](const primec::SemanticProgramQueryFact &fact) {
    const std::string callName =
        semanticProductText(semanticProgram, fact.callNameId, fact.callName);
    const std::string queryType =
        semanticProductText(semanticProgram,
                            fact.queryTypeTextId,
                            fact.queryTypeText);
    const std::string bindingType =
        semanticProductText(semanticProgram,
                            fact.bindingTypeTextId,
                            fact.bindingTypeText);
    return semanticProductText(semanticProgram, fact.scopePathId, fact.scopePath) ==
               scopePath &&
           startsWith(callName, callNamePrefix) &&
           startsWith(queryType, "/std/tuple/tuple__t") &&
           startsWith(bindingType, "/std/tuple/tuple__t");
  });
}

} // namespace

TEST_CASE("boolean literal validates") {
  const std::string source = R"(
[return<bool>]
main() {
  return(false)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("spawn publishes task facts and wait returns task result") {
  const std::string source = R"(
[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[effects(task), return<i32>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  [i32] leftResult{wait(left)}
  return(leftResult)
}
)";
  std::string error;
  primec::SemanticProgram semanticProgram;
  INFO(error);
  REQUIRE(validateProgramWithSemanticProduct(source, semanticProgram, error));
  CHECK(error.empty());

  CHECK(hasBindingFact(semanticProgram, "/main", "left", "Task<i32>"));
  CHECK(hasBindingFact(semanticProgram, "/main", "leftResult", "i32"));
  CHECK(hasQueryFact(semanticProgram, "/main", "wait", "i32", "i32"));
}

TEST_CASE("multi wait publishes stdlib tuple result facts") {
  const std::string source = R"(
namespace std {
  namespace tuple {
  [public struct]
  tuple<Ts...>() {
    [Ts...] values
  }
  }
}

[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[effects(task), return<i32>]
computeRight() {
  return(11i32)
}

[effects(task), return<i32>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  [Task<i32>] right{[spawn] computeRight()};
  [auto] both{wait(left, right)}
  return(0i32)
}
)";
  std::string error;
  primec::SemanticProgram semanticProgram;
  INFO(error);
  REQUIRE(validateProgramWithSemanticProduct(source, semanticProgram, error));
  CHECK(error.empty());

  CHECK(hasBindingFactWithTypePrefix(semanticProgram,
                                     "/main",
                                     "both",
                                     "/std/tuple/tuple__t"));
  CHECK(hasQueryFactWithTupleSpecialization(semanticProgram,
                                            "/main",
                                            "/std/tuple/tuple__t"));
}

TEST_CASE("spawn requires task effect") {
  const std::string source = R"(
[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[return<i32>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  [i32] leftResult{wait(left)}
  return(leftResult)
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("spawn requires task effect") != std::string::npos);
}

TEST_CASE("wait requires task effect") {
  const std::string source = R"(
[return<i32>]
main([Task<i32>] left) {
  return(wait(left))
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("wait requires task effect") != std::string::npos);
}

TEST_CASE("task handles must be waited before return") {
  const std::string source = R"(
[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[effects(task), return<i32>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  return(0i32)
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("task handle must be waited before return: left") !=
        std::string::npos);
}

TEST_CASE("task handles reject double wait") {
  const std::string source = R"(
[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[effects(task), return<i32>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  [i32] first{wait(left)}
  [i32] second{wait(left)}
  return(first)
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("task handle already waited: left") != std::string::npos);
}

TEST_CASE("multi wait consumes each task handle") {
  const std::string source = R"(
namespace std {
  namespace tuple {
  [public struct]
  tuple<Ts...>() {
    [Ts...] values
  }
  }
}

[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[effects(task), return<i32>]
computeRight() {
  return(11i32)
}

[effects(task), return<i32>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  [Task<i32>] right{[spawn] computeRight()};
  [auto] both{wait(left, right)}
  return(wait(left))
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("task handle already waited: left") != std::string::npos);
}

TEST_CASE("task handles cannot escape through return") {
  const std::string source = R"(
[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[effects(task), return<Task<i32>>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  return(left)
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("task handles cannot escape their spawning function: left") !=
        std::string::npos);
}

TEST_CASE("task handles cannot escape through call arguments") {
  const std::string source = R"(
[effects(task), return<i32>]
computeLeft() {
  return(7i32)
}

[return<void>]
consume([Task<i32>] left) {
  return()
}

[effects(task), return<i32>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  consume(left)
  return(wait(left))
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("task handles cannot escape their spawning function: left") !=
        std::string::npos);
}

TEST_CASE("spawn rejects mutable captures") {
  const std::string source = R"(
[effects(task), return<i32>]
echo([i32] value) {
  return(value)
}

[effects(task), return<i32>]
main() {
  [i32 mut] value{1i32}
  [Task<i32>] left{[spawn] echo(value)};
  return(wait(left))
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mutable binding cannot be captured by spawned task: value") !=
        std::string::npos);
}

TEST_CASE("spawn rejects reference captures") {
  const std::string source = R"(
[effects(task), return<i32>]
read([Reference<i32>] valueRef) {
  return(dereference(valueRef))
}

[effects(task), return<i32>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] valueRef{location(value)}
  [Task<i32>] left{[spawn] read(valueRef)};
  return(wait(left))
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reference binding cannot be captured by spawned task: valueRef") !=
        std::string::npos);
}

TEST_CASE("if statement sugar validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true) {
    assign(value, 2i32)
  } else {
    assign(value, 3i32)
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return inside if block validates") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) {
    return(2i32)
  } else {
    return(3i32)
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if blocks ignore colliding then definition") {
  const std::string source = R"(
[return<void>]
then() {
  return()
}

[return<int>]
main() {
  if(true) {
    return(1i32)
  } else {
    return(2i32)
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("missing return on some control paths fails") {
  const std::string source = R"(
[return<int>]
main() {
  if(true, then(){ return(2i32) }, else(){ })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("not all control paths return") != std::string::npos);
}

TEST_CASE("return after partial if validates") {
  const std::string source = R"(
[return<int>]
main() {
  if(true, then(){ return(2i32) }, else(){ })
  return(3i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return rejected in execution body") {
  const std::string source = R"(
[return<int>]
execute_repeat([i32] x) {
  return(x)
}

[return<int>]
main() {
  execute_repeat(1i32) { return(1i32) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return not allowed in execution body") != std::string::npos);
}

TEST_CASE("print requires io_out effect") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print("hello"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_out") != std::string::npos);
}

TEST_CASE("dispatch requires gpu_dispatch effect") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop() {
  return()
}

[return<int>]
main() {
  /std/gpu/dispatch(/noop, 1i32, 1i32, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("gpu_dispatch") != std::string::npos);
}

TEST_CASE("std gpu dispatch validates with effect") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop() {
  return()
}

[effects(gpu_dispatch) return<int>]
main() {
  /std/gpu/dispatch(/noop, 1i32, 1i32, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("definition validation context isolates compute flag between definitions") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop() {
  return()
}

[effects(gpu_dispatch) return<int>]
/host() {
  /std/gpu/dispatch(/noop, 1i32, 1i32, 1i32)
  return(0i32)
}

[return<int>]
main() {
  return(/host())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("definition validation context isolates effects between definitions") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop() {
  return()
}

[return<int>]
/host() {
  /std/gpu/dispatch(/noop, 1i32, 1i32, 1i32)
  return(0i32)
}

[effects(gpu_dispatch) return<int>]
main() {
  return(/host())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("dispatch requires gpu_dispatch effect") != std::string::npos);
}

TEST_CASE("std gpu buffer requires gpu_dispatch effect") {
  const std::string source = R"(
[return<int>]
main() {
  [Buffer<i32>] data{ /std/gpu/buffer<i32>(4i32) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer requires gpu_dispatch effect") != std::string::npos);
}

TEST_CASE("std gpu upload requires array input") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [i32] value{1i32}
  [Buffer<i32>] data{ /std/gpu/upload(value) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("upload requires array input") != std::string::npos);
}

TEST_CASE("std gpu upload rejects user definition named array call target") {
  const std::string source = R"(
[return<i32>]
array<T>([T] value) {
  return(0i32)
}

[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{ /std/gpu/upload(array<i32>(1i32)) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("upload requires array input") != std::string::npos);
}

TEST_CASE("std gpu upload accepts builtin array literal input") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{ /std/gpu/upload(array<i32>(1i32, 2i32)) }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("std gpu readback requires buffer input") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] values{array<i32>(1i32)}
  [array<i32>] out{ /std/gpu/readback(values) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("readback requires Buffer input") != std::string::npos);
}

TEST_CASE("std gpu buffer_load requires compute definition") {
  const std::string source = R"(
[effects(gpu_dispatch) return<i32>]
main() {
  [Buffer<i32>] data{ /std/gpu/buffer<i32>(4i32) }
  return(/std/gpu/buffer_load(data, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer_load requires a compute definition") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer load helper requires compute definition") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch) return<i32>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(1i32)}
  return(data.load(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer_load requires a compute definition") != std::string::npos);
}

TEST_CASE("std gpu global_id requires compute definition") {
  const std::string source = R"(
[return<i32>]
main() {
  return(/std/gpu/global_id_x())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("gpu builtins require a compute definition") != std::string::npos);
}

TEST_CASE("legacy gpu_buffer name is rejected") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{ gpu_buffer<i32>(4i32) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("legacy /gpu/global_id_x path is rejected") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop() {
  [i32] x{ /gpu/global_id_x() }
  return()
}

[effects(gpu_dispatch) return<int>]
main() {
  /std/gpu/dispatch(/noop, 1i32, 1i32, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("std gpu dispatch requires kernel name") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop() {
  return()
}

[effects(gpu_dispatch) return<int>]
main() {
  /std/gpu/dispatch(1i32, 1i32, 1i32, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("dispatch requires kernel name as first argument") != std::string::npos);
}

TEST_CASE("std gpu dispatch argument count mismatch") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] output) {
  return()
}

[effects(gpu_dispatch) return<int>]
main() {
  /std/gpu/dispatch(/noop, 1i32, 1i32, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("dispatch argument count mismatch") != std::string::npos);
}

TEST_CASE("std gpu buffer size requires integer expression") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{ /std/gpu/buffer<i32>(1.5f32) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer size requires integer expression") != std::string::npos);
}

TEST_CASE("std gpu buffer requires numeric element type") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<string>] data{ /std/gpu/buffer<string>(1i32) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer requires numeric/bool element type") != std::string::npos);
}

TEST_CASE("std gpu upload rejects template arguments") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [array<i32>] values{array<i32>(1i32)}
  [Buffer<i32>] data{ /std/gpu/upload<i32>(values) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("upload does not accept template arguments") != std::string::npos);
}

TEST_CASE("std gpu readback rejects template arguments") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{ /std/gpu/buffer<i32>(1i32) }
  [array<i32>] out{ /std/gpu/readback<i32>(data) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("readback does not accept template arguments") != std::string::npos);
}

TEST_CASE("std gpu buffer_load rejects template arguments") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] input) {
  [i32] value{ /std/gpu/buffer_load<i32>(input, 0i32) }
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer_load does not accept template arguments") != std::string::npos);
}

TEST_CASE("std gpu buffer_store rejects template arguments") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] output) {
  /std/gpu/buffer_store<i32>(output, 0i32, 1i32)
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer_store does not accept template arguments") != std::string::npos);
}

TEST_CASE("std gpu buffer_load requires integer index") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] input) {
  [i32] value{ /std/gpu/buffer_load(input, 1.5f32) }
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer_load requires integer index") != std::string::npos);
}

TEST_CASE("std gpu buffer_store rejects mismatched value type") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] output) {
  /std/gpu/buffer_store(output, 0i32, 1.5f32)
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer_store value type mismatch") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer store helper requires compute definition") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(1i32)}
  data.store(0i32, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer_store requires a compute definition") != std::string::npos);
}

TEST_CASE("std gpu compute builtins validate") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] output) {
  [i32] x{ /std/gpu/global_id_x() }
  /std/gpu/buffer_store(output, x, 1i32)
  return()
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

TEST_CASE("execution effects must be subset of definition effects") {
  const std::string source = R"(
[return<void>]
noop() {
}

[effects(io_out) return<void>]
main() {
  [effects(io_err)] noop()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution effects must be a subset of enclosing effects") != std::string::npos);
}

TEST_CASE("top-level execution effects must be subset of target effects") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}

[effects(io_out) return<void>]
task() {
  return()
}

[effects(io_err)]
task()
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution effects must be a subset of enclosing effects") != std::string::npos);
}

TEST_CASE("execution effects scope vector literals") {
  const std::string source = R"(
[effects(heap_alloc io_out) return<void>]
main() {
  [effects(io_out)] vector<i32>(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("collection literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("implicit default effects allow print") {
  const std::string source = R"(
main() {
  print_line("hello"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print_error requires io_err effect") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_error("oops"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_err") != std::string::npos);
}

TEST_CASE("notify requires pathspace_notify effect") {
  const std::string source = R"(
main() {
  notify("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_notify") != std::string::npos);
}

TEST_CASE("notify rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify rejects user-defined at on user-defined vector target") {
  const std::string source = R"(
[return<i32>]
vector<T>([T] value) {
  return(0i32)
}

[return<i32>]
at([i32] value, [i32] index) {
  return(plus(value, index))
}

[effects(pathspace_notify) return<int>]
main() {
  notify(at(vector<string>("/events/test"utf8), 0i32), 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify accepts string array access") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  [array<string>] values{array<string>("a"utf8)}
  notify(values[0i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("notify rejects string map access without path inference") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc, pathspace_notify)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  notify(/std/collections/map/at<i32, string>(values, 1i32), 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("notify requires string path argument") != std::string::npos);
}

TEST_CASE("notify rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify<i32>("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify does not accept template arguments") != std::string::npos);
}

TEST_CASE("notify rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify requires exactly 2 arguments") != std::string::npos);
}

TEST_CASE("notify rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify("/events/test"utf8, 1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify does not accept block arguments") != std::string::npos);
}

TEST_SUITE_END();
