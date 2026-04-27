#include "IrLowererStructLayoutHelpers.h"

#include <algorithm>
#include <cctype>
#include <limits>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {

namespace {

bool isSpecializedExperimentalStructTypeName(const std::string &name) {
  std::string normalized = trimTemplateTypeText(name);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
         normalized.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

std::string normalizeLayoutTypeName(const std::string &name) {
  const std::string normalizedCollectionName = normalizeCollectionBindingTypeName(name);
  if (normalizedCollectionName != name) {
    return normalizedCollectionName;
  }
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
  if (binding.typeTemplateArg.empty() && isSpecializedExperimentalStructTypeName(binding.typeName)) {
    structTypeNameOut = trimTemplateTypeText(binding.typeName);
    if (!structTypeNameOut.empty() && structTypeNameOut.front() != '/') {
      structTypeNameOut.insert(structTypeNameOut.begin(), '/');
    }
    return true;
  }
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
  if (normalized == "uninitialized") {
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
  if (normalized == "Pointer" || normalized == "Reference") {
    layoutOut = {8u, 8u};
    structTypeNameOut.clear();
    return true;
  }
  if (normalized == "array" || normalized == "vector" || normalized == "map" || normalized == "soa_vector") {
    layoutOut = {8u, 8u};
    structTypeNameOut.clear();
    return true;
  }
  if (normalized == "Result") {
    if (binding.typeTemplateArg.empty()) {
      errorOut = "Result requires one or two template arguments";
      return false;
    }
    std::vector<std::string> args;
    if (!splitTemplateArgs(binding.typeTemplateArg, args) || (args.size() != 1 && args.size() != 2)) {
      errorOut = "Result requires one or two template arguments";
      return false;
    }
    layoutOut = (args.size() == 1) ? BindingTypeLayout{4u, 4u} : BindingTypeLayout{8u, 8u};
    structTypeNameOut.clear();
    return true;
  }

  structTypeNameOut = normalized;
  return true;
}

bool isStructLikeSemanticProductCategory(const std::string &category) {
  return category == "struct" || category == "pod" || category == "handle" ||
         category == "gpu_lane";
}

std::size_t countSemanticProductLayoutFields(const Definition &def) {
  std::size_t count = 0;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding && !isStaticField(stmt)) {
      ++count;
    }
  }
  return count;
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

bool resolveBindingTypeLayout(
    const LayoutFieldBinding &binding,
    const std::string &namespacePrefix,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const Definition &, IrStructLayout &, std::string &)> &computeStructLayout,
    BindingTypeLayout &layoutOut,
    std::string &errorOut) {
  BindingTypeLayout classifiedLayout;
  std::string structTypeName;
  if (!classifyBindingTypeLayout(binding, classifiedLayout, structTypeName, errorOut)) {
    return false;
  }
  if (structTypeName.empty()) {
    layoutOut = classifiedLayout;
    return true;
  }

  const std::string structPath = resolveStructTypePath(structTypeName, namespacePrefix);
  auto defIt = defMap.find(structPath);
  if (defIt == defMap.end() || defIt->second == nullptr) {
    errorOut = "unknown struct type for layout: " + structTypeName;
    return false;
  }

  IrStructLayout nested;
  if (!computeStructLayout(*defIt->second, nested, errorOut)) {
    return false;
  }

  layoutOut.sizeBytes = nested.totalSizeBytes;
  layoutOut.alignmentBytes = nested.alignmentBytes;
  return true;
}

bool computeStructLayoutWithCache(
    const std::string &structPath,
    std::unordered_map<std::string, IrStructLayout> &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    const std::function<bool(IrStructLayout &, std::string &)> &computeUncachedLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut) {
  auto cached = layoutCache.find(structPath);
  if (cached != layoutCache.end()) {
    layoutOut = cached->second;
    return true;
  }
  if (!layoutStack.insert(structPath).second) {
    errorOut = "recursive struct layout not supported: " + structPath;
    return false;
  }

  IrStructLayout computed;
  if (!computeUncachedLayout(computed, errorOut)) {
    layoutStack.erase(structPath);
    return false;
  }

  layoutCache.emplace(structPath, computed);
  layoutStack.erase(structPath);
  layoutOut = std::move(computed);
  return true;
}

bool validateSemanticProductStructLayoutCoverage(const Program &program,
                                                 const SemanticProgram *semanticProgram,
                                                 std::string &errorOut) {
  if (semanticProgram == nullptr) {
    return true;
  }

  std::unordered_map<std::string, const Definition *> defMap;
  defMap.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  std::unordered_set<std::string> semanticStructPaths;
  for (const auto *typeMetadata : semanticProgramStructTypeMetadataView(*semanticProgram)) {
    if (typeMetadata == nullptr || typeMetadata->fullPath.empty() ||
        !isStructLikeSemanticProductCategory(typeMetadata->category)) {
      continue;
    }
    if (!semanticStructPaths.insert(typeMetadata->fullPath).second) {
      errorOut = "duplicate semantic-product type metadata: " + typeMetadata->fullPath;
      return false;
    }

    const auto defIt = defMap.find(typeMetadata->fullPath);
    if (defIt == defMap.end() || defIt->second == nullptr) {
      errorOut = "missing semantic-product struct provenance: " + typeMetadata->fullPath;
      return false;
    }

    const Definition &def = *defIt->second;
    const std::size_t layoutFieldCount = countSemanticProductLayoutFields(def);
    if (typeMetadata->fieldCount != layoutFieldCount) {
      errorOut = "semantic-product struct field count mismatch: " + def.fullPath;
      return false;
    }

    const auto semanticFields =
        semanticProgramStructFieldMetadataView(*semanticProgram, def.fullPath);
    std::size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticField(stmt)) {
        continue;
      }
      const auto semanticFieldIt =
          std::find_if(semanticFields.begin(),
                       semanticFields.end(),
                       [&](const SemanticProgramStructFieldMetadata *fieldMetadata) {
                         return fieldMetadata != nullptr &&
                                fieldMetadata->fieldIndex == fieldIndex &&
                                fieldMetadata->fieldName == stmt.name;
                       });
      if (semanticFieldIt == semanticFields.end()) {
        errorOut = "missing semantic-product struct field metadata: " +
                   def.fullPath + "/" + stmt.name;
        return false;
      }
      if ((*semanticFieldIt)->bindingTypeText.empty()) {
        errorOut = "missing semantic-product struct field binding type: " +
                   def.fullPath + "/" + stmt.name;
        return false;
      }
      ++fieldIndex;
    }
    if (semanticFields.size() != layoutFieldCount) {
      errorOut = "semantic-product struct field count mismatch: " + def.fullPath;
      return false;
    }
  }

  for (const auto &def : program.definitions) {
    if (isStructDefinition(def) && semanticStructPaths.count(def.fullPath) == 0) {
      errorOut = "missing semantic-product type metadata: " + def.fullPath;
      return false;
    }
  }

  return true;
}

