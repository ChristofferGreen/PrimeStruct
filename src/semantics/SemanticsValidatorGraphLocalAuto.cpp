#include "SemanticsValidatorGraphLocalAuto.h"

namespace primec::semantics {

void GraphLocalAutoBenchmarkShadow::LegacyShadowState::clear() {
  keyShadow.clear();
  bindingShadow.clear();
  initializerResolvedPathShadow.clear();
  initializerBindingShadow.clear();
  querySnapshotShadow.clear();
  tryValueShadow.clear();
  directCallPathShadow.clear();
  directCallReturnKindShadow.clear();
  methodCallPathShadow.clear();
  methodCallReturnKindShadow.clear();
}

GraphLocalAutoBenchmarkShadow::GraphLocalAutoBenchmarkShadow(
    bool legacyKeyShadowEnabled,
    bool legacySideChannelShadowEnabled)
    : legacyKeyShadowEnabled_(legacyKeyShadowEnabled),
      legacySideChannelShadowEnabled_(legacySideChannelShadowEnabled) {}

void GraphLocalAutoBenchmarkShadow::clear() {
  legacyState_.clear();
}

void GraphLocalAutoBenchmarkShadow::noteLegacyKey(std::string_view scopePath,
                                                  int sourceLine,
                                                  int sourceColumn) {
  if (!legacyKeyShadowEnabled_) {
    return;
  }
  std::string legacyKey;
  legacyKey.reserve(scopePath.size() + 32);
  legacyKey.append(scopePath);
  legacyKey.push_back('#');
  legacyKey.append(std::to_string(sourceLine));
  legacyKey.push_back(':');
  legacyKey.append(std::to_string(sourceColumn));
  legacyState_.keyShadow.insert(std::move(legacyKey));
}

void GraphLocalAutoBenchmarkShadow::rebuildFromFacts(
    const std::unordered_map<GraphLocalAutoKey, GraphLocalAutoFacts,
                             GraphLocalAutoKeyHash> &graphLocalAutoFacts) {
  if (!legacySideChannelShadowEnabled_) {
    return;
  }

  legacyState_.bindingShadow.clear();
  legacyState_.initializerResolvedPathShadow.clear();
  legacyState_.initializerBindingShadow.clear();
  legacyState_.querySnapshotShadow.clear();
  legacyState_.tryValueShadow.clear();
  legacyState_.directCallPathShadow.clear();
  legacyState_.directCallReturnKindShadow.clear();
  legacyState_.methodCallPathShadow.clear();
  legacyState_.methodCallReturnKindShadow.clear();

  legacyState_.bindingShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.initializerResolvedPathShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.initializerBindingShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.querySnapshotShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.tryValueShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.directCallPathShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.directCallReturnKindShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.methodCallPathShadow.reserve(graphLocalAutoFacts.size());
  legacyState_.methodCallReturnKindShadow.reserve(graphLocalAutoFacts.size());

  for (const auto &[bindingKey, fact] : graphLocalAutoFacts) {
    if (fact.hasBinding) {
      legacyState_.bindingShadow.try_emplace(bindingKey, fact.binding);
    }
    if (!fact.initializerResolvedPath.empty()) {
      legacyState_.initializerResolvedPathShadow.try_emplace(
          bindingKey, fact.initializerResolvedPath);
    }
    if (fact.hasInitializerBinding) {
      legacyState_.initializerBindingShadow.try_emplace(bindingKey,
                                                        fact.initializerBinding);
    }
    if (fact.hasQuerySnapshot) {
      legacyState_.querySnapshotShadow.try_emplace(bindingKey,
                                                   fact.querySnapshot);
    }
    if (fact.hasTryValue) {
      legacyState_.tryValueShadow.try_emplace(bindingKey, fact.tryValue);
    }
    if (!fact.directCallResolvedPath.empty()) {
      legacyState_.directCallPathShadow.try_emplace(bindingKey,
                                                    fact.directCallResolvedPath);
    }
    if (fact.hasDirectCallReturnKind) {
      legacyState_.directCallReturnKindShadow.try_emplace(
          bindingKey, fact.directCallReturnKind);
    }
    if (!fact.methodCallResolvedPath.empty()) {
      legacyState_.methodCallPathShadow.try_emplace(bindingKey,
                                                    fact.methodCallResolvedPath);
    }
    if (fact.hasMethodCallReturnKind) {
      legacyState_.methodCallReturnKindShadow.try_emplace(
          bindingKey, fact.methodCallReturnKind);
    }
  }
}

size_t GraphLocalAutoBenchmarkShadow::legacyBindingCount() const {
  return legacyState_.bindingShadow.size();
}

size_t GraphLocalAutoBenchmarkShadow::legacyBindingBucketCount() const {
  return legacyState_.bindingShadow.bucket_count();
}

size_t GraphLocalAutoBenchmarkShadow::legacyQueryCount() const {
  return legacyState_.querySnapshotShadow.size();
}

size_t GraphLocalAutoBenchmarkShadow::legacyQueryBucketCount() const {
  return legacyState_.querySnapshotShadow.bucket_count();
}

size_t GraphLocalAutoBenchmarkShadow::legacyTryCount() const {
  return legacyState_.tryValueShadow.size();
}

size_t GraphLocalAutoBenchmarkShadow::legacyTryBucketCount() const {
  return legacyState_.tryValueShadow.bucket_count();
}

} // namespace primec::semantics
