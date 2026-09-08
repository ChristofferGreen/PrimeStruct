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

// Step 1b: classifyReceiverElementFamilyJoint, pinning the two Step 1a
// quirks (R2b FileError method-name fallthrough, R6b template-shape
// gating) plus the parallel R4b/File-method-mismatch fallthroughs found
// while extending the classifier - see
// docs/ReceiverTargetResolutionConsolidation.md Row category A.

namespace {

using primec::ReceiverElementFamilyJointInput;
using primec::classifyReceiverElementFamilyJoint;

// Mirrors resolveArgsPackElementMethodTarget's own splitTemplateTypeName
// call for these tests: succeeds only for "Base<...>" shaped text.
ReceiverElementFamilyJointInput jointInputFor(std::string_view unwrapped,
                                              std::string_view method,
                                              std::string_view rawBase = {}) {
  ReceiverElementFamilyJointInput input;
  input.unwrappedElementType = unwrapped;
  input.rawElementBaseType = rawBase.empty() ? unwrapped : rawBase;
  input.normalizedMethodName = method;
  size_t open = unwrapped.find('<');
  if (open != std::string_view::npos && unwrapped.back() == '>') {
    input.isTemplateShaped = true;
    input.templateShapedBaseName = unwrapped.substr(0, open);
  }
  return input;
}

} // namespace

TEST_CASE("joint classifier: R1 string is unconditional, no method gating") {
  auto predicates = alwaysFalsePredicates();
  auto input = jointInputFor("string", "whatever_method");
  CHECK(classifyReceiverElementFamilyJoint(input, predicates).family ==
        ReceiverElementFamily::String);
}

TEST_CASE("joint classifier: R2 FileError with a recognized method resolves to FileError") {
  auto predicates = alwaysFalsePredicates();
  for (const char *method : {"why", "is_eof", "status", "result"}) {
    CAPTURE(method);
    auto input = jointInputFor("FileError", method);
    CHECK(classifyReceiverElementFamilyJoint(input, predicates).family ==
          ReceiverElementFamily::FileError);
  }
}

TEST_CASE("joint classifier: R2b FileError with an unrecognized method falls through, "
          "does not reject and does not stay FileError") {
  auto predicates = alwaysFalsePredicates();
  auto input = jointInputFor("FileError", "not_a_fileerror_method");
  auto result = classifyReceiverElementFamilyJoint(input, predicates);
  CHECK(result.family == ReceiverElementFamily::StructOrUnknown);
}

TEST_CASE("joint classifier: R3 vector/array/soa dispatch unconditionally on method name "
          "once template-shaped") {
  auto predicates = alwaysFalsePredicates();
  auto vectorInput = jointInputFor("vector<i32>", "push");
  auto vectorResult = classifyReceiverElementFamilyJoint(vectorInput, predicates);
  CHECK(vectorResult.family == ReceiverElementFamily::VectorLike);
  CHECK(vectorResult.collectionBaseName == "vector");

  ReceiverElementFamilyPredicates soaPredicates{
      [](std::string_view name) { return name == "soa"; },
      [](std::string_view) { return false; },
  };
  auto soaInput = jointInputFor("soa<i32>", "anything");
  CHECK(classifyReceiverElementFamilyJoint(soaInput, soaPredicates).family ==
        ReceiverElementFamily::Soa);
}

TEST_CASE("joint classifier: R4 Buffer with a recognized accessor method resolves to Buffer") {
  auto predicates = alwaysFalsePredicates();
  for (const char *method : {"count", "empty", "is_valid", "readback", "load", "store"}) {
    CAPTURE(method);
    auto input = jointInputFor("Buffer<i32>", method);
    CHECK(classifyReceiverElementFamilyJoint(input, predicates).family ==
          ReceiverElementFamily::Buffer);
  }
}