bool computeStructLayoutUncached(
    const Definition &def,
    const std::vector<LayoutFieldBinding> &fieldBindings,
    const std::function<bool(const LayoutFieldBinding &, BindingTypeLayout &, std::string &)> &resolveFieldTypeLayout,
    const SemanticProgramTypeMetadata *typeMetadata,
    IrStructLayout &layoutOut,
    std::string &errorOut) {
  layoutOut = IrStructLayout{};
  layoutOut.name = def.fullPath;
  uint32_t structAlign = 1;
  uint32_t explicitStructAlign = 1;
  bool hasStructAlign = false;
  if (typeMetadata != nullptr) {
    if (typeMetadata->hasExplicitAlignment) {
      explicitStructAlign = typeMetadata->explicitAlignmentBytes;
      hasStructAlign = true;
    }
  } else if (!extractAlignment(
                 def.transforms, "struct " + def.fullPath, explicitStructAlign, hasStructAlign, errorOut)) {
    return false;
  }

  uint32_t offset = 0;
  size_t fieldIndex = 0;
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      continue;
    }
    if (fieldIndex >= fieldBindings.size()) {
      errorOut = "internal error: mismatched struct field info for " + def.fullPath;
      return false;
    }

    const LayoutFieldBinding &binding = fieldBindings[fieldIndex];
    ++fieldIndex;
    if (!appendStructLayoutField(
            def.fullPath, stmt, binding, resolveFieldTypeLayout, offset, structAlign, layoutOut, errorOut)) {
      return false;
    }
  }

  if (hasStructAlign && explicitStructAlign < structAlign) {
    errorOut = "alignment requirement on struct " + def.fullPath + " is smaller than required alignment of " +
               std::to_string(structAlign);
    return false;
  }

  structAlign = hasStructAlign ? std::max(structAlign, explicitStructAlign) : structAlign;
  layoutOut.alignmentBytes = structAlign;
  layoutOut.totalSizeBytes = alignTo(offset, structAlign);
  return true;
}

