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
  CHECK(error.find("IR backends only support Result.map with 32-bit or string values") != std::string::npos);
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
  CHECK(error.find("native backend only supports Result.ok with 32-bit or string values") != std::string::npos);
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

TEST_CASE("ir lowerer supports Result.map2 builtin lambdas") {
  const std::string source = R"(
[return<int>]
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
  CHECK(error.find("IR backends only support Result.map2 with 32-bit or string values") != std::string::npos);
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
