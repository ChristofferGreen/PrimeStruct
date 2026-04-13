#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.result_helpers");

TEST_CASE("stdlib gfx error result helpers construct status and value results") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [GfxError] valueErr{framePresentFailed()}
  [Result<GfxError>] status{GfxError.status(queueSubmitFailed())}
  [Result<GfxError>] wrappedStatus{GfxError.status(err)}
  [Result<i32, GfxError>] valueStatus{valueErr.result<i32>()}
  [Result<i32, GfxError>] wrappedValue{err.result<i32>()}
  [bool] statusError{Result.error(status)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] directWhy{Result.why(GfxError.status(err))}
  [string] statusWhy{Result.why(status)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(statusError, wrappedStatusError),
         and(valueError, wrappedValueError)),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental stdlib gfx package status alias is removed") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Result<GfxError>] status{gfxErrorStatus(queueSubmitFailed())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib GfxError status helper rejects non gfx errors") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Result<GfxError>] status{GfxError.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") !=
        std::string::npos);
}

TEST_CASE("stdlib GfxError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/gfx/experimental/GfxError/status") !=
        std::string::npos);
}

TEST_CASE("canonical stdlib gfx error result helpers construct status and value results") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [GfxError] valueErr{framePresentFailed()}
  [Result<GfxError>] status{GfxError.status(queueSubmitFailed())}
  [Result<GfxError>] directStatus{GfxError.status(queueSubmitFailed())}
  [Result<GfxError>] wrappedStatus{GfxError.status(err)}
  [Result<i32, GfxError>] valueStatus{valueErr.result<i32>()}
  [Result<i32, GfxError>] directValueStatus{valueErr.result<i32>()}
  [Result<i32, GfxError>] wrappedValue{err.result<i32>()}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] directWhy{GfxError.why(err)}
  [string] statusWhy{Result.why(status)}
  [string] directStatusWhy{Result.why(directStatus)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(and(statusError, directStatusError), wrappedStatusError),
         and(and(valueError, directValueError), wrappedValueError)),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx error helpers return explicit strings") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] methodStatus{GfxError.status(err)}
  [Result<i32, GfxError>] methodValueStatus{err.result<i32>()}
  [string] direct{GfxError.why(err)}
  [string] method{GfxError.why(err)}
  [string] receiver{err.why()}
  [string] viaResult{Result.why(GfxError.status(err))}
  [string] viaMethodStatus{Result.why(methodStatus)}
  [string] viaMethodValue{Result.why(methodValueStatus)}
  [string] viaDirectMethodStatus{Result.why(methodStatus)}
  [string] viaDirectMethodValue{Result.why(methodValueStatus)}
  if(and(greater_than(count(direct), 0i32),
         and(and(greater_than(count(method), 0i32), greater_than(count(receiver), 0i32)),
             and(and(greater_than(count(viaResult), 0i32),
                     greater_than(count(viaMethodStatus), 0i32)),
                 and(greater_than(count(viaMethodValue), 0i32),
                     and(greater_than(count(viaDirectMethodStatus), 0i32),
                         greater_than(count(viaDirectMethodValue), 0i32)))))),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib GfxError constructors expose type-owned error values") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] windowErr{windowCreateFailed()}
  [GfxError] deviceErr{deviceCreateFailed()}
  [GfxError] swapchainErr{swapchainCreateFailed()}
  [GfxError] meshErr{meshCreateFailed()}
  [GfxError] pipelineErr{pipelineCreateFailed()}
  [GfxError] materialErr{materialCreateFailed()}
  [GfxError] frameErr{frameAcquireFailed()}
  [GfxError] queueErr{queueSubmitFailed()}
  [GfxError] presentErr{framePresentFailed()}
  [string] windowWhy{Result.why(GfxError.status(windowErr))}
  [string] presentWhy{Result.why(GfxError.status(presentErr))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx package value alias is removed") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Result<i32, GfxError>] value{gfxErrorResult<i32>(framePresentFailed())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("canonical root GfxError compatibility wrappers are removed") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Result<GfxError>] status{/GfxError/status(queueSubmitFailed())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("canonical stdlib GfxError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/gfx/GfxError/status") != std::string::npos);
}

TEST_CASE("canonical stdlib GfxError constructors reject unexpected arguments") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{windowCreateFailed(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/gfx/windowCreateFailed") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx error why helper rejects non gfx errors") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [string] why{GfxError.why(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib gfx Buffer helpers cover experimental method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 3i32)}
  [i32] methodCount{fullBuffer.count()}
  [i32] directCount{/std/gfx/experimental/Buffer/count(emptyBuffer)}
  [bool] methodEmpty{emptyBuffer.empty()}
  [bool] directEmpty{/std/gfx/experimental/Buffer/empty(fullBuffer)}
  [bool] methodValid{fullBuffer.is_valid()}
  [bool] directValid{/std/gfx/experimental/Buffer/is_valid(emptyBuffer)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer helpers cover method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 3i32)}
  [i32] methodCount{fullBuffer.count()}
  [i32] directCount{/std/gfx/Buffer/count(emptyBuffer)}
  [bool] methodEmpty{emptyBuffer.empty()}
  [bool] directEmpty{/std/gfx/Buffer/empty(fullBuffer)}
  [bool] methodValid{fullBuffer.is_valid()}
  [bool] directValid{/std/gfx/Buffer/is_valid(emptyBuffer)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx Buffer readback helpers cover experimental method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/experimental/Buffer/readback(data)}
  return(plus(viaMethod.count(), viaDirect.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer readback helpers cover method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/Buffer/readback(data)}
  return(plus(viaMethod.count(), viaDirect.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx Buffer compute access helpers cover experimental method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[compute workgroup_size(1, 1, 1)]
/copy_values([Buffer<i32>] input, [Buffer<i32>] output) {
  [i32] viaMethod{input.load(0i32)}
  [i32] viaDirect{/std/gfx/experimental/Buffer/load(input, 0i32)}
  output.store(0i32, viaMethod)
  /std/gfx/experimental/Buffer/store(output, 1i32, viaDirect)
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

TEST_CASE("canonical stdlib gfx Buffer compute access helpers cover method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[compute workgroup_size(1, 1, 1)]
/copy_values([Buffer<i32>] input, [Buffer<i32>] output) {
  [i32] viaMethod{input.load(0i32)}
  [i32] viaDirect{/std/gfx/Buffer/load(input, 0i32)}
  output.store(0i32, viaMethod)
  /std/gfx/Buffer/store(output, 1i32, viaDirect)
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

TEST_CASE("experimental stdlib gfx Buffer arg-pack method receivers cover value, borrowed, and pointer packs") {
  const std::string source = R"(
import /std/gfx/experimental/*

[compute workgroup_size(1, 1, 1)]
/copy_values([args<Buffer<i32>>] values) {
  [i32] viaMethod{values.at(0i32).load(0i32)}
  values[0i32].store(0i32, viaMethod)
  return()
}

[compute workgroup_size(1, 1, 1)]
/copy_refs([args<Reference<Buffer<i32>>>] values) {
  [i32] viaMethod{dereference(values.at(0i32)).load(0i32)}
  dereference(values[0i32]).store(0i32, viaMethod)
  return()
}

[compute workgroup_size(1, 1, 1)]
/copy_ptrs([args<Pointer<Buffer<i32>>>] values) {
  [i32] viaMethod{dereference(values.at(0i32)).load(0i32)}
  dereference(values[0i32]).store(0i32, viaMethod)
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

TEST_CASE("stdlib gfx Buffer allocation helpers cover experimental slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/allocate<i32>(2i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer allocation helpers cover slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(2i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx Buffer upload helpers cover experimental slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(out.count(), out[0i32]))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer upload helpers cover slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Buffer<i32>] data{/std/gfx/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(out.count(), out[0i32]))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer Result try consumers validate with explicit typed bindings") {
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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  return(/std/gfx/Buffer/count(data))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/count") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer upload slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Buffer<i32>] data{/std/gfx/Buffer/upload(values)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/upload") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer readback slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  [array<i32>] out{/std/gfx/Buffer/readback(data)}
  return(out.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/readback") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer load slash-call helpers require stdlib import") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] data) {
  [i32] value{/std/gfx/Buffer/load(data, 0i32)}
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/load") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer store slash-call helpers require stdlib import") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] data) {
  /std/gfx/Buffer/store(data, 0i32, 1i32)
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/store") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer allocate slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(2i32)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/allocate") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer allocate helper rejects non-integer size") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(1.5f32)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer size requires integer expression") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer allocate helper rejects unsupported element type") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<string>] data{/std/gfx/Buffer/allocate<string>(1i32)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer requires numeric/bool element type") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer readback helper rejects unsupported element type") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<string>] data{Buffer<string>([token] 1i32, [elementCount] 1i32)}
  [array<string>] out{data.readback()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("readback requires numeric/bool element type") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer upload helper rejects unsupported element type") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<string>] values{array<string>("x"utf8)}
  [Buffer<string>] data{/std/gfx/Buffer/upload(values)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("upload requires numeric/bool element type") != std::string::npos);
}

TEST_SUITE_END();
