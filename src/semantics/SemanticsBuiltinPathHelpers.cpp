#include "SemanticsHelpers.h"

#include <array>
#include <string_view>

namespace primec::semantics {

namespace {

bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

struct HelperSuffixInfo {
  std::string_view suffix;
  std::string_view placement;
};

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  const bool hasLeadingSlash = !normalized.empty() && normalized[0] == '/';
  if (hasLeadingSlash) {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (hasLeadingSlash) {
    out = normalized;
    return true;
  }
  if (!allowBare) {
    return false;
  }
  out = normalized;
  return true;
}

bool parseGpuName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/gpu/", 0) == 0) {
    out = normalized.substr(8);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool parseMemoryName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/intrinsics/memory/", 0) == 0) {
    out = normalized.substr(22);
    return true;
  }
  return false;
}

} // namespace

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinMutationName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "increment" || name == "decrement") {
    out = name;
    return true;
  }
  return false;
}

bool isRootBuiltinName(const std::string &name) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  std::string gpuName;
  if (parseGpuName(normalized, gpuName)) {
    return gpuName == "global_id_x" || gpuName == "global_id_y" || gpuName == "global_id_z";
  }
  std::string memoryName;
  if (parseMemoryName(normalized, memoryName)) {
    return memoryName == "alloc" || memoryName == "free" || memoryName == "realloc";
  }
  bool isStdGpuQualified = false;
  if (normalized.rfind("std/gpu/", 0) == 0) {
    normalized = normalized.substr(8);
    isStdGpuQualified = true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  Expr probe;
  probe.name = normalized;
  std::string builtinName;
  if (getBuiltinOperatorName(probe, builtinName) || getBuiltinComparisonName(probe, builtinName)) {
    return true;
  }
  if (normalized == "increment" || normalized == "decrement") {
    return true;
  }
  return normalized == "assign" || normalized == "move" || normalized == "if" || normalized == "then" ||
         normalized == "else" ||
         normalized == "loop" || normalized == "while" || normalized == "for" || normalized == "repeat" ||
         normalized == "return" || normalized == "array" || normalized == "vector" || normalized == "map" ||
         normalized == "File" || normalized == "try" || normalized == "count" || normalized == "capacity" ||
         normalized == "to_soa" || normalized == "to_aos" ||
         normalized == "to_aos_ref" ||
         normalized == "push" || normalized == "pop" ||
         normalized == "reserve" || normalized == "clear" || normalized == "remove_at" || normalized == "remove_swap" ||
         normalized == "at" || normalized == "at_unsafe" || normalized == "convert" ||
         normalized == "location" || normalized == "dereference" ||
         normalized == "block" || normalized == "print" || normalized == "print_line" ||
         normalized == "print_error" || normalized == "print_line_error" || normalized == "notify" ||
         normalized == "insert" || normalized == "take" ||
         (isStdGpuQualified &&
          (normalized == "dispatch" || normalized == "buffer" || normalized == "upload" || normalized == "readback" ||
           normalized == "buffer_load" || normalized == "buffer_store"));
}

bool getBuiltinClampName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  if (out == "lerp" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" || out == "fract" ||
      out == "sqrt" || out == "cbrt" || out == "pow" || out == "exp" || out == "exp2" || out == "log" ||
      out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" || out == "asin" ||
      out == "acos" || out == "atan" || out == "atan2" || out == "radians" || out == "degrees" || out == "sinh" ||
      out == "cosh" || out == "tanh" || out == "asinh" || out == "acosh" || out == "atanh" || out == "fma" ||
      out == "hypot" || out == "copysign" || out == "is_nan" || out == "is_inf" || out == "is_finite") {
    return true;
  }
  return false;
}

bool isBuiltinMathConstant(const std::string &name, bool allowBare) {
  std::string candidate;
  if (!parseMathName(name, candidate, allowBare)) {
    return false;
  }
  return candidate == "pi" || candidate == "tau" || candidate == "e";
}

bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverPath, std::string rawMethodName) {
  if (!rawMethodName.empty() && rawMethodName.front() == '/') {
    rawMethodName.erase(rawMethodName.begin());
  }

  std::string_view helperName;
  const bool isVectorFamilyReceiver = receiverPath == "/array" || receiverPath == "/vector";
  if (isVectorFamilyReceiver) {
    if (rawMethodName.rfind("array/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("array/").size());
    } else if (rawMethodName.rfind("std/collections/vector/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("std/collections/vector/").size());
    }
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(helperName);
  }

  if (receiverPath != "/map") {
    return false;
  }
  if (rawMethodName.rfind("map/", 0) == 0) {
    helperName = std::string_view(rawMethodName).substr(std::string_view("map/").size());
  } else if (rawMethodName.rfind("std/collections/map/", 0) == 0) {
    helperName = std::string_view(rawMethodName).substr(std::string_view("std/collections/map/").size());
  }
  return !helperName.empty() && isRemovedMapCompatibilityHelper(helperName);
}

bool isExplicitRemovedCollectionCallAlias(std::string rawPath) {
  if (!rawPath.empty() && rawPath.front() == '/') {
    rawPath.erase(rawPath.begin());
  }

  std::string_view helperName;
  if (rawPath.rfind("array/", 0) == 0) {
    helperName = std::string_view(rawPath).substr(std::string_view("array/").size());
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(helperName);
  }
  if (rawPath.rfind("map/", 0) == 0) {
    helperName = std::string_view(rawPath).substr(std::string_view("map/").size());
    return !helperName.empty() && isRemovedMapCompatibilityHelper(helperName);
  }
  return false;
}

bool isLifecycleHelperName(const std::string &fullPath) {
  static const std::array<HelperSuffixInfo, 10> suffixes = {{
      {"Create", ""},
      {"Destroy", ""},
      {"Copy", ""},
      {"Move", ""},
      {"CreateStack", "stack"},
      {"DestroyStack", "stack"},
      {"CreateHeap", "heap"},
      {"DestroyHeap", "heap"},
      {"CreateBuffer", "buffer"},
      {"DestroyBuffer", "buffer"},
  }};
  for (const auto &info : suffixes) {
    const std::string_view suffix = info.suffix;
    if (fullPath.size() < suffix.size() + 1) {
      continue;
    }
    const size_t suffixStart = fullPath.size() - suffix.size();
    if (fullPath[suffixStart - 1] != '/') {
      continue;
    }
    if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
      continue;
    }
    return true;
  }
  return false;
}

bool isMathBuiltinName(const std::string &name) {
  Expr probe;
  probe.name = name;
  std::string builtinName;
  return getBuiltinMathName(probe, builtinName, true) || getBuiltinClampName(probe, builtinName, true) ||
         getBuiltinMinMaxName(probe, builtinName, true) || getBuiltinAbsSignName(probe, builtinName, true) ||
         getBuiltinSaturateName(probe, builtinName, true) || isBuiltinMathConstant(name, true);
}

bool getBuiltinGpuName(const Expr &expr, std::string &out) {
  if (!parseGpuName(expr.name, out)) {
    return false;
  }
  return out == "global_id_x" || out == "global_id_y" || out == "global_id_z";
}

bool getBuiltinMemoryName(const Expr &expr, std::string &out) {
  if (!parseMemoryName(expr.name, out)) {
    return false;
  }
  return out == "alloc" || out == "free" || out == "realloc" || out == "at" || out == "at_unsafe" ||
         out == "reinterpret";
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name == "dereference" || name == "location" || name == "soa_vector/dereference" ||
      name == "soa_vector/location") {
    if (name == "soa_vector/dereference") {
      out = "dereference";
      return true;
    }
    if (name == "soa_vector/location") {
      out = "location";
      return true;
    }
    out = name;
    return true;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "convert") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = name.substr(std::string("map/").size());
    if (alias == "map") {
      out = "map";
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "array" || name == "vector" || name == "map" || name == "soa_vector") {
    out = name;
    return true;
  }
  return false;
}

