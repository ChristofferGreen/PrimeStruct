#include "third_party/doctest.h"

#include "primec/SemanticProduct.h"

#include <string>

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_packs");

TEST_CASE("type pack index resolves return type and field access") {
  const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}

[return<Ts[Index]>]
get<Index, Ts...>([Tuple<Ts...>] values) {
  return(pack_at<Index, values>(values))
}

[return<i32>]
main() {
  [auto] tuple{Tuple<i32, bool>{7i32, true}}
  return(get<0, i32, bool>(tuple))
}
)";

  std::string error;
  primec::Program program;
  INFO(error);
  REQUIRE(validateProgramCapturingProgram(source, "/main", error, program));

  const primec::Definition *getSpecialization = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind("/get__t", 0) == 0) {
      getSpecialization = &def;
      break;
    }
  }
  REQUIRE(getSpecialization != nullptr);
  REQUIRE(getSpecialization->transforms.size() == 1);
  REQUIRE(getSpecialization->transforms[0].templateArgs.size() == 1);
  CHECK(getSpecialization->transforms[0].templateArgs[0] == "i32");
  REQUIRE(getSpecialization->statements.size() == 1);
  REQUIRE(getSpecialization->statements[0].args.size() == 1);
  const primec::Expr &fieldAccess = getSpecialization->statements[0].args[0];
  CHECK(fieldAccess.isFieldAccess);
  CHECK(fieldAccess.name == "__pack_values_0");
}

TEST_CASE("type pack index supports borrowed field access") {
  const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}

[return<Reference<Ts[Index]>>]
get_ref<Index, Ts...>([Reference<Tuple<Ts...>>] values) {
  return(pack_at<Index, values>(values))
}

[return<i32>]
main() {
  [auto] tuple{Tuple<i32, bool>{7i32, true}}
  [auto] picked{get_ref<1, i32, bool>(location(tuple))}
  return(0i32)
}
)";

  std::string error;
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("type pack index reports invalid index diagnostics") {
  {
    const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}

[return<Ts[Index]>]
bad<Index, Ts...>() {
  return(0i32)
}

[return<i32>]
main() {
  return(bad<NotInteger, i32>())
}
)";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("type-pack index must resolve to an integer template argument: Index") !=
          std::string::npos);
  }

  {
    const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}

[return<Ts[Index]>]
bad<Index, Ts...>() {
  return(0i32)
}

[return<i32>]
main() {
  return(bad<2, i32, bool>())
}
)";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("type-pack index out of range: Ts[Index] has 2 elements") !=
          std::string::npos);
  }

  {
    const std::string source = R"(
[return<T[Index]>]
bad<Index, T>() {
  return(0i32)
}

[return<i32>]
main() {
  return(bad<0, i32>())
}
)";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("unknown type-pack parameter for type index: T") !=
          std::string::npos);
  }
}

TEST_SUITE_END();
