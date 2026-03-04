#include "IrLowererStructLayoutHelpers.h"

#include <algorithm>
#include <cctype>
#include <limits>

#include "IrLowererStructTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string normalizeLayoutTypeName(const std::string &name) {
  if (name == "int") {
    return "i32";
  }
  if (name == "float") {
    return "f32";
  }
  return name;
}

bool classifyBindingTypeLayoutInternal(const LayoutFieldBinding &binding,
                                       BindingTypeLayout &layoutOut,
                                       std::string &structTypeNameOut,
                                       std::string &errorOut) {
  const std::string normalized = normalizeLayoutTypeName(binding.typeName);
  if (normalized == "i32" || normalized == "f32") {
    layoutOut = {4u, 4u};
    structTypeNameOut.clear();
    return true;
  }
  if (normalized == "i64" || normalized == "u64" || normalized == "f64") {
    layoutOut = {8u, 8u};
    structTypeNameOut.clear();
    return true;
  }
  if (normalized == "bool") {
    layoutOut = {1u, 1u};
    structTypeNameOut.clear();
    return true;
  }
  if (normalized == "string") {
    layoutOut = {8u, 8u};
    structTypeNameOut.clear();
    return true;
  }
  if (binding.typeName == "uninitialized") {
    if (binding.typeTemplateArg.empty()) {
      errorOut = "uninitialized requires a template argument for layout";
      return false;
    }
    std::string base;
    std::string arg;
    LayoutFieldBinding unwrapped = binding;
    if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
      unwrapped.typeName = base;
      unwrapped.typeTemplateArg = arg;
    } else {
      unwrapped.typeName = binding.typeTemplateArg;
      unwrapped.typeTemplateArg.clear();
    }
    return classifyBindingTypeLayoutInternal(unwrapped, layoutOut, structTypeNameOut, errorOut);
  }
  if (binding.typeName == "Pointer" || binding.typeName == "Reference") {
    layoutOut = {8u, 8u};
    structTypeNameOut.clear();
    return true;
  }
  if (binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "map") {
    layoutOut = {8u, 8u};
    structTypeNameOut.clear();
    return true;
  }

  structTypeNameOut = normalized;
  return true;
}

} // namespace

uint32_t alignTo(uint32_t value, uint32_t alignment) {
  if (alignment == 0) {
    return value;
  }
  const uint32_t remainder = value % alignment;
  if (remainder == 0) {
    return value;
  }
  return value + (alignment - remainder);
}

bool parsePositiveInt(const std::string &text, int &valueOut) {
  std::string digits = text;
  if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
    digits.resize(digits.size() - 3);
  }
  if (digits.empty()) {
    return false;
  }
  int parsed = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    const int digit = c - '0';
    if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
  }
  if (parsed <= 0) {
    return false;
  }
  valueOut = parsed;
  return true;
}

bool extractAlignment(const std::vector<Transform> &transforms,
                      const std::string &context,
                      uint32_t &alignmentOut,
                      bool &hasAlignment,
                      std::string &error) {
  alignmentOut = 1;
  hasAlignment = false;
  for (const auto &transform : transforms) {
    if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
      continue;
    }
    if (hasAlignment) {
      error = "duplicate " + transform.name + " transform on " + context;
      return false;
    }
    if (transform.arguments.size() != 1 || !transform.templateArgs.empty()) {
      error = transform.name + " requires exactly one integer argument";
      return false;
    }
    int value = 0;
    if (!parsePositiveInt(transform.arguments[0], value)) {
      error = transform.name + " requires a positive integer argument";
      return false;
    }
    uint64_t bytes = static_cast<uint64_t>(value);
    if (transform.name == "align_kbytes") {
      bytes *= 1024ull;
    }
    if (bytes > std::numeric_limits<uint32_t>::max()) {
      error = transform.name + " alignment too large on " + context;
      return false;
    }
    alignmentOut = static_cast<uint32_t>(bytes);
    hasAlignment = true;
  }
  return true;
}

