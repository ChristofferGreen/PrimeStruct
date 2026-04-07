#pragma once

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
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
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
  [Buffer<i32>] data{ Buffer<i32>(1i32) }
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
  [Buffer<i32>] data{ Buffer<i32>(1i32) }
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
