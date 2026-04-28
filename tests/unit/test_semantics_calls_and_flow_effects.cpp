#include "third_party/doctest.h"

#include "test_semantics_helpers.h"


TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.effects");

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
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
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

TEST_CASE("print_line_error requires io_err effect") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line_error("oops"utf8)
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

TEST_CASE("notify accepts string map access") {
  const std::string source = R"(
import /std/collections/*

[effects(pathspace_notify)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  notify(values[1i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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

TEST_CASE("insert requires pathspace_insert effect") {
  const std::string source = R"(
main() {
  insert("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_insert") != std::string::npos);
}

TEST_CASE("insert rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert<i32>("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept template arguments") != std::string::npos);
}

TEST_CASE("insert rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("insert accepts string array access") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  [array<string>] values{array<string>("/events/test"utf8)}
  insert(values[0i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("insert rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8, 1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept block arguments") != std::string::npos);
}

TEST_CASE("insert rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert requires exactly 2 arguments") != std::string::npos);
}

TEST_CASE("insert not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_insert)]
main() {
  return(insert("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("take requires pathspace_take effect") {
  const std::string source = R"(
main() {
  take("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_take") != std::string::npos);
}

TEST_CASE("take rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take<i32>("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept template arguments") != std::string::npos);
}

TEST_CASE("take rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("take accepts string map access") {
  const std::string source = R"(
import /std/collections/*

[effects(pathspace_take)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  take(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("take rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take("/events/test"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept block arguments") != std::string::npos);
}

TEST_CASE("take rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires exactly 1 argument") != std::string::npos);
}

TEST_CASE("take not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_take)]
main() {
  return(take("/events/test"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("bind requires pathspace_bind effect") {
  const std::string source = R"(
main() {
  bind("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_bind") != std::string::npos);
}

TEST_CASE("unbind requires pathspace_bind effect") {
  const std::string source = R"(
main() {
  unbind("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_bind") != std::string::npos);
}

TEST_CASE("schedule requires pathspace_schedule effect") {
  const std::string source = R"(
main() {
  schedule("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_schedule") != std::string::npos);
}

TEST_CASE("schedule rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_schedule)]
main() {
  schedule(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_notify)]
main() {
  return(notify("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("print not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  return(print_line("hello"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("print rejects pointer argument") {
  const std::string source = R"(
[effects(io_out) return<int>]
main() {
  [i32] value{1i32}
  print(location(value))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects collection argument") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print(array<i32>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects missing arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_line rejects block arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line does not accept block arguments") != std::string::npos);
}


TEST_CASE("print_line rejects missing arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_error rejects missing arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_error()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_error requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_error rejects block arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_error("oops"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_error does not accept block arguments") != std::string::npos);
}

TEST_CASE("print_line_error rejects missing arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_line_error()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line_error requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_line_error rejects block arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_line_error("oops"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line_error does not accept block arguments") != std::string::npos);
}

TEST_CASE("array literal rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>(1i32) { 2i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("map literal rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, 2i32) { 3i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("vector literal rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  vector<i32>(1i32, 2i32) { 3i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("print accepts string array access") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [array<string>] values{array<string>("hi"utf8)}
  print_line(values[0i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts string map access") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "hi"utf8)}
  print_line(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts string vector literal access") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out), effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hi"utf8)}
  print_line(/std/collections/vector/at(values, 0i32))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("definition rejects duplicate identical effects transforms") {
  const std::string source = R"(
[effects(io_out), effects(io_out), return<int>]
main() {
  print_line("hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform on /main") != std::string::npos);
}

TEST_CASE("print rejects user-defined at on user-defined vector target") {
  const std::string source = R"(
Thing() {
  [i32] value{1i32}
}

[return<i32>]
vector<T>([T] value) {
  return(0i32)
}

[return<Thing>]
at([i32] value, [i32] index) {
  return(Thing{})
}

[effects(io_out) return<int>]
main() {
  print_line(at(vector<string>("hi"utf8), 0i32))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects struct block value") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
}

[effects(io_out)]
main() {
  print_line(block(){
    [Thing] item{Thing{}}
    item
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print accepts string binding") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [string] greeting{"hi"utf8}
  print_line(greeting)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts bool binding") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [bool] ready{true}
  print_line(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts bool literal") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(true)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print rejects float literal") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(1.0f32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print_error accepts bool binding") {
  const std::string source = R"(
[effects(io_err)]
main() {
  [bool] ready{true}
  print_error(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print_line_error accepts bool binding") {
  const std::string source = R"(
[effects(io_err)]
main() {
  [bool] ready{true}
  print_line_error(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow print") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello"utf8)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"io_out"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow print_error") {
  const std::string source = R"(
[return<void>]
main() {
  print_line_error("oops"utf8)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"io_err"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow vector literal") {
  const std::string source = R"(
[return<int>]
main() {
  vector<i32>(1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"heap_alloc"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects reject invalid names") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgramWithDefaults(source, "/main", {"bad-effect"}, error));
  CHECK(error.find("invalid default effect: bad-effect") != std::string::npos);
}

TEST_CASE("definition validation context isolates moved bindings") {
  const std::string source = R"(
[return<void>]
consume() {
  [i32] value{1i32}
  move(value)
}

[return<i32>]
main() {
  [i32] value{2i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("definition validation context isolates active effects") {
  const std::string source = R"(
[effects(heap_alloc), return<i32>]
warmup() {
  vector<i32>(1i32)
  return(0i32)
}

[return<i32>]
main() {
  vector<i32>(1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("definition validation context isolates on_error handlers") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
warmup() {
  [File<Write>] file{ File<Write>("warmup.txt"utf8)? }
  return(Result.ok())
}

[return<Result<FileError>> effects(file_write)]
main() {
  [File<Write>] file{ File<Write>("main.txt"utf8)? }
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("missing on_error for ? usage") != std::string::npos);
}

TEST_CASE("File constructor requires file_read effect for read mode") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("File requires file_read effect") != std::string::npos);
}

TEST_CASE("file read methods require file_read effect") {
  const std::string source = R"(
[return<void>]
read_in([File<Read>] file, [i32 mut] value) {
  file.read_byte(value)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("file operations require file_read effect") != std::string::npos);
}

TEST_CASE("file write methods still require file_write effect") {
  const std::string source = R"(
[effects(file_read), return<void>]
write_out([File<Write>] file) {
  file.write_line("hello"utf8)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("file operations require file_write effect") != std::string::npos);
}

TEST_CASE("file_write effect allows file operations") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  file.write_line("hello"utf8)?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("file_write effect still allows read operations") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  [i32 mut] value{0i32}
  file.read_byte(value)?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("file read_byte accepts mutable integer binding") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_read) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  [i32 mut] value{0i32}
  file.read_byte(value)?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("file read_byte rejects immutable binding") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_read) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  [i32] value{0i32}
  file.read_byte(value)?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("read_byte requires mutable integer binding") != std::string::npos);
}

TEST_CASE("file write_bytes accepts builtin array argument") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  file.write_bytes(array<i32>(1i32, 2i32))?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("file write_bytes rejects user definition named array") {
  const std::string source = R"(
[return<i32>]
array<T>([T] value) {
  return(0i32)
}

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  file.write_bytes(array<i32>(1i32))?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("write_bytes requires array argument") != std::string::npos);
}

TEST_CASE("try requires on_error") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write)]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  return(Result.ok())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("missing on_error for ? usage") != std::string::npos);
}

TEST_CASE("user-defined try helper keeps named arguments") {
  const std::string source = R"(
[return<int>]
try([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(try([value] 7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin try rejects named arguments") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [Result<FileError>] status{Result.ok()}
  try([value] status)
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("on_error allows int return type for graphics-style flow") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

[struct]
Frame() {
  [i32] token{0i32}
}

[return<Result<Frame, GfxError>>]
acquire_frame() {
  return(Result.ok(Frame{[token] 9i32}))
}

namespace Frame {
  [return<Result<GfxError>>]
  submit([Frame] self) {
    return(Result.ok())
  }
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  frame{acquire_frame()?}
  frame.submit()?
  return(frame.token)
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error("gfx error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("type namespace helper call drops synthetic receiver") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{0i32}
}

namespace Foo {
  [return<i32>]
  project([Foo] item) {
    return(item.value)
  }
}

[return<int>]
main() {
  [Foo] foo{Foo{[value] 7i32}}
  return(Foo.project(foo))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("type namespace helper auto inference drops synthetic receiver") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{0i32}
}

namespace Foo {
  [return<i32>]
  project([Foo] item) {
    return(item.value)
  }
}

[return<int>]
main() {
  [Foo] foo{Foo{[value] 7i32}}
  [auto] inferred{Foo.project(foo)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("on_error rejects non-Result non-int return type") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

[return<bool> on_error<GfxError, /log_gfx_error>]
main() {
  return(false)
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error("gfx error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("on_error requires Result or int return type") != std::string::npos);
}

TEST_CASE("statement on_error rejects non-block execution target") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [on_error<FileError, /log_file_error>] print_line("x"utf8)
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("on_error transform is not allowed on executions: /print_line") != std::string::npos);
}

TEST_CASE("statement block on_error reports block return requirement") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [on_error<FileError, /log_file_error>] block(){
    print_line("x"utf8)
  }
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("on_error requires Result or int return type on /main/block") != std::string::npos);
}

TEST_CASE("try rejects mismatched error type") {
  const std::string source = R"(
[struct]
MyError() {
  [public i32] code{0i32}
}

[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [Result<MyError>] bad{Result.ok()}
  bad?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("try error type mismatch") != std::string::npos);
}

TEST_SUITE_END();