bool classifyBindingTypeLayout(const LayoutFieldBinding &binding,
                               BindingTypeLayout &layoutOut,
                               std::string &structTypeNameOut,
                               std::string &errorOut) {
  structTypeNameOut.clear();
  errorOut.clear();
  return classifyBindingTypeLayoutInternal(binding, layoutOut, structTypeNameOut, errorOut);
}

bool appendStructLayoutField(const std::string &structPath,
                             const Expr &fieldExpr,
                             const LayoutFieldBinding &binding,
                             const std::function<bool(const LayoutFieldBinding &,
                                                      BindingTypeLayout &,
                                                      std::string &)> &resolveTypeLayout,
                             uint32_t &offsetInOut,
                             uint32_t &structAlignmentInOut,
                             IrStructLayout &layoutOut,
                             std::string &errorOut) {
  if (isStaticField(fieldExpr)) {
    IrStructField field;
    field.name = fieldExpr.name;
    field.envelope = formatLayoutFieldEnvelope(binding);
    field.offsetBytes = 0;
    field.sizeBytes = 0;
    field.alignmentBytes = 1;
    field.paddingKind = IrStructPaddingKind::None;
    field.category = fieldCategory(fieldExpr);
    field.visibility = fieldVisibility(fieldExpr);
    field.isStatic = true;
    layoutOut.fields.push_back(std::move(field));
    return true;
  }

  BindingTypeLayout typeLayout;
  if (!resolveTypeLayout(binding, typeLayout, errorOut)) {
    return false;
  }

  uint32_t explicitFieldAlign = 1;
  bool hasFieldAlign = false;
  const std::string fieldContext = "field " + structPath + "/" + fieldExpr.name;
  if (!extractAlignment(fieldExpr.transforms, fieldContext, explicitFieldAlign, hasFieldAlign, errorOut)) {
    return false;
  }
  if (hasFieldAlign && explicitFieldAlign < typeLayout.alignmentBytes) {
    errorOut = "alignment requirement on " + fieldContext + " is smaller than required alignment of " +
               std::to_string(typeLayout.alignmentBytes);
    return false;
  }

  const uint32_t fieldAlign = hasFieldAlign ? std::max(explicitFieldAlign, typeLayout.alignmentBytes)
                                            : typeLayout.alignmentBytes;
  const uint32_t alignedOffset = alignTo(offsetInOut, fieldAlign);

  IrStructField field;
  field.name = fieldExpr.name;
  field.envelope = formatLayoutFieldEnvelope(binding);
  field.offsetBytes = alignedOffset;
  field.sizeBytes = typeLayout.sizeBytes;
  field.alignmentBytes = fieldAlign;
  field.paddingKind = (alignedOffset != offsetInOut) ? IrStructPaddingKind::Align : IrStructPaddingKind::None;
  field.category = fieldCategory(fieldExpr);
  field.visibility = fieldVisibility(fieldExpr);
  field.isStatic = false;
  layoutOut.fields.push_back(std::move(field));

  offsetInOut = alignedOffset + typeLayout.sizeBytes;
  structAlignmentInOut = std::max(structAlignmentInOut, fieldAlign);
  return true;
}

bool isLayoutQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "static" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "pod" ||
         name == "handle" || name == "gpu_lane";
}

IrStructFieldCategory fieldCategory(const Expr &expr) {
  bool hasPod = false;
  bool hasHandle = false;
  bool hasGpuLane = false;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "pod") {
      hasPod = true;
    } else if (transform.name == "handle") {
      hasHandle = true;
    } else if (transform.name == "gpu_lane") {
      hasGpuLane = true;
    }
  }
  if (hasPod) {
    return IrStructFieldCategory::Pod;
  }
  if (hasHandle) {
    return IrStructFieldCategory::Handle;
  }
  if (hasGpuLane) {
    return IrStructFieldCategory::GpuLane;
  }
  return IrStructFieldCategory::Default;
}

IrStructVisibility fieldVisibility(const Expr &expr) {
  IrStructVisibility visibility = IrStructVisibility::Public;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "public") {
      visibility = IrStructVisibility::Public;
    } else if (transform.name == "private") {
      visibility = IrStructVisibility::Private;
    }
  }
  return visibility;
}

bool isStaticField(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

} // namespace primec::ir_lowerer