std::string soaFieldViewHelperPath(std::string_view fieldName) {
  return "/std/collections/soa_vector/field_view/" + std::string(fieldName);
}

bool splitSoaFieldViewHelperPath(std::string_view path, std::string *fieldNameOut) {
  constexpr std::string_view kCanonicalSoaFieldViewPrefix =
      "/std/collections/soa_vector/field_view/";
  if (path.starts_with(kCanonicalSoaFieldViewPrefix)) {
    if (fieldNameOut != nullptr) {
      *fieldNameOut = std::string(path.substr(kCanonicalSoaFieldViewPrefix.size()));
    }
    return true;
  }
  return false;
}

std::string canonicalizeLegacySoaToAosHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t templateSuffix = canonicalPath.find("__t");
  if (templateSuffix != std::string::npos) {
    canonicalPath.erase(templateSuffix);
  }
  if (canonicalPath == "/to_aos") {
    return "/std/collections/soa_vector/to_aos";
  }
  if (canonicalPath == "/to_aos_ref") {
    return "/std/collections/soa_vector/to_aos_ref";
  }
  return canonicalPath;
}

std::string canonicalizeLegacySoaRefHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t templateSuffix = canonicalPath.find("__t");
  if (templateSuffix != std::string::npos) {
    canonicalPath.erase(templateSuffix);
  }
  if (canonicalPath == "/soa_vector/ref") {
    return "/std/collections/soa_vector/ref";
  }
  if (canonicalPath == "/soa_vector/ref_ref") {
    return "/std/collections/soa_vector/ref_ref";
  }
  return canonicalPath;
}

std::string canonicalizeLegacySoaGetHelperPath(std::string_view path) {
  const std::string canonicalPath = canonicalizeLegacySoaRefHelperPath(path);
  if (canonicalPath == "/soa_vector/get") {
    return "/std/collections/soa_vector/get";
  }
  if (canonicalPath == "/soa_vector/get_ref") {
    return "/std/collections/soa_vector/get_ref";
  }
  return canonicalPath;
}

bool isLegacyOrCanonicalSoaHelperPath(std::string_view path, std::string_view helperName) {
  constexpr std::string_view kLegacyPrefix = "/soa_vector/";
  constexpr std::string_view kCanonicalPrefix = "/std/collections/soa_vector/";
  if (path.starts_with(kLegacyPrefix)) {
    return path.substr(kLegacyPrefix.size()) == helperName;
  }
  if (path.starts_with(kCanonicalPrefix)) {
    return path.substr(kCanonicalPrefix.size()) == helperName;
  }
  return false;
}

bool isCanonicalStdlibSoaHelperPath(std::string_view path, std::string_view helperName) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind("/std/collections/soa_vector/", 0) == 0 &&
         isLegacyOrCanonicalSoaHelperPath(canonicalPath, helperName);
}

bool isCanonicalSoaRefLikeHelperPath(std::string_view path) {
  return path == "/std/collections/soa_vector/ref" ||
         path == "/std/collections/soa_vector/ref_ref";
}

bool isExperimentalSoaCountLikeHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == "/std/collections/experimental_soa_vector/soaVectorCount" ||
         canonicalPath == "/std/collections/experimental_soa_vector/soaVectorCountRef";
}

bool isExperimentalSoaBorrowedHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == "/std/collections/experimental_soa_vector/soaVectorCountRef" ||
         canonicalPath == "/std/collections/experimental_soa_vector/soaVectorGetRef" ||
         canonicalPath == "/std/collections/experimental_soa_vector/soaVectorRefRef";
}

bool isExperimentalSoaGetLikeHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == "/std/collections/experimental_soa_vector/soaVectorGet" ||
         canonicalPath == "/std/collections/experimental_soa_vector/soaVectorGetRef";
}

