// collection-surface-audit: exempt
#include "primec/support/ReceiverElementFamilyClassifier.h"

namespace primec {

bool isVectorLikeCollectionBaseName(std::string_view baseName) {
  return baseName == "vector" || baseName == "array";
}

bool isBufferAccessorMethodName(std::string_view methodName) {
  return methodName == "count" || methodName == "empty" ||
         methodName == "is_valid" || methodName == "readback" ||
         methodName == "load" || methodName == "store";
}

bool isFileHandleMethodName(std::string_view methodName) {
  // Mirrors SemanticsValidatorMethodTargetResolutionDetail.cpp's
  // isFileMethodName exactly (Step 1 - pure duplication, not yet migrated
  // onto this classifier; see docs/ReceiverTargetResolutionConsolidation.md).
  return methodName == "write" || methodName == "writeLine" ||
         methodName == "write_line" || methodName == "writeByte" ||
         methodName == "write_byte" || methodName == "readByte" ||
         methodName == "read_byte" || methodName == "writeBytes" ||
         methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

bool isPrimitiveReceiverElementTypeName(std::string_view name) {
  return name == "int" || name == "i32" || name == "i64" || name == "u64" ||
         name == "float" || name == "f32" || name == "f64" ||
         name == "integer" || name == "decimal" || name == "complex" ||
         name == "bool" || name == "string" || name == "auto";
}

ReceiverElementFamilyResult classifyReceiverElementFamily(
    std::string_view normalizedElementTypeText,
    const ReceiverElementFamilyPredicates &predicates) {
  ReceiverElementFamilyResult result;

  std::string_view baseType = normalizedElementTypeText;
  size_t templateOpen = baseType.find('<');
  std::string_view base =
      templateOpen == std::string_view::npos ? baseType : baseType.substr(0, templateOpen);

  std::string normalizedBase(base.front() == '/' ? base.substr(1) : base);
  result.normalizedElementBaseType =
      std::string(normalizedElementTypeText.front() == '/'
                       ? normalizedElementTypeText.substr(1)
                       : normalizedElementTypeText);

  if (normalizedElementTypeText == "string" || normalizedBase == "string") {
    result.family = ReceiverElementFamily::String;
    return result;
  }
  if (normalizedBase == "FileError") {
    result.family = ReceiverElementFamily::FileError;
    return result;
  }
  if (isVectorLikeCollectionBaseName(normalizedBase)) {
    result.family = ReceiverElementFamily::VectorLike;
    result.collectionBaseName = normalizedBase;
    return result;
  }
  if (predicates.isInternalSoaCollectionTypeName &&
      predicates.isInternalSoaCollectionTypeName(normalizedBase)) {
    result.family = ReceiverElementFamily::Soa;
    result.collectionBaseName = normalizedBase;
    return result;
  }
  if (normalizedBase == "Buffer") {
    result.family = ReceiverElementFamily::Buffer;
    return result;
  }
  if (predicates.isKeyValueSurfaceTypeName &&
      predicates.isKeyValueSurfaceTypeName(normalizedBase)) {
    result.family = ReceiverElementFamily::KeyValue;
    return result;
  }
  if (normalizedBase == "File") {
    result.family = ReceiverElementFamily::File;
    return result;
  }
  if (isPrimitiveReceiverElementTypeName(result.normalizedElementBaseType)) {
    result.family = ReceiverElementFamily::Primitive;
    return result;
  }
  result.family = ReceiverElementFamily::StructOrUnknown;
  return result;
}

} // namespace primec