TEST_CASE("joint classifier: R4b Buffer<T> with an unrecognized method falls through to "
          "StructOrUnknown, not rejected and not stuck as Buffer") {
  auto predicates = alwaysFalsePredicates();
  auto input = jointInputFor("Buffer<i32>", "not_a_buffer_method");
  auto result = classifyReceiverElementFamilyJoint(input, predicates);
  CHECK(result.family == ReceiverElementFamily::StructOrUnknown);
}

TEST_CASE("joint classifier: R5 key-value has no method-name gating") {
  ReceiverElementFamilyPredicates predicates{
      [](std::string_view) { return false; },
      [](std::string_view name) { return name == "map"; },
  };
  auto input = jointInputFor("map<i32, i32>", "anything_at_all");
  CHECK(classifyReceiverElementFamilyJoint(input, predicates).family ==
        ReceiverElementFamily::KeyValue);
}

TEST_CASE("joint classifier: R6 File<T> with a recognized handle method resolves to File") {
  auto predicates = alwaysFalsePredicates();
  auto input = jointInputFor("File<i32>", "write");
  CHECK(classifyReceiverElementFamilyJoint(input, predicates).family ==
        ReceiverElementFamily::File);
}

TEST_CASE("joint classifier: R6 File<T> with an unrecognized method falls through to "
          "StructOrUnknown") {
  auto predicates = alwaysFalsePredicates();
  auto input = jointInputFor("File<i32>", "not_a_file_method");
  auto result = classifyReceiverElementFamilyJoint(input, predicates);
  CHECK(result.family == ReceiverElementFamily::StructOrUnknown);
}

TEST_CASE("joint classifier: R6b bare non-template Buffer/File never reach Buffer/File "
          "dispatch regardless of method name") {
  auto predicates = alwaysFalsePredicates();
  auto bufferInput = jointInputFor("Buffer", "count"); // "count" IS a Buffer accessor method
  auto bufferResult = classifyReceiverElementFamilyJoint(bufferInput, predicates);
  CHECK(bufferResult.family == ReceiverElementFamily::StructOrUnknown);

  auto fileInput = jointInputFor("File", "write"); // "write" IS a File handle method
  auto fileResult = classifyReceiverElementFamilyJoint(fileInput, predicates);
  CHECK(fileResult.family == ReceiverElementFamily::StructOrUnknown);
}

TEST_CASE("joint classifier: R7 primitive check uses the raw (non-unwrapped) base type text, "
          "reproducing production's Reference<T>-unwrap asymmetry verbatim") {
  auto predicates = alwaysFalsePredicates();
  // unwrapped == "i32" (as if Reference<i32> had already been unwrapped by
  // the caller for the family checks), but rawElementBaseType is still the
  // *wrapped* "Reference<i32>" text, matching resolveArgsPackElementMethodTarget's
  // own normalizedElemBaseType (computed pre-unwrap). Production's R7 check
  // runs against that wrapped text, so "Reference<i32>" is NOT primitive
  // even though the unwrapped element is - this must fall to StructOrUnknown,
  // not Primitive.
  auto input = jointInputFor("i32", "method_name", "Reference<i32>");
  auto result = classifyReceiverElementFamilyJoint(input, predicates);
  CHECK(result.family == ReceiverElementFamily::StructOrUnknown);

  // A directly (non-wrapped) primitive element does classify as Primitive.
  auto directInput = jointInputFor("i32", "method_name");
  CHECK(classifyReceiverElementFamilyJoint(directInput, predicates).family ==
        ReceiverElementFamily::Primitive);
}

TEST_CASE("joint classifier: struct-typed element falls back to StructOrUnknown") {
  auto predicates = alwaysFalsePredicates();
  auto input = jointInputFor("MyStruct", "any_method");
  auto result = classifyReceiverElementFamilyJoint(input, predicates);
  CHECK(result.family == ReceiverElementFamily::StructOrUnknown);
  CHECK(result.normalizedElementBaseType == "MyStruct");
}

TEST_SUITE_END();
