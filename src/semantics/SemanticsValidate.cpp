#include "primec/Semantics.h"
#include "primec/testing/SemanticsGraphHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include "SemanticsValidateConvertConstructors.h"
#include "SemanticsValidateExperimentalGfxConstructors.h"
#include "SemanticsValidateReflectionGeneratedHelpers.h"
#include "SemanticsValidateReflectionMetadata.h"
#include "SemanticsValidateTransforms.h"
#include "SemanticPublicationBuilders.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidator.h"
#include "TypeResolutionGraphPreparation.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#include <unistd.h>
#endif

namespace primec {

namespace semantics {
bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error);

namespace {

std::string returnKindSnapshotName(ReturnKind kind) {
  switch (kind) {
    case ReturnKind::Unknown:
      return "unknown";
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Integer:
      return "integer";
    case ReturnKind::Decimal:
      return "decimal";
    case ReturnKind::Complex:
      return "complex";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Void:
      return "void";
    case ReturnKind::Array:
      return "array";
  }
  return "unknown";
}

std::string bindingTypeTextForSnapshot(const BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

uint64_t hashSemanticNodePath(const std::string &path) {
  constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;
  constexpr uint64_t FnvPrime = 1099511628211ull;

  uint64_t hash = FnvOffsetBasis;
  for (unsigned char ch : path) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= FnvPrime;
  }
  return hash == 0 ? 1 : hash;
}

std::string makeIndexedSemanticNodePath(const std::string &base, const char *segment, size_t index) {
  return base + "/" + segment + "[" + std::to_string(index) + "]";
}

void assignExprSemanticNodeIds(Expr &expr, const std::string &path) {
  expr.semanticNodeId = hashSemanticNodePath(path);
  for (size_t i = 0; i < expr.args.size(); ++i) {
    assignExprSemanticNodeIds(expr.args[i], makeIndexedSemanticNodePath(path, "arg", i));
  }
  for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
    assignExprSemanticNodeIds(expr.bodyArguments[i], makeIndexedSemanticNodePath(path, "body", i));
  }
}

void assignDefinitionSemanticNodeIds(Definition &def) {
  const std::string basePath = "definition:" + def.fullPath;
  def.semanticNodeId = hashSemanticNodePath(basePath);
  for (size_t i = 0; i < def.parameters.size(); ++i) {
    assignExprSemanticNodeIds(def.parameters[i], makeIndexedSemanticNodePath(basePath, "parameter", i));
  }
  for (size_t i = 0; i < def.statements.size(); ++i) {
    assignExprSemanticNodeIds(def.statements[i], makeIndexedSemanticNodePath(basePath, "statement", i));
  }
  if (def.returnExpr.has_value()) {
    assignExprSemanticNodeIds(*def.returnExpr, basePath + "/return");
  }
}

void assignExecutionSemanticNodeIds(Execution &exec) {
  const std::string basePath = "execution:" + exec.fullPath;
  exec.semanticNodeId = hashSemanticNodePath(basePath);
  for (size_t i = 0; i < exec.arguments.size(); ++i) {
    assignExprSemanticNodeIds(exec.arguments[i], makeIndexedSemanticNodePath(basePath, "argument", i));
  }
  for (size_t i = 0; i < exec.bodyArguments.size(); ++i) {
    assignExprSemanticNodeIds(exec.bodyArguments[i], makeIndexedSemanticNodePath(basePath, "body", i));
  }
}

void assignSemanticNodeIds(Program &program) {
  for (auto &def : program.definitions) {
    assignDefinitionSemanticNodeIds(def);
  }
  for (auto &exec : program.executions) {
    assignExecutionSemanticNodeIds(exec);
  }
}

template <typename CaptureFn>
bool runTypeResolutionSnapshot(
    Program &program,
    const std::string &entryPath,
    std::string &error,
    const std::vector<std::string> &semanticTransforms,
    CaptureFn &&capture) {
  error.clear();
  if (!semantics::prepareProgramForTypeResolutionAnalysis(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  semantics::SemanticsValidator validator(program, entryPath, error, defaults, defaults, nullptr, false);
  if (!validator.run()) {
    return false;
  }
  capture(validator);
  return true;
}

} // namespace
}

namespace {

thread_local bool gDisableSemanticAllocatorReliefForBenchmark = false;

void relieveSemanticAllocatorPressure() {
#if defined(__APPLE__)
  (void)malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__) && defined(__GLIBC__)
  (void)malloc_trim(0);
#endif
}

void maybeRelieveSemanticAllocatorPressure() {
  if (gDisableSemanticAllocatorReliefForBenchmark) {
    return;
  }
  if (std::getenv("PRIMEC_DISABLE_SEMANTIC_ALLOCATOR_RELIEF") != nullptr) {
    return;
  }
  relieveSemanticAllocatorPressure();
}

SemanticProgram buildSemanticProgram(const Program &program,
                                     const std::string &entryPath,
                                     semantics::SemanticsValidator &validator,
                                     const SemanticProductBuildConfig *buildConfig) {
  auto publicationSurface =
      validator.takeSemanticPublicationSurfaceForSemanticProduct(buildConfig);
  maybeRelieveSemanticAllocatorPressure();
  SemanticProgram semanticProgram = semantics::buildSemanticProgramFromPublicationSurface(
      program,
      entryPath,
      std::move(publicationSurface),
      buildConfig);
  maybeRelieveSemanticAllocatorPressure();
  return semanticProgram;
}

uint64_t semanticProgramFactCount(const SemanticProgram &semanticProgram) {
  return static_cast<uint64_t>(
      semanticProgram.definitions.size() +
      semanticProgram.executions.size() +
      semanticProgram.directCallTargets.size() +
      semanticProgram.methodCallTargets.size() +
      semanticProgram.bridgePathChoices.size() +
      semanticProgram.callableSummaries.size() +
      semanticProgram.typeMetadata.size() +
      semanticProgram.structFieldMetadata.size() +
      semanticProgram.bindingFacts.size() +
      semanticProgram.returnFacts.size() +
      semanticProgram.localAutoFacts.size() +
      semanticProgram.queryFacts.size() +
      semanticProgram.tryFacts.size() +
      semanticProgram.onErrorFacts.size());
}

struct ProcessAllocationSample {
  bool valid = false;
  uint64_t allocationCount = 0;
  uint64_t allocatedBytes = 0;
};

struct ProcessRssSample {
  bool valid = false;
  uint64_t residentBytes = 0;
};

ProcessAllocationSample captureProcessAllocationSample() {
  ProcessAllocationSample sample;
#if defined(__APPLE__)
  malloc_statistics_t stats{};
  malloc_zone_statistics(nullptr, &stats);
  sample.valid = true;
  sample.allocationCount = static_cast<uint64_t>(stats.blocks_in_use);
  sample.allocatedBytes = static_cast<uint64_t>(stats.size_in_use);
#elif defined(__linux__)
  const struct mallinfo2 info = mallinfo2();
  sample.valid = true;
  sample.allocationCount = info.ordblks > 0 ? static_cast<uint64_t>(info.ordblks) : 0;
  sample.allocatedBytes = info.uordblks > 0 ? static_cast<uint64_t>(info.uordblks) : 0;
#endif
  return sample;
}

ProcessRssSample captureProcessRssSample() {
  ProcessRssSample sample;
#if defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  const kern_return_t status = task_info(mach_task_self(),
                                         MACH_TASK_BASIC_INFO,
                                         reinterpret_cast<task_info_t>(&info),
                                         &count);
  if (status == KERN_SUCCESS) {
    sample.valid = true;
    sample.residentBytes = static_cast<uint64_t>(info.resident_size);
  }
#elif defined(__linux__)
  std::ifstream statm("/proc/self/statm");
  uint64_t residentPages = 0;
  if (statm.good()) {
    statm.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
    if (statm >> residentPages) {
      const long pageSize = sysconf(_SC_PAGESIZE);
      if (pageSize > 0) {
        sample.valid = true;
        sample.residentBytes = residentPages * static_cast<uint64_t>(pageSize);
      }
    }
  }
#endif
  return sample;
}

uint64_t saturatingSubtract(uint64_t after, uint64_t before) {
  return after > before ? (after - before) : 0;
}

void populateAllocationDelta(SemanticPhaseCounterSnapshot &snapshot,
                             const ProcessAllocationSample &before,
                             const ProcessAllocationSample &after) {
  if (!before.valid || !after.valid) {
    return;
  }
  snapshot.allocationCount = saturatingSubtract(after.allocationCount, before.allocationCount);
  snapshot.allocatedBytes = saturatingSubtract(after.allocatedBytes, before.allocatedBytes);
}

void populateRssCheckpoints(SemanticPhaseCounterSnapshot &snapshot,
                            const ProcessRssSample &before,
                            const ProcessRssSample &after) {
  if (!before.valid || !after.valid) {
    return;
  }
  snapshot.rssBeforeBytes = before.residentBytes;
  snapshot.rssAfterBytes = after.residentBytes;
}

bool isExperimentalMapTypeText(const std::string &typeText) {
  std::string keyType;
  std::string valueType;
  if (!semantics::extractMapKeyValueTypesFromTypeText(typeText, keyType, valueType)) {
    return false;
  }
  const std::string normalizedInner = semantics::normalizeBindingTypeName(typeText);
  return normalizedInner == "Map" ||
         normalizedInner.rfind("Map<", 0) == 0 ||
         normalizedInner.rfind("/std/collections/experimental_map/Map", 0) == 0 ||
         normalizedInner.rfind("std/collections/experimental_map/Map", 0) == 0;
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalMapBinding(const Expr &expr) {
  semantics::BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "Reference" &&
        transform.templateArgs.size() == 1 &&
        isExperimentalMapTypeText(transform.templateArgs.front())) {
      info.typeName = "Reference";
      info.typeTemplateArg = transform.templateArgs.front();
      return info;
    }
  }
  return std::nullopt;
}

std::string bindingTypeText(const semantics::BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool isExperimentalMapValueBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalMapTypeText(bindingTypeText(binding));
}

std::optional<semantics::BindingInfo> extractExperimentalMapBinding(const Expr &expr) {
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  if (!isExperimentalMapValueBinding(binding)) {
    return std::nullopt;
  }
  return binding;
}

bool isBorrowedExperimentalMapBinding(const semantics::BindingInfo &binding) {
  return semantics::normalizeBindingTypeName(binding.typeName) == "Reference" &&
         isExperimentalMapTypeText(binding.typeTemplateArg);
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalMapReturnBinding(
    const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(transform.templateArgs.front(), base, argText)) {
      continue;
    }
    if (semantics::normalizeBindingTypeName(base) != "Reference") {
      continue;
    }
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1 ||
        !isExperimentalMapTypeText(args.front())) {
      continue;
    }
    semantics::BindingInfo info;
    info.typeName = "Reference";
    info.typeTemplateArg = args.front();
    return info;
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalMapReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    std::string base;
    std::string argText;
    semantics::BindingInfo binding;
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isExperimentalMapValueBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::string borrowedExperimentalMapHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "mapCountRef";
  }
  if (methodName == "contains") {
    return "mapContainsRef";
  }
  if (methodName == "tryAt") {
    return "mapTryAtRef";
  }
  if (methodName == "at") {
    return "mapAtRef";
  }
  if (methodName == "at_unsafe") {
    return "mapAtUnsafeRef";
  }
  if (methodName == "insert") {
    return "mapInsertRef";
  }
  return {};
}

std::string experimentalMapValueHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "mapCount";
  }
  if (methodName == "contains") {
    return "mapContains";
  }
  if (methodName == "tryAt") {
    return "mapTryAt";
  }
  if (methodName == "at") {
    return "mapAt";
  }
  if (methodName == "at_unsafe") {
    return "mapAtUnsafe";
  }
  if (methodName == "insert") {
    return "mapInsert";
  }
  return {};
}

bool isBuiltinVectorTypeText(const std::string &typeText) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  return normalizedType == "vector" || normalizedType.rfind("vector<", 0) == 0;
}

bool isBuiltinSoaVectorTypeText(const std::string &typeText) {
  std::string rawType = typeText;
  if (!rawType.empty() && rawType.front() == '/') {
    rawType.erase(rawType.begin());
  }
  if (rawType == "SoaVector" || rawType.rfind("SoaVector<", 0) == 0 ||
      rawType == "std/collections/experimental_soa_vector/SoaVector" ||
      rawType.rfind("std/collections/experimental_soa_vector/SoaVector<", 0) == 0 ||
      semantics::isExperimentalSoaVectorSpecializedTypePath(rawType)) {
    return false;
  }
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  return normalizedType == "soa_vector" || normalizedType.rfind("soa_vector<", 0) == 0;
}

bool isExperimentalSoaVectorBaseName(const std::string &typeName) {
  std::string normalizedType = typeName;
  if (!normalizedType.empty() && normalizedType.front() == '/') {
    normalizedType.erase(normalizedType.begin());
  }
  return normalizedType == "SoaVector" ||
         normalizedType == "std/collections/experimental_soa_vector/SoaVector" ||
         semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedType);
}

bool isExperimentalSoaVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalSoaVectorBaseName(binding.typeName);
}

bool isExperimentalSoaVectorOrBorrowedTypeText(std::string typeText) {
  typeText = semantics::normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(typeText, base, argText) || base.empty()) {
      return isExperimentalSoaVectorBaseName(typeText);
    }
    const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
    if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      typeText = args.front();
      continue;
    }
    return isExperimentalSoaVectorBaseName(base);
  }
}

bool isBuiltinVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isBuiltinVectorTypeText(bindingTypeText(binding));
}

bool isBuiltinSoaVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isBuiltinSoaVectorTypeText(bindingTypeText(binding));
}

