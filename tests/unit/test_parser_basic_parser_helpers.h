#pragma once

#include "test_parser_basic_helpers.h"

#include "src/parser/ParserHelpers.h"

TEST_CASE("parser helper recognizes core builtin names") {
  CHECK(primec::parser::isBuiltinName("assign", false));
  CHECK(primec::parser::isBuiltinName("take", false));
}

TEST_CASE("parser helper validates math builtin qualification") {
  CHECK(primec::parser::isBuiltinName("sin", true));
  CHECK_FALSE(primec::parser::isBuiltinName("sin", false));
  CHECK(primec::parser::isBuiltinName("/std/math/sin", false));
  CHECK_FALSE(primec::parser::isBuiltinName("std/math/not_builtin", false));
}

TEST_CASE("parser helper validates gpu builtin qualification") {
  CHECK(primec::parser::isBuiltinName("/std/gpu/dispatch", false));
  CHECK_FALSE(primec::parser::isBuiltinName("dispatch", false));
  CHECK_FALSE(primec::parser::isBuiltinName("std/gpu/not_builtin", false));
}

TEST_CASE("parser helper rejects non-builtin slash paths") {
  CHECK_FALSE(primec::parser::isBuiltinName("foo/bar", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/foo/bar", false));
  CHECK_FALSE(primec::parser::isBuiltinName("totally_unknown_name", false));
}
