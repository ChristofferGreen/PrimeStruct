#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include "primec/Lexer.h"
#include "primec/Parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <utility>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#endif

namespace primec::semantics {

namespace {

bool isSlashlessMapHelperName(std::string_view name) {
  if (!name.empty() && name.front() == '/') {
    name.remove_prefix(1);
  }
  return name.rfind("map/", 0) == 0 || name.rfind("std/collections/map/", 0) == 0;
}

uint64_t captureCurrentResidentBytes() {
#if defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(),
                MACH_TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&info),
                &count) == KERN_SUCCESS) {
    return static_cast<uint64_t>(info.resident_size);
  }
  return 0;
#elif defined(__linux__)
  std::ifstream statm("/proc/self/statm");
  uint64_t pages = 0;
  uint64_t residentPages = 0;
  if (!(statm >> pages >> residentPages)) {
    return 0;
  }
  const long pageSize = sysconf(_SC_PAGESIZE);
  if (pageSize <= 0) {
    return 0;
  }
  return residentPages * static_cast<uint64_t>(pageSize);
#else
  return 0;
#endif
}

uint64_t captureCurrentAllocatedBytes() {
#if defined(__APPLE__)
  malloc_statistics_t stats{};
  malloc_zone_statistics(nullptr, &stats);
  return static_cast<uint64_t>(stats.size_in_use);
#elif defined(__linux__) && defined(__GLIBC__)
  const struct mallinfo2 info = mallinfo2();
  return info.uordblks > 0 ? static_cast<uint64_t>(info.uordblks) : 0;
#else
  return 0;
#endif
}

} // namespace

SemanticsValidator::SemanticsValidator(const Program &program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       const std::vector<std::string> &defaultEffects,
                                       const std::vector<std::string> &entryDefaultEffects,
                                       SemanticDiagnosticInfo *diagnosticInfo,
                                       bool collectDiagnostics,
                                       uint32_t benchmarkSemanticDefinitionValidationWorkerCount,
                                       bool benchmarkSemanticPhaseCountersEnabled,
                                       bool benchmarkSemanticDisableMethodTargetMemoization,
                                       bool benchmarkSemanticGraphLocalAutoLegacyKeyShadow,
                                       bool benchmarkSemanticGraphLocalAutoLegacySideChannelShadow,
                                       bool benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr)
    : program_(program),
      entryPath_(entryPath),
      error_(error),
      defaultEffects_(defaultEffects),
      entryDefaultEffects_(entryDefaultEffects),
      diagnosticInfo_(diagnosticInfo),
      diagnosticSink_(diagnosticInfo),
      collectDiagnostics_(collectDiagnostics),
      benchmarkSemanticDefinitionValidationWorkerCount_(
          benchmarkSemanticDefinitionValidationWorkerCount),
      benchmarkSemanticPhaseCountersEnabled_(benchmarkSemanticPhaseCountersEnabled),
      methodTargetMemoizationEnabled_(!benchmarkSemanticDisableMethodTargetMemoization),
      benchmarkGraphLocalAutoLegacyKeyShadowEnabled_(
          benchmarkSemanticGraphLocalAutoLegacyKeyShadow),
      benchmarkGraphLocalAutoLegacySideChannelShadowEnabled_(
          benchmarkSemanticGraphLocalAutoLegacySideChannelShadow),
      benchmarkGraphLocalAutoDependencyScratchPmrEnabled_(
          !benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr) {
  diagnosticSink_.reset();
  auto registerMathImport = [&](const std::string &importPath) {
    if (importPath == "/std/math/*") {
      mathImportAll_ = true;
      return;
    }
    if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
      std::string name = importPath.substr(10);
      if (name.find('/') == std::string::npos && name != "*") {
        mathImports_.insert(std::move(name));
      }
    }
  };
  for (const auto &importPath : program_.sourceImports) {
    registerMathImport(importPath);
  }
  for (const auto &importPath : program_.imports) {
    registerMathImport(importPath);
  }
  if (std::getenv("PRIMEC_BENCHMARK_DISABLE_STRUCT_RETURN_MEMOIZATION") !=
      nullptr) {
    structReturnMemoizationEnabled_ = false;
  }
}

