#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.result_helpers");

TEST_CASE("stdlib image error result helpers construct status and value results") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] status{imageErrorStatus(imageReadUnsupported())}
  [Result<ImageError>] directStatus{/ImageError/status(imageReadUnsupported())}
  [Result<ImageError>] methodStatus{err.status()}
  [Result<i32, ImageError>] valueStatus{imageErrorResult<i32>(imageInvalidOperation())}
  [Result<i32, ImageError>] directValueStatus{/ImageError/result<i32>(imageInvalidOperation())}
  [Result<i32, ImageError>] methodValueStatus{err.result<i32>()}
  [Result<ImageError>] wrappedStatus{/ImageError/status(err)}
  [Result<i32, ImageError>] wrappedValue{/ImageError/result<i32>(err)}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] methodStatusError{Result.error(methodStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] methodValueError{Result.error(methodValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] directWhy{/ImageError/why(err)}
  [string] receiverWhy{err.why()}
  [string] statusWhy{Result.why(status)}
  [string] directStatusWhy{Result.why(directStatus)}
  [string] methodStatusWhy{Result.why(methodStatus)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] methodValueWhy{Result.why(methodValueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(and(and(statusError, directStatusError), methodStatusError), wrappedStatusError),
         and(and(and(valueError, directValueError), methodValueError), wrappedValueError)),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib ImageError why wrapper covers direct and Result-based access") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{imageReadUnsupported()}
  [string] direct{/ImageError/why(err)}
  [string] viaType{ImageError.why(err)}
  [string] viaReceiver{err.why()}
  [string] viaResult{Result.why(imageErrorStatus(err))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib ImageError constructor wrappers expose type-owned error values") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] readErr{/ImageError/read_unsupported()}
  [ImageError] writeErr{/ImageError/write_unsupported()}
  [ImageError] invalidErr{/ImageError/invalid_operation()}
  [string] readWhy{Result.why(imageErrorStatus(readErr))}
  [string] writeWhy{Result.why(imageErrorStatus(writeErr))}
  [string] invalidWhy{Result.why(imageErrorStatus(invalidErr))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib ImageError why wrapper rejects non image errors") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [string] why{/ImageError/why(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /ImageError/why parameter err") != std::string::npos);
}

TEST_CASE("stdlib ImageError constructor wrappers reject unexpected arguments") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{/ImageError/read_unsupported(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /ImageError/read_unsupported") != std::string::npos);
}

TEST_CASE("stdlib image error result helpers reject non image errors") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [Result<i32, ImageError>] status{/ImageError/result<i32>(true)}
  return()
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /ImageError/result__") != std::string::npos);
  CHECK(error.find("parameter err") != std::string::npos);
}

TEST_CASE("stdlib ImageError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/image/ImageError/status") != std::string::npos);
}

TEST_CASE("stdlib container error result helpers construct status and value results") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] err{containerMissingKey()}
  [Result<ContainerError>] status{containerErrorStatus(containerMissingKey())}
  [Result<ContainerError>] directStatus{/ContainerError/status(containerMissingKey())}
  [Result<i32, ContainerError>] valueStatus{containerErrorResult<i32>(containerCapacityExceeded())}
  [Result<i32, ContainerError>] directValueStatus{/ContainerError/result<i32>(containerCapacityExceeded())}
  [Result<ContainerError>] wrappedStatus{/ContainerError/status(err)}
  [Result<i32, ContainerError>] wrappedValue{/ContainerError/result<i32>(err)}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] direct{/ContainerError/why(err)}
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

TEST_CASE("stdlib ContainerError camelCase constructor helpers expose type-owned error values") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] missing{ContainerError.missingKey()}
  [ContainerError] oob{ContainerError.indexOutOfBounds()}
  [ContainerError] emptyErr{ContainerError.empty()}
  [ContainerError] capacity{ContainerError.capacityExceeded()}
  [ContainerError] rootMissing{/ContainerError/missingKey()}
  [ContainerError] rootOob{/ContainerError/indexOutOfBounds()}
  [ContainerError] rootCapacity{/ContainerError/capacityExceeded()}
  [ContainerError] compatMissing{/ContainerError/missing_key()}
  [ContainerError] compatOob{/ContainerError/index_out_of_bounds()}
  [ContainerError] compatCapacity{/ContainerError/capacity_exceeded()}
  [string] missingWhy{Result.why(containerErrorStatus(missing))}
  [string] oobWhy{Result.why(containerErrorStatus(oob))}
  [string] emptyWhy{Result.why(containerErrorStatus(emptyErr))}
  [string] capacityWhy{Result.why(containerErrorStatus(capacity))}
  [string] rootMissingWhy{Result.why(containerErrorStatus(rootMissing))}
  [string] rootOobWhy{Result.why(containerErrorStatus(rootOob))}
  [string] rootCapacityWhy{Result.why(containerErrorStatus(rootCapacity))}
  [string] compatMissingWhy{Result.why(containerErrorStatus(compatMissing))}
  [string] compatOobWhy{Result.why(containerErrorStatus(compatOob))}
  [string] compatCapacityWhy{Result.why(containerErrorStatus(compatCapacity))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("exact ContainerError import keeps method helpers") {
  const std::string source = R"(
import /std/collections/ContainerError

[return<void>]
main() {
  [ContainerError] err{ContainerError.missingKey()}
  [Result<ContainerError>] status{err.status()}
  [Result<i32, ContainerError>] value{err.result<i32>()}
  [string] whyText{err.why()}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib container error status helper rejects non container errors") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [Result<ContainerError>] status{containerErrorStatus(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/containerErrorStatus parameter err") != std::string::npos);
}

TEST_CASE("stdlib ContainerError status helper rejects non container errors") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [Result<ContainerError>] status{ContainerError.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "argument type mismatch for /std/collections/ContainerError/status parameter err") !=
        std::string::npos);
}

TEST_CASE("stdlib ContainerError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] err{containerMissingKey()}
  [Result<ContainerError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/ContainerError/status") !=
        std::string::npos);
}

TEST_CASE("stdlib ContainerError camelCase constructor helpers reject unexpected arguments") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] err{ContainerError.missingKey(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/ContainerError/missingKey") !=
        std::string::npos);
}

TEST_CASE("stdlib ContainerError why wrapper rejects non container errors") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [string] why{/ContainerError/why(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /ContainerError/why parameter err") != std::string::npos);
}

TEST_SUITE_END();
