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

namespace {

std::string stripLeadingSlash(std::string_view text) {
  return std::string(!text.empty() && text.front() == '/' ? text.substr(1) : text);
}

} // namespace

ReceiverElementFamilyResult classifyReceiverElementFamilyJoint(
    const ReceiverElementFamilyJointInput &input,
    const ReceiverElementFamilyPredicates &predicates) {
  ReceiverElementFamilyResult result;
  result.normalizedElementBaseType = stripLeadingSlash(input.unwrappedElementType);

  const std::string rawBase = stripLeadingSlash(input.rawElementBaseType);

  // R1: string check - unconditional, no method-name/template-shape gating.
  if (input.unwrappedElementType == "string" || rawBase == "string") {
    result.family = ReceiverElementFamily::String;
    return result;
  }

  // R2 / R2b: FileError only commits when the method name matches; on a
  // mismatch it falls through (does NOT return/reject here) to whatever
  // the template-shape block and R7 would otherwise decide - which, for
  // the literal "FileError" base (never template-shaped in practice),
  // lands on StructOrUnknown by falling all the way through below.
  if (input.unwrappedElementType == "FileError") {
    const std::string_view m = input.normalizedMethodName;
    if (m == "why" || m == "is_eof" || m == "status" || m == "result") {
      result.family = ReceiverElementFamily::FileError;
      return result;
    }
    // R2b fallthrough: continue past this check, do not return.
  }

  // R3-R6b: template-shape gated. A non-template-shaped element type (even
  // a bare "Buffer"/"File") skips this entire block, per the Step 1a
  // "template-shape gating" quirk.
  if (input.isTemplateShaped) {
    const std::string elemBase = stripLeadingSlash(input.templateShapedBaseName);
    if (isVectorLikeCollectionBaseName(elemBase)) {
      result.family = ReceiverElementFamily::VectorLike;
      result.collectionBaseName = elemBase;
      return result;
    }
    if (predicates.isInternalSoaCollectionTypeName &&
        predicates.isInternalSoaCollectionTypeName(elemBase)) {
      result.family = ReceiverElementFamily::Soa;
      result.collectionBaseName = elemBase;
      return result;
    }
    if (elemBase == "Buffer") {
      if (isBufferAccessorMethodName(input.normalizedMethodName)) {
        result.family = ReceiverElementFamily::Buffer;
        return result;
      }
      // R4b fallthrough: continue past this check, do not return.
    } else if (predicates.isKeyValueSurfaceTypeName &&
               predicates.isKeyValueSurfaceTypeName(elemBase)) {
      // R5: no method-name gating.
      result.family = ReceiverElementFamily::KeyValue;
      return result;
    } else if (elemBase == "File") {
      if (isFileHandleMethodName(input.normalizedMethodName)) {
        result.family = ReceiverElementFamily::File;
        return result;
      }
      // R6b (method-mismatch case, distinct from the bare-non-template R6b
      // named in the header): continue past this check, do not return.
    }
  }

  // R7: primitive check - deliberately uses the *non-unwrapped* raw base
  // type text, matching production's asymmetry (see header comment).
  if (isPrimitiveReceiverElementTypeName(rawBase)) {
    result.family = ReceiverElementFamily::Primitive;
    return result;
  }

  // R8/R9: caller resolves the struct-type path (or reports unresolved).
  result.family = ReceiverElementFamily::StructOrUnknown;
  return result;
}

} // namespace primec