bool isExperimentalSoaGrowthHelperPath(std::string_view path) {
  constexpr std::string_view kExperimentalSoaVectorPrefix =
      "/std/collections/experimental_soa_vector/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  if (!canonicalPath.starts_with(kExperimentalSoaVectorPrefix)) {
    return false;
  }
  const std::string_view helperName =
      std::string_view(canonicalPath).substr(kExperimentalSoaVectorPrefix.size());
  return helperName == "push" || helperName == "reserve" ||
         helperName == "soaVectorPush" || helperName == "soaVectorReserve";
}

bool isExperimentalSoaFieldViewHelperPath(std::string_view path) {
  constexpr std::string_view kExperimentalSoaFieldViewPrefix =
      "/std/collections/experimental_soa_vector/soaVectorFieldView";
  constexpr std::string_view kExperimentalSoaColumnFieldViewUnsafePrefix =
      "/std/collections/internal_soa_storage/soaColumnFieldViewUnsafe";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(kExperimentalSoaFieldViewPrefix, 0) == 0 ||
         canonicalPath.rfind(kExperimentalSoaColumnFieldViewUnsafePrefix, 0) == 0;
}

bool isExperimentalSoaFieldViewReadHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             "/std/collections/internal_soa_storage/soaFieldViewRead",
             0) == 0;
}

bool isExperimentalSoaFieldViewRefHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == "/std/collections/internal_soa_storage/soaFieldViewRef";
}

bool isExperimentalSoaColumnSlotHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             "/std/collections/internal_soa_storage/soaColumnSlotUnsafe",
             0) == 0;
}

bool isExperimentalSoaColumnFieldSchemaHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             "/std/collections/internal_soa_storage/soaColumnField",
             0) == 0;
}

bool isExperimentalSoaMethodRefLikeHelperPath(std::string_view path) {
  constexpr std::string_view kExperimentalSoaVectorPrefix =
      "/std/collections/experimental_soa_vector/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  if (canonicalPath.rfind(kExperimentalSoaVectorPrefix, 0) != 0) {
    return false;
  }
  return std::string_view(canonicalPath).ends_with("/ref") ||
         std::string_view(canonicalPath).ends_with("/ref_ref");
}

bool isExperimentalSoaRefLikeHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == "/std/collections/experimental_soa_vector/soaVectorRef" ||
         canonicalPath == "/std/collections/experimental_soa_vector/soaVectorRefRef" ||
         canonicalPath == "/std/collections/internal_soa_storage/soaColumnRef";
}

bool isExperimentalSoaVectorHelperFamilyPath(std::string_view path) {
  constexpr std::string_view kExperimentalSoaVectorPrefix =
      "/std/collections/experimental_soa_vector/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.starts_with(kExperimentalSoaVectorPrefix) ||
         isExperimentalSoaVectorConversionFamilyPath(canonicalPath);
}

bool isExperimentalSoaVectorTypePath(std::string_view path) {
  constexpr std::string_view kExperimentalSoaVectorTypePrefix =
      "std/collections/experimental_soa_vector/SoaVector";
  constexpr std::string_view kExperimentalSoaVectorTypePrefixWithSlash =
      "/std/collections/experimental_soa_vector/SoaVector";
  const auto matchesExperimentalSoaVectorTypePrefix =
      [&](std::string_view prefix) {
        return path == prefix || path.starts_with(std::string(prefix) + "<") ||
               path.starts_with(std::string(prefix) + "__");
      };
  return matchesExperimentalSoaVectorTypePrefix("SoaVector") ||
         matchesExperimentalSoaVectorTypePrefix(kExperimentalSoaVectorTypePrefix) ||
         matchesExperimentalSoaVectorTypePrefix(kExperimentalSoaVectorTypePrefixWithSlash) ||
         isExperimentalSoaVectorSpecializedTypePath(path);
}

bool isExperimentalSoaVectorSpecializedTypePath(std::string_view path) {
  constexpr std::string_view kExperimentalSoaVectorSpecializedPrefix =
      "/std/collections/experimental_soa_vector/SoaVector__";
  constexpr std::string_view kExperimentalSoaVectorSpecializedPrefixNoSlash =
      "std/collections/experimental_soa_vector/SoaVector__";
  constexpr std::string_view kExperimentalSoaVectorSpecializedPrefixBare =
      "SoaVector__";
  return path.starts_with(kExperimentalSoaVectorSpecializedPrefix) ||
         path.starts_with(kExperimentalSoaVectorSpecializedPrefixNoSlash) ||
         path.starts_with(kExperimentalSoaVectorSpecializedPrefixBare);
}

