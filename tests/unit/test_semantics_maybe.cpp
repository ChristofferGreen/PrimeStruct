#include "third_party/doctest.h"

#include <algorithm>
#include <string_view>

#include "primec/SemanticProduct.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.maybe");

namespace {
const std::string kMaybePrelude = R"(
[public sum]
Maybe<T> {
  none
  [T] some
}

[public return<Maybe<T>>]
some<T>([T] value) {
  [Maybe<T>] result{[some] value}
  return(result)
}

[public return<Maybe<T>>]
none<T>() {
  [Maybe<T>] result{}
  return(result)
}

[public return<bool>]
/Maybe/isEmpty<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(true)
    }
    some(value) {
      return(false)
    }
  })
}

[public return<bool>]
/Maybe/isSome<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(false)
    }
    some(value) {
      return(true)
    }
  })
}

[public return<bool>]
/Maybe/is_empty<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(true)
    }
    some(value) {
      return(false)
    }
  })
}

[public return<bool>]
/Maybe/is_some<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(false)
    }
    some(value) {
      return(true)
    }
  })
}
)";

bool captureStdlibMaybeSemanticProduct(const std::string &source,
                                       primec::CompilePipelineOutput &output,
                                       std::string &error) {
  const std::filesystem::path tempPath = makeTempSemanticSourcePath();
  {
    std::ofstream file(tempPath);
    if (!file) {
      error = "failed to write semantic test source";
      return false;
    }
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "native";
  options.wasmProfile = "wasi";
  options.defaultEffects = {"io_out", "io_err"};
  options.entryDefaultEffects = {"io_out", "io_err"};
  options.dumpStage = "semantic_product";
  applySemanticsCompilePipelineSemanticProductIntentForTesting(
      options, SemanticsCompilePipelineSemanticProductIntentForTesting::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, nullptr);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  if (ok && !output.hasSemanticProgram) {
    error = "compile pipeline did not publish semantic product";
    return false;
  }
  return ok;
}

const primec::SemanticProgramMethodCallTarget *findMaybeMethodTarget(
    const primec::SemanticProgram &semanticProgram,
    std::string_view methodName) {
  const auto targets = primec::semanticProgramMethodCallTargetView(semanticProgram);
  const auto it = std::find_if(
      targets.begin(),
      targets.end(),
      [methodName](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry != nullptr && entry->scopePath == "/main" && entry->methodName == methodName;
      });
  return it == targets.end() ? nullptr : *it;
}
} // namespace

TEST_CASE("maybe some constructs present sum value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{some<i32>(1i32)}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe none constructs default sum value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  return(pick(value) {
    none {
      return(1i32)
    }
    some(v) {
      return(0i32)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe default and explicit none are equivalent") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] defaultValue{}
  [Maybe<i32>] explicitValue{none<i32>()}
  [i32] left{pick(defaultValue) {
    none {
      1i32
    }
    some(v) {
      0i32
    }
  }}
  [i32] right{pick(explicitValue) {
    none {
      1i32
    }
    some(v) {
      0i32
    }
  }}
  return(left + right)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe some supports struct values") {
  const std::string source = kMaybePrelude + R"(
[struct]
Widget() {
  [i32] id{7i32}
}

[return<int>]
main() {
  [Maybe<Widget>] value{some<Widget>(Widget{})}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(widget) {
      return(widget.id)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe some returns from helper") {
  const std::string source = kMaybePrelude + R"(
[return<Maybe<i32>>]
make() {
  return(some<i32>(5i32))
}

[return<int>]
main() {
  [Maybe<i32>] value{make()}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe none used in branches") {
  const std::string source = kMaybePrelude + R"(
[return<Maybe<i32>>]
choose([bool] flag) {
  if(flag) {
    return(some<i32>(2i32))
  } else {
    return(none<i32>())
  }
}

[return<int>]
main() {
  [Maybe<i32>] value{choose(false)}
  return(pick(value) {
    none {
      return(1i32)
    }
    some(v) {
      return(0i32)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe helpers report empty and some") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  if(value.is_some()) {
    return(0i32)
  }
  if(value.is_empty()) {
    return(1i32)
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe camelCase helpers report empty and some") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  if(value.isSome()) {
    return(0i32)
  }
  if(value.isEmpty()) {
    return(1i32)
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib maybe import resolves specialized helper methods") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] empty{none<i32>()}
  [Maybe<i32>] value{some<i32>(1i32)}
  if(not(empty.is_empty())) {
    return(0i32)
  }
  if(not(value.isSome())) {
    return(0i32)
  }
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe direct constructor accepts explicit present variant") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{Maybe<i32>{[some] 1i32}}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe target-typed initializer accepts unique inferred payload") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{1i32}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe pick payload supports implicit helper inference") {
  const std::string source = kMaybePrelude + R"(
[return<T>]
identity<T>([T] value) {
  return(value)
}

[return<int>]
main() {
  [Maybe<i32>] value{some<i32>(3i32)}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(identity(v))
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib maybe helper methods publish rooted semantic-product targets") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] empty{none<i32>()}
  [Maybe<i32>] value{some<i32>(1i32)}
  if(not(empty.is_empty())) {
    return(0i32)
  }
  if(not(value.isSome())) {
    return(0i32)
  }
  return(1i32)
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  REQUIRE(captureStdlibMaybeSemanticProduct(source, output, error));
  CHECK(error.empty());

  const auto *snakeTarget = findMaybeMethodTarget(output.semanticProgram, "is_empty");
  REQUIRE(snakeTarget != nullptr);
  CHECK(primec::semanticProgramMethodCallTargetResolvedPath(
            output.semanticProgram, *snakeTarget) == "/Maybe/is_empty");

  const auto *camelTarget = findMaybeMethodTarget(output.semanticProgram, "isSome");
  REQUIRE(camelTarget != nullptr);
  CHECK(primec::semanticProgramMethodCallTargetResolvedPath(
            output.semanticProgram, *camelTarget) == "/Maybe/isSome");
}

TEST_CASE("imported stdlib maybe helpers validate namespaced sum bodies") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] present{some<i32>(9i32)}
  [Maybe<i32>] missing{none<i32>()}
  [i32] presentValue{pick(present) {
    none {
      0i32
    }
    some(value) {
      value
    }
  }}
  [i32] missingValue{pick(missing) {
    none {
      1i32
    }
    some(value) {
      value
    }
  }}
  return(presentValue + missingValue)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe mutable struct helpers are retired on sum values") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32> mut] value{none<i32>()}
  value.set(9i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK((error.find("unknown call target") != std::string::npos ||
         error.find("unknown method") != std::string::npos));
  CHECK(error.find("set") != std::string::npos);
}

TEST_SUITE_END();