std::optional<semantics::BindingInfo> extractBuiltinVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isBuiltinVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isBuiltinSoaVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isExperimentalSoaVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinVectorReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isBuiltinVectorBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isBuiltinSoaVectorBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorReturnBindingImpl(
    const Definition &def,
    bool allowBorrowed) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string returnType = transform.templateArgs.front();
    if (semantics::splitTemplateTypeName(returnType, base, argText)) {
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
          !allowBorrowed) {
        continue;
      }
      binding.typeName = base;
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = returnType;
    }
    if ((allowBorrowed && isExperimentalSoaVectorOrBorrowedTypeText(bindingTypeText(binding))) ||
        (!allowBorrowed && isExperimentalSoaVectorBinding(binding))) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorOrBorrowedReturnBinding(
    const Definition &def) {
  return extractExperimentalSoaVectorReturnBindingImpl(def, true);
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                const std::unordered_map<std::string, std::string>
                                                                    &specializedSoaVectorElementTypes,
                                                                std::string &elemTypeOut) {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (semantics::splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = semantics::normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = semantics::normalizeBindingTypeName(args.front());
          continue;
        }
        if (!base.empty() && base.front() == '/') {
          base.erase(base.begin());
        }
        if ((base == "SoaVector" ||
             base == "soa_vector" ||
             base == "std/collections/soa_vector" ||
             base == "std/collections/experimental_soa_vector/SoaVector") &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      auto specializedIt = specializedSoaVectorElementTypes.find(resolvedPath);
      if (specializedIt != specializedSoaVectorElementTypes.end()) {
        elemTypeOut = specializedIt->second;
        return true;
      }
      std::string normalizedResolvedPath = semantics::normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
        return false;
      }
      auto normalizedIt = specializedSoaVectorElementTypes.find("/" + normalizedResolvedPath);
      if (normalizedIt == specializedSoaVectorElementTypes.end()) {
        normalizedIt = specializedSoaVectorElementTypes.find(normalizedResolvedPath);
      }
      if (normalizedIt == specializedSoaVectorElementTypes.end()) {
        return false;
      }
      elemTypeOut = normalizedIt->second;
      return true;
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(semantics::normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      semantics::normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                std::string &elemTypeOut) {
  static const std::unordered_map<std::string, std::string> emptySpecializedSoaVectorElementTypes;
  return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
      binding, emptySpecializedSoaVectorElementTypes, elemTypeOut);
}

struct SoaFieldViewFieldInfo {
  size_t index = 0;
  std::string typeText;
};

Expr makeI32LiteralExpr(uint64_t value, int sourceLine, int sourceColumn) {
  Expr expr;
  expr.kind = Expr::Kind::Literal;
  expr.literalValue = value;
  expr.intWidth = 32;
  expr.isUnsigned = false;
  expr.sourceLine = sourceLine;
  expr.sourceColumn = sourceColumn;
  return expr;
}

bool extractSoaFieldViewElementTypeText(std::string typeText, std::string &elemTypeOut) {
  elemTypeOut.clear();
  std::string normalized = semantics::normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(normalized, base, argText) || base.empty()) {
      return false;
    }
    base = semantics::normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalized = semantics::normalizeBindingTypeName(args.front());
      continue;
    }
    if (base == "SoaFieldView" ||
        base == "std/collections/internal_soa_storage/SoaFieldView") {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      elemTypeOut = args.front();
      return true;
    }
    return false;
  }
}

bool extractSoaFieldViewElementTypeFromBinding(const semantics::BindingInfo &binding,
                                               std::string &elemTypeOut) {
  if (binding.typeName.empty()) {
    elemTypeOut.clear();
    return false;
  }
  const std::string typeText = binding.typeTemplateArg.empty()
                                   ? binding.typeName
                                   : binding.typeName + "<" + binding.typeTemplateArg + ">";
  return extractSoaFieldViewElementTypeText(typeText, elemTypeOut);
}

std::string qualifySoaFieldViewTypeText(const std::string &typeText,
                                        const std::string &namespacePrefix,
                                        const std::unordered_set<std::string> &structPaths) {
  const std::string normalized = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args)) {
      return normalized;
    }
    for (std::string &arg : args) {
      arg = qualifySoaFieldViewTypeText(arg, namespacePrefix, structPaths);
    }
    const std::string resolvedBase = semantics::resolveStructTypePath(
        base, namespacePrefix, structPaths);
    const std::string qualifiedBase = resolvedBase.empty() ? base : resolvedBase;
    std::string rebuilt = qualifiedBase;
    rebuilt.push_back('<');
    for (size_t i = 0; i < args.size(); ++i) {
      if (i != 0) {
        rebuilt.append(", ");
      }
      rebuilt.append(args[i]);
    }
    rebuilt.push_back('>');
    return rebuilt;
  }
  const std::string resolved = semantics::resolveStructTypePath(
      normalized, namespacePrefix, structPaths);
  return resolved.empty() ? normalized : resolved;
}

bool extractExperimentalSoaColumnElementTypeFromSpecializedDefinition(
    const Definition &def,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  if (def.fullPath.rfind("/std/collections/internal_soa_storage/SoaColumn__", 0) != 0) {
    return false;
  }
  for (const auto &fieldExpr : def.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    semantics::BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    static const std::unordered_set<std::string> emptyStructTypes;
    static const std::unordered_map<std::string, std::string> emptyImportAliases;
    if (!semantics::parseBindingInfo(fieldExpr,
                                     def.namespacePrefix,
                                     emptyStructTypes,
                                     emptyImportAliases,
                                     fieldBinding,
                                     restrictType,
                                     parseError)) {
      continue;
    }
    if (semantics::normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!semantics::splitTemplateTypeName(semantics::normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                                          pointeeBase,
                                          pointeeArgText) ||
        semantics::normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!semantics::splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool extractExperimentalSoaVectorElementTypeFromSpecializedDefinition(
    const Definition &def,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  if (!semantics::isExperimentalSoaVectorSpecializedTypePath(def.fullPath)) {
    return false;
  }
  for (const auto &fieldExpr : def.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "storage") {
      continue;
    }
    semantics::BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    static const std::unordered_set<std::string> emptyStructTypes;
    static const std::unordered_map<std::string, std::string> emptyImportAliases;
    if (!semantics::parseBindingInfo(fieldExpr,
                                     def.namespacePrefix,
                                     emptyStructTypes,
                                     emptyImportAliases,
                                     fieldBinding,
                                     restrictType,
                                     parseError)) {
      continue;
    }
    std::string normalizedFieldType = semantics::normalizeBindingTypeName(fieldBinding.typeName);
    if (!normalizedFieldType.empty() && normalizedFieldType.front() == '/') {
      normalizedFieldType.erase(normalizedFieldType.begin());
    }
    if (!fieldBinding.typeTemplateArg.empty() &&
        (normalizedFieldType == "SoaColumn" ||
         normalizedFieldType == "std/collections/internal_soa_storage/SoaColumn")) {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      elemTypeOut = args.front();
      return true;
    }
    std::string resolvedFieldPath = semantics::normalizeBindingTypeName(fieldBinding.typeName);
    if (!resolvedFieldPath.empty() && resolvedFieldPath.front() != '/') {
      resolvedFieldPath.insert(resolvedFieldPath.begin(), '/');
    }
    auto defIt = definitionMap.find(resolvedFieldPath);
    if (defIt == definitionMap.end() || defIt->second == nullptr) {
      continue;
    }
    if (!extractExperimentalSoaColumnElementTypeFromSpecializedDefinition(*defIt->second, elemTypeOut)) {
      continue;
    }
    return true;
  }
  return false;
}

std::unordered_map<std::string, std::string> buildSpecializedExperimentalSoaVectorElementTypes(
    const Program &program) {
  std::unordered_map<std::string, const Definition *> definitionMap;
  for (const Definition &def : program.definitions) {
    definitionMap[def.fullPath] = &def;
  }

  std::unordered_map<std::string, std::string> elemTypes;
  for (const Definition &def : program.definitions) {
    std::string elemType;
    if (!extractExperimentalSoaVectorElementTypeFromSpecializedDefinition(def, definitionMap, elemType)) {
      continue;
    }
    elemTypes[def.fullPath] = elemType;
    if (!def.fullPath.empty() && def.fullPath.front() == '/') {
      elemTypes[def.fullPath.substr(1)] = elemType;
    }
  }
  return elemTypes;
}

bool extractExperimentalSoaVectorElementTypeForToAosRewrite(const semantics::BindingInfo &binding,
                                                            std::string &elemTypeOut) {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (semantics::splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = semantics::normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = semantics::normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if ((normalizedBase == "SoaVector" ||
             normalizedBase == "std/collections/experimental_soa_vector/SoaVector") &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = semantics::normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
        return false;
      }
      elemTypeOut = resolvedPath;
      return true;
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(semantics::normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      semantics::normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorFieldViewReceiverBinding(const Expr &expr) {
  if (auto directBinding = extractExperimentalSoaVectorBinding(expr); directBinding.has_value()) {
    return directBinding;
  }

  for (const auto &transform : expr.transforms) {
    if ((transform.name != "Reference" && transform.name != "Pointer") ||
        transform.templateArgs.size() != 1 || !transform.arguments.empty()) {
      continue;
    }
    semantics::BindingInfo binding;
    binding.typeName = transform.name;
    binding.typeTemplateArg = transform.templateArgs.front();
    std::string elemType;
    if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(binding, elemType)) {
      return binding;
    }
  }
  return std::nullopt;
}

bool localImportPathCoversTarget(const std::string &importPath, const std::string &targetPath) {
  if (importPath.empty() || importPath.front() != '/') {
    return false;
  }
  if (importPath == targetPath) {
    return true;
  }
  if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
    const std::string prefix = importPath.substr(0, importPath.size() - 2);
    return targetPath == prefix || targetPath.rfind(prefix + "/", 0) == 0;
  }
  return false;
}

bool hasVisibleRootSoaHelper(const Program &program, std::string_view helperName) {
  const std::string rootPath = "/" + std::string(helperName);
  const std::string samePath = "/soa_vector/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if ((def.fullPath == rootPath || def.fullPath == samePath) &&
        !def.parameters.empty() &&
        extractBuiltinSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (localImportPathCoversTarget(importPath, rootPath) ||
        localImportPathCoversTarget(importPath, samePath)) {
      return true;
    }
  }
  return false;
}

bool hasVisibleRootSoaHelperForReceiverType(const Program &program,
                                            std::string_view helperName,
                                            std::string_view receiverTypeName) {
  const std::string rootPath = "/" + std::string(helperName);
  const std::string samePath = "/soa_vector/" + std::string(helperName);
  auto matchesReceiverType = [&](const Expr &parameter) {
    if (receiverTypeName == "soa_vector") {
      return extractBuiltinSoaVectorBinding(parameter).has_value();
    }
    if (receiverTypeName == "vector") {
      return extractBuiltinVectorBinding(parameter).has_value();
    }
    return false;
  };
  for (const Definition &def : program.definitions) {
    if ((def.fullPath == rootPath || def.fullPath == samePath) &&
        !def.parameters.empty() &&
        matchesReceiverType(def.parameters.front())) {
      return true;
    }
  }
  return false;
}

bool hasVisibleExperimentalSoaSamePathHelper(const Program &program,
                                             std::string_view helperName) {
  const std::string samePath = "/soa_vector/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if (def.fullPath != samePath || def.parameters.empty()) {
      continue;
    }
    if (extractExperimentalSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (localImportPathCoversTarget(importPath, samePath)) {
      return true;
    }
  }
  return false;
}

bool hasVisibleRootExperimentalSoaHelper(const Program &program,
                                         std::string_view helperName) {
  const std::string rootPath = "/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if (def.fullPath != rootPath || def.parameters.empty()) {
      continue;
    }
    if (extractExperimentalSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> candidateDefinitionPaths(const Expr &expr, const std::string &definitionNamespace) {
  std::vector<std::string> candidatePaths;
  if (!expr.name.empty() && expr.name.front() == '/') {
    candidatePaths.push_back(expr.name);
    return candidatePaths;
  }
  if (!expr.namespacePrefix.empty()) {
    candidatePaths.push_back(expr.namespacePrefix + "/" + expr.name);
  }
  if (!definitionNamespace.empty()) {
    candidatePaths.push_back(definitionNamespace + "/" + expr.name);
  }
  candidatePaths.push_back("/" + expr.name);
  candidatePaths.push_back(expr.name);
  return candidatePaths;
}

std::optional<semantics::BindingInfo> extractParsedBindingInfo(
    const Expr &expr,
    const std::unordered_set<std::string> *structTypes = nullptr) {
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!semantics::parseBindingInfo(
          expr,
          expr.namespacePrefix,
          structTypes != nullptr ? *structTypes : emptyStructTypes,
          emptyImportAliases,
          binding,
          restrictType,
          parseError)) {
    return std::nullopt;
  }
  return binding;
}

std::optional<semantics::BindingInfo> extractParsedOrExperimentalSoaBindingInfo(
    const Expr &expr,
    const std::unordered_set<std::string> *structTypes = nullptr) {
  if (auto parsed = extractParsedBindingInfo(expr, structTypes); parsed.has_value()) {
    return parsed;
  }
  if (!expr.isBinding) {
    return std::nullopt;
  }
  return extractExperimentalSoaVectorFieldViewReceiverBinding(expr);
}

std::string resolveStructReceiverPathFromBinding(
    const semantics::BindingInfo &binding,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structPaths) {
  std::string typeText = semantics::normalizeBindingTypeName(
      binding.typeTemplateArg.empty() ? binding.typeName : binding.typeName + "<" + binding.typeTemplateArg + ">");
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(typeText, base, argText)) {
      break;
    }
    base = semantics::normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      typeText = base;
      break;
    }
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return {};
    }
    typeText = semantics::normalizeBindingTypeName(args.front());
  }
  return semantics::resolveStructTypePath(typeText, namespacePrefix, structPaths);
}

std::optional<semantics::BindingInfo> extractBuiltinCollectionBindingFromTypeText(
    const std::string &typeText,
    std::string_view expectedBase) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(normalizedType, base, argText) ||
      semantics::normalizeBindingTypeName(base) != expectedBase) {
    return std::nullopt;
  }
  semantics::BindingInfo binding;
  binding.typeName = std::string(expectedBase);
  binding.typeTemplateArg = argText;
  return binding;
}

std::optional<semantics::BindingInfo> extractBuiltinCollectionBindingFromWrappedTypeText(
    const std::string &typeText,
    std::string_view expectedBase) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(normalizedType, base, argText)) {
    return std::nullopt;
  }
  const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
  if (normalizedBase == "args") {
    return extractBuiltinCollectionBindingFromWrappedTypeText(argText, expectedBase);
  }
  if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
    return std::nullopt;
  }
  return extractBuiltinCollectionBindingFromTypeText(argText, expectedBase);
}

std::string builtinSoaConversionMethodName(std::string_view methodName) {
  std::string normalized(methodName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("vector/", 0) == 0) {
    normalized = normalized.substr(std::string("vector/").size());
  } else if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
    normalized = normalized.substr(std::string("std/collections/vector/").size());
  }
  if (normalized == "to_soa") {
    return "to_soa";
  }
  if (normalized == "to_aos") {
    return "to_aos";
  }
  return {};
}