bool computeStructLayoutFromFieldInfo(
    const Definition &def,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const Definition &, IrStructLayout &)> &computeStructLayout,
    const SemanticProgram *semanticProgram,
    IrStructLayout &layoutOut,
    std::string &errorOut) {
  auto fieldInfoIt = structFieldInfoByName.find(def.fullPath);
  if (fieldInfoIt == structFieldInfoByName.end()) {
    errorOut = "internal error: missing struct field info for " + def.fullPath;
    return false;
  }

  auto computeNestedStructLayout = [&](const Definition &nestedDef,
                                       IrStructLayout &nestedLayout,
                                       std::string &nestedError) -> bool {
    (void)nestedError;
    return computeStructLayout(nestedDef, nestedLayout);
  };
  auto resolveFieldTypeLayout = [&](const LayoutFieldBinding &fieldBinding,
                                    BindingTypeLayout &fieldTypeLayout,
                                    std::string &fieldError) -> bool {
    return resolveBindingTypeLayout(fieldBinding,
                                    def.namespacePrefix,
                                    resolveStructTypePath,
                                    defMap,
                                    computeNestedStructLayout,
                                    fieldTypeLayout,
                                    fieldError);
  };
  const SemanticProgramTypeMetadata *typeMetadata =
      semanticProgram == nullptr ? nullptr : semanticProgramLookupTypeMetadata(*semanticProgram, def.fullPath);
  if (semanticProgram != nullptr && typeMetadata == nullptr) {
    errorOut = "missing semantic-product type metadata: " + def.fullPath;
    return false;
  }
  return computeStructLayoutUncached(
      def, fieldInfoIt->second, resolveFieldTypeLayout, typeMetadata, layoutOut, errorOut);
}

bool appendProgramStructLayouts(
    const Program &program,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const SemanticProgram *semanticProgram,
    const std::function<bool(const Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut) {
  if (semanticProgram != nullptr) {
    for (const SemanticProgramTypeMetadata *typeMetadata :
         semanticProgramStructTypeMetadataView(*semanticProgram)) {
      if (typeMetadata == nullptr || typeMetadata->fullPath.empty()) {
        continue;
      }
      const auto defIt = defMap.find(typeMetadata->fullPath);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        errorOut = "internal error: missing struct provenance for " + typeMetadata->fullPath;
        return false;
      }
      IrStructLayout layout;
      if (!computeStructLayout(*defIt->second, layout)) {
        if (errorOut.empty()) {
          errorOut = "failed to compute struct layout: " + typeMetadata->fullPath;
        }
        return false;
      }
      layoutsOut.push_back(std::move(layout));
    }
    return true;
  }

  for (const auto &def : program.definitions) {
    if (!isStructDefinition(def, semanticProgram)) {
      continue;
    }
    IrStructLayout layout;
    if (!computeStructLayout(def, layout)) {
      if (errorOut.empty()) {
        errorOut = "failed to compute struct layout: " + def.fullPath;
      }
      return false;
    }
    layoutsOut.push_back(std::move(layout));
  }
  return true;
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