void SemanticsValidator::observeCallVisited() {
  if (!benchmarkSemanticPhaseCountersEnabled_) {
    return;
  }
  ++validationCounters_.callsVisited;
}

void SemanticsValidator::observeLocalMapSize(std::size_t size) {
  if (!benchmarkSemanticPhaseCountersEnabled_) {
    return;
  }
  validationCounters_.peakLocalMapSize =
      std::max<uint64_t>(validationCounters_.peakLocalMapSize,
                         static_cast<uint64_t>(size));
}

void SemanticsValidator::insertLocalBinding(
    std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &name,
    BindingInfo info) {
  auto [it, inserted] = locals.emplace(name, std::move(info));
  if (!inserted) {
    return;
  }
  bumpLocalBindingMemoRevision(&locals);
  if (activeLocalBindingScopes_.empty()) {
    return;
  }
  for (auto scopeIt = activeLocalBindingScopes_.rbegin();
       scopeIt != activeLocalBindingScopes_.rend();
       ++scopeIt) {
    if (&((*scopeIt)->locals) != &locals) {
      continue;
    }
    (*scopeIt)->insertedNames.push_back(it->first);
    return;
  }
}

uint64_t SemanticsValidator::currentLocalBindingMemoRevision(const void *localsIdentity) const {
  if (localsIdentity == nullptr) {
    return 0;
  }
  const auto it = localBindingMemoRevisionByIdentity_.find(localsIdentity);
  if (it == localBindingMemoRevisionByIdentity_.end()) {
    return 0;
  }
  return it->second;
}

void SemanticsValidator::bumpLocalBindingMemoRevision(const void *localsIdentity) {
  if (localsIdentity == nullptr) {
    return;
  }
  auto [it, inserted] = localBindingMemoRevisionByIdentity_.try_emplace(localsIdentity, 1);
  if (!inserted) {
    ++it->second;
  }
}

std::string SemanticsValidator::formatUnknownCallTarget(const Expr &expr) const {
  if (!isSlashlessMapHelperName(expr.name)) {
    return expr.name;
  }
  const std::string resolved = resolveCalleePath(expr);
  return resolved.empty() ? expr.name : resolved;
}

std::string SemanticsValidator::diagnosticCallTargetPath(const std::string &path) const {
  if (path.empty()) {
    return path;
  }
  if (path.rfind("/std/collections/map/count__t", 0) == 0) {
    return "/std/collections/map/count";
  }
  if (path.rfind("/map/count__t", 0) == 0) {
    return "/map/count";
  }
  const size_t lastSlash = path.find_last_of('/');
  const size_t nameStart = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  const size_t suffix = path.find("__t", nameStart);
  if (suffix == std::string::npos) {
    return path;
  }
  const std::string basePath = path.substr(0, suffix);
  auto it = defMap_.find(basePath);
  if (it == defMap_.end() || it->second == nullptr || it->second->templateArgs.empty()) {
    return path;
  }
  return basePath;
}

void SemanticsValidator::releaseTransientSnapshotCaches() {
  queryFactSnapshotCacheValid_ = false;
  tryValueSnapshotCacheValid_ = false;
  callBindingSnapshotCacheValid_ = false;

  std::vector<QueryFactSnapshotEntry>().swap(queryFactSnapshotCache_);
  std::vector<TryValueSnapshotEntry>().swap(tryValueSnapshotCache_);
  std::vector<CallBindingSnapshotEntry>().swap(callBindingSnapshotCache_);

  callTargetResolutionScratch_.resetArena();

  inferExprReturnKindMemo_.clear();
  inferExprReturnKindMemo_.rehash(0);
  inferStructReturnMemo_.clear();
  inferStructReturnMemo_.rehash(0);
  structFieldReturnKindMemo_.clear();
  structFieldReturnKindMemo_.rehash(0);
  localBindingMemoRevisionByIdentity_.clear();
  localBindingMemoRevisionByIdentity_.rehash(0);
}

