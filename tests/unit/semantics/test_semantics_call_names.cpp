#include "third_party/doctest.h"

#include "primec/support/CallNames.h"

#include <string>

// The spellings of call names and scalar type names that semantics, the lowerer, the IR printer and the AST printer
// read the same way. The table pins them.

TEST_SUITE_BEGIN("primestruct.semantics.call_names");

TEST_CASE("a leading slash is trimmed once") {
  CHECK(primec::trimLeadingSlash("/std/math") == "std/math");
  CHECK(primec::trimLeadingSlash("std/math") == "std/math");
  CHECK(primec::trimLeadingSlash("//x") == "/x");
  CHECK(primec::trimLeadingSlash("/").empty());
  CHECK(primec::trimLeadingSlash("").empty());
}

TEST_CASE("a single-segment name has one optional leading slash and no other slash") {
  std::string name = "stale";
  CHECK(primec::readSingleSegmentName("return", name));
  CHECK(name == "return");
  CHECK(primec::readSingleSegmentName("/return", name));
  CHECK(name == "return");
  CHECK_FALSE(primec::readSingleSegmentName("/foo/return", name));
  CHECK(name.empty());
  CHECK_FALSE(primec::readSingleSegmentName("foo/return", name));
  CHECK_FALSE(primec::readSingleSegmentName("//return", name));
  CHECK(primec::readSingleSegmentName("", name));
  CHECK(name.empty());
}

TEST_CASE("a single-segment name matches its target only") {
  CHECK(primec::isSingleSegmentName("return", "return"));
  CHECK(primec::isSingleSegmentName("/return", "return"));
  CHECK_FALSE(primec::isSingleSegmentName("/foo/return", "return"));
  CHECK_FALSE(primec::isSingleSegmentName("returns", "return"));
  CHECK_FALSE(primec::isSingleSegmentName("", "return"));
}

TEST_CASE("scalar type spellings map to one kind each") {
  using primec::ScalarTypeKind;
  CHECK(primec::scalarTypeKindForName("int") == ScalarTypeKind::Int32);
  CHECK(primec::scalarTypeKindForName("i32") == ScalarTypeKind::Int32);
  CHECK(primec::scalarTypeKindForName("i64") == ScalarTypeKind::Int64);
  CHECK(primec::scalarTypeKindForName("u64") == ScalarTypeKind::UInt64);
  CHECK(primec::scalarTypeKindForName("u8") == ScalarTypeKind::UInt8);
  CHECK(primec::scalarTypeKindForName("bool") == ScalarTypeKind::Bool);
  CHECK(primec::scalarTypeKindForName("string") == ScalarTypeKind::String);
  CHECK(primec::scalarTypeKindForName("float") == ScalarTypeKind::Float32);
  CHECK(primec::scalarTypeKindForName("f32") == ScalarTypeKind::Float32);
  CHECK(primec::scalarTypeKindForName("f64") == ScalarTypeKind::Float64);
  CHECK(primec::scalarTypeKindForName("void") == ScalarTypeKind::Void);
  // `never`, error types and templates are each layer's own business.
  for (const char *name : {"never", "FileError", "Result<i32>", "array<i32>", "", "I32", "/i32"}) {
    CAPTURE(name);
    CHECK(primec::scalarTypeKindForName(name) == ScalarTypeKind::None);
  }
}

TEST_SUITE_END();
