#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrLowerer.h"
#include "primec/ir/IrSerializer.h"
#include "primec/runtime/Vm.h"
#include "../test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.conversions");

TEST_CASE("ir lowerer supports string vector literals") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(vector<string>("a"raw_utf8)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowerer supports float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f32}
  [float] other{2.0f32}
  return(convert<int>(plus(value, other)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawAdd = false;
  bool sawConvert = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddF32) {
      sawAdd = true;
    } else if (inst.op == primec::IrOpcode::ConvertF32ToI32) {
      sawConvert = true;
    }
  }
  CHECK(sawAdd);
  CHECK(sawConvert);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowerer rejects string literal statements") {
  const std::string source = R"(
[return<int>]
main() {
  "hello"utf8
  return(1i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("native backend does not support string literal statements") != std::string::npos);
}

TEST_CASE("ir lowerer rejects lambda expressions") {
  const std::string source = R"(
[return<void>]
holder([int] value) {
  return()
}

[return<void>]
main() {
  holder([]([i32] value) { plus(value, 1i32) })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends do not support lambdas") != std::string::npos);
}

TEST_CASE("ir lowerer supports Result.map builtin lambdas") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] mapped{
    Result.map(ok, []([i32] value) { return(multiply(value, 4i32)) })
  }
  return(try(mapped))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("ir lowerer supports bool payloads for Result.map family") {
  const std::string source = R"(
import /std/file/*

swallow_file_error([FileError] err) {}

[return<int> effects(io_err) on_error<FileError, /swallow_file_error>]
main() {
  [Result<bool, FileError>] mapped{
    Result.map(Result.ok(2i32), []([i32] value) { return(greater_than(value, 1i32)) })
  }
  [Result<bool, FileError>] chained{
    Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(equal(value, 2i32))) })
  }
  [Result<bool, FileError>] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) {
      return(less_than(left, right))
    })
  }
  [i32 mut] score{0i32}
  if(try(mapped), then() { assign(score, plus(score, 1i32)) }, else() { })
  if(try(chained), then() { assign(score, plus(score, 2i32)) }, else() { })
  if(try(summed), then() { assign(score, plus(score, 4i32)) }, else() { })
  return(score)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer rejects Result.map wide payloads") {
  const std::string source = R"(
[return<int>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i64, FileError>] mapped{
    Result.map(ok, []([i32] value) { return(3i64) })
  }
  if(Result.error(mapped), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.map with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer supports Result.map f32 payloads") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<f32, FileError>] ok{Result.ok(1.5f32)}
  [Result<f32, FileError>] mapped{
    Result.map(ok, []([f32] value) { return(plus(value, 0.5f32)) })
  }
  return(convert<int>(multiply(try(mapped), 10.0f32)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 20);
}

TEST_CASE("ir lowerer supports Result.map with direct Result.ok source") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] mapped{
    Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) })
  }
  return(try(mapped))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("ir lowerer supports direct packed ContainerError and ImageError Result payloads") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*
import /std/image/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [ContainerError] container{try(Result.ok(ContainerError{4i32}))}
  [ImageError] image{try(Result.ok(ImageError{3i32}))}
  return(plus(container.code, image.code))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer supports packed error struct Result combinator payloads") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*
import /std/image/*
import /std/gfx/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<ContainerError, FileError>] mappedStatus{
    Result.map(Result.ok(2i32), []([i32] value) { return(ContainerError{value}) })
  }
  [ContainerError] mapped{try(mappedStatus)}
  [Result<ImageError, FileError>] chainedStatus{
    Result.and_then(Result.ok(3i32), []([i32] value) { return(Result.ok(ImageError{value})) })
  }
  [ImageError] chained{try(chainedStatus)}
  [Result<GfxError, FileError>] summedStatus{
    Result.map2(Result.ok(4i32), Result.ok(5i32), []([i32] left, [i32] right) {
      return(GfxError{plus(left, right)})
    })
  }
  [GfxError] summed{try(summedStatus)}
  [i32] mappedCode{mapped.code}
  [i32] chainedCode{chained.code}
  [i32] summedCode{summed.code}
  [i32] tail{plus(chainedCode, summedCode)}
  return(plus(mappedCode, tail))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 14);
}

TEST_CASE("ir lowerer supports direct single-slot struct Result.ok payloads") {
  const std::string source = R"(
import /std/file/*

[struct]
Label() {
  [i32] code{0i32}
}

[return<Result<Label, FileError>>]
make_label() {
  return(Result.ok(Label{[code] 7i32}))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Label] value{try(make_label())}
  print_line(value.code)
  return(value.code)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer supports single-slot struct Result combinator payloads") {
  const std::string source = R"(
import /std/file/*

[struct]
Label() {
  [i32] code{0i32}
}

[return<Result<Label, FileError>>]
make_label([i32] code) {
  return(Result.ok(Label{[code] code}))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Label] mapped{try(Result.map(make_label(2i32), []([Label] value) {
    return(Label{[code] plus(value.code, 5i32)})
  }))}
  print_line(mapped.code)
  [Label] chained{try(Result.and_then(make_label(2i32), []([Label] value) {
    return(Result.ok(Label{[code] plus(value.code, 3i32)}))
  }))}
  print_line(chained.code)
  [Label] summed{try(Result.map2(make_label(2i32), make_label(5i32), []([Label] left, [Label] right) {
    return(Label{[code] plus(left.code, right.code)})
  }))}
  print_line(summed.code)
  return(plus(7i32, plus(5i32, summed.code)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 19);
}

TEST_CASE("ir lowerer supports direct File Result payloads") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(file_read, io_err) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{File<Read>("input.txt"utf8)?}
  [Result<File<Read>, FileError>] wrapped{Result.ok(file)}
  [File<Read>] reopened{try(wrapped)}
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer supports packed File Result combinator payloads") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(file_read, io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] openedA{File<Read>("input.txt"utf8)?}
  [File<Read>] mapped{try(Result.map(Result.ok(openedA), []([File<Read>] file) { return(file) }))}
  [i32 mut] first{0i32}
  mapped.read_byte(first)?
  mapped.close()?

  [File<Read>] openedB{File<Read>("input.txt"utf8)?}
  [File<Read>] chained{try(Result.and_then(Result.ok(openedB), []([File<Read>] file) { return(Result.ok(file)) }))}
  [i32 mut] second{0i32}
  chained.read_byte(second)?
  chained.close()?

  [File<Read>] openedC{File<Read>("input.txt"utf8)?}
  [File<Read>] openedD{File<Read>("input.txt"utf8)?}
  [File<Read>] combined{
    try(Result.map2(Result.ok(openedC), Result.ok(openedD), []([File<Read>] left, [File<Read>] right) {
      return(left)
    }))
  }
  [i32 mut] third{0i32}
  combined.read_byte(third)?
  combined.close()?
  return(plus(first, plus(second, third)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer supports direct multi-slot struct Result.ok payloads") {
  const std::string source = R"(
import /std/file/*

[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Result<Pair, FileError>>]
make_pair() {
  return(Result.ok(Pair{[left] 1i32, [right] 2i32}))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Pair] value{try(make_pair())}
  print_line(value.left)
  print_line(value.right)
  return(plus(value.left, value.right))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowerer supports multi-slot struct Result combinator payloads") {
  const std::string source = R"(
import /std/file/*

[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Result<Pair, FileError>>]
make_pair([i32] left, [i32] right) {
  return(Result.ok(Pair{[left] left, [right] right}))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Pair] mapped{try(Result.map(make_pair(2i32, 3i32), []([Pair] value) {
    return(Pair{[left] plus(value.left, 5i32), [right] plus(value.right, 7i32)})
  }))}
  [Pair] chained{try(Result.and_then(make_pair(2i32, 3i32), []([Pair] value) {
    return(Result.ok(Pair{[left] plus(value.left, value.right), [right] 9i32}))
  }))}
  [Pair] summed{try(Result.map2(make_pair(1i32, 4i32), make_pair(2i32, 5i32), []([Pair] left, [Pair] right) {
    return(Pair{[left] plus(left.left, right.left), [right] plus(left.right, right.right)})
  }))}
  print_line(mapped.left)
  print_line(mapped.right)
  print_line(chained.left)
  print_line(chained.right)
  print_line(summed.left)
  print_line(summed.right)
  return(plus(mapped.left, plus(mapped.right, plus(chained.left, plus(chained.right, plus(summed.left, summed.right))))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 43);
}

TEST_CASE("ir lowerer supports direct array and vector Result payloads") {
  const std::string source = R"(
import /std/file/*

[return<Result<array<i32>, FileError>>]
make_numbers() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(values))
}

[return<Result<vector<i32>, FileError>> effects(heap_alloc)]
make_vector() {
  return(Result.ok(vector<i32>(4i32, 5i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err, heap_alloc) on_error<FileError, /log_file_error>]
main() {
  [array<i32>] direct{try(make_numbers())}
  [vector<i32>] vector_values{try(make_vector())}
  print_line(count(direct))
  print_line(direct[0i32])
  print_line(direct[2i32])
  return(plus(direct[0i32], direct[2i32]))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("retired direct map Result payload literal rejects before lowering") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[return<Result<map<i32, i32>, FileError>>]
make_values() {
  [map<i32, i32>] values{1i32=7i32, 3i32=9i32}
  return(Result.ok(values))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [map<i32, i32>] direct{try(make_values())}
  print_line(/std/collections/map/count<i32, i32>(direct))
  print_line(/std/collections/map/at_unsafe<i32, i32>(direct, 1i32))
  print_line(/std/collections/map/at_unsafe<i32, i32>(direct, 3i32))
  return(plus(/std/collections/map/at_unsafe<i32, i32>(direct, 1i32), /std/collections/map/at_unsafe<i32, i32>(direct, 3i32)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("ir lowerer supports Buffer Result payloads on IR-backed VM paths") {
  const std::string source = R"(
import /std/gfx/*

[return<Result<Buffer<i32>, GfxError>> effects(gpu_dispatch)]
make_buffer() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(/std/gfx/Buffer/upload(values)))
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> effects(gpu_dispatch, io_out, io_err) on_error<GfxError, /log_gfx_error>]
main() {
  [Result<Buffer<i32>, GfxError>] directStatus{make_buffer()}
  [Buffer<i32>] direct{try(directStatus)}
  [Result<Buffer<i32>, GfxError>] mappedStatus{
    Result.map(make_buffer(), []([Buffer<i32>] value) { return(value) })
  }
  [Buffer<i32>] mappedValue{try(mappedStatus)}
  [Result<Buffer<i32>, GfxError>] chainedStatus{
    Result.and_then(make_buffer(), []([Buffer<i32>] value) { return(Result.ok(value)) })
  }
  [Buffer<i32>] chainedValue{try(chainedStatus)}
  [Result<Buffer<i32>, GfxError>] combinedStatus{
    Result.map2(make_buffer(), make_buffer(), []([Buffer<i32>] left, [Buffer<i32>] right) { return(right) })
  }
  [Buffer<i32>] combinedValue{try(combinedStatus)}
  [array<i32>] directOut{direct.readback()}
  [array<i32>] mappedOut{mappedValue.readback()}
  [array<i32>] chainedOut{chainedValue.readback()}
  [array<i32>] combinedOut{combinedValue.readback()}
  return(plus(plus(direct.count(), mappedOut[0i32]), plus(chainedValue.count(), combinedOut[2i32])))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 10);
}

TEST_CASE("ir lowerer rejects f64 Result payloads that remain unsupported") {
  const std::string source = R"(
import /std/file/*

[return<int>]
main() {
  [Result<f64, FileError>] wrapped{Result.ok(0.5f64)}
  if(Result.error(wrapped)) {
    return(1i32)
  }
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.ok with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer supports Result.and_then builtin lambdas") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] chained{
    Result.and_then(ok, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  return(try(chained))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("ir lowerer rejects Result.and_then wide payloads") {
  const std::string source = R"(
[return<int>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i64, FileError>] chained{
    Result.and_then(ok, []([i32] value) { return(Result.ok(3i64)) })
  }
  if(Result.error(chained), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.and_then with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer supports Result.and_then status-only returns") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
swallow_file_error([FileError] err) {}

[return<int>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<FileError>] chained{
    Result.and_then(ok, []([i32] value) { return(FileError.status(FileError.eof())) })
  }
  if(not(Result.error(chained))) {
    return(1i32)
  }
  if(not(equal(count(Result.why(chained)), 3i32))) {
    return(2i32)
  }
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowerer supports try on imported status-only Result sum") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<int> on_error<MyError, /swallow>]
ok_flow() {
  [Result<MyError>] status{ok<MyError>()}
  return(try(status))
}

[return<int> on_error<MyError, /swallow>]
error_flow() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 7i32}}}
  return(try(status))
}

[return<int>]
main() {
  return(plus(ok_flow(), error_flow()))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer propagates imported status-only Result sum errors") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<Result<MyError>> on_error<MyError, /swallow>]
forward_error() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 9i32}}}
  try(status)
  return(ok<MyError>())
}

[return<int>]
main() {
  [Result<MyError>] status{forward_error()}
  return(pick(status) {
    ok {
      0i32
    }
    error(err) {
      err.code
    }
  })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowerer supports try on direct imported status-only Result sums") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<Result<MyError>>]
make_ok() {
  return(ok<MyError>())
}

[return<Result<MyError>>]
make_error() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 11i32}}}
  return(status)
}

[return<int> on_error<MyError, /swallow>]
ok_flow() {
  return(try(make_ok()))
}

[return<int> on_error<MyError, /swallow>]
error_flow() {
  return(try(make_error()))
}

[return<int>]
main() {
  return(plus(ok_flow(), error_flow()))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

TEST_CASE("ir lowerer propagates direct imported status-only Result sum errors") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<Result<MyError>>]
make_error() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 13i32}}}
  return(status)
}

[return<Result<MyError>> on_error<MyError, /swallow>]
forward_error() {
  try(make_error())
  return(ok<MyError>())
}

[return<int>]
main() {
  [Result<MyError>] status{forward_error()}
  return(pick(status) {
    ok {
      0i32
    }
    error(err) {
      err.code
    }
  })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 13);
}

TEST_CASE("ir lowerer supports try on borrowed imported status-only Result sums") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<int> on_error<MyError, /swallow>]
read_reference_ok() {
  [Result<MyError>] status{ok<MyError>()}
  [Reference<Result<MyError>>] ref{location(status)}
  return(try(dereference(ref)))
}

[return<int> on_error<MyError, /swallow>]
read_reference_error() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 17i32}}}
  [Reference<Result<MyError>>] ref{location(status)}
  return(try(dereference(ref)))
}

[return<int> on_error<MyError, /swallow>]
read_pointer_ok() {
  [Result<MyError>] status{ok<MyError>()}
  [Pointer<Result<MyError>>] ptr{location(status)}
  return(try(dereference(ptr)))
}

[return<int> on_error<MyError, /swallow>]
read_pointer_error() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 19i32}}}
  [Pointer<Result<MyError>>] ptr{location(status)}
  return(try(dereference(ptr)))
}

[return<int>]
main() {
  return(plus(plus(read_reference_ok(), read_reference_error()),
              plus(read_pointer_ok(), read_pointer_error())))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 36);
}

TEST_CASE("ir lowerer propagates borrowed imported status-only Result sum errors") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<Result<MyError>> on_error<MyError, /swallow>]
forward_reference_error() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 23i32}}}
  [Reference<Result<MyError>>] ref{location(status)}
  try(dereference(ref))
  return(ok<MyError>())
}

[return<Result<MyError>> on_error<MyError, /swallow>]
forward_pointer_error() {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] 29i32}}}
  [Pointer<Result<MyError>>] ptr{location(status)}
  try(dereference(ptr))
  return(ok<MyError>())
}

[return<int>]
main() {
  [Result<MyError>] left{forward_reference_error()}
  [Result<MyError>] right{forward_pointer_error()}
  [i32] leftCode{pick(left) {
    ok {
      0i32
    }
    error(err) {
      err.code
    }
  }}
  [i32] rightCode{pick(right) {
    ok {
      0i32
    }
    error(err) {
      err.code
    }
  }}
  return(plus(leftCode, rightCode))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 52);
}

TEST_CASE("ir lowerer supports status-only imported Result helper calls") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

namespace MyError {
  [return<string>]
  why([MyError] err) {
    return(if(equal(err.code, 7i32), then() { "seven"utf8 }, else() { "nine"utf8 }))
  }
}

[return<Result<MyError>>]
make_ok() {
  return(ok<MyError>())
}

[return<Result<MyError>>]
make_error([i32] code) {
  [Result<MyError>] status{Result<MyError>{[error] MyError{[code] code}}}
  return(status)
}

[return<int>]
main() {
  [Result<MyError>] localOk{ok<MyError>()}
  [Result<MyError>] localError{Result<MyError>{[error] MyError{[code] 7i32}}}
  [Reference<Result<MyError>>] ref{location(localError)}
  [Pointer<Result<MyError>>] ptr{location(localError)}
  if(Result.error(localOk)) {
    return(1i32)
  }
  if(not(Result.error(localError))) {
    return(2i32)
  }
  if(not(equal(count(Result.why(localOk)), 0i32))) {
    return(3i32)
  }
  if(not(equal(count(Result.why(localError)), 5i32))) {
    return(4i32)
  }
  if(Result.error(make_ok())) {
    return(5i32)
  }
  if(not(Result.error(make_error(9i32)))) {
    return(6i32)
  }
  if(not(equal(count(Result.why(make_ok())), 0i32))) {
    return(7i32)
  }
  if(not(equal(count(Result.why(make_error(9i32))), 4i32))) {
    return(8i32)
  }
  if(not(Result.error(dereference(ref)))) {
    return(9i32)
  }
  if(not(equal(count(Result.why(dereference(ref))), 5i32))) {
    return(10i32)
  }
  if(not(Result.error(dereference(ptr)))) {
    return(11i32)
  }
  if(not(equal(count(Result.why(dereference(ptr))), 5i32))) {
    return(12i32)
  }
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowerer supports Result.and_then f32 payloads") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<f32, FileError>] ok{Result.ok(1.5f32)}
  [Result<f32, FileError>] chained{
    Result.and_then(ok, []([f32] value) { return(Result.ok(multiply(value, 2.0f32))) })
  }
  return(convert<int>(multiply(try(chained), 10.0f32)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 30);
}

TEST_CASE("ir lowerer supports Result.and_then with direct Result.ok source") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] chained{
    Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  return(try(chained))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("ir lowerer supports block-bodied Result.and_then lambdas") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      [i32] adjusted{plus(value, 1i32)}
      return(Result.ok(multiply(adjusted, 3i32)))
    })
  }
  return(try(chained))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowerer supports final-if Result.and_then lambdas") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      if(equal(value, 2i32),
        then(){ return(Result.ok(multiply(value, 5i32))) },
        else(){ return(Result.ok(0i32)) })
    })
  }
  return(try(chained))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 10);
}

TEST_CASE("ir lowerer rejects block-bodied Result.and_then wide payloads") {
  const std::string source = R"(
[return<int>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i64, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      [i32] adjusted{plus(value, 1i32)}
      return(Result.ok(convert<i64>(adjusted)))
    })
  }
  if(Result.error(chained), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.and_then with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer rejects final-if Result.and_then wide payloads") {
  const std::string source = R"(
[return<int>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i64, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      if(equal(value, 2i32),
        then(){ return(Result.ok(3i64)) },
        else(){ return(Result.ok(4i64)) })
    })
  }
  if(Result.error(chained), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.and_then with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer supports Result.map2 builtin lambdas") {
  const std::string source = R"(
swallow_file_error([FileError] err) {}

[return<int> on_error<FileError, /swallow_file_error>]
main() {
  [Result<i32, FileError>] first{Result.ok(2i32)}
  [Result<i32, FileError>] second{Result.ok(3i32)}
  [Result<i32, FileError>] summed{
    Result.map2(first, second, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  return(try(summed))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer rejects Result.map2 wide payloads") {
  const std::string source = R"(
[return<int>]
main() {
  [Result<i32, FileError>] first{Result.ok(2i32)}
  [Result<i32, FileError>] second{Result.ok(3i32)}
  [Result<i64, FileError>] summed{
    Result.map2(first, second, []([i32] left, [i32] right) { return(3i64) })
  }
  if(Result.error(summed), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.map2 with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer supports Result.map2 f32 payloads") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<f32, FileError>] first{Result.ok(1.25f32)}
  [Result<f32, FileError>] second{Result.ok(0.75f32)}
  [Result<f32, FileError>] summed{
    Result.map2(first, second, []([f32] left, [f32] right) { return(plus(left, right)) })
  }
  return(convert<int>(multiply(try(summed), 10.0f32)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 20);
}

TEST_CASE("ir lowerer supports Result.map2 with direct Result.ok sources") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  return(try(summed))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer supports direct ok Result combinator consumers") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
swallow_file_error([FileError] err) {}

[return<int> effects(io_err) on_error<FileError, /swallow_file_error>]
main() {
  [i32] mapped{try(Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) }))}
  [i32] chained{try(Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) }))}
  [i32] summed{
    try(Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) }))
  }
  return(plus(plus(mapped, chained), summed))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 18);
}

TEST_CASE("ir lowerer supports direct string Result combinator consumers") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

swallow_parse_error([ParseError] err) {}

[return<int> on_error<ParseError, /swallow_parse_error>]
main() {
  [string] mapped{try(Result.map(Result.ok("alpha"utf8), []([string] value) { return(value) }))}
  [string] chained{try(Result.and_then(Result.ok("beta"utf8), []([string] value) { return(Result.ok(value)) }))}
  [string] combined{try(Result.map2(Result.ok("gamma"utf8), Result.ok("delta"utf8), []([string] left, [string] right) {
    return(left)
  }))}
  return(plus(plus(count(mapped), count(chained)), count(combined)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 14);
}

TEST_CASE("ir lowerer supports definition-backed string Result combinator sources") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[struct]
Reader() {
  [i32] marker{0i32}
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[return<Result<string, ParseError>>]
/Reader/read([Reader] self) {
  return(Result.ok("beta"utf8))
}

swallow_parse_error([ParseError] err) {}

[return<int> on_error<ParseError, /swallow_parse_error>]
main() {
  [Reader] reader{Reader{}}
  [string] mapped{try(Result.map(greeting(), []([string] value) { return(value) }))}
  [string] chained{try(Result.and_then(reader.read(), []([string] value) { return(Result.ok(value)) }))}
  [string] combined{try(Result.map2(greeting(), reader.read(), []([string] left, [string] right) { return(left) }))}
  return(plus(plus(count(mapped), count(chained)), count(combined)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 14);
}

// TODO-4950: this test hand-splices a synthetic map2/lambda call tree into a
// real SemanticProgram's "/consume" parameter default. Fixed by giving every
// spliced Call expr a nonzero semanticNodeId (required unconditionally by
// validateSemanticProductDirectCallCoverage for non-method calls and by
// validateSemanticProductMethodCallCoverage for method calls, both in
// IrLowererCallResolution.cpp) and, for the three calls that resolve to a
// real semantic-product target ("greeting()" a direct call; "read()"/"map2()"
// both method calls - method-call coverage has no "doesn't resolve to a
// published target" bypass the way direct-call coverage does, so it's
// unconditional there), also registering a companion
// SemanticProgramDirectCallTarget/SemanticProgramMethodCallTarget +
// publishedRoutingLookups id-by-expr entry + moduleResolvedArtifacts index -
// mirroring what real semantic publication does for genuine call sites and
// the addBindingFact/addQueryFact-style companion-registration pattern
// already used elsewhere in this suite. See docs/todo.md TODO-4950 for the
// full resolution trace (including why a bare fabricated id without the
// companion registration still fails, and why interning "/Reader/read"
// needs a direct callTargetStringTable append instead of
// semanticProgramInternCallTargetString - parseAndValidate already freezes
// the SemanticProgram's published storage, which blocks that function from
// minting genuinely new strings).
TEST_CASE("ir lowerer preserves inline-call Result metadata from caller-scoped parameter defaults") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[struct]
Reader() {
  [i32] marker{0i32}
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[return<Result<string, ParseError>>]
/Reader/read([Reader] self) {
  return(Result.ok("beta"utf8))
}

[return<int> on_error<ParseError, /swallow_parse_error>]
consume([Result<string, ParseError>] status) {
  return(count(try(status)))
}

swallow_parse_error([ParseError] err) {}

[return<int>]
main() {
  [Reader] reader{Reader{}}
  return(consume(greeting()))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  auto makeName = [](const std::string &name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeCall = [](const std::string &name, std::vector<primec::Expr> args, bool isMethodCall = false) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.args = std::move(args);
    expr.isMethodCall = isMethodCall;
    return expr;
  };

  auto consumeIt =
      std::find_if(program.definitions.begin(), program.definitions.end(), [](const primec::Definition &def) {
        return def.fullPath == "/consume";
      });
  REQUIRE(consumeIt != program.definitions.end());
  REQUIRE(consumeIt->parameters.size() == 1);

  auto greetingIt =
      std::find_if(program.definitions.begin(), program.definitions.end(), [](const primec::Definition &def) {
        return def.fullPath == "/greeting";
      });
  REQUIRE(greetingIt != program.definitions.end());

  auto readIt =
      std::find_if(program.definitions.begin(), program.definitions.end(), [](const primec::Definition &def) {
        return def.fullPath == "/Reader/read";
      });
  REQUIRE(readIt != program.definitions.end());

  // parseAndValidate has already frozen the SemanticProgram's published
  // storage (freezeSemanticProgramPublishedStorage), so
  // semanticProgramInternCallTargetString silently returns InvalidSymbolId
  // for any of these (interning new strings is a write, blocked post-freeze).
  // Every resolved path used below is already interned from real publication
  // (greetingIt/readIt's own fullPath, or "/result/map2" - the canonical
  // resolved path SemanticsValidatorExprResultFile.cpp resolves
  // "Result.map2(...)" method calls to, confirmed by its own
  // `resolved == "/result/map2"` special-case and its `args.size() != 4`
  // check matching this fixture's 4-arg {Result, left, right, lambda} shape
  // exactly) though, so a read-only lookup finds each one's existing id
  // (semanticProgramLookupCallTargetStringId falls back to a linear scan of
  // callTargetStringTable once frozen, since the fast hash index is cleared
  // by the freeze).
  // "/greeting" and "/result/map2" are already interned (real call sites for
  // both exist in the untouched source/semantics), but "/Reader/read" is
  // not - main() never actually calls it, only consume()'s spliced-in
  // parameter default does, so real publishing never had a reason to intern
  // it. semanticProgramInternCallTargetString can't mint it post-freeze
  // (see above), but nothing stops appending directly to the public
  // callTargetStringTable vector and computing the SymbolId the same way
  // semanticProgramInternCallTargetString does when not frozen (1-indexed,
  // id == table.size() after appending) - semanticProgramResolveCallTargetString
  // reads directly from that same table, so this is a legitimate append, not
  // a workaround around the frozen invariant (callTargetStringIdsByText, the
  // thing freezing actually protects, is only a dedup cache for interning -
  // we don't need dedup here).
  auto lookupOrInternCallTargetStringId = [&](const std::string &text) -> primec::SymbolId {
    if (const auto id = primec::semanticProgramLookupCallTargetStringId(semanticProgram, text)) {
      return *id;
    }
    semanticProgram.callTargetStringTable.push_back(text);
    return static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size());
  };

  // Real publishing (SemanticPublicationBuilders.cpp's
  // publishDirectCallTargetFacts/publishMethodCallTargetFacts) always
  // registers a fact both in its owning vector *and* in the matching
  // publishedRoutingLookups::*IdsByExpr map *and* in some module's index
  // list (semanticProgramDirectCallTargetView/semanticProgramMethodCallTargetView
  // only return entries reachable through a module's index once
  // moduleResolvedArtifacts is non-empty, which it always is for a real
  // parsed program - anything pushed straight onto directCallTargets/
  // methodCallTargets without a matching module index entry is silently
  // invisible to validateSemanticProductDirectCallCoverage/
  // validateSemanticProductMethodCallCoverage's own local index, even
  // though publishedRoutingLookups already resolves it). These helpers
  // mirror all three registrations, using whichever module already owns
  // /consume's real (non-synthetic) facts of the same kind - the
  // addBindingFact/addQueryFact-style companion registration pattern used
  // throughout
  // test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_buffer_store_diagnostics.cpp.
  // This fixture's whole source is a single file with no imports, so real
  // publishing puts every fact in the same (and only) module bucket -
  // registering into moduleResolvedArtifacts[0] mirrors that directly
  // rather than searching for a specific scope's existing entries (which
  // "/consume" may not have any of for a given fact family, e.g. its body's
  // only calls are the builtin-handled "try"/"count", neither of which
  // necessarily publishes an ordinary method-call-target fact).
  REQUIRE_FALSE(semanticProgram.moduleResolvedArtifacts.empty());
  auto registerDirectCallTarget = [&](uint64_t semanticNodeId, const std::string &resolvedPath) {
    primec::SemanticProgramDirectCallTarget target;
    target.scopePath = "/consume";
    target.semanticNodeId = semanticNodeId;
    target.resolvedPathId = lookupOrInternCallTargetStringId(resolvedPath);
    semanticProgram.directCallTargets.push_back(target);
    const std::size_t index = semanticProgram.directCallTargets.size() - 1;
    semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
        semanticNodeId, target.resolvedPathId);
    semanticProgram.moduleResolvedArtifacts.front().directCallTargetIndices.push_back(index);
  };
  auto registerMethodCallTarget = [&](uint64_t semanticNodeId, const std::string &resolvedPath) {
    primec::SemanticProgramMethodCallTarget target;
    target.scopePath = "/consume";
    target.semanticNodeId = semanticNodeId;
    target.resolvedPathId = lookupOrInternCallTargetStringId(resolvedPath);
    semanticProgram.methodCallTargets.push_back(target);
    const std::size_t index = semanticProgram.methodCallTargets.size() - 1;
    semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
        semanticNodeId, target.resolvedPathId);
    semanticProgram.moduleResolvedArtifacts.front().methodCallTargetIndices.push_back(index);
  };

  primec::Expr leftParam = makeName("left");
  primec::Expr rightParam = makeName("right");
  primec::Expr returnLeft = makeCall("return", {makeName("left")});
  // TODO-4950: validateSemanticProductDirectCallCoverage
  // (IrLowererCallResolution.cpp) requires every non-method-call Call expr
  // reachable from a definition to carry a nonzero semanticNodeId. "return"
  // does not resolve to any published definition family target (it isn't a
  // user Definition at all - resolveCallPathFromPublishedLookups maps it to
  // "/return", and resolvesToPublishedDefinitionFamilyTarget never matches
  // that against any real definition's fullPath/templated/specialized/
  // overload prefix), so - like every other bare return(...) call in every
  // other passing test in this suite - it only needs a nonzero id, not a
  // registered SemanticProgramDirectCallTarget. Verified by reading
  // validateSemanticProductDirectCallCoverage /
  // resolvesToPublishedDefinitionFamilyTarget directly.
  returnLeft.semanticNodeId = 900001;

  primec::Expr map2Lambda;
  map2Lambda.kind = primec::Expr::Kind::Call;
  map2Lambda.isLambda = true;
  map2Lambda.hasBodyArguments = true;
  map2Lambda.args = {leftParam, rightParam};
  map2Lambda.bodyArguments = {returnLeft};
  // Same reasoning as returnLeft above: a lambda-literal Call node has an
  // empty `name`, which resolves to a bare "/" path that never matches a
  // published definition family target either - nonzero id only.
  map2Lambda.semanticNodeId = 900002;

  // Unlike "return" and the lambda literal, "greeting"/"read"/"map2" all
  // resolve to real semantic-product targets, so - unlike the direct-call
  // coverage check (which skips the target requirement entirely for calls
  // that don't resolve to a published definition family, e.g. "return") -
  // both validateSemanticProductDirectCallCoverage (for "greeting", a bare
  // call) and validateSemanticProductMethodCallCoverage (for "read" and
  // "map2", both isMethodCall=true) unconditionally require a registered
  // target for every one of these. "read" resolves to the real
  // "/Reader/read" definition; "map2" resolves to the canonical
  // "/result/map2" builtin path (SemanticsValidatorExprResultFile.cpp
  // special-cases `resolved == "/result/map2"` for exactly this 4-arg
  // {Result, left, right, lambda} isMethodCall shape - the IR lowerer's own
  // separate isResultBuiltinCall(expr, "map2", 4) special-casing bypasses
  // resolveMethodCallDefinition entirely for actual emission, but the
  // semantic-product coverage gate this test hits runs earlier and doesn't
  // know about that bypass). See registerDirectCallTarget/
  // registerMethodCallTarget above for why a plain nonzero id isn't enough
  // (findSemanticProductDirectCallTarget/findSemanticProductMethodCallTarget
  // still return empty without a full companion registration - confirmed
  // live, this is exactly the error a fabricated id alone produces) and why
  // interning can't mint fresh strings here. This mirrors what real semantic
  // publication would have done had this call tree been parsed as part of
  // /consume's parameter default instead of spliced in afterward.
  primec::Expr greetingCall = makeCall("greeting", {});
  greetingCall.semanticNodeId = 900003;
  registerDirectCallTarget(greetingCall.semanticNodeId, greetingIt->fullPath);

  primec::Expr readCall = makeCall("read", {makeName("reader")}, true);
  readCall.semanticNodeId = 900004;
  registerMethodCallTarget(readCall.semanticNodeId, readIt->fullPath);

  primec::Expr map2Call = makeCall(
      "map2", {makeName("Result"), greetingCall, readCall, map2Lambda}, true);
  map2Call.semanticNodeId = 900005;
  registerMethodCallTarget(map2Call.semanticNodeId, "/result/map2");

  consumeIt->parameters.front().args = {map2Call};

  primec::IrLowerer lowerer;
  primec::IrModule module;
  const bool lowerOk = lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error);
  CAPTURE(error);
  REQUIRE(lowerOk);
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer rejects wide Result.ok payloads") {
  const std::string source = R"(
import /std/file/*

[return<Result<i64, FileError>>]
main() {
  return(Result.ok(3i64))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.ok with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer accepts move builtin") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [i32] moved{move(value)}
  return(moved)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer rejects non-literal string bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message{1i32}
  return(1i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
}

TEST_CASE("ir lowerer supports print_line with string literals") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("hello"utf8)
  return(1i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(module.stringTable.size() == 1);
  bool sawPrintString = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintString) {
      sawPrintString = true;
      CHECK((primec::decodePrintFlags(inst.imm) & primec::PrintFlagNewline) != 0);
      CHECK((primec::decodePrintFlags(inst.imm) & primec::PrintFlagStderr) == 0);
      break;
    }
  }
  CHECK(sawPrintString);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());
  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  CHECK(decoded.stringTable == module.stringTable);
}

TEST_CASE("ir lowerer supports print_line with string bindings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string] message{"hello"utf8}
  print_line(message)
  return(1i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(module.stringTable.size() == 1);
  bool sawPrintString = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintString) {
      sawPrintString = true;
      CHECK((primec::decodePrintFlags(inst.imm) & primec::PrintFlagNewline) != 0);
      CHECK((primec::decodePrintFlags(inst.imm) & primec::PrintFlagStderr) == 0);
      break;
    }
  }
  CHECK(sawPrintString);
}

TEST_CASE("ir lowerer supports string binding copy") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string] message{"hello"utf8}
  [string] copy{message}
  print_line(copy)
  return(1i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(module.stringTable.size() == 1);
  CHECK(module.stringTable[0] == "hello");
}

TEST_CASE("ir lowerer supports print_error with string literals") {
  const std::string source = R"(
[return<int> effects(io_err)]
main() {
  print_error("oops"utf8)
  print_line_error("warn"utf8)
  return(1i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(module.stringTable.size() == 2);
  bool sawNoNewlineErr = false;
  bool sawNewlineErr = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintString) {
      const uint64_t flags = primec::decodePrintFlags(inst.imm);
      if ((flags & primec::PrintFlagStderr) != 0) {
        if ((flags & primec::PrintFlagNewline) != 0) {
          sawNewlineErr = true;
        } else {
          sawNoNewlineErr = true;
        }
      }
    }
  }
  CHECK(sawNoNewlineErr);
  CHECK(sawNewlineErr);
}

TEST_CASE("ir lowerer accepts string literal suffixes") {
  const std::string source = R"PS(
[return<int> effects(io_out)]
main() {
  print_line("hello"ascii)
  print_line("world"raw_utf8)
  return(1i32)
}
 )PS";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(module.stringTable.size() == 2);
  CHECK(module.stringTable[0] == "hello");
  CHECK(module.stringTable[1] == "world");
}

TEST_CASE("ir lowerer preserves raw string bytes") {
  const std::string source = R"PS(
[return<int> effects(io_out)]
main() {
  print_line("line\nnext"raw_utf8)
  return(1i32)
}
)PS";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(module.stringTable.size() == 1);
  CHECK(module.stringTable[0] == "line\\nnext");
}

TEST_CASE("ir lowerer rejects mixed signed/unsigned arithmetic") {
  const std::string source = R"(
[return<i64>]
main() {
  return(plus(1i64, 2u64))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("ir lowerer rejects mixed signed/unsigned comparison") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_than(1i64, 2u64))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("ir lowerer rejects dereference of value") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference(value))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("dereference requires a pointer or reference") != std::string::npos);
}

TEST_CASE("ir lowerer accepts i64 literals") {
  const std::string source = R"(
[return<i64>]
main() {
  return(9i64)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].instructions.size() == 2);
  CHECK(module.functions[0].instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(module.functions[0].instructions[1].op == primec::IrOpcode::ReturnI64);
}

TEST_CASE("semantics reject pointer on the right") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  return(plus(1i32, ptr))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("pointer arithmetic requires pointer on the left") != std::string::npos);
}

TEST_CASE("semantics reject location of non-local") {
  const std::string source = R"(
[return<int>]
main() {
  return(dereference(location(plus(1i32, 2i32))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("location requires a local binding") != std::string::npos);
}

TEST_CASE("ir lowerer supports numeric array literals") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(plus(values.count(), values[1i32]))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawBoundsMessage = false;
  for (const auto &text : module.stringTable) {
    if (text == "array index out of bounds") {
      sawBoundsMessage = true;
      break;
    }
  }
  CHECK(sawBoundsMessage);

  bool sawBoundsPrint = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintString) {
      sawBoundsPrint = true;
      break;
    }
  }
  CHECK(sawBoundsPrint);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 10);
}

TEST_CASE("ir lowerer rejects negative loop counts at runtime") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] count{minus(0i32, 1i32)}
  loop(count, do() { })
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawLoopMessage = false;
  for (const auto &text : module.stringTable) {
    if (text == "loop count must be non-negative") {
      sawLoopMessage = true;
      break;
    }
  }
  CHECK(sawLoopMessage);

  bool sawLoopPrint = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintString) {
      sawLoopPrint = true;
      break;
    }
  }
  CHECK(sawLoopPrint);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowerer supports numeric vector literals") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(plus(/std/collections/vector/count(values), values[1i32]))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 10);
}

TEST_CASE("ir lowerer supports array access u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1u64])
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer supports array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer supports array unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer supports array unsafe access u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1u64))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowerer supports entry args count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions[0].instructions.size() == 2);
  CHECK(module.functions[0].instructions[0].op == primec::IrOpcode::PushArgc);
  CHECK(module.functions[0].instructions[1].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error, 3));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_SUITE_END();
