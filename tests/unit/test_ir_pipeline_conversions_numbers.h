  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer supports string vector literals") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(vector<string>("a"raw_utf8)))
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
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

TEST_CASE("ir lowerer supports packed error struct Result payloads") {
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
  [Result<ContainerError, FileError>] mappedContainer{
    Result.map(Result.ok(/ContainerError/missing_key()),
      []([ContainerError] value) { return(/ContainerError/capacity_exceeded()) })
  }
  [Result<ImageError, FileError>] chainedImage{
    Result.and_then(Result.ok(/ImageError/read_unsupported()),
      []([ImageError] value) { return(Result.ok(/ImageError/invalid_operation())) })
  }
  [Result<GfxError, FileError>] summedGfx{
    Result.map2(Result.ok(GfxError.frame_acquire_failed()),
      Result.ok(GfxError.queue_submit_failed()),
      []([GfxError] left, [GfxError] right) { return(right) })
  }
  [ContainerError] container{try(mappedContainer)}
  [ImageError] image{try(chainedImage)}
  [GfxError] gfx{try(summedGfx)}
  print_line(container.why())
  print_line(image.why())
  print_line(gfx.why())
  return(plus(container.code, plus(image.code, gfx.code)))
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
  CHECK(result == 15);
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
  print_line(try(Result.map(make_label(2i32), []([Label] value) {
    return(Label([code] plus(value.code, 5i32)))
  })).code)
  print_line(try(Result.and_then(make_label(2i32), []([Label] value) {
    return(Result.ok(Label([code] plus(value.code, 3i32))))
  })).code)
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

TEST_CASE("ir lowerer supports File Result payload combinators") {
  const std::string source = R"(
import /std/file/*

[return<Result<File<Read>, FileError>> effects(file_read)]
open_file([string] path) {
  [File<Read>] file{ File<Read>(path)? }
  return(Result.ok(file))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(file_read, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<File<Read>, FileError>] mapped{
    Result.map(open_file("input.txt"utf8), []([File<Read>] file) { return(file) })
  }
  [Result<File<Read>, FileError>] chained{
    Result.and_then(open_file("input.txt"utf8), []([File<Read>] file) { return(Result.ok(file)) })
  }
  [Result<File<Read>, FileError>] summed{
    Result.map2(open_file("input.txt"utf8), open_file("input.txt"utf8),
      []([File<Read>] left, [File<Read>] right) { return(left) })
  }
  [File<Read>] mappedFile{try(mapped)}
  [File<Read>] chainedFile{try(chained)}
  [File<Read>] summedFile{try(summed)}
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
  CHECK(result == 33);
}

TEST_CASE("ir lowerer supports array and vector Result payloads") {
  const std::string source = R"(
import /std/file/*

[return<Result<array<i32>, FileError>>]
make_numbers() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(values))
}

[return<Result<vector<i32>, FileError>>]
make_vector() {
  return(Result.ok(vector<i32>(4i32, 5i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [array<i32>] direct{try(make_numbers())}
  [array<i32>] mapped{try(Result.map(make_numbers(), []([array<i32>] values) {
    return(values)
  }))}
  [vector<i32>] chained{try(Result.and_then(make_vector(), []([vector<i32>] values) {
    return(Result.ok(values))
  }))}
  [array<i32>] summed{try(Result.map2(make_numbers(), make_numbers(), []([array<i32>] left, [array<i32>] right) {
    return(right)
  }))}
  print_line(count(direct))
  print_line(direct[0i32])
  print_line(mapped[1i32])
  print_line(chained[1i32])
  print_line(count(summed))
  print_line(summed[2i32])
  return(plus(direct[0i32], plus(mapped[1i32], plus(chained[1i32], summed[2i32]))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer supports map Result payloads") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[return<Result<map<i32, i32>, FileError>>]
make_values() {
  return(Result.ok(map<i32, i32>(1i32, 7i32, 3i32, 9i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [map<i32, i32>] direct{try(make_values())}
  [map<i32, i32>] mapped{try(Result.map(make_values(), []([map<i32, i32>] values) {
    return(values)
  }))}
  [map<i32, i32>] chained{try(Result.and_then(make_values(), []([map<i32, i32>] values) {
    return(Result.ok(values))
  }))}
  [map<i32, i32>] summed{try(Result.map2(make_values(), make_values(), []([map<i32, i32>] left, [map<i32, i32>] right) {
    return(right)
  }))}
  print_line(mapCount<i32, i32>(direct))
  print_line(try(direct.tryAt(1i32)))
  print_line(try(mapped.tryAt(3i32)))
  print_line(try(chained.tryAt(1i32)))
  print_line(mapCount<i32, i32>(summed))
  print_line(try(summed.tryAt(3i32)))
  return(plus(try(direct.tryAt(1i32)),
              plus(try(mapped.tryAt(3i32)),
                   plus(try(chained.tryAt(1i32)), try(summed.tryAt(3i32))))))
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
  CHECK(result == 32);
}

TEST_CASE("ir lowerer supports Buffer Result payloads") {
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
  [Buffer<i32>] mapped{try(Result.map(make_buffer(), []([Buffer<i32>] values) {
    return(values)
  }))}
  [Buffer<i32>] chained{try(Result.and_then(make_buffer(), []([Buffer<i32>] values) {
    return(Result.ok(values))
  }))}
  [Buffer<i32>] summed{try(Result.map2(make_buffer(), make_buffer(), []([Buffer<i32>] left, [Buffer<i32>] right) {
    return(right)
  }))}
  [array<i32>] directOut{direct.readback()}
  [array<i32>] mappedOut{mapped.readback()}
  [array<i32>] chainedOut{chained.readback()}
  [array<i32>] summedOut{summed.readback()}
  print_line(directOut.count())
  print_line(directOut[0i32])
  print_line(mappedOut[1i32])
  print_line(chainedOut[2i32])
  print_line(summedOut.count())
  print_line(summedOut[2i32])
  return(plus(directOut[0i32],
              plus(mappedOut[1i32],
                   plus(chainedOut[2i32], summedOut[2i32]))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer rejects f64 Result payloads that remain unsupported") {
  const std::string source = R"(
[return<int>]
main() {
  [auto] wrapped{Result.ok(0.5f64)}
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
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
      if(greater_than(adjusted, 2i32),
        then(){ return(Result.ok(multiply(adjusted, 3i32))) },
        else(){ return(Result.ok(0i32)) })
    })
  }
  return(try(chained))
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
  CHECK(result == 9);
}

TEST_CASE("ir lowerer rejects block-bodied Result.and_then wide payloads") {
  const std::string source = R"(
[return<int>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i64, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      [i32] adjusted{plus(value, 1i32)}
      if(greater_than(adjusted, 2i32),
        then(){ return(Result.ok(3i64)) },
        else(){ return(Result.ok(4i64)) })
    })
  }
  if(Result.error(chained), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  CHECK(result == 5);
}

TEST_CASE("ir lowerer supports direct Result combinator consumers") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
swallow_file_error([FileError] err) {}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /swallow_file_error>]
main() {
  [Result<i32, FileError>] failed{FileError.result<i32>(fileReadEof())}
  [i32] mapped{try(Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) }))}
  [i32] chained{try(Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) }))}
  [i32] summed{
    try(Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) }))
  }
  if(not(Result.error(Result.and_then(failed, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })))) {
    return(1i32)
  }
  if(not(equal(count(Result.why(Result.map(failed, []([i32] value) { return(multiply(value, 4i32)) }))), 3i32))) {
    return(2i32)
  }
  if(not(equal(count(Result.why(Result.map2(Result.ok(2i32), failed, []([i32] left, [i32] right) { return(plus(left, right)) }))), 3i32))) {
    return(3i32)
  }
  return(plus(plus(mapped, chained), summed))
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
  return(
    plus(
      plus(
        count(try(Result.map(Result.ok("alpha"utf8), []([string] value) { return(value) }))),
        count(try(Result.and_then(Result.ok("beta"utf8), []([string] value) { return(Result.ok(value)) })))
      ),
      count(try(Result.map2(Result.ok("gamma"utf8), Result.ok("delta"utf8), []([string] left, [string] right) {
        return(left)
      })))
    )
  )
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
  [Reader] reader{Reader()}
  return(
    plus(
      plus(
        count(try(Result.map(greeting(), []([string] value) { return(value) }))),
        count(try(Result.and_then(reader.read(), []([string] value) { return(Result.ok(value)) })))
      ),
      count(try(Result.map2(greeting(), reader.read(), []([string] left, [string] right) { return(left) })))
    )
  )
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
  [Reader] reader{Reader()}
  return(consume(greeting()))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
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

  primec::Expr leftParam = makeName("left");
  primec::Expr rightParam = makeName("right");
  primec::Expr returnLeft = makeCall("return", {makeName("left")});

  primec::Expr map2Lambda;
  map2Lambda.kind = primec::Expr::Kind::Call;
  map2Lambda.isLambda = true;
  map2Lambda.hasBodyArguments = true;
  map2Lambda.args = {leftParam, rightParam};
  map2Lambda.bodyArguments = {returnLeft};

  consumeIt->parameters.front().args = {
      makeCall("map2",
               {
                   makeName("Result"),
                   makeCall("greeting", {}),
                   makeCall("read", {makeName("reader")}, true),
                   map2Lambda,
               },
               true),
  };

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
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
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
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
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
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
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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

TEST_CASE("ir lowerer supports array access u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1u64])
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

TEST_CASE("ir lowerer supports array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
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

TEST_CASE("ir lowerer supports array unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1i32))
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

TEST_CASE("ir lowerer supports array unsafe access u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1u64))
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

TEST_CASE("ir lowerer supports entry args count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
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
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions[0].instructions.size() == 2);
  CHECK(module.functions[0].instructions[0].op == primec::IrOpcode::PushArgc);
  CHECK(module.functions[0].instructions[1].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error, 3));
  CHECK(error.empty());