std::string builtinSoaAccessHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  }
  if (normalized == "get" || normalized == "get_ref" ||
      normalized == "ref" || normalized == "ref_ref") {
    return normalized;
  }
  return {};
}

std::string builtinSoaCountHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  }
  if (normalized == "count") {
    return normalized;
  }
  return {};
}

bool isOldExplicitSoaCountHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "soa_vector/count";
}

std::string builtinSoaMutatorHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  }
  if (normalized == "push" || normalized == "reserve") {
    return normalized;
  }
  return {};
}

std::string oldExplicitSoaMutatorHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("soa_vector/", 0) != 0) {
    return {};
  }
  normalized = normalized.substr(std::string("soa_vector/").size());
  if (normalized == "push" || normalized == "reserve") {
    return normalized;
  }
  return {};
}

void rewriteBuiltinSoaConversionMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteBuiltinSoaConversionMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaConversionMethodExpr(
        stmt, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaConversionMethodStatements(
          stmt.bodyArguments, bodyBindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaConversionMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaConversionMethodExpr(
        arg, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = builtinSoaConversionMethodName(expr.name);
  if (helperName.empty()) {
    return;
  }
  auto matchesBuiltinReceiverBinding = [&](const semantics::BindingInfo &binding) {
    if (helperName == "to_soa") {
      return isBuiltinVectorBinding(binding);
    }
    if (helperName == "to_aos") {
      return isBuiltinSoaVectorBinding(binding);
    }
    return false;
  };
  const auto &returnDefinitions =
      helperName == "to_soa" ? vectorReturnDefinitions : soaVectorReturnDefinitions;
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && matchesBuiltinReceiverBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = returnDefinitions.find(candidatePath);
      if (returnIt != returnDefinitions.end() &&
          matchesBuiltinReceiverBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = helperName;
  expr.namespacePrefix.clear();
}

bool rewriteBuiltinSoaConversionMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaConversionMethodStatements(
        def.statements, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaConversionMethodExpr(
          *def.returnExpr, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

void rewriteBuiltinSoaToAosCallExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper);

void rewriteBuiltinSoaToAosCallStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaToAosCallExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaToAosCallStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveVisibleRootSoaHelper,
          preserveVisibleRootVectorHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaToAosCallExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        builtinSoaConversionMethodName(candidate.name) == "to_soa" &&
        candidate.args.size() == 1) {
      auto vectorBinding = findBuiltinVectorValueBinding(candidate.args.front());
      if (!vectorBinding.has_value() || vectorBinding->typeTemplateArg.empty()) {
        return std::nullopt;
      }
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = vectorBinding->typeTemplateArg;
      return binding;
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaToAosCallExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
  }
  if (expr.kind != Expr::Kind::Call ||
      expr.args.size() != 1 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty() ||
      builtinSoaConversionMethodName(expr.name) != "to_aos") {
    return;
  }

  const bool hasBuiltinSoaReceiver =
      findBuiltinSoaValueBinding(expr.args.front()).has_value();
  const bool hasBuiltinVectorReceiver =
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }
  const bool isExplicitRootToAosSurface =
      !expr.name.empty() && expr.name.front() == '/';
  if (isExplicitRootToAosSurface) {
    if (expr.isMethodCall &&
        ((hasBuiltinSoaReceiver && preserveVisibleRootSoaHelper) ||
         (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper))) {
      expr.isMethodCall = false;
      expr.isFieldAccess = false;
      expr.name = "/to_aos";
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
    }
    return;
  }
  if (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper && expr.isMethodCall) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to_aos";
    expr.namespacePrefix.clear();
    expr.templateArgs.clear();
    return;
  }
  if ((hasBuiltinSoaReceiver && preserveVisibleRootSoaHelper) ||
      (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper)) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/to_aos";
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
}

bool rewriteBuiltinSoaToAosCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveVisibleRootSoaHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "to_aos", "soa_vector");
  const bool preserveVisibleRootVectorHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "to_aos", "vector");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaToAosCallStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaToAosCallExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveVisibleRootSoaHelper,
          preserveVisibleRootVectorHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaAccessExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper);

void rewriteBuiltinSoaAccessStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaAccessExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaAccessStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveGetRefHelper,
          preserveRefHelper,
          preserveRefRefHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaAccessExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaAccessExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaAccessHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  if ((helperName == "get" && preserveGetHelper) ||
      (helperName == "get_ref" && preserveGetRefHelper) ||
      (helperName == "ref" && preserveRefHelper) ||
      (helperName == "ref_ref" && preserveRefRefHelper)) {
    return;
  }
  const bool hasBuiltinSoaReceiver =
      findBuiltinSoaValueBinding(expr.args.front()).has_value();
  const bool hasBuiltinVectorReceiver =
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }

  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const auto fallbackVectorBinding = receiverBinding.has_value()
                                         ? std::optional<semantics::BindingInfo>{}
                                         : findBuiltinVectorValueBinding(expr.args.front());

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/" + helperName;
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaAccessCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveGetHelper = hasVisibleRootSoaHelper(program, "get");
  const bool preserveGetRefHelper = hasVisibleRootSoaHelper(program, "get_ref");
  const bool preserveRefHelper = hasVisibleRootSoaHelper(program, "ref");
  const bool preserveRefRefHelper = hasVisibleRootSoaHelper(program, "ref_ref");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaAccessStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaAccessExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveGetRefHelper,
          preserveRefHelper,
          preserveRefRefHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaCountExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper);

void rewriteBuiltinSoaCountStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaCountExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveCountHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaCountStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveCountHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaCountExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaCountExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveCountHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 1 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaCountHelperName(expr.name);
  if (helperName.empty() || preserveCountHelper) {
    return;
  }

  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const bool explicitOldSoaCount = isOldExplicitSoaCountHelperName(expr.name);
  const auto fallbackVectorBinding =
      receiverBinding.has_value() || !explicitOldSoaCount
          ? std::optional<semantics::BindingInfo>{}
          : findBuiltinVectorValueBinding(expr.args.front());
  if (!receiverBinding.has_value() && !fallbackVectorBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/count";
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaCountCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveCountHelper = hasVisibleRootSoaHelper(program, "count");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaCountStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveCountHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaCountExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveCountHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper);

void rewriteBuiltinSoaMutatorStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaMutatorExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaMutatorStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper,
          preserveVectorPushHelper,
          preserveVectorReserveHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaMutatorExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string rawHelperName =
      expr.namespacePrefix.empty() ? expr.name : expr.namespacePrefix + "/" + expr.name;
  const std::string helperName = builtinSoaMutatorHelperName(rawHelperName);
  if (helperName.empty()) {
    return;
  }
  if ((helperName == "push" && preservePushHelper) ||
      (helperName == "reserve" && preserveReserveHelper)) {
    return;
  }
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const auto fallbackVectorBinding =
      receiverBinding.has_value()
          ? std::optional<semantics::BindingInfo>{}
          : findBuiltinVectorValueBinding(expr.args.front());
  const std::string explicitOldHelperName = oldExplicitSoaMutatorHelperName(rawHelperName);
  const bool preserveVectorHelper =
      (helperName == "push" && preserveVectorPushHelper) ||
      (helperName == "reserve" && preserveVectorReserveHelper);
  if (fallbackVectorBinding.has_value() && preserveVectorHelper) {
    if (expr.isMethodCall) {
      expr.isMethodCall = false;
      expr.isFieldAccess = false;
      expr.name = "/soa_vector/" + helperName;
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
    }
    return;
  }
  if (fallbackVectorBinding.has_value() && explicitOldHelperName.empty()) {
    return;
  }
  if (!receiverBinding.has_value() && !fallbackVectorBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/" + helperName;
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() &&
             !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaMutatorCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preservePushHelper = hasVisibleRootSoaHelper(program, "push");
  const bool preserveReserveHelper = hasVisibleRootSoaHelper(program, "reserve");
  const bool preserveVectorPushHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "push", "vector");
  const bool preserveVectorReserveHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "reserve", "vector");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaMutatorStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaMutatorExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper,
          preserveVectorPushHelper,
          preserveVectorReserveHelper);
    }
  }
  return true;
}

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper);

std::vector<std::string> candidatePathsForExprCall(
    const Expr &callExpr,
    const std::string &definitionNamespace,
    const std::unordered_map<std::string, semantics::BindingInfo> *bindings,
    const std::unordered_set<std::string> *structPaths);

Expr canonicalizeResolvedCallPath(const Expr &callExpr, const std::string &resolvedPath);

bool isStructLikeDefinition(const Definition &def);

void rewriteExperimentalSoaSamePathHelperMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers);

void rewriteExperimentalSoaSamePathHelperMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaSamePathHelperMethodExpr(
        stmt,
        bindings,
        soaVectorReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaSamePathHelperMethodStatements(
          stmt.bodyArguments,
          bodyBindings,
          soaVectorReturnDefinitions,
          structPaths,
          definitionNamespace,
          visibleSoaHelpers);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
          binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaSamePathHelperMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaSamePathHelperMethodExpr(
        arg,
        bindings,
        soaVectorReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }

  std::string helperName = expr.name;
  if (!helperName.empty() && helperName.front() == '/') {
    helperName.erase(helperName.begin());
  }
  if (helperName.rfind("std/collections/soa_vector/", 0) == 0) {
    helperName = helperName.substr(std::string("std/collections/soa_vector/").size());
  } else if (helperName.rfind("soa_vector/", 0) == 0) {
    helperName = helperName.substr(std::string("soa_vector/").size());
  }
  if (helperName != "count" && helperName != "get" &&
      helperName != "ref" && helperName != "push" &&
      helperName != "reserve") {
    return;
  }
  const std::string helperPath = "/soa_vector/" + helperName;
  const bool hasVisibleSamePathHelper =
      visibleSoaHelpers.count(helperPath) > 0;

  std::optional<semantics::BindingInfo> receiverBinding;
  std::optional<Expr> canonicalReceiverExpr;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isExperimentalSoaVectorBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    for (const std::string &candidatePath :
         candidatePathsForExprCall(receiver, definitionNamespace, &bindings, &structPaths)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() &&
          isExperimentalSoaVectorBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = hasVisibleSamePathHelper ? helperPath
                                       : "/std/collections/soa_vector/" + helperName;
  expr.namespacePrefix.clear();
  if (canonicalReceiverExpr.has_value()) {
    expr.args.front() = *canonicalReceiverExpr;
  }
}

bool rewriteExperimentalSoaSamePathHelperMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaHelpers;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorReturnBindingImpl(def, false);
        binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (std::string_view helperName : {
           std::string_view("count"),
           std::string_view("get"),
           std::string_view("ref"),
           std::string_view("push"),
           std::string_view("reserve")}) {
    if (hasVisibleExperimentalSoaSamePathHelper(program, helperName)) {
      visibleSoaHelpers.insert("/soa_vector/" + std::string(helperName));
    }
  }
  for (Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa_vector/", 0) == 0 ||
        def.fullPath.rfind("/std/collections/soa_vector/", 0) == 0 ||
        def.fullPath.rfind("/std/collections/experimental_soa_vector/", 0) == 0) {
      continue;
    }
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths);
          binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaSamePathHelperMethodStatements(
        def.statements,
        bindings,
        soaVectorReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
            binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaSamePathHelperMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaVectorReturnDefinitions,
          structPaths,
          definitionNamespace,
          visibleSoaHelpers);
    }
  }
  return true;
}

void rewriteExperimentalSoaToAosMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaToAosMethodExpr(
        stmt,
        bindings,
        soaVectorReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaToAosMethodStatements(
          stmt.bodyArguments,
          bodyBindings,
          soaVectorReturnDefinitions,
          structPaths,
          definitionNamespace,
          hasVisibleRootToAosHelper,
          hasVisibleCanonicalToAosHelper);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaToAosMethodExpr(
        arg,
        bindings,
        soaVectorReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  if (builtinSoaConversionMethodName(expr.name) != "to_aos") {
    return;
  }

  std::optional<semantics::BindingInfo> receiverBinding;
  std::optional<Expr> canonicalReceiverExpr;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForToAosRewrite(binding, ignoredElemType);
  };
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (semantics::isSimpleCallName(receiver, "dereference") && receiver.args.size() == 1 &&
        receiver.args.front().kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.args.front().name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        receiverBinding = bindingIt->second;
      }
    }
    const std::vector<std::string> candidatePaths =
        candidatePathsForExprCall(receiver, definitionNamespace, &bindings, &structPaths);
    if (!receiverBinding.has_value()) {
      for (const std::string &candidatePath : candidatePaths) {
        auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
        if (returnIt != soaVectorReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          receiverBinding = returnIt->second;
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          break;
        }
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  const bool isExplicitRootToAosSurface =
      !expr.name.empty() && expr.name.front() == '/';
  if (isExplicitRootToAosSurface) {
    if (!hasVisibleRootToAosHelper) {
      return;
    }
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to_aos";
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }
  if (hasVisibleRootToAosHelper) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to_aos";
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }
  if (hasVisibleCanonicalToAosHelper) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/std/collections/soa_vector/to_aos";
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/to_aos";
  expr.namespacePrefix.clear();
  if (canonicalReceiverExpr.has_value()) {
    expr.args.front() = *canonicalReceiverExpr;
  }
}

bool rewriteExperimentalSoaToAosMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  const bool hasVisibleRootToAosHelper =
      hasVisibleRootExperimentalSoaHelper(program, "to_aos");
  bool hasVisibleCanonicalToAosHelper = false;
  auto canonicalizeSoaToAosDefinitionPath = [](std::string path) {
    const size_t specializationSuffix = path.find("__");
    if (specializationSuffix != std::string::npos) {
      path.erase(specializationSuffix);
    }
    return path;
  };
  auto isCanonicalSoaToAosDefinitionPath = [&](std::string_view path) {
    const std::string canonicalPath =
        canonicalizeSoaToAosDefinitionPath(std::string(path));
    return canonicalPath.rfind("/std/collections/soa_vector/", 0) == 0 &&
           semantics::isLegacyOrCanonicalSoaHelperPath(canonicalPath, "to_aos");
  };
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
    if (isCanonicalSoaToAosDefinitionPath(def.fullPath)) {
      hasVisibleCanonicalToAosHelper = true;
    }
  }
  const auto &importPaths =
      program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    const std::string canonicalToAosImportTarget =
        "/std/collections/soa_vector/to_aos";
    if (isCanonicalSoaToAosDefinitionPath(canonicalToAosImportTarget) &&
        localImportPathCoversTarget(importPath, canonicalToAosImportTarget)) {
      hasVisibleCanonicalToAosHelper = true;
    }
    if (hasVisibleCanonicalToAosHelper) {
      break;
    }
  }
  for (Definition &def : program.definitions) {
    if (def.fullPath == "/to_aos" ||
        def.fullPath.rfind("/to_aos__", 0) == 0 ||
        isCanonicalSoaToAosDefinitionPath(def.fullPath)) {
      continue;
    }
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaToAosMethodStatements(
        def.statements,
        bindings,
        soaVectorReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaToAosMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaVectorReturnDefinitions,
          structPaths,
          definitionNamespace,
          hasVisibleRootToAosHelper,
          hasVisibleCanonicalToAosHelper);
    }
  }
  return true;
}