bool isExperimentalSoaVectorConversionFamilyPath(std::string_view path) {
  constexpr std::string_view kExperimentalSoaVectorConversionsPrefix =
      "/std/collections/experimental_soa_vector_conversions/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  if (!canonicalPath.starts_with(kExperimentalSoaVectorConversionsPrefix)) {
    return false;
  }
  const std::string_view helperName =
      std::string_view(canonicalPath).substr(kExperimentalSoaVectorConversionsPrefix.size());
  return helperName == "soaVectorToAos" || helperName == "soaVectorToAosRef";
}

namespace {

std::string canonicalSoaPendingHelperPath(std::string_view resolvedPath) {
  std::string resolvedPathNoTemplate;
  std::string_view normalizedResolvedPath = resolvedPath;
  const size_t templateSuffix = resolvedPath.find("__t");
  if (templateSuffix != std::string_view::npos) {
    resolvedPathNoTemplate = std::string(resolvedPath.substr(0, templateSuffix));
    normalizedResolvedPath = resolvedPathNoTemplate;
  }
  std::string fieldName;
  if (splitSoaFieldViewHelperPath(normalizedResolvedPath, &fieldName)) {
    return soaFieldViewHelperPath(fieldName);
  }
  const std::string canonicalSoaRefPath =
      canonicalizeLegacySoaRefHelperPath(normalizedResolvedPath);
  if (isCanonicalSoaRefLikeHelperPath(canonicalSoaRefPath)) {
    return canonicalSoaRefPath;
  }
  const std::string canonicalSoaGetPath =
      canonicalizeLegacySoaGetHelperPath(normalizedResolvedPath);
  if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get") ||
      isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get_ref")) {
    return canonicalSoaGetPath;
  }
  if (normalizedResolvedPath == "/soa_vector/count") {
    return "/std/collections/soa_vector/count";
  }
  if (normalizedResolvedPath == "/soa_vector/count_ref") {
    return "/std/collections/soa_vector/count_ref";
  }
  return std::string(resolvedPath);
}

} // namespace

std::string soaUnavailableMethodDiagnostic(std::string_view resolvedPath) {
  return "unknown method: " + canonicalSoaPendingHelperPath(resolvedPath);
}

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/vector/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("array/", 0) == 0) {
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("map/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/map/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  name = stripTemplateSpecializationSuffix(name);
  if (name == "at" || name == "at_unsafe") {
    out = name;
    return true;
  }
  return false;
}

bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut) {
  collectionOut.clear();
  helperOut.clear();
  if (expr.name.empty()) {
    return false;
  }
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }

  auto extractHelper = [&](const std::string &prefix, const std::string &collectionName) -> bool {
    if (normalized.rfind(prefix, 0) != 0) {
      return false;
    }
    collectionOut = collectionName;
    helperOut = stripTemplateSpecializationSuffix(normalized.substr(prefix.size()));
    return !helperOut.empty();
  };

  if (extractHelper("std/collections/vector/", "vector") || extractHelper("map/", "map") ||
      extractHelper("std/collections/map/", "map")) {
    return true;
  }
  if (extractHelper("array/", "vector")) {
    if (helperOut == "count" || helperOut == "capacity" || helperOut == "at" || helperOut == "at_unsafe" ||
        helperOut == "push" || helperOut == "pop" || helperOut == "reserve" || helperOut == "clear" ||
        helperOut == "remove_at" || helperOut == "remove_swap") {
      collectionOut.clear();
      helperOut.clear();
      return false;
    }
    return true;
  }
  collectionOut.clear();
  helperOut.clear();
  return false;
}

} // namespace primec::semantics
