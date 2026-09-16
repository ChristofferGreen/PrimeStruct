// collection-surface-audit: exempt
#include "primec/support/BuiltinArrayAccessNameClassifier.h"

namespace primec {

namespace {

std::string stripTemplateSpecializationSuffixLocal(std::string value) {
  const size_t suffix = value.find("__t");
  if (suffix != std::string::npos) {
    value.erase(suffix);
  }
  return value;
}

std::string stripGeneratedSuffixLocal(std::string value) {
  const size_t suffix = value.find("__");
  if (suffix != std::string::npos) {
    value.erase(suffix);
  }
  return value;
}

}  // namespace

std::optional<std::string> classifyAccessAliasToken(std::string memberName, AccessAliasSpellingMode mode) {
  memberName = stripGeneratedSuffixLocal(stripTemplateSpecializationSuffixLocal(std::move(memberName)));
  const bool bareAt = memberName == "at" || memberName == "at_ref";
  const bool bareAtUnsafe = memberName == "at_unsafe" || memberName == "at_unsafe_ref";
  const bool concatAt = memberName == "vectorAt";
  const bool concatAtUnsafe = memberName == "vectorAtUnsafe";

  bool matchesAt = false;
  bool matchesAtUnsafe = false;
  switch (mode) {
    case AccessAliasSpellingMode::kBareOnly:
      matchesAt = bareAt;
      matchesAtUnsafe = bareAtUnsafe;
      break;
    case AccessAliasSpellingMode::kConcatenatedOnly:
      matchesAt = concatAt;
      matchesAtUnsafe = concatAtUnsafe;
      break;
    case AccessAliasSpellingMode::kFull:
      matchesAt = bareAt || concatAt;
      matchesAtUnsafe = bareAtUnsafe || concatAtUnsafe;
      break;
  }
  if (matchesAt) {
    return std::string("at");
  }
  if (matchesAtUnsafe) {
    return std::string("at_unsafe");
  }
  return std::nullopt;
}

BuiltinArrayAccessAliasResult matchBuiltinArrayAccessAliasUnderPrefix(
    std::string_view name, std::string_view prefix, std::string_view receiverBase,
    AccessAliasSpellingMode mode, bool rejectOnRawResidualSlash) {
  BuiltinArrayAccessAliasResult result;
  if (name.substr(0, prefix.size()) != prefix) {
    result.outcome = BuiltinArrayAccessAliasOutcome::kNoMatch;
    return result;
  }
  std::string alias(name.substr(prefix.size()));
  if (!receiverBase.empty()) {
    const size_t slash = alias.find('/');
    if (slash != std::string::npos) {
      const std::string receiverPath = alias.substr(0, slash);
      const std::string receiverBaseStr(receiverBase);
      if (receiverPath != receiverBaseStr && receiverPath.rfind(receiverBaseStr + "__", 0) != 0) {
        result.outcome = BuiltinArrayAccessAliasOutcome::kReject;
        return result;
      }
      alias = alias.substr(slash + 1);
    }
  } else if (rejectOnRawResidualSlash) {
    // The common ("legacy") shape both stages' plain-prefix matchers use:
    // reject on any residual '/' in the RAW (pre-strip) alias, before ever
    // attempting suffix stripping - matches
    // semantics' `matchStdlibLegacyAccessAlias` and ir_lowerer's
    // `matchLegacyAccessAlias` exactly. Note this is NOT equivalent to
    // "strip suffixes, then let the literal compare reject a residual
    // slash" for a contrived alias like "at__t9/foo" - stripping first
    // would erase everything from the first "__t" onward (leaving "at",
    // which classifies) where this raw pre-strip check rejects outright.
    // See `classifyBuiltinArrayAccessNameForSemantics`'s `stdVectorRoot`
    // call, the one real call site that passes
    // `rejectOnRawResidualSlash = false` for exactly that reason.
    if (alias.find('/') != std::string::npos) {
      result.outcome = BuiltinArrayAccessAliasOutcome::kReject;
      return result;
    }
  }
  // When `rejectOnRawResidualSlash` is false (semantics' `stdVectorRoot`
  // shape only), no explicit pre-check runs here - `classifyAccessAliasToken`
  // strips suffixes first and its literal compare naturally rejects any
  // alias that still contains '/' after stripping, since none of the
  // recognized spellings contain '/'.
  std::optional<std::string> token = classifyAccessAliasToken(std::move(alias), mode);
  if (token) {
    result.outcome = BuiltinArrayAccessAliasOutcome::kAccept;
    result.token = std::move(*token);
  } else {
    result.outcome = BuiltinArrayAccessAliasOutcome::kReject;
  }
  return result;
}

bool classifyBuiltinArrayAccessNameForSemantics(
    const std::string &name, const std::string &rawName, const std::string &stdCollectionsPrefix,
    const std::string &experimentalVectorPrefix, const std::string &experimentalMapPrefix,
    const std::string &stdVectorRootPrefix,
    const BuiltinArrayAccessKeyValueLookup &resolveKeyValueHelper, std::string &out) {
  out.clear();
  if (rawName.empty()) {
    return false;
  }

  auto tryLegacyRoot = [&](const std::string &prefix) -> bool {
    BuiltinArrayAccessAliasResult result =
        matchBuiltinArrayAccessAliasUnderPrefix(name, prefix, {}, AccessAliasSpellingMode::kFull);
    if (result.outcome == BuiltinArrayAccessAliasOutcome::kAccept) {
      out = result.token;
      return true;
    }
    return false;
  };
  if (tryLegacyRoot(stdCollectionsPrefix) || tryLegacyRoot(experimentalVectorPrefix) ||
      tryLegacyRoot(experimentalMapPrefix)) {
    return true;
  }

  const BuiltinArrayAccessAliasResult stdVectorResult = matchBuiltinArrayAccessAliasUnderPrefix(
      name, stdVectorRootPrefix, {}, AccessAliasSpellingMode::kFull, /*rejectOnRawResidualSlash=*/false);
  if (stdVectorResult.outcome != BuiltinArrayAccessAliasOutcome::kNoMatch) {
    // Hard stop: `name` is under the std vector root, so the verdict is
    // decided here regardless of whether it classified, exactly like
    // semantics' real inline `stdVectorRoot` block.
    if (stdVectorResult.outcome == BuiltinArrayAccessAliasOutcome::kAccept) {
      out = stdVectorResult.token;
      return true;
    }
    return false;
  }

  if (name.rfind("array/", 0) == 0) {
    return false;
  }

  const BuiltinArrayAccessAliasResult keyValueResult = resolveKeyValueHelper(name);
  if (keyValueResult.outcome == BuiltinArrayAccessAliasOutcome::kAccept) {
    out = keyValueResult.token;
    return true;
  }
  if (keyValueResult.outcome == BuiltinArrayAccessAliasOutcome::kReject) {
    return false;
  }

  if (name.find('/') != std::string::npos) {
    if (rawName.find('/') != std::string::npos) {
      return false;
    }
    std::optional<std::string> token = classifyAccessAliasToken(rawName, AccessAliasSpellingMode::kFull);
    if (token) {
      out = *token;
      return true;
    }
    return false;
  }

  std::optional<std::string> token = classifyAccessAliasToken(name, AccessAliasSpellingMode::kFull);
  if (token) {
    out = *token;
    return true;
  }
  return false;
}

bool classifyBuiltinArrayAccessNameForIrLowerer(
    const std::string &scopedName, const std::string &rawName, const std::string &stdCollectionsPrefix,
    const std::string &stdVectorRootPrefix, const std::string &experimentalVectorRootPrefix,
    const std::string &legacyVectorFolderPrefix, const std::string &internalSoaStorageFolderPrefix,
    const std::string &vectorHelperPathAtName, const std::string &vectorHelperPathAtUnsafeName,
    const BuiltinArrayAccessKeyValueLookup &resolveKeyValueHelper,
    const std::function<std::optional<std::string>(const std::string &)> &resolveInternalSoaStorageFallbackAlias,
    std::string &out) {
  out.clear();

  const std::string scopedNameWithoutSuffix = stripGeneratedSuffixLocal(scopedName);
  if (scopedNameWithoutSuffix == vectorHelperPathAtName || scopedNameWithoutSuffix == vectorHelperPathAtUnsafeName) {
    return false;
  }

  auto tryAccept = [&](const std::string &prefix, const std::string &receiverBase, AccessAliasSpellingMode mode) -> bool {
    BuiltinArrayAccessAliasResult result =
        matchBuiltinArrayAccessAliasUnderPrefix(scopedName, prefix, receiverBase, mode);
    if (result.outcome == BuiltinArrayAccessAliasOutcome::kAccept) {
      out = result.token;
      return true;
    }
    return false;
  };

  // Branch 3: std vector root, receiver-base-disambiguated bare spellings,
  // then the plain concatenated-only legacy spelling under the wider
  // "std/collections/" root.
  if (tryAccept(stdVectorRootPrefix, "Vector", AccessAliasSpellingMode::kBareOnly)) {
    return true;
  }
  if (tryAccept(stdCollectionsPrefix, {}, AccessAliasSpellingMode::kConcatenatedOnly)) {
    return true;
  }
  if (scopedName.rfind(stdVectorRootPrefix, 0) == 0) {
    return false;
  }

  // Branch 3 again: experimental vector root.
  if (tryAccept(experimentalVectorRootPrefix, "Vector", AccessAliasSpellingMode::kBareOnly)) {
    return true;
  }
  if (tryAccept(experimentalVectorRootPrefix, {}, AccessAliasSpellingMode::kConcatenatedOnly)) {
    return true;
  }
  if (scopedName.rfind(experimentalVectorRootPrefix, 0) == 0) {
    return false;
  }

  if (tryAccept(legacyVectorFolderPrefix, {}, AccessAliasSpellingMode::kConcatenatedOnly)) {
    return true;
  }
  if (scopedName.rfind(legacyVectorFolderPrefix, 0) == 0) {
    return false;
  }

  // Branch 4: internal-SOA-storage-column receiver.
  if (tryAccept(internalSoaStorageFolderPrefix, "SoaColumn", AccessAliasSpellingMode::kBareOnly)) {
    return true;
  }
  if (scopedName.rfind(internalSoaStorageFolderPrefix, 0) == 0) {
    if (resolveInternalSoaStorageFallbackAlias) {
      std::optional<std::string> alias = resolveInternalSoaStorageFallbackAlias(scopedName);
      if (alias && (*alias == "at" || *alias == "at_unsafe")) {
        out = *alias;
        return true;
      }
    }
    return false;
  }

  if (scopedName.rfind("array/", 0) == 0) {
    return false;
  }
  if (scopedName.rfind("vector/", 0) == 0) {
    return false;
  }

  const BuiltinArrayAccessAliasResult keyValueResult = resolveKeyValueHelper(scopedName);
  if (keyValueResult.outcome == BuiltinArrayAccessAliasOutcome::kReject) {
    return false;
  }
  // ir_lowerer's real body never accepts through the key-value lookup -
  // kAccept is not a real outcome its own resolveKeyValueHelper adapter
  // ever produces, but is handled the same as kNoMatch here for symmetry
  // with the shared BuiltinArrayAccessAliasOutcome contract.
  if (keyValueResult.outcome == BuiltinArrayAccessAliasOutcome::kAccept) {
    out = keyValueResult.token;
    return true;
  }

  std::string strippedRawName = stripGeneratedSuffixLocal(rawName);
  if (strippedRawName.find('/') != std::string::npos) {
    return false;
  }
  if (strippedRawName == "at" || strippedRawName == "at_unsafe") {
    out = strippedRawName;
    return true;
  }
  return false;
}

}  // namespace primec
