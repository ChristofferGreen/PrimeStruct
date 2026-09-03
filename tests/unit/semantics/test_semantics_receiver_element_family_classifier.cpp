#include "third_party/doctest.h"

#include <string>
#include <string_view>

#include "primec/support/ReceiverElementFamilyClassifier.h"

TEST_SUITE_BEGIN("primestruct.semantics.receiver_element_family_classifier");

namespace {

using primec::ReceiverElementFamily;
using primec::ReceiverElementFamilyPredicates;
using primec::classifyReceiverElementFamily;

ReceiverElementFamilyPredicates alwaysFalsePredicates() {
  return ReceiverElementFamilyPredicates{
      [](std::string_view) { return false; },
      [](std::string_view) { return false; },
  };
}

} // namespace

TEST_CASE("classifier recognizes string by exact and slash-prefixed spelling") {
  auto predicates = alwaysFalsePredicates();
  CHECK(classifyReceiverElementFamily("string", predicates).family ==
        ReceiverElementFamily::String);
  CHECK(classifyReceiverElementFamily("/string", predicates).family ==
        ReceiverElementFamily::String);
}

TEST_CASE("classifier recognizes FileError") {
  auto predicates = alwaysFalsePredicates();
  CHECK(classifyReceiverElementFamily("FileError", predicates).family ==
        ReceiverElementFamily::FileError);
}

TEST_CASE("classifier recognizes vector/array as VectorLike with base name") {
  auto predicates = alwaysFalsePredicates();
  auto vectorResult = classifyReceiverElementFamily("vector<i32>", predicates);
  CHECK(vectorResult.family == ReceiverElementFamily::VectorLike);
  CHECK(vectorResult.collectionBaseName == "vector");

  auto arrayResult = classifyReceiverElementFamily("array<i32>", predicates);
  CHECK(arrayResult.family == ReceiverElementFamily::VectorLike);
  CHECK(arrayResult.collectionBaseName == "array");
}

TEST_CASE("classifier defers soa membership to the stage predicate") {
  ReceiverElementFamilyPredicates predicates{
      [](std::string_view name) { return name == "soa"; },
      [](std::string_view) { return false; },
  };
  CHECK(classifyReceiverElementFamily("soa<i32>", predicates).family ==
        ReceiverElementFamily::Soa);
  CHECK(classifyReceiverElementFamily("notsoa<i32>", predicates).family !=
        ReceiverElementFamily::Soa);
}

TEST_CASE("classifier recognizes Buffer") {
  auto predicates = alwaysFalsePredicates();
  CHECK(classifyReceiverElementFamily("Buffer", predicates).family ==
        ReceiverElementFamily::Buffer);
}

TEST_CASE("classifier defers key-value membership to the stage predicate") {
  ReceiverElementFamilyPredicates predicates{
      [](std::string_view) { return false; },
      [](std::string_view name) { return name == "map"; },
  };
  CHECK(classifyReceiverElementFamily("map<i32, i32>", predicates).family ==
        ReceiverElementFamily::KeyValue);
  CHECK(classifyReceiverElementFamily("notmap<i32, i32>", predicates).family !=
        ReceiverElementFamily::KeyValue);
}

TEST_CASE("classifier recognizes File") {
  auto predicates = alwaysFalsePredicates();
  CHECK(classifyReceiverElementFamily("File", predicates).family ==
        ReceiverElementFamily::File);
}

TEST_CASE("classifier recognizes primitive names") {
  auto predicates = alwaysFalsePredicates();
  for (const char *name : {"int", "i32", "i64", "u64", "float", "f32", "f64",
                            "integer", "decimal", "complex", "bool", "auto"}) {
    CAPTURE(name);
    CHECK(classifyReceiverElementFamily(name, predicates).family ==
          ReceiverElementFamily::Primitive);
  }
}

TEST_CASE("classifier falls back to StructOrUnknown for a struct-typed element") {
  auto predicates = alwaysFalsePredicates();
  auto result = classifyReceiverElementFamily("/MyStruct", predicates);
  CHECK(result.family == ReceiverElementFamily::StructOrUnknown);
  CHECK(result.normalizedElementBaseType == "MyStruct");
}

TEST_CASE("name-set predicates match the historical literal lists") {
  CHECK(primec::isVectorLikeCollectionBaseName("vector"));
  CHECK(primec::isVectorLikeCollectionBaseName("array"));
  CHECK_FALSE(primec::isVectorLikeCollectionBaseName("map"));

  for (const char *name : {"count", "empty", "is_valid", "readback", "load", "store"}) {
    CAPTURE(name);
    CHECK(primec::isBufferAccessorMethodName(name));
  }
  CHECK_FALSE(primec::isBufferAccessorMethodName("push"));

  for (const char *name : {"write", "writeLine", "write_line", "writeByte",
                            "write_byte", "readByte", "read_byte", "writeBytes",
                            "write_bytes", "flush", "close"}) {
    CAPTURE(name);
    CHECK(primec::isFileHandleMethodName(name));
  }
  CHECK_FALSE(primec::isFileHandleMethodName("open"));

  for (const char *name : {"int", "i32", "i64", "u64", "float", "f32", "f64",
                            "integer", "decimal", "complex", "bool", "string",
                            "auto"}) {
    CAPTURE(name);
    CHECK(primec::isPrimitiveReceiverElementTypeName(name));
  }
  CHECK_FALSE(primec::isPrimitiveReceiverElementTypeName("Buffer"));
}

TEST_SUITE_END();