bool SemanticsValidator::run() {
  const bool benchmarkDumpValidatorState =
      std::getenv("PRIMEC_BENCHMARK_SEMANTIC_VALIDATOR_STATE") != nullptr;
  auto dumpValidatorState = [&](const char *stageName) {
    if (!benchmarkDumpValidatorState) {
      return;
    }
    size_t parameterCount = 0;
    for (const auto &entry : paramsByDef_) {
      parameterCount += entry.second.size();
    }
    std::cerr << "[benchmark-semantic-validator-state] {\"stage\":\""
              << stageName
              << "\",\"definitions\":" << defMap_.size()
              << ",\"definition_buckets\":" << defMap_.bucket_count()
              << ",\"structs\":" << structNames_.size()
              << ",\"struct_buckets\":" << structNames_.bucket_count()
              << ",\"params_by_def\":" << paramsByDef_.size()
              << ",\"params_by_def_buckets\":" << paramsByDef_.bucket_count()
              << ",\"params_total\":" << parameterCount
              << ",\"return_kinds\":" << returnKinds_.size()
              << ",\"return_kind_buckets\":" << returnKinds_.bucket_count()
              << ",\"return_structs\":" << returnStructs_.size()
              << ",\"return_struct_buckets\":" << returnStructs_.bucket_count()
              << ",\"return_bindings\":" << returnBindings_.size()
              << ",\"return_binding_buckets\":" << returnBindings_.bucket_count()
              << ",\"graph_local_auto_scope_paths\":" << graphLocalAutoScopePathInterner_.size()
              << ",\"graph_local_auto_facts\":" << graphLocalAutoFacts_.size()
              << ",\"graph_local_auto_fact_buckets\":" << graphLocalAutoFacts_.bucket_count()
              << ",\"graph_local_auto_legacy_binding\":" << graphLocalAutoLegacyBindingShadow_.size()
              << ",\"graph_local_auto_legacy_binding_buckets\":"
              << graphLocalAutoLegacyBindingShadow_.bucket_count()
              << ",\"graph_local_auto_legacy_query\":" << graphLocalAutoLegacyQuerySnapshotShadow_.size()
              << ",\"graph_local_auto_legacy_query_buckets\":"
              << graphLocalAutoLegacyQuerySnapshotShadow_.bucket_count()
              << ",\"graph_local_auto_legacy_try\":" << graphLocalAutoLegacyTryValueShadow_.size()
              << ",\"graph_local_auto_legacy_try_buckets\":"
              << graphLocalAutoLegacyTryValueShadow_.bucket_count()
              << ",\"query_fact_snapshot_cache\":" << queryFactSnapshotCache_.size()
              << ",\"try_value_snapshot_cache\":" << tryValueSnapshotCache_.size()
              << ",\"call_binding_snapshot_cache\":" << callBindingSnapshotCache_.size()
              << ",\"pilot_routing_direct_call_collectors\":" << collectedDirectCallTargets_.size()
              << ",\"pilot_routing_method_call_collectors\":" << collectedMethodCallTargets_.size()
              << ",\"pilot_routing_bridge_collectors\":" << collectedBridgePathChoices_.size()
              << ",\"pilot_routing_callable_collectors\":" << collectedCallableSummaries_.size()
              << ",\"on_error_snapshot_cache\":" << onErrorSnapshotCache_.size()
              << ",\"effect_free_def_cache\":" << effectFreeDefCache_.size()
              << ",\"effect_free_def_cache_buckets\":" << effectFreeDefCache_.bucket_count()
              << ",\"effect_free_struct_cache\":" << effectFreeStructCache_.size()
              << ",\"effect_free_struct_cache_buckets\":" << effectFreeStructCache_.bucket_count()
              << ",\"import_aliases\":" << importAliases_.size()
              << ",\"import_alias_buckets\":" << importAliases_.bucket_count()
              << ",\"overload_family_paths\":" << overloadFamilyBasePaths_.size()
              << ",\"overload_family_path_buckets\":" << overloadFamilyBasePaths_.bucket_count()
              << ",\"unique_specialization_paths\":" << uniqueSpecializationPathByBase_.size()
              << ",\"unique_specialization_path_buckets\":"
              << uniqueSpecializationPathByBase_.bucket_count()
              << ",\"ambiguous_specialization_paths\":" << ambiguousSpecializationBasePaths_.size()
              << ",\"ambiguous_specialization_path_buckets\":"
              << ambiguousSpecializationBasePaths_.bucket_count()
              << ",\"default_effects\":" << defaultEffectSet_.size()
              << ",\"default_effect_buckets\":" << defaultEffectSet_.bucket_count()
              << ",\"entry_default_effects\":" << entryDefaultEffectSet_.size()
              << ",\"entry_default_effect_buckets\":" << entryDefaultEffectSet_.bucket_count()
              << ",\"active_effects\":" << currentValidationState_.context.activeEffects.size()
              << ",\"active_effect_buckets\":"
              << currentValidationState_.context.activeEffects.bucket_count()
              << ",\"moved_bindings\":" << currentValidationState_.movedBindings.size()
              << ",\"moved_binding_buckets\":"
              << currentValidationState_.movedBindings.bucket_count()
              << ",\"ended_reference_borrows\":" << currentValidationState_.endedReferenceBorrows.size()
              << ",\"ended_reference_borrow_buckets\":"
              << currentValidationState_.endedReferenceBorrows.bucket_count()
              << ",\"inference_stack\":" << inferenceStack_.size()
              << ",\"return_binding_inference_stack\":" << returnBindingInferenceStack_.size()
              << ",\"query_type_inference_definition_stack\":" << queryTypeInferenceDefinitionStack_.size()
              << ",\"query_type_inference_expr_stack\":" << queryTypeInferenceExprStack_.size()
              << ",\"active_local_scopes\":" << activeLocalBindingScopes_.size()
              << ",\"local_binding_memo_revisions\":" << localBindingMemoRevisionByIdentity_.size()
              << ",\"local_binding_memo_revision_buckets\":"
              << localBindingMemoRevisionByIdentity_.bucket_count()
              << ",\"expr_return_memo\":" << inferExprReturnKindMemo_.size()
              << ",\"expr_return_memo_buckets\":" << inferExprReturnKindMemo_.bucket_count()
              << ",\"struct_return_memo\":" << inferStructReturnMemo_.size()
              << ",\"struct_return_memo_buckets\":" << inferStructReturnMemo_.bucket_count()
              << ",\"call_cache_key_interner\":" << callTargetResolutionScratch_.keyInterner.size()
              << ",\"call_cache_definition_family\":" << callTargetResolutionScratch_.definitionFamilyPathCache.size()
              << ",\"call_cache_overload_family\":" << callTargetResolutionScratch_.overloadFamilyPathCache.size()
              << ",\"call_cache_overload_prefix\":" << callTargetResolutionScratch_.overloadFamilyPrefixCache.size()
              << ",\"call_cache_specialization_prefix\":" << callTargetResolutionScratch_.specializationPrefixCache.size()
              << ",\"call_cache_overload_candidate\":" << callTargetResolutionScratch_.overloadCandidatePathCache.size()
              << ",\"call_cache_rooted\":" << callTargetResolutionScratch_.rootedCallNamePathCache.size()
              << ",\"call_cache_namespace\":" << callTargetResolutionScratch_.normalizedNamespacePrefixCache.size()
              << ",\"call_cache_joined\":" << callTargetResolutionScratch_.joinedCallPathCache.size()
              << ",\"call_cache_normalized_method\":" << callTargetResolutionScratch_.normalizedMethodNameCache.size()
              << ",\"call_cache_explicit_removed\":" << callTargetResolutionScratch_.explicitRemovedMethodPathCache.size()
              << ",\"call_cache_method_memo\":" << callTargetResolutionScratch_.methodTargetMemoCache.size()
              << ",\"call_cache_concrete_candidates\":" << callTargetResolutionScratch_.concreteCallBaseCandidates.size()
              << ",\"call_cache_method_receiver_candidates\":" << callTargetResolutionScratch_.methodReceiverResolutionCandidates.size()
              << ",\"call_cache_receiver_alias\":" << callTargetResolutionScratch_.canonicalReceiverAliasPathCache.size()
              << ",\"allocated_bytes\":" << captureCurrentAllocatedBytes()
              << ",\"rss_bytes\":" << captureCurrentResidentBytes()
              << "}" << std::endl;
  };
  auto runStage = [&](const char *stageName, auto &&fn) {
    try {
      const bool ok = fn();
      if (!ok && error_.empty()) {
        return failUncontextualizedDiagnostic(std::string(stageName) +
                                             " failed without diagnostic");
      }
      return ok;
    } catch (...) {
      return failUncontextualizedDiagnostic(
          std::string("semantic validator exception in ") + stageName);
    }
  };
  if (collectDiagnostics_ &&
      !runStage("collectDuplicateDefinitionDiagnostics",
                [&] { return collectDuplicateDefinitionDiagnostics(); })) {
    return false;
  }
  if (!runStage("buildDefinitionMaps", [&] { return buildDefinitionMaps(); })) {
    return false;
  }
  dumpValidatorState("buildDefinitionMaps");
  if (!runStage("inferUnknownReturnKinds",
                [&] { return inferUnknownReturnKinds(); })) {
    return false;
  }
  dumpValidatorState("inferUnknownReturnKinds");
  if (!runStage("validateTraitConstraints",
                [&] { return validateTraitConstraints(); })) {
    return false;
  }
  dumpValidatorState("validateTraitConstraints");
  if (!runStage("validateStructLayouts",
                [&] { return validateStructLayouts(); })) {
    return false;
  }
  dumpValidatorState("validateStructLayouts");
  if (!runStage("validateDefinitions",
                [&] { return validateDefinitions(); })) {
    return false;
  }
  dumpValidatorState("validateDefinitions");
  if (!runStage("validateExecutions",
                [&] { return validateExecutions(); })) {
    return false;
  }
  dumpValidatorState("validateExecutions");
  const bool entryOk = runStage("validateEntry", [&] { return validateEntry(); });
  if (!entryOk) {
    return false;
  }
  dumpValidatorState("validateEntry");
  collectPilotRoutingSemanticProductFacts();
  dumpValidatorState("collectPilotRoutingSemanticProductFacts");
  return true;
}