std::vector<std::string> candidatePathsForExprCall(
    const Expr &callExpr,
    const std::string &definitionNamespace,
    const std::unordered_map<std::string, semantics::BindingInfo> *bindings = nullptr,
    const std::unordered_set<std::string> *structPaths = nullptr) {
  std::vector<std::string> candidatePaths;
  if (callExpr.isMethodCall && !callExpr.args.empty() && bindings != nullptr && structPaths != nullptr &&
      callExpr.args.front().kind == Expr::Kind::Name) {
    const Expr &receiver = callExpr.args.front();
    auto bindingIt = bindings->find(receiver.name);
    if (bindingIt != bindings->end()) {
      const std::string receiverNamespace =
          !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : definitionNamespace;
      const std::string receiverStructPath = resolveStructReceiverPathFromBinding(
          bindingIt->second, receiverNamespace, *structPaths);
      if (!receiverStructPath.empty()) {
        candidatePaths.push_back(receiverStructPath + "/" + callExpr.name);
      }
    }
  }
  if (!callExpr.name.empty() && callExpr.name.front() == '/') {
    candidatePaths.push_back(callExpr.name);
  } else {
    if (!callExpr.namespacePrefix.empty()) {
      candidatePaths.push_back(callExpr.namespacePrefix + "/" + callExpr.name);
    }
    if (!definitionNamespace.empty()) {
      candidatePaths.push_back(definitionNamespace + "/" + callExpr.name);
    }
    candidatePaths.push_back("/" + callExpr.name);
    candidatePaths.push_back(callExpr.name);
  }
  return candidatePaths;
}

Expr canonicalizeResolvedCallPath(const Expr &callExpr, const std::string &resolvedPath) {
  Expr rewritten = callExpr;
  rewritten.name = resolvedPath;
  rewritten.namespacePrefix.clear();
  rewritten.isMethodCall = false;
  rewritten.isFieldAccess = false;
  return rewritten;
}

std::optional<Expr> normalizeExperimentalSoaInlineBorrowReceiver(
    const Expr &receiver,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> *soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> *structPaths) {
  auto canonicalExperimentalSoaReceiver = [&](const Expr &expr) -> std::optional<Expr> {
    if (expr.kind == Expr::Kind::Name) {
      const std::string &name = expr.name;
      auto bindingIt = bindings.find(name);
      if (bindingIt == bindings.end()) {
        return std::nullopt;
      }
      std::string ignoredElemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              bindingIt->second, ignoredElemType)) {
        return expr;
      }
      return std::nullopt;
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding || soaVectorReturnDefinitions == nullptr) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(expr, definitionNamespace, &bindings, structPaths)) {
      auto returnIt = soaVectorReturnDefinitions->find(candidatePath);
      if (returnIt == soaVectorReturnDefinitions->end()) {
        continue;
      }
      std::string ignoredElemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              returnIt->second, ignoredElemType)) {
        return canonicalizeResolvedCallPath(expr, candidatePath);
      }
    }
    return std::nullopt;
  };
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return std::nullopt;
  }
  if (semantics::isSimpleCallName(receiver, "location") && receiver.args.size() == 1) {
    if (auto canonicalReceiver = canonicalExperimentalSoaReceiver(receiver.args.front());
        canonicalReceiver.has_value()) {
      return canonicalReceiver;
    }
  }
  if (semantics::isSimpleCallName(receiver, "dereference") &&
      receiver.args.size() == 1) {
    const Expr &borrowedExpr = receiver.args.front();
    if (borrowedExpr.kind == Expr::Kind::Call &&
        !borrowedExpr.isBinding &&
        semantics::isSimpleCallName(borrowedExpr, "location") &&
        borrowedExpr.args.size() == 1) {
      if (auto canonicalReceiver = canonicalExperimentalSoaReceiver(borrowedExpr.args.front());
          canonicalReceiver.has_value()) {
        return canonicalReceiver;
      }
    }
  }
  return std::nullopt;
}

std::optional<Expr> normalizeExperimentalSoaBorrowedHelperReceiver(
    const Expr &receiver,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &structPaths) {
  auto makeDereferenceCall = [](Expr borrowedExpr) {
    Expr dereferenceCall;
    dereferenceCall.kind = Expr::Kind::Call;
    dereferenceCall.name = "dereference";
    dereferenceCall.sourceLine = borrowedExpr.sourceLine;
    dereferenceCall.sourceColumn = borrowedExpr.sourceColumn;
    dereferenceCall.args.push_back(std::move(borrowedExpr));
    dereferenceCall.argNames.resize(dereferenceCall.args.size());
    return dereferenceCall;
  };
  auto isBorrowedBinding = [&](const semantics::BindingInfo &binding) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(binding.typeName);
    if (normalizedType != "Reference" && normalizedType != "Pointer") {
      return false;
    }
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, ignoredElemType);
  };
  auto canonicalBorrowedExperimentalSoaCall = [&](const Expr &expr) -> std::optional<Expr> {
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(expr, definitionNamespace, &bindings, &structPaths)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() &&
          isBorrowedBinding(returnIt->second)) {
        return canonicalizeResolvedCallPath(expr, candidatePath);
      }
    }
    return std::nullopt;
  };
  if (auto normalizedInline = normalizeExperimentalSoaInlineBorrowReceiver(
          receiver, bindings, &soaVectorReturnDefinitions, definitionNamespace, &structPaths);
      normalizedInline.has_value()) {
    return normalizedInline;
  }
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
      return makeDereferenceCall(receiver);
    }
  }
  if (auto canonicalReceiver = canonicalBorrowedExperimentalSoaCall(receiver);
      canonicalReceiver.has_value()) {
    return makeDereferenceCall(*canonicalReceiver);
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      semantics::isSimpleCallName(receiver, "dereference") &&
      receiver.args.size() == 1) {
    const Expr &borrowedSource = receiver.args.front();
    if (borrowedSource.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(borrowedSource.name);
      if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
        return receiver;
      }
    }
    if (auto canonicalBorrowedSource = canonicalBorrowedExperimentalSoaCall(borrowedSource);
        canonicalBorrowedSource.has_value()) {
      Expr normalizedReceiver = receiver;
      normalizedReceiver.args.front() = *canonicalBorrowedSource;
      return normalizedReceiver;
    }
  }
  return std::nullopt;
}

bool normalizeExperimentalSoaBorrowedHelperMethodCall(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  auto isBorrowedBinding = [&](const semantics::BindingInfo &binding) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(binding.typeName);
    if (normalizedType != "Reference" && normalizedType != "Pointer") {
      return false;
    }
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, ignoredElemType);
  };
  auto canonicalBorrowedExperimentalSoaCall = [&](const Expr &candidate)
      -> std::optional<Expr> {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(candidate,
                                   definitionNamespace,
                                   &bindings,
                                   &structPaths)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() &&
          isBorrowedBinding(returnIt->second)) {
        return canonicalizeResolvedCallPath(candidate, candidatePath);
      }
    }
    return std::nullopt;
  };
  auto normalizedBorrowedReceiver = [&](const Expr &receiver)
      -> std::optional<Expr> {
    if (receiver.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.name);
      if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
        return receiver;
      }
      return std::nullopt;
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return std::nullopt;
    }
    if (semantics::isSimpleCallName(receiver, "location") &&
        receiver.args.size() == 1) {
      const Expr &target = receiver.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(target.name);
        if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
          return target;
        }
      }
      if (auto canonicalBorrowedCall = canonicalBorrowedExperimentalSoaCall(target);
          canonicalBorrowedCall.has_value()) {
        return canonicalBorrowedCall;
      }
      return std::nullopt;
    }
    if (auto canonicalBorrowedCall = canonicalBorrowedExperimentalSoaCall(receiver);
        canonicalBorrowedCall.has_value()) {
      return canonicalBorrowedCall;
    }
    if (semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (borrowedSource.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(borrowedSource.name);
        if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
          return borrowedSource;
        }
      }
      if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding &&
          semantics::isSimpleCallName(borrowedSource, "location") &&
          borrowedSource.args.size() == 1) {
        const Expr &target = borrowedSource.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto bindingIt = bindings.find(target.name);
          if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
            return target;
          }
        }
        if (auto canonicalBorrowedTarget =
                canonicalBorrowedExperimentalSoaCall(target);
            canonicalBorrowedTarget.has_value()) {
          return canonicalBorrowedTarget;
        }
        return std::nullopt;
      }
      if (auto canonicalBorrowedSource =
              canonicalBorrowedExperimentalSoaCall(borrowedSource);
          canonicalBorrowedSource.has_value()) {
        return canonicalBorrowedSource;
      }
    }
    return std::nullopt;
  };
  auto borrowedReceiverElementType = [&](const Expr &receiver)
      -> std::optional<std::string> {
    auto bindingElementType = [&](const semantics::BindingInfo &binding)
        -> std::optional<std::string> {
      std::string elemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              binding, elemType) &&
          !elemType.empty()) {
        return elemType;
      }
      return std::nullopt;
    };
    auto elementTypeForBorrowedSource = [&](const Expr &borrowedSource)
        -> std::optional<std::string> {
      if (borrowedSource.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(borrowedSource.name);
        return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                           : std::nullopt;
      }
      if (borrowedSource.kind != Expr::Kind::Call || borrowedSource.isBinding) {
        return std::nullopt;
      }
      for (const std::string &candidatePath :
           candidatePathsForExprCall(borrowedSource,
                                     definitionNamespace,
                                     &bindings,
                                     &structPaths)) {
        auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
        if (returnIt != soaVectorReturnDefinitions.end()) {
          if (auto elemType = bindingElementType(returnIt->second);
              elemType.has_value()) {
            return elemType;
          }
        }
      }
      return std::nullopt;
    };
    if (auto normalizedBorrowed =
            normalizeExperimentalSoaBorrowedHelperReceiver(
                receiver,
                bindings,
                soaVectorReturnDefinitions,
                definitionNamespace,
                structPaths);
        normalizedBorrowed.has_value() &&
        normalizedBorrowed->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*normalizedBorrowed, "dereference") &&
        normalizedBorrowed->args.size() == 1) {
      if (auto elemType = elementTypeForBorrowedSource(normalizedBorrowed->args.front());
          elemType.has_value()) {
        return elemType;
      }
    }
    if (receiver.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.name);
      return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                         : std::nullopt;
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidateDefinitionPaths(receiver, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end()) {
        if (auto elemType = bindingElementType(returnIt->second);
            elemType.has_value()) {
          return elemType;
        }
      }
    }
    if (semantics::isSimpleCallName(receiver, "location") &&
        receiver.args.size() == 1) {
      const Expr &target = receiver.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(target.name);
        return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                           : std::nullopt;
      }
      if (target.kind == Expr::Kind::Call && !target.isBinding) {
        for (const std::string &candidatePath :
             candidateDefinitionPaths(target, definitionNamespace)) {
          auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
          if (returnIt != soaVectorReturnDefinitions.end()) {
            if (auto elemType = bindingElementType(returnIt->second);
                elemType.has_value()) {
              return elemType;
            }
          }
        }
      }
      for (const std::string &candidatePath :
           candidatePathsForExprCall(target,
                                     definitionNamespace,
                                     &bindings,
                                     &structPaths)) {
        auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
        if (returnIt != soaVectorReturnDefinitions.end()) {
          if (auto elemType = bindingElementType(returnIt->second);
              elemType.has_value()) {
            return elemType;
          }
        }
      }
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(receiver,
                                   definitionNamespace,
                                   &bindings,
                                   &structPaths)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end()) {
        if (auto elemType = bindingElementType(returnIt->second);
            elemType.has_value()) {
          return elemType;
        }
      }
    }
    if (semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (auto elemType = elementTypeForBorrowedSource(borrowedSource);
          elemType.has_value()) {
        return elemType;
      }
      if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding &&
          semantics::isSimpleCallName(borrowedSource, "location") &&
          borrowedSource.args.size() == 1) {
        const Expr &target = borrowedSource.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto bindingIt = bindings.find(target.name);
          return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                             : std::nullopt;
        }
        if (target.kind == Expr::Kind::Call && !target.isBinding) {
          for (const std::string &candidatePath :
               candidateDefinitionPaths(target, definitionNamespace)) {
            auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
            if (returnIt != soaVectorReturnDefinitions.end()) {
              if (auto elemType = bindingElementType(returnIt->second);
                  elemType.has_value()) {
                return elemType;
              }
            }
          }
        }
      }
      if (auto canonicalBorrowedSource =
              canonicalBorrowedExperimentalSoaCall(borrowedSource);
          canonicalBorrowedSource.has_value()) {
        for (const std::string &candidatePath :
             candidatePathsForExprCall(borrowedSource,
                                       definitionNamespace,
                                       &bindings,
                                       &structPaths)) {
          auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
          if (returnIt != soaVectorReturnDefinitions.end()) {
            if (auto elemType = bindingElementType(returnIt->second);
                elemType.has_value()) {
              return elemType;
            }
          }
        }
      }
    }
    return std::nullopt;
  };
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return false;
  }
  const std::string normalizedMethodName = [&]() {
    std::string name = expr.name;
    if (!name.empty() && name.front() == '/') {
      name.erase(name.begin());
    }
    if (name.rfind("std/collections/soa_vector/", 0) == 0) {
      name = name.substr(std::string("std/collections/soa_vector/").size());
    } else if (name.rfind("soa_vector/", 0) == 0) {
      name = name.substr(std::string("soa_vector/").size());
    }
    return name;
  }();
  if (normalizedMethodName != "count" &&
      normalizedMethodName != "get" &&
      normalizedMethodName != "ref" &&
      normalizedMethodName != "to_aos") {
    return false;
  }
  if (auto borrowedReceiver = normalizedBorrowedReceiver(expr.args.front());
      borrowedReceiver.has_value()) {
    const auto borrowedElemType = borrowedReceiverElementType(expr.args.front());
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.namespacePrefix.clear();
    expr.args.front() = *borrowedReceiver;
    if (borrowedElemType.has_value()) {
      expr.templateArgs.clear();
      expr.templateArgs.push_back(*borrowedElemType);
    }
    if (normalizedMethodName == "count") {
      expr.name = "/std/collections/soa_vector/count_ref";
      return true;
    }
    if (normalizedMethodName == "get") {
      expr.name = "/std/collections/soa_vector/get_ref";
      return true;
    }
    if (normalizedMethodName == "ref") {
      expr.name = "/std/collections/soa_vector/ref_ref";
      return true;
    }
    expr.name = "/std/collections/soa_vector/to_aos_ref";
    return true;
  }
  if (!expr.isMethodCall && expr.name.find('/') != std::string::npos) {
    return false;
  }
  const auto normalizedReceiver = normalizeExperimentalSoaBorrowedHelperReceiver(
      expr.args.front(), bindings, soaVectorReturnDefinitions, definitionNamespace, structPaths);
  if (!normalizedReceiver.has_value()) {
    return false;
  }
  expr.args.front() = *normalizedReceiver;
  if (!expr.isMethodCall) {
    expr.isMethodCall = true;
  }
  return true;
}

void rewriteExperimentalSoaInlineBorrowMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace);

bool isStructLikeDefinition(const Definition &def);

void rewriteExperimentalSoaInlineBorrowMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaInlineBorrowMethodExpr(
        stmt, bindings, soaVectorReturnDefinitions, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaInlineBorrowMethodStatements(
          stmt.bodyArguments, bodyBindings, soaVectorReturnDefinitions, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaInlineBorrowMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaInlineBorrowMethodExpr(
        arg, bindings, soaVectorReturnDefinitions, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call) {
    return;
  }
  if (expr.isFieldAccess && !expr.args.empty()) {
    Expr &receiverExpr = expr.args.front();
    normalizeExperimentalSoaBorrowedHelperMethodCall(
        receiverExpr, bindings, soaVectorReturnDefinitions, structPaths, definitionNamespace);
  }
  normalizeExperimentalSoaBorrowedHelperMethodCall(
      expr, bindings, soaVectorReturnDefinitions, structPaths, definitionNamespace);
}

bool rewriteExperimentalSoaInlineBorrowMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaInlineBorrowMethodStatements(
        def.statements, bindings, soaVectorReturnDefinitions, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaInlineBorrowMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaVectorReturnDefinitions,
          structPaths,
          definitionNamespace);
    }
  }
  return true;
}

bool isStructLikeDefinition(const Definition &def) {
  bool hasStructTransform = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (semantics::isStructTransformName(transform.name)) {
      hasStructTransform = true;
    }
  }
  bool fieldOnlyStruct = false;
  if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() &&
      !def.hasReturnStatement && !def.returnExpr.has_value()) {
    fieldOnlyStruct = true;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        fieldOnlyStruct = false;
        break;
      }
    }
  }
  return hasStructTransform || fieldOnlyStruct;
}

void rewriteExperimentalSoaFieldViewIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaVectorReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewIndexStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaVectorReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewIndexExpr(
        stmt,
        bindings,
        allBindings,
        soaVectorReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaFieldViewIndexStatements(
          stmt.bodyArguments,
          bodyBindings,
          allBindings,
          soaVectorReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldNames,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto soaBinding = extractExperimentalSoaVectorFieldViewReceiverBinding(stmt);
          soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaVectorReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewIndexExpr(
        arg,
        bindings,
        allBindings,
        soaVectorReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      expr.templateArgs.size() != 0 || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || semantics::hasNamedArguments(expr.argNames) ||
      expr.args.size() != 2) {
    return;
  }

  std::string builtinAccessName;
  if (!semantics::getBuiltinArrayAccessName(expr, builtinAccessName) ||
      builtinAccessName != "at") {
    return;
  }

  const Expr &fieldViewExpr = expr.args.front();
  if (fieldViewExpr.kind != Expr::Kind::Call || fieldViewExpr.isBinding ||
      fieldViewExpr.name.empty() ||
      fieldViewExpr.name.find('/') != std::string::npos ||
      !fieldViewExpr.templateArgs.empty() || fieldViewExpr.hasBodyArguments ||
      !fieldViewExpr.bodyArguments.empty() ||
      semantics::hasNamedArguments(fieldViewExpr.argNames) ||
      fieldViewExpr.args.size() != 1) {
    return;
  }

  if (visibleSoaFieldHelpers.count("/soa_vector/" + fieldViewExpr.name) > 0) {
    return;
  }

  std::string receiverElemType;
  bool receiverNeedsDereference = false;
  const Expr &receiver = fieldViewExpr.args.front();
  std::optional<Expr> canonicalReceiverExpr;
  const Expr *getReceiverExpr = &receiver;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    receiverNeedsDereference =
        semantics::normalizeBindingTypeName(binding.typeName) == "Reference" ||
        semantics::normalizeBindingTypeName(binding.typeName) == "Pointer";
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, specializedSoaVectorElementTypes, receiverElemType);
  };
  auto candidatePathsForCall = [&](const Expr &callExpr) {
    return candidatePathsForExprCall(callExpr, definitionNamespace, &allBindings, &structPaths);
  };
  auto tryLocationReceiverBinding = [&](const Expr &locationExpr) -> bool {
    if (!semantics::isSimpleCallName(locationExpr, "location") ||
        locationExpr.args.size() != 1) {
      return false;
    }
    const Expr &locationTarget = locationExpr.args.front();
    if (locationTarget.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(locationTarget.name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
      auto allBindingIt = allBindings.find(locationTarget.name);
      if (allBindingIt != allBindings.end() &&
          tryReceiverBinding(allBindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
    } else if (locationTarget.kind == Expr::Kind::Call && !locationTarget.isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(locationTarget)) {
        auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
        if (returnIt != soaVectorReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(locationTarget, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          return true;
        }
      }
    }
    return false;
  };
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      // handled
    } else {
      auto allBindingIt = allBindings.find(receiver.name);
      if (allBindingIt != allBindings.end()) {
        tryReceiverBinding(allBindingIt->second);
      }
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (!tryLocationReceiverBinding(receiver) &&
        semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (tryLocationReceiverBinding(borrowedSource)) {
        // handled
      } else if (borrowedSource.kind == Expr::Kind::Name) {
        const std::string &sourceName = borrowedSource.name;
        auto bindingIt = bindings.find(sourceName);
        if (bindingIt != bindings.end()) {
          tryReceiverBinding(bindingIt->second);
        }
        if (receiverElemType.empty()) {
          auto allBindingIt = allBindings.find(sourceName);
          if (allBindingIt != allBindings.end()) {
            tryReceiverBinding(allBindingIt->second);
          }
        }
      } else if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding) {
        for (const std::string &candidatePath : candidatePathsForCall(borrowedSource)) {
          auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
          if (returnIt != soaVectorReturnDefinitions.end() &&
              tryReceiverBinding(returnIt->second)) {
            canonicalReceiverExpr = canonicalizeResolvedCallPath(borrowedSource, candidatePath);
            getReceiverExpr = &*canonicalReceiverExpr;
            break;
          }
        }
      }
    }
    if (receiverElemType.empty()) {
      for (const std::string &candidatePath : candidatePathsForCall(receiver)) {
        auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
        if (returnIt != soaVectorReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          break;
        }
      }
    }
  }
  if (receiverElemType.empty()) {
    return;
  }

  const std::string normalizedElemType =
      semantics::normalizeBindingTypeName(receiverElemType);
  if (normalizedElemType.empty()) {
    return;
  }
  const std::string lookupNamespace =
      !getReceiverExpr->namespacePrefix.empty() ? getReceiverExpr->namespacePrefix : definitionNamespace;
  const std::string elementStructPath =
      semantics::resolveStructTypePath(normalizedElemType, lookupNamespace, structPaths);
  auto fieldIt = structFieldNames.find(elementStructPath);
  if (elementStructPath.empty() || fieldIt == structFieldNames.end() ||
      fieldIt->second.count(fieldViewExpr.name) == 0) {
    return;
  }

  Expr getCall;
  getCall.kind = Expr::Kind::Call;
  getCall.name = "/std/collections/experimental_soa_vector/soaVectorGet";
  getCall.templateArgs = {receiverElemType};
  auto appendReceiverValueExpr = [&](Expr &callExpr) {
    if (!receiverNeedsDereference) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    Expr dereferenceCall;
    dereferenceCall.kind = Expr::Kind::Call;
    dereferenceCall.name = "dereference";
    dereferenceCall.args.push_back(*getReceiverExpr);
    dereferenceCall.argNames.resize(dereferenceCall.args.size());
    dereferenceCall.sourceLine = getReceiverExpr->sourceLine;
    dereferenceCall.sourceColumn = getReceiverExpr->sourceColumn;
    callExpr.args.push_back(std::move(dereferenceCall));
  };
  appendReceiverValueExpr(getCall);
  getCall.args.push_back(expr.args[1]);
  getCall.argNames.resize(getCall.args.size());
  getCall.sourceLine = expr.sourceLine;
  getCall.sourceColumn = expr.sourceColumn;

  expr = {};
  expr.kind = Expr::Kind::Call;
  expr.name = fieldViewExpr.name;
  expr.isMethodCall = true;
  expr.isFieldAccess = true;
  expr.args.push_back(std::move(getCall));
  expr.argNames.push_back(std::nullopt);
  expr.sourceLine = fieldViewExpr.sourceLine;
  expr.sourceColumn = fieldViewExpr.sourceColumn;
}

