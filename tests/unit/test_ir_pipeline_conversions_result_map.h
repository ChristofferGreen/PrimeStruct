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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 8);
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  [ContainerError] container{try(Result.ok(ContainerError(4i32)))}
  [ImageError] image{try(Result.ok(ImageError(3i32)))}
  return(plus(container.code, image.code))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  [ContainerError] mapped{
    try(Result.map(Result.ok(2i32), []([i32] value) { return(ContainerError(value)) }))
  }
  [ImageError] chained{
    try(Result.and_then(Result.ok(3i32), []([i32] value) { return(Result.ok(ImageError(value))) }))
  }
  [GfxError] summed{
    try(Result.map2(Result.ok(4i32), Result.ok(5i32), []([i32] left, [i32] right) {
      return(GfxError(plus(left, right)))
    }))
  }
  return(plus(mapped.code, plus(chained.code, summed.code)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  return(Result.ok(Label([code] 7i32)))
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  return(Result.ok(Label([code] code)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Label] mapped{try(Result.map(make_label(2i32), []([Label] value) {
    return(Label([code] plus(value.code, 5i32)))
  }))}
  print_line(mapped.code)
  [Label] chained{try(Result.and_then(make_label(2i32), []([Label] value) {
    return(Result.ok(Label([code] plus(value.code, 3i32))))
  }))}
  print_line(chained.code)
  [Label] summed{try(Result.map2(make_label(2i32), make_label(5i32), []([Label] left, [Label] right) {
    return(Label([code] plus(left.code, right.code)))
  }))}
  print_line(summed.code)
  return(plus(7i32, plus(5i32, summed.code)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  return(Result.ok(Pair([left] 1i32, [right] 2i32)))
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  return(Result.ok(Pair([left] left, [right] right)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Pair] mapped{try(Result.map(make_pair(2i32, 3i32), []([Pair] value) {
    return(Pair([left] plus(value.left, 5i32), [right] plus(value.right, 7i32)))
  }))}
  [Pair] chained{try(Result.and_then(make_pair(2i32, 3i32), []([Pair] value) {
    return(Result.ok(Pair([left] plus(value.left, value.right), [right] 9i32)))
  }))}
  [Pair] summed{try(Result.map2(make_pair(1i32, 4i32), make_pair(2i32, 5i32), []([Pair] left, [Pair] right) {
    return(Pair([left] plus(left.left, right.left), [right] plus(left.right, right.right)))
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowerer supports direct map Result payloads") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[return<Result<map<i32, i32>, FileError>>]
make_values() {
  [map<i32, i32>] values{map<i32, i32>{1i32=7i32, 3i32=9i32}}
  return(Result.ok(values))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [map<i32, i32>] direct{try(make_values())}
  print_line(mapCount<i32, i32>(direct))
  print_line(mapAtUnsafe<i32, i32>(direct, 1i32))
  print_line(mapAtUnsafe<i32, i32>(direct, 3i32))
  return(plus(mapAtUnsafe<i32, i32>(direct, 1i32), mapAtUnsafe<i32, i32>(direct, 3i32)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 16);
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
  [Buffer<i32>] direct{try(make_buffer())}
  [Buffer<i32>] mappedValue{
    try(Result.map(make_buffer(), []([Buffer<i32>] value) { return(value) }))
  }
  [Buffer<i32>] chainedValue{
    try(Result.and_then(make_buffer(), []([Buffer<i32>] value) { return(Result.ok(value)) }))
  }
  [Buffer<i32>] combinedValue{
    try(Result.map2(make_buffer(), make_buffer(), []([Buffer<i32>] left, [Buffer<i32>] right) { return(right) }))
  }
  [array<i32>] directOut{direct.readback()}
  [array<i32>] mappedOut{mappedValue.readback()}
  [array<i32>] chainedOut{chainedValue.readback()}
  [array<i32>] combinedOut{combinedValue.readback()}
  return(plus(plus(direct.count(), mappedOut[0i32]), plus(chainedValue.count(), combinedOut[2i32])))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.ok with supported payload values") != std::string::npos);
}