bool SemanticsValidator::allowMathBareName(const std::string &name) const {
  (void)name;
  if (name.empty() || name.find('/') != std::string::npos) {
    return false;
  }
  if (!currentValidationState_.context.definitionPath.empty()) {
    if (currentValidationState_.context.definitionPath == "/std/math" ||
        currentValidationState_.context.definitionPath.rfind("/std/math/", 0) == 0) {
      return true;
    }
    if (currentValidationState_.context.definitionPath.rfind("/std/", 0) == 0) {
      for (const auto &importPath : program_.imports) {
        if (importPath == "/std/math/*") {
          return true;
        }
        if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
          const std::string importedName = importPath.substr(10);
          if (!importedName.empty() && importedName.find('/') == std::string::npos) {
            return true;
          }
        }
      }
    }
  }
  return hasAnyMathImport();
}

bool SemanticsValidator::hasAnyMathImport() const {
  return mathImportAll_ || !mathImports_.empty();
}

bool SemanticsValidator::isEntryArgsName(const std::string &name) const {
  if (currentValidationState_.context.definitionPath != entryPath_) {
    return false;
  }
  if (entryArgsName_.empty()) {
    return false;
  }
  return name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgsAccess(const Expr &expr) const {
  if (currentValidationState_.context.definitionPath != entryPath_ || entryArgsName_.empty()) {
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string accessName;
  if (!getBuiltinArrayAccessName(expr, accessName) || expr.args.size() != 2) {
    return false;
  }
  if (expr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  return expr.args.front().name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgStringBinding(const std::unordered_map<std::string, BindingInfo> &locals,
                                                 const Expr &expr) const {
  if (currentValidationState_.context.definitionPath != entryPath_) {
    return false;
  }
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  auto it = locals.find(expr.name);
  return it != locals.end() && it->second.isEntryArgString;
}

bool SemanticsValidator::parseTransformArgumentExpr(const std::string &text,
                                                    const std::string &namespacePrefix,
                                                    Expr &out) {
  auto failParseTransformArgumentExpr = [&](std::string message) -> bool {
    return failUncontextualizedDiagnostic(std::move(message));
  };
  Lexer lexer(text);
  Parser parser(lexer.tokenize());
  std::string parseError;
  if (!parser.parseExpression(out, namespacePrefix, parseError)) {
    return failParseTransformArgumentExpr(
        parseError.empty() ? "invalid transform argument expression" : parseError);
  }
  return true;
}

} // namespace primec::semantics