bool rewriteExperimentalSoaFieldViewIndexes(Program &program, std::string &error) {
  error.clear();

  std::unordered_map<std::string, std::unordered_set<std::string>> structFieldNames;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaFieldHelpers;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  const auto specializedSoaVectorElementTypes =
      buildSpecializedExperimentalSoaVectorElementTypes(program);

  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind("/std/collections/soa_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string("/std/collections/soa_vector/").size());
      visibleSoaFieldHelpers.insert("/soa_vector/" + helperSuffix);
    }
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (!isStructLikeDefinition(def)) {
      continue;
    }
    structPaths.insert(def.fullPath);
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    std::unordered_set<std::string> fieldNames;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticField(stmt)) {
        continue;
      }
      fieldNames.insert(stmt.name);
    }
    if (fieldNames.empty()) {
      continue;
    }
    structFieldNames.emplace(def.fullPath, std::move(fieldNames));
  }

  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    auto allBindings = bindings;
    std::function<void(const std::vector<Expr> &)> collectBindings =
        [&](const std::vector<Expr> &statements) {
          for (const Expr &stmt : statements) {
            if (stmt.isBinding) {
              if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
                allBindings[stmt.name] = *binding;
              }
            }
            if (!stmt.bodyArguments.empty()) {
              collectBindings(stmt.bodyArguments);
            }
          }
        };
    collectBindings(def.statements);
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewIndexStatements(
        def.statements,
        bindings,
        allBindings,
        soaVectorReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewIndexExpr(
          *def.returnExpr,
          bindings,
          allBindings,
          soaVectorReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldNames,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewHelperExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaVectorReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewHelperStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaVectorReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewHelperExpr(
        stmt,
        bindings,
        allBindings,
        soaVectorReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaFieldViewHelperStatements(
          stmt.bodyArguments,
          bodyBindings,
          allBindings,
          soaVectorReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldInfo,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewHelperExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaVectorReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewHelperExpr(
        arg,
        bindings,
        allBindings,
        soaVectorReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.isBinding ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      !expr.templateArgs.empty()) {
    return;
  }

  std::string fieldName;
  if (!expr.name.empty() && expr.name.front() == '/' &&
      semantics::splitSoaFieldViewHelperPath(expr.name, &fieldName)) {
  } else {
    if (!expr.namespacePrefix.empty()) {
      return;
    }
    fieldName = expr.name;
    if (!fieldName.empty() && fieldName.front() == '/') {
      fieldName.erase(fieldName.begin());
    }
    if (fieldName.empty() || fieldName.find('/') != std::string::npos ||
        fieldName == "count" || fieldName == "get" || fieldName == "ref" ||
        fieldName == "to_soa" || fieldName == "to_aos" ||
        fieldName == "to_aos_ref") {
      return;
    }
  }

  if (visibleSoaFieldHelpers.count("/soa_vector/" + fieldName) > 0) {
    return;
  }
  if (expr.args.size() != 1) {
    return;
  }

  std::string receiverElemType;
  bool receiverNeedsDereference = false;
  const Expr &receiver = expr.args.front();
  std::optional<Expr> canonicalReceiverExpr;
  const Expr *getReceiverExpr = &receiver;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    receiverNeedsDereference =
        semantics::normalizeBindingTypeName(binding.typeName) == "Reference" ||
        semantics::normalizeBindingTypeName(binding.typeName) == "Pointer";
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, specializedSoaVectorElementTypes, receiverElemType);
  };
  auto candidatePathsForCall = [&](const Expr &callExpr) {
    return candidatePathsForExprCall(callExpr, definitionNamespace, &allBindings, &structPaths);
  };
  auto tryLocationReceiverBinding = [&](const Expr &locationExpr) -> bool {
    if (!semantics::isSimpleCallName(locationExpr, "location") ||
        locationExpr.args.size() != 1) {
      return false;
    }
    const Expr &locationTarget = locationExpr.args.front();
    if (locationTarget.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(locationTarget.name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
      auto allBindingIt = allBindings.find(locationTarget.name);
      if (allBindingIt != allBindings.end() &&
          tryReceiverBinding(allBindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
    } else if (locationTarget.kind == Expr::Kind::Call && !locationTarget.isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(locationTarget)) {
        auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
        if (returnIt != soaVectorReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(locationTarget, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          return true;
        }
      }
    }
    return false;
  };
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      // handled
    } else {
      auto allBindingIt = allBindings.find(receiver.name);
      if (allBindingIt != allBindings.end()) {
        tryReceiverBinding(allBindingIt->second);
      }
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (!tryLocationReceiverBinding(receiver) &&
        semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (tryLocationReceiverBinding(borrowedSource)) {
        // handled
      } else if (borrowedSource.kind == Expr::Kind::Name) {
        const std::string &sourceName = borrowedSource.name;
        auto bindingIt = bindings.find(sourceName);
        if (bindingIt != bindings.end()) {
          tryReceiverBinding(bindingIt->second);
        }
        if (receiverElemType.empty()) {
          auto allBindingIt = allBindings.find(sourceName);
          if (allBindingIt != allBindings.end()) {
            tryReceiverBinding(allBindingIt->second);
          }
        }
      } else if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding) {
        for (const std::string &candidatePath : candidatePathsForCall(borrowedSource)) {
          auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
          if (returnIt != soaVectorReturnDefinitions.end() &&
              tryReceiverBinding(returnIt->second)) {
            canonicalReceiverExpr = canonicalizeResolvedCallPath(borrowedSource, candidatePath);
            getReceiverExpr = &*canonicalReceiverExpr;
            break;
          }
        }
      }
    }
    if (receiverElemType.empty()) {
      for (const std::string &candidatePath : candidatePathsForCall(receiver)) {
        auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
        if (returnIt != soaVectorReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          break;
        }
      }
    }
  }
  if (receiverElemType.empty()) {
    return;
  }

  const std::string normalizedElemType =
      semantics::normalizeBindingTypeName(receiverElemType);
  if (normalizedElemType.empty()) {
    return;
  }
  const std::string lookupNamespace =
      !getReceiverExpr->namespacePrefix.empty() ? getReceiverExpr->namespacePrefix : definitionNamespace;
  const std::string elementStructPath =
      semantics::resolveStructTypePath(normalizedElemType, lookupNamespace, structPaths);
  auto structIt = structFieldInfo.find(elementStructPath);
  if (elementStructPath.empty() || structIt == structFieldInfo.end()) {
    return;
  }
  auto fieldIt = structIt->second.find(fieldName);
  if (fieldIt == structIt->second.end()) {
    return;
  }

  Expr fieldViewCall;
  fieldViewCall.kind = Expr::Kind::Call;
  fieldViewCall.name = "/std/collections/experimental_soa_vector/soaVectorFieldView";
  fieldViewCall.templateArgs = {receiverElemType, fieldIt->second.typeText};
  auto appendReceiverValueExpr = [&](Expr &callExpr) {
    if (!receiverNeedsDereference) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    Expr dereferenceCall;
    dereferenceCall.kind = Expr::Kind::Call;
    dereferenceCall.name = "dereference";
    dereferenceCall.args.push_back(*getReceiverExpr);
    dereferenceCall.argNames.resize(dereferenceCall.args.size());
    dereferenceCall.sourceLine = getReceiverExpr->sourceLine;
    dereferenceCall.sourceColumn = getReceiverExpr->sourceColumn;
    callExpr.args.push_back(std::move(dereferenceCall));
  };
  appendReceiverValueExpr(fieldViewCall);
  fieldViewCall.args.push_back(makeI32LiteralExpr(
      static_cast<uint64_t>(fieldIt->second.index),
      expr.sourceLine,
      expr.sourceColumn));
  fieldViewCall.argNames.resize(fieldViewCall.args.size());
  fieldViewCall.sourceLine = expr.sourceLine;
  fieldViewCall.sourceColumn = expr.sourceColumn;

  expr = std::move(fieldViewCall);
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.namespacePrefix.clear();
}

bool rewriteExperimentalSoaFieldViewHelpers(Program &program, std::string &error) {
  error.clear();

  std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>> structFieldInfo;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaFieldHelpers;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  const auto specializedSoaVectorElementTypes =
      buildSpecializedExperimentalSoaVectorElementTypes(program);

  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind("/std/collections/soa_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string("/std/collections/soa_vector/").size());
      visibleSoaFieldHelpers.insert("/soa_vector/" + helperSuffix);
    }
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }

  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  for (const Definition &def : program.definitions) {
    if (!isStructLikeDefinition(def)) {
      continue;
    }
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    std::unordered_map<std::string, SoaFieldViewFieldInfo> fields;
    size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticField(stmt)) {
        continue;
      }
      semantics::BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (semantics::parseBindingInfo(stmt,
                                      def.namespacePrefix,
                                      structPaths,
                                      emptyImportAliases,
                                      binding,
                                      restrictType,
                                      parseError)) {
        std::string typeText = binding.typeName;
        if (!binding.typeTemplateArg.empty()) {
          typeText += "<" + binding.typeTemplateArg + ">";
        }
        fields.emplace(stmt.name,
                       SoaFieldViewFieldInfo{
                           fieldIndex,
                           qualifySoaFieldViewTypeText(typeText,
                                                       def.namespacePrefix,
                                                       structPaths)});
      }
      ++fieldIndex;
    }
    if (!fields.empty()) {
      structFieldInfo.emplace(def.fullPath, std::move(fields));
    }
  }

  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    auto allBindings = bindings;
    std::function<void(const std::vector<Expr> &)> collectBindings =
        [&](const std::vector<Expr> &statements) {
          for (const Expr &stmt : statements) {
            if (stmt.isBinding) {
              if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
                allBindings[stmt.name] = *binding;
              }
            }
            if (!stmt.bodyArguments.empty()) {
              collectBindings(stmt.bodyArguments);
            }
          }
        };
    collectBindings(def.statements);
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewHelperStatements(
        def.statements,
        bindings,
        allBindings,
        soaVectorReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewHelperExpr(
          *def.returnExpr,
          bindings,
          allBindings,
          soaVectorReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldInfo,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewCarrierIndexStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, std::string> fieldViewBindings,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        stmt, fieldViewBindings, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = fieldViewBindings;
      rewriteExperimentalSoaFieldViewCarrierIndexStatements(
          stmt.bodyArguments, bodyBindings, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto parsed = extractParsedBindingInfo(stmt, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[stmt.name] = elemType;
        }
      }
      if (stmt.args.size() == 1 && fieldViewBindings.count(stmt.name) == 0) {
        const Expr &initializer = stmt.args.front();
        if (initializer.kind == Expr::Kind::Call && !initializer.isBinding) {
          std::string initPath = initializer.name;
          if (!initPath.empty() && initPath.front() != '/') {
            initPath.insert(initPath.begin(), '/');
          }
          if (semantics::isExperimentalSoaFieldViewHelperPath(initPath)) {
            if (initializer.templateArgs.size() >= 2) {
              fieldViewBindings[stmt.name] =
                  qualifySoaFieldViewTypeText(initializer.templateArgs[1],
                                              definitionNamespace,
                                              structPaths);
            }
          }
        }
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        arg, fieldViewBindings, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      expr.templateArgs.size() != 0 || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || semantics::hasNamedArguments(expr.argNames) ||
      expr.args.size() != 2) {
    return;
  }

  std::string builtinAccessName;
  if (!semantics::getBuiltinArrayAccessName(expr, builtinAccessName) ||
      builtinAccessName != "at") {
    return;
  }

  const Expr &fieldViewExpr = expr.args.front();
  std::string elemType;
  if (fieldViewExpr.kind == Expr::Kind::Name) {
    auto bindingIt = fieldViewBindings.find(fieldViewExpr.name);
    if (bindingIt != fieldViewBindings.end()) {
      elemType = bindingIt->second;
    }
  } else if (fieldViewExpr.kind == Expr::Kind::Call && !fieldViewExpr.isBinding) {
    std::string callPath = fieldViewExpr.name;
    if (!callPath.empty() && callPath.front() != '/') {
      callPath.insert(callPath.begin(), '/');
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(callPath)) {
      if (fieldViewExpr.templateArgs.size() >= 2) {
        elemType = qualifySoaFieldViewTypeText(fieldViewExpr.templateArgs[1],
                                               definitionNamespace,
                                               structPaths);
      }
    }
  }
  if (elemType.empty()) {
    return;
  }

  Expr readCall;
  readCall.kind = Expr::Kind::Call;
  readCall.name = "/std/collections/internal_soa_storage/soaFieldViewRead";
  readCall.templateArgs = {elemType};
  readCall.args.push_back(fieldViewExpr);
  readCall.args.push_back(expr.args[1]);
  readCall.argNames.resize(readCall.args.size());
  readCall.sourceLine = expr.sourceLine;
  readCall.sourceColumn = expr.sourceColumn;
  expr = std::move(readCall);
}

bool rewriteExperimentalSoaFieldViewCarrierIndexes(Program &program, std::string &error) {
  error.clear();
  std::unordered_set<std::string> structPaths;
  for (const Definition &def : program.definitions) {
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    for (const Expr &param : def.parameters) {
      if (auto parsed = extractParsedBindingInfo(param, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[param.name] = elemType;
        }
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewCarrierIndexStatements(
        def.statements, fieldViewBindings, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          *def.returnExpr, fieldViewBindings, structPaths, definitionNamespace);
    }
  }
  for (auto &exec : program.executions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    std::string definitionNamespace;
    const size_t slash = exec.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = exec.fullPath.substr(0, slash);
    }
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structPaths, definitionNamespace);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structPaths, definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewAssignTargetsExpr(Expr &expr) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(bodyArg);
  }

  if (!semantics::isAssignCall(expr) || expr.args.size() != 2) {
    return;
  }

  Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::Call && !target.isBinding) {
    static constexpr std::string_view fieldRefPrefix =
        "/std/collections/internal_soa_storage/soaFieldViewRef";
    if (semantics::isExperimentalSoaFieldViewReadHelperPath(target.name) &&
        target.args.size() == 2 && !target.templateArgs.empty()) {
      Expr refCall;
      refCall.kind = Expr::Kind::Call;
      refCall.name = std::string(fieldRefPrefix);
      refCall.templateArgs = {target.templateArgs.front()};
      refCall.args = target.args;
      refCall.argNames.resize(refCall.args.size());
      refCall.sourceLine = target.sourceLine;
      refCall.sourceColumn = target.sourceColumn;

      Expr dereferenceCall;
      dereferenceCall.kind = Expr::Kind::Call;
      dereferenceCall.name = "dereference";
      dereferenceCall.args.push_back(std::move(refCall));
      dereferenceCall.argNames.resize(dereferenceCall.args.size());
      dereferenceCall.sourceLine = target.sourceLine;
      dereferenceCall.sourceColumn = target.sourceColumn;

      target = std::move(dereferenceCall);
      return;
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(target.name)) {
      return;
    }
  }
  if (target.kind != Expr::Kind::Call || !target.isFieldAccess ||
      target.args.size() != 1) {
    return;
  }

  Expr &receiver = target.args.front();
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return;
  }

  static constexpr std::string_view getPrefix =
      "/std/collections/experimental_soa_vector/soaVectorGet";
  static constexpr std::string_view refPrefix =
      "/std/collections/experimental_soa_vector/soaVectorRef";
  static constexpr std::string_view fieldReadPrefix =
      "/std/collections/internal_soa_storage/soaFieldViewRead";
  static constexpr std::string_view fieldRefPrefix =
      "/std/collections/internal_soa_storage/soaFieldViewRef";
  if (!semantics::isExperimentalSoaGetLikeHelperPath(receiver.name)) {
    if (!semantics::isExperimentalSoaFieldViewReadHelperPath(receiver.name)) {
      return;
    }
    receiver.name.replace(0, fieldReadPrefix.size(), fieldRefPrefix);
    return;
  }
  receiver.name.replace(0, getPrefix.size(), refPrefix);
}

bool rewriteExperimentalSoaFieldViewAssignTargets(Program &program,
                                                  std::string &error) {
  error.clear();
  for (Definition &def : program.definitions) {
    for (Expr &stmt : def.statements) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(stmt);
    }
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(*def.returnExpr);
    }
  }
  for (auto &exec : program.executions) {
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
  }
  return true;
}

void rewriteBorrowedExperimentalMapMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteBorrowedExperimentalMapMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBorrowedExperimentalMapMethodExpr(
        stmt, bindings, borrowedReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBorrowedExperimentalMapMethodStatements(
          stmt.bodyArguments, bodyBindings, borrowedReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractBorrowedExperimentalMapBinding(stmt); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteBorrowedExperimentalMapMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBorrowedExperimentalMapMethodExpr(
        arg, bindings, borrowedReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = borrowedExperimentalMapHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isBorrowedExperimentalMapBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = borrowedReturnDefinitions.find(candidatePath);
      if (returnIt != borrowedReturnDefinitions.end() &&
          isBorrowedExperimentalMapBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }
  std::string keyType;
  std::string valueType;
  if (expr.templateArgs.empty() &&
      !semantics::extractMapKeyValueTypesFromTypeText(
          receiverBinding->typeTemplateArg, keyType, valueType)) {
    return;
  }
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = helperName;
  expr.namespacePrefix.clear();
  if (expr.templateArgs.empty()) {
    expr.templateArgs = {keyType, valueType};
  }
  expr.argNames.clear();
}

bool rewriteBorrowedExperimentalMapMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> borrowedReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBorrowedExperimentalMapReturnBinding(def); binding.has_value()) {
      borrowedReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        borrowedReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractBorrowedExperimentalMapBinding(param); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBorrowedExperimentalMapMethodStatements(
        def.statements, bindings, borrowedReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteBorrowedExperimentalMapMethodExpr(
          *def.returnExpr, bindings, borrowedReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalMapValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteExperimentalMapValueMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalMapValueMethodExpr(stmt, bindings, valueReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalMapValueMethodStatements(
          stmt.bodyArguments, bodyBindings, valueReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractExperimentalMapBinding(stmt); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalMapValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalMapValueMethodExpr(arg, bindings, valueReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = experimentalMapValueHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isExperimentalMapValueBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = valueReturnDefinitions.find(candidatePath);
      if (returnIt != valueReturnDefinitions.end() &&
          isExperimentalMapValueBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }
  std::string keyType;
  std::string valueType;
  if (expr.templateArgs.empty() &&
      !semantics::extractMapKeyValueTypesFromTypeText(bindingTypeText(*receiverBinding), keyType, valueType)) {
    return;
  }
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = helperName;
  expr.namespacePrefix.clear();
  if (expr.templateArgs.empty()) {
    expr.templateArgs = {keyType, valueType};
  }
  expr.argNames.clear();
}

bool rewriteExperimentalMapValueMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> valueReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalMapReturnBinding(def); binding.has_value()) {
      valueReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        valueReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractExperimentalMapBinding(param); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalMapValueMethodStatements(
        def.statements, bindings, valueReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalMapValueMethodExpr(
          *def.returnExpr, bindings, valueReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

bool isBuiltinMapMutationBinding(const semantics::BindingInfo &binding) {
  if (isExperimentalMapTypeText(bindingTypeText(binding))) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  return semantics::extractMapKeyValueTypesFromTypeText(
      bindingTypeText(binding), keyType, valueType);
}

bool isBuiltinMapReferenceBinding(const semantics::BindingInfo &binding) {
  if (!isBuiltinMapMutationBinding(binding)) {
    return false;
  }
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  return normalizedType == "Reference" || normalizedType == "Pointer";
}

semantics::BindingInfo bindingInfoFromTypeText(const std::string &typeText) {
  semantics::BindingInfo binding;
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalizedType, base, argText)) {
    binding.typeName = semantics::normalizeBindingTypeName(base);
    binding.typeTemplateArg = argText;
  } else {
    binding.typeName = normalizedType;
  }
  return binding;
}

std::optional<semantics::BindingInfo> extractDefinitionReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    return bindingInfoFromTypeText(transform.templateArgs.front());
  }
  return std::nullopt;
}

constexpr std::string_view kBuiltinCanonicalMapInsertPath = "/std/collections/map/insert";
constexpr std::string_view kBuiltinCanonicalMapInsertRefPath = "/std/collections/map/insert_ref";
constexpr std::string_view kBuiltinCanonicalMapInsertBuiltinPath =
    "/std/collections/map/insert_builtin";
constexpr std::string_view kBuiltinMapInsertAliasPath = "/map/insert";
constexpr std::string_view kBuiltinMapInsertRefAliasPath = "/map/insert_ref";
constexpr std::string_view kBuiltinMapInsertWrapperPath = "/std/collections/mapInsert";
constexpr std::string_view kBuiltinMapInsertRefWrapperPath = "/std/collections/mapInsertRef";
constexpr std::string_view kBuiltinExperimentalMapInsertPath =
    "/std/collections/experimental_map/mapInsert";
constexpr std::string_view kBuiltinExperimentalMapInsertRefPath =
    "/std/collections/experimental_map/mapInsertRef";

bool isBuiltinMapInsertValueHelperName(std::string_view name) {
  return name == "insert" || name == kBuiltinCanonicalMapInsertPath ||
         name == kBuiltinMapInsertAliasPath || name == "mapInsert" ||
         name == kBuiltinMapInsertWrapperPath ||
         name == kBuiltinExperimentalMapInsertPath;
}

bool isBuiltinMapInsertReferenceHelperName(std::string_view name) {
  return name == "insert_ref" || name == kBuiltinCanonicalMapInsertRefPath ||
         name == kBuiltinMapInsertRefAliasPath || name == "mapInsertRef" ||
         name == kBuiltinMapInsertRefWrapperPath ||
         name == kBuiltinExperimentalMapInsertRefPath;
}

bool isBuiltinMapInsertHelperName(std::string_view name) {
  return isBuiltinMapInsertValueHelperName(name) ||
         isBuiltinMapInsertReferenceHelperName(name);
}

std::optional<semantics::BindingInfo> resolveBuiltinMapInsertReceiverBinding(
    const Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  if (expr.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(expr.name);
    if (bindingIt == bindings.end()) {
      return std::nullopt;
    }
    return bindingIt->second;
  }

  if (expr.isFieldAccess && expr.args.size() == 1) {
    auto receiverBinding = resolveBuiltinMapInsertReceiverBinding(
        expr.args.front(), bindings, definitionMap, structPaths, definitionNamespace);
    if (!receiverBinding.has_value()) {
      return std::nullopt;
    }
    const std::string receiverNamespace =
        !expr.args.front().namespacePrefix.empty() ? expr.args.front().namespacePrefix : definitionNamespace;
    const std::string receiverStructPath = resolveStructReceiverPathFromBinding(
        *receiverBinding, receiverNamespace, structPaths);
    if (receiverStructPath.empty()) {
      return std::nullopt;
    }
    auto defIt = definitionMap.find(receiverStructPath);
    if (defIt == definitionMap.end() || defIt->second == nullptr) {
      return std::nullopt;
    }
    for (const Expr &fieldExpr : defIt->second->statements) {
      if (!fieldExpr.isBinding || fieldExpr.name != expr.name) {
        continue;
      }
      return extractParsedBindingInfo(fieldExpr, &structPaths);
    }
    return std::nullopt;
  }

  if (semantics::isSimpleCallName(expr, "location") && expr.args.size() == 1) {
    auto pointeeBinding = resolveBuiltinMapInsertReceiverBinding(
        expr.args.front(), bindings, definitionMap, structPaths, definitionNamespace);
    if (!pointeeBinding.has_value()) {
      return std::nullopt;
    }
    semantics::BindingInfo binding;
    binding.typeName = "Reference";
    binding.typeTemplateArg = bindingTypeText(*pointeeBinding);
    return binding;
  }

  if (semantics::isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
    auto borrowedBinding = resolveBuiltinMapInsertReceiverBinding(
        expr.args.front(), bindings, definitionMap, structPaths, definitionNamespace);
    if (!borrowedBinding.has_value()) {
      return std::nullopt;
    }
    const std::string normalizedType = semantics::normalizeBindingTypeName(borrowedBinding->typeName);
    if ((normalizedType != "Reference" && normalizedType != "Pointer") ||
        borrowedBinding->typeTemplateArg.empty()) {
      return std::nullopt;
    }
    return bindingInfoFromTypeText(borrowedBinding->typeTemplateArg);
  }

  if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
    for (const std::string &candidatePath :
         candidateDefinitionPaths(expr, definitionNamespace)) {
      auto defIt = definitionMap.find(candidatePath);
      if (defIt == definitionMap.end() || defIt->second == nullptr) {
        continue;
      }
      if (auto returnBinding = extractDefinitionReturnBinding(*defIt->second);
          returnBinding.has_value()) {
        return *returnBinding;
      }
    }
  }

  return std::nullopt;
}

void rewriteBuiltinMapInsertExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBuiltinMapInsertExpr(arg, bindings, definitionMap, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return;
  }

  const bool matchesBuiltinInsertMethod =
      expr.isMethodCall && isBuiltinMapInsertHelperName(expr.name);
  const bool matchesBuiltinInsertCall =
      !expr.isMethodCall && isBuiltinMapInsertHelperName(expr.name);
  if (!matchesBuiltinInsertMethod && !matchesBuiltinInsertCall) {
    return;
  }

  const Expr &receiver = expr.args.front();
  auto receiverBinding = resolveBuiltinMapInsertReceiverBinding(
      receiver, bindings, definitionMap, structPaths, definitionNamespace);
  if (!receiverBinding.has_value() || !isBuiltinMapMutationBinding(*receiverBinding)) {
    return;
  }
  const bool receiverIsReference = isBuiltinMapReferenceBinding(*receiverBinding);
  const bool expectsReferenceSurface =
      !expr.isMethodCall && isBuiltinMapInsertReferenceHelperName(expr.name);
  const bool expectsValueSurface =
      !expr.isMethodCall && isBuiltinMapInsertValueHelperName(expr.name);
  if ((expectsReferenceSurface && !receiverIsReference) ||
      (expectsValueSurface && receiverIsReference)) {
    return;
  }
  std::string keyType;
  std::string valueType;
  if (expr.templateArgs.empty() &&
      !semantics::extractMapKeyValueTypesFromTypeText(
          bindingTypeText(*receiverBinding), keyType, valueType)) {
    return;
  }
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = std::string(kBuiltinCanonicalMapInsertBuiltinPath);
  expr.namespacePrefix.clear();
  if (expr.templateArgs.empty()) {
    expr.templateArgs = {keyType, valueType};
  }
  expr.argNames.clear();
}

void rewriteBuiltinMapInsertStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBuiltinMapInsertExpr(stmt, bindings, definitionMap, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinMapInsertStatements(
          stmt.bodyArguments, bodyBindings, definitionMap, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

bool rewriteBuiltinMapInsertMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, const Definition *> definitionMap;
  std::unordered_set<std::string> structPaths;
  for (const Definition &def : program.definitions) {
    definitionMap[def.fullPath] = &def;
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinMapInsertStatements(
        def.statements, bindings, definitionMap, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteBuiltinMapInsertExpr(
          *def.returnExpr, returnBindings, definitionMap, structPaths, definitionNamespace);
    }
  }
  return true;
}

bool rewriteOmittedStructInitializers(Program &program, std::string &error) {
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
      fieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          fieldOnlyStruct = false;
          break;
        }
      }
    }
    if (hasStructTransform || fieldOnlyStruct) {
      structNames.insert(def.fullPath);
    }
  }

  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool sawPublic = false;
    bool sawPrivate = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "public") {
        sawPublic = true;
      } else if (transform.name == "private") {
        sawPrivate = true;
      }
    }
    if (sawPublic && !sawPrivate) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  auto isWildcardImport = [](const std::string &path, std::string &prefixOut) -> bool {
    if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
      prefixOut = path.substr(0, path.size() - 2);
      return true;
    }
    if (path.find('/', 1) == std::string::npos) {
      prefixOut = path;
      return true;
    }
    return false;
  };
  for (const auto &importPath : program.imports) {
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      std::string scopedPrefix = prefix;
      if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
        scopedPrefix += "/";
      }
      for (const auto &def : program.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions.count(def.fullPath) == 0) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (publicDefinitions.count(importPath) == 0) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }

  auto rewriteExpr = [&](auto &self, Expr &expr) -> bool {
    if (expr.isLambda) {
      for (auto &arg : expr.bodyArguments) {
        if (!self(self, arg)) {
          return false;
        }
      }
      return true;
    }
    for (auto &arg : expr.args) {
      if (!self(self, arg)) {
        return false;
      }
    }
    for (auto &arg : expr.bodyArguments) {
      if (!self(self, arg)) {
        return false;
      }
    }
    auto isEmptyBuiltinBlockInitializer = [&](const Expr &initializer) -> bool {
      if (!initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
        return false;
      }
      if (!initializer.args.empty() || !initializer.templateArgs.empty() ||
          semantics::hasNamedArguments(initializer.argNames)) {
        return false;
      }
      return initializer.kind == Expr::Kind::Call && !initializer.isBinding &&
             initializer.name == "block" && initializer.namespacePrefix.empty();
    };
    const bool omittedInitializer = expr.args.empty() ||
                                    (expr.args.size() == 1 && isEmptyBuiltinBlockInitializer(expr.args.front()));
    if (!omittedInitializer) {
      return true;
    }
    if (!expr.isBinding && expr.transforms.empty()) {
      return true;
    }
    semantics::BindingInfo info;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!semantics::parseBindingInfo(expr, expr.namespacePrefix, structNames, importAliases, info, restrictType, parseError)) {
      error = parseError;
      return false;
    }
    const std::string normalizedType = semantics::normalizeBindingTypeName(info.typeName);
    if (!info.typeTemplateArg.empty()) {
      if (normalizedType != "vector" && normalizedType != "soa_vector") {
        error = "omitted initializer requires vector or soa_vector type: " + info.typeName;
        return false;
      }
      std::vector<std::string> templateArgs;
      if (!semantics::splitTopLevelTemplateArgs(info.typeTemplateArg, templateArgs) || templateArgs.size() != 1) {
        error = normalizedType + " requires exactly one template argument";
        return false;
      }
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = info.typeName;
      call.namespacePrefix = expr.namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      expr.args.clear();
      expr.argNames.clear();
      expr.args.push_back(std::move(call));
      expr.argNames.push_back(std::nullopt);
      return true;
    }
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = info.typeName;
    call.namespacePrefix = expr.namespacePrefix;
    expr.args.clear();
    expr.argNames.clear();
    expr.args.push_back(std::move(call));
    expr.argNames.push_back(std::nullopt);
    return true;
  };

  for (auto &def : program.definitions) {
    for (auto &stmt : def.statements) {
      if (!rewriteExpr(rewriteExpr, stmt)) {
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      if (!rewriteExpr(rewriteExpr, *def.returnExpr)) {
        return false;
      }
    }
  }
  for (auto &exec : program.executions) {
    for (auto &arg : exec.arguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
    for (auto &arg : exec.bodyArguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
  }
  return true;
}
} // namespace

namespace {

bool runSemanticValidation(Program &program,
                           const std::string &entryPath,
                           std::string &error,
                           const std::vector<std::string> &defaultEffects,
                           const std::vector<std::string> &entryDefaultEffects,
                           const std::vector<std::string> &semanticTransforms,
                           SemanticDiagnosticInfo *diagnosticInfo,
                           bool collectDiagnostics,
                           SemanticProgram *semanticProgramOut,
                           const SemanticProductBuildConfig *semanticProductBuildConfig,
                           const SemanticValidationBenchmarkConfig *benchmarkConfig,
                           const SemanticValidationBenchmarkObserver *benchmarkObserver) {
  const uint32_t benchmarkSemanticDefinitionValidationWorkerCount =
      benchmarkConfig != nullptr ? benchmarkConfig->definitionValidationWorkerCount : 1;
  SemanticPhaseCounters *benchmarkSemanticPhaseCounters =
      benchmarkObserver != nullptr ? benchmarkObserver->phaseCounters : nullptr;
  const bool benchmarkSemanticAllocationCountersEnabled =
      benchmarkObserver != nullptr && benchmarkObserver->allocationCountersEnabled;
  const bool benchmarkSemanticRssCheckpointsEnabled =
      benchmarkObserver != nullptr && benchmarkObserver->rssCheckpointsEnabled;
  const bool benchmarkSemanticDisableMethodTargetMemoization =
      benchmarkConfig != nullptr && benchmarkConfig->disableMethodTargetMemoization;
  const bool benchmarkSemanticGraphLocalAutoLegacyKeyShadow =
      benchmarkConfig != nullptr && benchmarkConfig->graphLocalAutoLegacyKeyShadow;
  const bool benchmarkSemanticGraphLocalAutoLegacySideChannelShadow =
      benchmarkConfig != nullptr && benchmarkConfig->graphLocalAutoLegacySideChannelShadow;
  const bool benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr =
      benchmarkConfig != nullptr && benchmarkConfig->disableGraphLocalAutoDependencyScratchPmr;

  error.clear();
  if (benchmarkSemanticPhaseCounters != nullptr) {
    *benchmarkSemanticPhaseCounters = {};
  }
  DiagnosticSink diagnosticSink(diagnosticInfo);
  diagnosticSink.reset();
  bool validationSucceeded = false;
  struct ValidationDiagnosticScope {
    DiagnosticSink &diagnosticSink;
    std::string &error;
    bool &validationSucceeded;

    ~ValidationDiagnosticScope() {
      if (!validationSucceeded && !error.empty()) {
        diagnosticSink.setSummary(error);
      }
    }
  } validationDiagnosticScope{diagnosticSink, error, validationSucceeded};
  struct ScopedBenchmarkAllocatorReliefDisable {
    explicit ScopedBenchmarkAllocatorReliefDisable(bool enabled)
        : previous(gDisableSemanticAllocatorReliefForBenchmark) {
      if (enabled) {
        gDisableSemanticAllocatorReliefForBenchmark = true;
      }
    }
    ~ScopedBenchmarkAllocatorReliefDisable() {
      gDisableSemanticAllocatorReliefForBenchmark = previous;
    }
    bool previous = false;
  };
  const ScopedBenchmarkAllocatorReliefDisable scopedBenchmarkAllocatorReliefDisable(
      benchmarkSemanticAllocationCountersEnabled || benchmarkSemanticRssCheckpointsEnabled);
  if (!semantics::applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  if (!semantics::rewriteExperimentalGfxConstructors(program, error)) {
    return false;
  }
  if (!semantics::rewriteReflectionGeneratedHelpers(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaConversionMethods(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaToAosCalls(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaAccessCalls(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaCountCalls(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaMutatorCalls(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaInlineBorrowMethods(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaSamePathHelperMethods(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaToAosMethods(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaFieldViewIndexes(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaFieldViewHelpers(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaFieldViewCarrierIndexes(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaFieldViewAssignTargets(program, error)) {
    return false;
  }
  if (!rewriteBorrowedExperimentalMapMethods(program, error)) {
    return false;
  }
  if (!rewriteExperimentalMapValueMethods(program, error)) {
    return false;
  }
  if (!rewriteBuiltinMapInsertMethods(program, error)) {
    return false;
  }
  try {
    if (!semantics::monomorphizeTemplates(program, entryPath, error)) {
      return false;
    }
  } catch (const std::exception &ex) {
    error = std::string("template monomorphization exception: ") + ex.what();
    return false;
  }
  if (!semantics::rewriteReflectionMetadataQueries(program, error)) {
    return false;
  }
  if (!semantics::rewriteConvertConstructors(program, error)) {
    return false;
  }
  const bool benchmarkValidatorLifetime =
      std::getenv("PRIMEC_BENCHMARK_SEMANTIC_VALIDATOR_LIFETIME") != nullptr;
  ProcessAllocationSample validatorLifetimeAllocationBefore;
  ProcessAllocationSample validatorLifetimeAllocationAfterRun;
  ProcessAllocationSample validatorLifetimeAllocationAfterDestroy;
  ProcessRssSample validatorLifetimeRssBefore;
  ProcessRssSample validatorLifetimeRssAfterRun;
  ProcessRssSample validatorLifetimeRssAfterDestroy;
  if (benchmarkValidatorLifetime) {
    validatorLifetimeAllocationBefore = captureProcessAllocationSample();
    validatorLifetimeRssBefore = captureProcessRssSample();
  }

  ProcessAllocationSample validationAllocationBefore;
  ProcessAllocationSample validationAllocationAfter;
  ProcessRssSample validationRssBefore;
  ProcessRssSample validationRssAfter;
  semantics::SemanticsValidator::ValidationCounters validationCounters;
  bool hasValidationAllocationAfter = false;
  bool hasValidationRssAfter = false;
  if (benchmarkSemanticPhaseCounters != nullptr && benchmarkSemanticAllocationCountersEnabled) {
    validationAllocationBefore = captureProcessAllocationSample();
  }
  if (benchmarkSemanticPhaseCounters != nullptr && benchmarkSemanticRssCheckpointsEnabled) {
    validationRssBefore = captureProcessRssSample();
  }
  {
    semantics::SemanticsValidator validator(
        program,
        entryPath,
        error,
        defaultEffects,
        entryDefaultEffects,
        diagnosticInfo,
        collectDiagnostics,
        benchmarkSemanticDefinitionValidationWorkerCount,
        benchmarkSemanticPhaseCounters != nullptr,
        benchmarkSemanticDisableMethodTargetMemoization,
        benchmarkSemanticGraphLocalAutoLegacyKeyShadow,
        benchmarkSemanticGraphLocalAutoLegacySideChannelShadow,
        benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr);
    try {
      if (!validator.run()) {
        return false;
      }
    } catch (const std::exception &ex) {
      error = std::string("semantic validator exception: ") + ex.what();
      return false;
    }
    validationCounters = validator.validationCounters();
    if (!rewriteOmittedStructInitializers(program, error)) {
      return false;
    }
    semantics::assignSemanticNodeIds(program);
    validator.invalidatePilotRoutingSemanticCollectors();
    maybeRelieveSemanticAllocatorPressure();
    if (benchmarkSemanticPhaseCounters != nullptr && benchmarkSemanticAllocationCountersEnabled) {
      validationAllocationAfter = captureProcessAllocationSample();
      hasValidationAllocationAfter = true;
    }
    if (benchmarkSemanticPhaseCounters != nullptr && benchmarkSemanticRssCheckpointsEnabled) {
      validationRssAfter = captureProcessRssSample();
      hasValidationRssAfter = true;
    }
    if (benchmarkValidatorLifetime) {
      validatorLifetimeAllocationAfterRun = captureProcessAllocationSample();
      validatorLifetimeRssAfterRun = captureProcessRssSample();
    }
    if (semanticProgramOut != nullptr) {
      ProcessAllocationSample semanticProductAllocationBefore;
      ProcessRssSample semanticProductRssBefore;
      if (benchmarkSemanticPhaseCounters != nullptr && benchmarkSemanticAllocationCountersEnabled) {
        semanticProductAllocationBefore = captureProcessAllocationSample();
      }
      if (benchmarkSemanticPhaseCounters != nullptr && benchmarkSemanticRssCheckpointsEnabled) {
        semanticProductRssBefore = captureProcessRssSample();
      }
      *semanticProgramOut = buildSemanticProgram(program, entryPath, validator, semanticProductBuildConfig);
      if (benchmarkSemanticPhaseCounters != nullptr) {
        benchmarkSemanticPhaseCounters->semanticProductBuild.callsVisited = 1;
        benchmarkSemanticPhaseCounters->semanticProductBuild.factsProduced =
            semanticProgramFactCount(*semanticProgramOut);
        if (benchmarkSemanticAllocationCountersEnabled) {
          const ProcessAllocationSample semanticProductAllocationAfter = captureProcessAllocationSample();
          populateAllocationDelta(benchmarkSemanticPhaseCounters->semanticProductBuild,
                                  semanticProductAllocationBefore,
                                  semanticProductAllocationAfter);
        }
        if (benchmarkSemanticRssCheckpointsEnabled) {
          const ProcessRssSample semanticProductRssAfter = captureProcessRssSample();
          populateRssCheckpoints(benchmarkSemanticPhaseCounters->semanticProductBuild,
                                 semanticProductRssBefore,
                                 semanticProductRssAfter);
        }
      }
    }
  }

  if (benchmarkValidatorLifetime) {
    validatorLifetimeAllocationAfterDestroy = captureProcessAllocationSample();
    validatorLifetimeRssAfterDestroy = captureProcessRssSample();
    std::cerr << "[benchmark-semantic-validator-lifetime] "
              << "{\"schema\":\"primestruct_semantic_validator_lifetime_v1\""
              << ",\"allocation_before_bytes\":" << validatorLifetimeAllocationBefore.allocatedBytes
              << ",\"allocation_after_run_bytes\":" << validatorLifetimeAllocationAfterRun.allocatedBytes
              << ",\"allocation_after_destroy_bytes\":" << validatorLifetimeAllocationAfterDestroy.allocatedBytes
              << ",\"rss_before_bytes\":" << validatorLifetimeRssBefore.residentBytes
              << ",\"rss_after_run_bytes\":" << validatorLifetimeRssAfterRun.residentBytes
              << ",\"rss_after_destroy_bytes\":" << validatorLifetimeRssAfterDestroy.residentBytes
              << "}" << std::endl;
  }

  if (benchmarkSemanticPhaseCounters != nullptr) {
    benchmarkSemanticPhaseCounters->validation.callsVisited =
        validationCounters.callsVisited;
    benchmarkSemanticPhaseCounters->validation.peakLocalMapSize =
        validationCounters.peakLocalMapSize;
    if (benchmarkSemanticAllocationCountersEnabled && hasValidationAllocationAfter) {
      populateAllocationDelta(benchmarkSemanticPhaseCounters->validation,
                              validationAllocationBefore,
                              validationAllocationAfter);
    }
    if (benchmarkSemanticRssCheckpointsEnabled && hasValidationRssAfter) {
      populateRssCheckpoints(
          benchmarkSemanticPhaseCounters->validation, validationRssBefore, validationRssAfter);
    }
  }
  error.clear();
  validationSucceeded = true;
  return true;
}

} // namespace

bool Semantics::validate(Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects,
                         const std::vector<std::string> &entryDefaultEffects,
                         const std::vector<std::string> &semanticTransforms,
                         SemanticDiagnosticInfo *diagnosticInfo,
                         bool collectDiagnostics,
                         SemanticProgram *semanticProgramOut,
                         const SemanticProductBuildConfig *semanticProductBuildConfig) const {
  return runSemanticValidation(program,
                               entryPath,
                               error,
                               defaultEffects,
                               entryDefaultEffects,
                               semanticTransforms,
                               diagnosticInfo,
                               collectDiagnostics,
                               semanticProgramOut,
                               semanticProductBuildConfig,
                               nullptr,
                               nullptr);
}

bool Semantics::validateForBenchmark(
    Program &program,
    const std::string &entryPath,
    std::string &error,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::vector<std::string> &semanticTransforms,
    SemanticDiagnosticInfo *diagnosticInfo,
    bool collectDiagnostics,
    SemanticProgram *semanticProgramOut,
    const SemanticProductBuildConfig *semanticProductBuildConfig,
    const SemanticValidationBenchmarkConfig &benchmarkConfig,
    const SemanticValidationBenchmarkObserver &benchmarkObserver) const {
  return runSemanticValidation(program,
                               entryPath,
                               error,
                               defaultEffects,
                               entryDefaultEffects,
                               semanticTransforms,
                               diagnosticInfo,
                               collectDiagnostics,
                               semanticProgramOut,
                               semanticProductBuildConfig,
                               &benchmarkConfig,
                               &benchmarkObserver);
}

bool semantics::computeTypeResolutionReturnSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionReturnSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.returnResolutionSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionReturnSnapshotEntry{
          entry.definitionPath,
          returnKindSnapshotName(entry.kind),
          entry.structPath,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

std::string semantics::runSemanticsReturnKindNameStep(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &error) {
  return returnKindSnapshotName(getReturnKind(def, structNames, importAliases, error));
}

bool semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionLocalBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.localAutoBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionLocalBindingSnapshotEntry{
          entry.scopePath,
          entry.bindingName,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
          entry.initializerResolvedPath,
          bindingTypeTextForSnapshot(entry.initializerBinding),
          bindingTypeTextForSnapshot(entry.initializerReceiverBinding),
          entry.initializerQueryTypeText,
          entry.initializerResultHasValue,
          entry.initializerResultValueType,
          entry.initializerResultErrorType,
          entry.initializerHasTry,
          entry.initializerTryOperandResolvedPath,
          bindingTypeTextForSnapshot(entry.initializerTryOperandBinding),
          bindingTypeTextForSnapshot(entry.initializerTryOperandReceiverBinding),
          entry.initializerTryOperandQueryTypeText,
          entry.initializerTryValueType,
          entry.initializerTryErrorType,
          entry.initializerHasTry ? returnKindSnapshotName(entry.initializerTryContextReturnKind) : std::string{},
          entry.initializerTryOnErrorHandlerPath,
          entry.initializerTryOnErrorErrorType,
          entry.initializerTryOnErrorBoundArgCount,
          entry.initializerDirectCallResolvedPath,
          entry.initializerDirectCallReturnKind != ReturnKind::Unknown
              ? returnKindSnapshotName(entry.initializerDirectCallReturnKind)
              : std::string{},
          entry.initializerMethodCallResolvedPath,
          entry.initializerMethodCallReturnKind != ReturnKind::Unknown
              ? returnKindSnapshotName(entry.initializerMethodCallReturnKind)
              : std::string{},
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryCallSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryCallSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.typeText.empty()) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryCallSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          entry.typeText,
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.binding.typeName.empty()) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryResultTypeSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (!entry.hasResultType) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryResultTypeSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          entry.resultTypeHasValue,
          entry.resultValueType,
          entry.resultErrorType,
      });
    }
  });
}

bool semantics::computeTypeResolutionTryValueSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionTryValueSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.tryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionTryValueSnapshotEntry{
          entry.scopePath,
          entry.operandResolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.operandBinding),
          bindingTypeTextForSnapshot(entry.operandReceiverBinding),
          entry.operandQueryTypeText,
          entry.valueType,
          entry.errorType,
          returnKindSnapshotName(entry.contextReturnKind),
          entry.onErrorHandlerPath,
          entry.onErrorErrorType,
          entry.onErrorBoundArgCount,
      });
    }
  });
}

bool semantics::computeTypeResolutionCallBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionCallBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.callBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionCallBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryReceiverBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryReceiverBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.receiverBinding.typeName.empty()) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryReceiverBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.receiverBinding),
      });
    }
  });
}

bool semantics::computeTypeResolutionOnErrorSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionOnErrorSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.onErrorFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionOnErrorSnapshotEntry{
          entry.definitionPath,
          returnKindSnapshotName(entry.returnKind),
          entry.handlerPath,
          entry.errorType,
          entry.boundArgCount,
          entry.returnResultHasValue,
          entry.returnResultValueType,
          entry.returnResultErrorType,
      });
    }
  });
}

bool semantics::computeTypeResolutionValidationContextSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionValidationContextSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.takeCollectedCallableSummariesForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.isExecution) {
        continue;
      }
      out.entries.push_back(TypeResolutionValidationContextSnapshotEntry{
          entry.fullPath,
          returnKindSnapshotName(entry.returnKind),
          entry.isCompute,
          entry.isUnsafe,
          entry.activeEffects,
          entry.hasResultType,
          entry.resultTypeHasValue,
          entry.resultValueType,
          entry.resultErrorType,
          entry.hasOnError,
          entry.onErrorHandlerPath,
          entry.onErrorErrorType,
          entry.onErrorBoundArgCount,
      });
    }
  });
}

} // namespace primec
