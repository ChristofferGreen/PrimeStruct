#pragma once

#include <cstddef>
#include <array>
#include <functional>
#include <cstdint>
#include <memory_resource>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <vector>

#include "SemanticsHelpers.h"
#include "primec/SemanticsDefinitionPrepass.h"
#include "primec/SymbolInterner.h"
#include "primec/Semantics.h"

namespace primec::semantics {

struct TypeResolutionGraph;
struct TypeResolutionGraphNode;

class SemanticsValidator {
public:
  struct ReturnResolutionSnapshotEntry {
    std::string definitionPath;
    ReturnKind kind = ReturnKind::Unknown;
    std::string structPath;
    BindingInfo binding;
  };

  struct LocalAutoBindingSnapshotEntry {
    std::string scopePath;
    std::string bindingName;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo binding;
    std::string initializerResolvedPath;
    BindingInfo initializerBinding;
    BindingInfo initializerReceiverBinding;
    std::string initializerQueryTypeText;
    bool initializerResultHasValue = false;
    std::string initializerResultValueType;
    std::string initializerResultErrorType;
    bool initializerHasTry = false;
    std::string initializerTryOperandResolvedPath;
    BindingInfo initializerTryOperandBinding;
    BindingInfo initializerTryOperandReceiverBinding;
    std::string initializerTryOperandQueryTypeText;
    std::string initializerTryValueType;
    std::string initializerTryErrorType;
    ReturnKind initializerTryContextReturnKind = ReturnKind::Unknown;
    std::string initializerTryOnErrorHandlerPath;
    std::string initializerTryOnErrorErrorType;
    size_t initializerTryOnErrorBoundArgCount = 0;
    uint64_t semanticNodeId = 0;
    std::string initializerDirectCallResolvedPath;
    ReturnKind initializerDirectCallReturnKind = ReturnKind::Unknown;
    std::string initializerMethodCallResolvedPath;
    ReturnKind initializerMethodCallReturnKind = ReturnKind::Unknown;
  };

  struct LocalAutoTrySnapshotData {
    std::string operandResolvedPath;
    BindingInfo operandBinding;
    BindingInfo operandReceiverBinding;
    std::string operandQueryTypeText;
    std::string valueType;
    std::string errorType;
    ReturnKind contextReturnKind = ReturnKind::Unknown;
    std::string onErrorHandlerPath;
    std::string onErrorErrorType;
    size_t onErrorBoundArgCount = 0;
  };

  struct ResultTypeInfo {
    bool isResult = false;
    bool hasValue = false;
    std::string valueType;
    std::string errorType;
  };

  struct QuerySnapshotData {
    std::string resolvedPath;
    std::string typeText;
    BindingInfo binding;
    ResultTypeInfo resultInfo;
    BindingInfo receiverBinding;
  };

  struct CallSnapshotData {
    std::string resolvedPath;
    BindingInfo binding;
  };

  struct TryValueSnapshotEntry {
    std::string scopePath;
    std::string operandResolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo operandBinding;
    BindingInfo operandReceiverBinding;
    std::string operandQueryTypeText;
    std::string valueType;
    std::string errorType;
    ReturnKind contextReturnKind = ReturnKind::Unknown;
    std::string onErrorHandlerPath;
    std::string onErrorErrorType;
    size_t onErrorBoundArgCount = 0;
    uint64_t semanticNodeId = 0;
  };

  struct CallBindingSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo binding;
  };

  struct DirectCallTargetSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    uint64_t semanticNodeId = 0;
  };

  struct OnErrorSnapshotEntry {
    std::string definitionPath;
    ReturnKind returnKind = ReturnKind::Unknown;
    std::string handlerPath;
    std::string errorType;
    size_t boundArgCount = 0;
    std::vector<std::string> boundArgTexts;
    bool returnResultHasValue = false;
    std::string returnResultValueType;
    std::string returnResultErrorType;
    uint64_t semanticNodeId = 0;
  };

  struct MethodCallTargetSnapshotEntry {
    std::string scopePath;
    std::string methodName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo receiverBinding;
    uint64_t semanticNodeId = 0;
  };

  struct BridgePathChoiceSnapshotEntry {
    std::string scopePath;
    std::string collectionFamily;
    std::string helperName;
    std::string chosenPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    uint64_t semanticNodeId = 0;
  };

  struct CallableSummarySnapshotEntry {
    std::string fullPath;
    bool isExecution = false;
    ReturnKind returnKind = ReturnKind::Unknown;
    bool isCompute = false;
    bool isUnsafe = false;
    std::vector<std::string> activeEffects;
    std::vector<std::string> activeCapabilities;
    bool hasResultType = false;
    bool resultTypeHasValue = false;
    std::string resultValueType;
    std::string resultErrorType;
    bool hasOnError = false;
    std::string onErrorHandlerPath;
    std::string onErrorErrorType;
    size_t onErrorBoundArgCount = 0;
    uint64_t semanticNodeId = 0;
  };

  struct TypeMetadataSnapshotEntry {
    std::string fullPath;
    std::string category;
    bool isPublic = false;
    bool hasNoPadding = false;
    bool hasPlatformIndependentPadding = false;
    bool hasExplicitAlignment = false;
    uint32_t explicitAlignmentBytes = 0;
    size_t fieldCount = 0;
    size_t enumValueCount = 0;
    int sourceLine = 0;
    int sourceColumn = 0;
    uint64_t semanticNodeId = 0;
  };

  struct StructFieldMetadataSnapshotEntry {
    std::string structPath;
    std::string fieldName;
    size_t fieldIndex = 0;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo binding;
    uint64_t semanticNodeId = 0;
  };

  struct BindingFactSnapshotEntry {
    std::string scopePath;
    std::string siteKind;
    std::string name;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo binding;
    uint64_t semanticNodeId = 0;
  };

  struct ReturnFactSnapshotEntry {
    std::string definitionPath;
    ReturnKind kind = ReturnKind::Unknown;
    std::string structPath;
    BindingInfo binding;
    int sourceLine = 0;
    int sourceColumn = 0;
    uint64_t semanticNodeId = 0;
  };

  struct QueryFactSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    std::string typeText;
    BindingInfo binding;
    BindingInfo receiverBinding;
    bool hasResultType = false;
    bool resultTypeHasValue = false;
    std::string resultValueType;
    std::string resultErrorType;
    uint64_t semanticNodeId = 0;
  };

  struct ValidationCounters {
    uint64_t callsVisited = 0;
    uint64_t peakLocalMapSize = 0;
  };

  SemanticsValidator(const Program &program,
                     const std::string &entryPath,
                     std::string &error,
                     const std::vector<std::string> &defaultEffects,
                     const std::vector<std::string> &entryDefaultEffects,
                     SemanticDiagnosticInfo *diagnosticInfo,
                     bool collectDiagnostics,
                     uint32_t benchmarkSemanticDefinitionValidationWorkerCount = 1,
                     bool benchmarkSemanticPhaseCountersEnabled = false,
                     bool benchmarkSemanticDisableMethodTargetMemoization = false,
                     bool benchmarkSemanticGraphLocalAutoLegacyKeyShadow = false,
                     bool benchmarkSemanticGraphLocalAutoLegacySideChannelShadow = false,
                     bool benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr = false);

  bool run();
  const ValidationCounters &validationCounters() const { return validationCounters_; }
  std::vector<ReturnResolutionSnapshotEntry> returnResolutionSnapshotForTesting() const;
  std::vector<LocalAutoBindingSnapshotEntry> localAutoBindingSnapshotForTesting() const;
  std::vector<CallBindingSnapshotEntry> callBindingSnapshotForTesting();
  std::vector<DirectCallTargetSnapshotEntry> directCallTargetSnapshotForSemanticProduct() const;
  std::vector<MethodCallTargetSnapshotEntry> methodCallTargetSnapshotForSemanticProduct();
  std::vector<BridgePathChoiceSnapshotEntry> bridgePathChoiceSnapshotForSemanticProduct() const;
  std::vector<CallableSummarySnapshotEntry> callableSummarySnapshotForSemanticProduct() const;
  std::vector<TypeMetadataSnapshotEntry> typeMetadataSnapshotForSemanticProduct() const;
  std::vector<StructFieldMetadataSnapshotEntry> structFieldMetadataSnapshotForSemanticProduct();
  std::vector<BindingFactSnapshotEntry> bindingFactSnapshotForSemanticProduct();
  std::vector<ReturnFactSnapshotEntry> returnFactSnapshotForSemanticProduct();
  std::vector<LocalAutoBindingSnapshotEntry> localAutoFactSnapshotForSemanticProduct() const;
  std::vector<QueryFactSnapshotEntry> queryFactSnapshotForSemanticProduct();
  std::vector<TryValueSnapshotEntry> tryFactSnapshotForSemanticProduct();
  std::vector<OnErrorSnapshotEntry> onErrorFactSnapshotForSemanticProduct();
  void releaseTransientSnapshotCaches();

private:
  struct GraphLocalAutoKey {
    SymbolId scopePathId = InvalidSymbolId;
    int32_t sourceLine = 0;
    int32_t sourceColumn = 0;

    bool operator==(const GraphLocalAutoKey &other) const {
      return scopePathId == other.scopePathId &&
             sourceLine == other.sourceLine &&
             sourceColumn == other.sourceColumn;
    }
  };

  struct GraphLocalAutoKeyHash {
    std::size_t operator()(const GraphLocalAutoKey &key) const {
      std::size_t hash = std::hash<SymbolId>{}(key.scopePathId);
      hash ^= std::hash<int32_t>{}(key.sourceLine) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      hash ^= std::hash<int32_t>{}(key.sourceColumn) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  struct GraphLocalAutoFacts {
    bool hasBinding = false;
    BindingInfo binding;
    std::string initializerResolvedPath;
    bool hasInitializerBinding = false;
    BindingInfo initializerBinding;
    bool hasQuerySnapshot = false;
    QuerySnapshotData querySnapshot;
    bool hasTryValue = false;
    LocalAutoTrySnapshotData tryValue;
    std::string directCallResolvedPath;
    bool hasDirectCallReturnKind = false;
    ReturnKind directCallReturnKind = ReturnKind::Unknown;
    std::string methodCallResolvedPath;
    bool hasMethodCallReturnKind = false;
    ReturnKind methodCallReturnKind = ReturnKind::Unknown;
  };

  #include "SemanticsValidatorPrivateCore.h"
  #include "SemanticsValidatorPrivateExprValidation.h"
  #include "SemanticsValidatorPrivateExprInference.h"
  #include "SemanticsValidatorPrivateStatements.h"

  struct EffectScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    EffectScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationState_.context.activeEffects)) {
      validator.currentValidationState_.context.activeEffects = std::move(nextIn);
    }
    ~EffectScope() {
      validator.currentValidationState_.context.activeEffects = std::move(previous);
    }
  };
  struct ValidationStateScope {
    SemanticsValidator &validator;
    ValidationState previous;
    ValidationStateScope(SemanticsValidator &validatorIn, ValidationState state)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationState_)) {
      validator.currentValidationState_ = std::move(state);
    }
    ~ValidationStateScope() {
      validator.currentValidationState_ = std::move(previous);
    }
  };
  struct DefinitionValidationSliceContext {
    const Definition *definition = nullptr;
    const std::vector<ParameterInfo> *params = nullptr;
    ValidationState validationState;
    std::unordered_map<std::string, BindingInfo> locals;
    ReturnKind returnKind = ReturnKind::Unknown;
    bool sawReturn = false;
  };
  struct WorkerLocalDefinitionValidationScope {
    SemanticsValidator &validator;
    DefinitionValidationSliceContext &context;
    const Definition *previousDefinition = nullptr;
    ValidationState previousValidationState;

    WorkerLocalDefinitionValidationScope(SemanticsValidator &validatorIn,
                                         DefinitionValidationSliceContext &contextIn)
        : validator(validatorIn),
          context(contextIn),
          previousDefinition(validatorIn.currentDefinitionContext_),
          previousValidationState(std::move(validatorIn.currentValidationState_)) {
      validator.currentDefinitionContext_ = context.definition;
      validator.currentValidationState_ = std::move(context.validationState);
    }

    ~WorkerLocalDefinitionValidationScope() {
      if (!validator.error_.empty() && context.definition != nullptr) {
        validator.captureDefinitionContext(*context.definition);
      }
      context.validationState = std::move(validator.currentValidationState_);
      validator.currentDefinitionContext_ = previousDefinition;
      validator.currentValidationState_ = std::move(previousValidationState);
    }
  };
  struct EntryArgStringScope {
    SemanticsValidator &validator;
    bool previous = false;
    EntryArgStringScope(SemanticsValidator &validatorIn, bool allow)
        : validator(validatorIn), previous(validatorIn.allowEntryArgStringUse_) {
      validator.allowEntryArgStringUse_ = allow;
    }
    ~EntryArgStringScope() {
      validator.allowEntryArgStringUse_ = previous;
    }
  };
  struct OnErrorScope {
    SemanticsValidator &validator;
    std::optional<OnErrorHandler> previous;
    OnErrorScope(SemanticsValidator &validatorIn, std::optional<OnErrorHandler> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationState_.context.onError)) {
      validator.currentValidationState_.context.onError = std::move(nextIn);
    }
    ~OnErrorScope() {
      validator.currentValidationState_.context.onError = std::move(previous);
    }
  };
  struct MovedScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    MovedScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationState_.movedBindings)) {
      validator.currentValidationState_.movedBindings = std::move(nextIn);
    }
    ~MovedScope() {
      validator.currentValidationState_.movedBindings = std::move(previous);
    }
  };
  struct BorrowEndScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    BorrowEndScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationState_.endedReferenceBorrows)) {
      validator.currentValidationState_.endedReferenceBorrows = std::move(nextIn);
    }
    ~BorrowEndScope() {
      validator.currentValidationState_.endedReferenceBorrows = std::move(previous);
    }
  };
  struct LocalBindingScope {
    SemanticsValidator &validator;
    std::unordered_map<std::string, BindingInfo> &locals;
    std::vector<std::string> insertedNames;
    explicit LocalBindingScope(SemanticsValidator &validatorIn,
                               std::unordered_map<std::string, BindingInfo> &localsIn)
        : validator(validatorIn), locals(localsIn) {
      validator.activeLocalBindingScopes_.push_back(this);
    }
    ~LocalBindingScope() {
      bool erasedAny = false;
      for (auto it = insertedNames.rbegin(); it != insertedNames.rend(); ++it) {
        erasedAny = locals.erase(*it) > 0 || erasedAny;
      }
      if (erasedAny) {
        validator.bumpLocalBindingMemoRevision(&locals);
      }
      if (!validator.activeLocalBindingScopes_.empty()) {
        validator.activeLocalBindingScopes_.pop_back();
      }
    }
  };
  struct ExprContextScope {
    SemanticsValidator &validator;
    const Expr *previous;
    const Expr &current;
    ExprContextScope(SemanticsValidator &validatorIn, const Expr &exprIn)
        : validator(validatorIn), previous(validatorIn.currentExprContext_), current(exprIn) {
      validator.currentExprContext_ = &exprIn;
    }
    ~ExprContextScope() {
      if (!validator.error_.empty()) {
        validator.captureExprContext(current);
      }
      validator.currentExprContext_ = previous;
    }
  };
  struct DefinitionContextScope {
    SemanticsValidator &validator;
    const Definition *previous;
    const Definition &current;
    DefinitionContextScope(SemanticsValidator &validatorIn, const Definition &defIn)
        : validator(validatorIn), previous(validatorIn.currentDefinitionContext_), current(defIn) {
      validator.currentDefinitionContext_ = &defIn;
    }
    ~DefinitionContextScope() {
      if (!validator.error_.empty()) {
        validator.captureDefinitionContext(current);
      }
      validator.currentDefinitionContext_ = previous;
    }
  };
  struct ExecutionContextScope {
    SemanticsValidator &validator;
    const Execution *previous;
    const Execution &current;
    ExecutionContextScope(SemanticsValidator &validatorIn, const Execution &execIn)
        : validator(validatorIn), previous(validatorIn.currentExecutionContext_), current(execIn) {
      validator.currentExecutionContext_ = &execIn;
    }
    ~ExecutionContextScope() {
      if (!validator.error_.empty()) {
        validator.captureExecutionContext(current);
      }
      validator.currentExecutionContext_ = previous;
    }
  };

  struct CallTargetResolutionScratch {
    struct SymbolPairKey {
      SymbolId first = InvalidSymbolId;
      SymbolId second = InvalidSymbolId;

      bool operator==(const SymbolPairKey &other) const {
        return first == other.first && second == other.second;
      }
    };

    struct SymbolPairKeyHash {
      std::size_t operator()(const SymbolPairKey &key) const {
        std::size_t hash = std::hash<SymbolId>{}(key.first);
        hash ^= std::hash<SymbolId>{}(key.second) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
      }
    };

    struct MethodTargetMemoKey {
      uint64_t semanticNodeId = 0;
      int sourceLine = 0;
      int sourceColumn = 0;
      uint64_t localsRevision = 0;
      SymbolId receiverTypeText = InvalidSymbolId;
      SymbolId methodName = InvalidSymbolId;

      bool operator==(const MethodTargetMemoKey &other) const {
        return semanticNodeId == other.semanticNodeId &&
               sourceLine == other.sourceLine &&
               sourceColumn == other.sourceColumn &&
               localsRevision == other.localsRevision &&
               receiverTypeText == other.receiverTypeText &&
               methodName == other.methodName;
      }
    };

    struct MethodTargetMemoKeyHash {
      std::size_t operator()(const MethodTargetMemoKey &key) const {
        std::size_t hash = std::hash<uint64_t>{}(key.semanticNodeId);
        hash ^= std::hash<int>{}(key.sourceLine) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>{}(key.sourceColumn) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>{}(key.localsRevision) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<SymbolId>{}(key.receiverTypeText) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<SymbolId>{}(key.methodName) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
      }
    };

    struct SymbolIndexKey {
      SymbolId symbol = InvalidSymbolId;
      uint32_t index = 0;

      bool operator==(const SymbolIndexKey &other) const {
        return symbol == other.symbol && index == other.index;
      }
    };

    struct SymbolIndexKeyHash {
      std::size_t operator()(const SymbolIndexKey &key) const {
        std::size_t hash = std::hash<SymbolId>{}(key.symbol);
        hash ^= std::hash<uint32_t>{}(key.index) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
      }
    };

    using PmrBoolMap = std::pmr::unordered_map<SymbolId, bool>;
    using PmrSymbolStringMap = std::pmr::unordered_map<SymbolId, std::string>;
    using PmrPairStringMap = std::pmr::unordered_map<SymbolPairKey, std::string, SymbolPairKeyHash>;
    using PmrMethodMemoMap = std::pmr::unordered_map<MethodTargetMemoKey, std::string, MethodTargetMemoKeyHash>;
    using PmrIndexStringMap = std::pmr::unordered_map<SymbolIndexKey, std::string, SymbolIndexKeyHash>;
    using PmrStringVector = std::pmr::vector<std::string>;

    const Definition *definitionOwner = nullptr;
    const Execution *executionOwner = nullptr;
    SymbolInterner keyInterner;
    std::array<std::byte, 8192> inlineArenaBuffer{};
    std::pmr::monotonic_buffer_resource arenaResource{
        inlineArenaBuffer.data(),
        inlineArenaBuffer.size(),
        std::pmr::new_delete_resource()};
    PmrBoolMap definitionFamilyPathCache{&arenaResource};
    PmrBoolMap overloadFamilyPathCache{&arenaResource};
    PmrSymbolStringMap overloadFamilyPrefixCache{&arenaResource};
    PmrSymbolStringMap specializationPrefixCache{&arenaResource};
    PmrIndexStringMap overloadCandidatePathCache{&arenaResource};
    PmrSymbolStringMap rootedCallNamePathCache{&arenaResource};
    PmrSymbolStringMap normalizedNamespacePrefixCache{&arenaResource};
    PmrPairStringMap joinedCallPathCache{&arenaResource};
    PmrSymbolStringMap normalizedMethodNameCache{&arenaResource};
    PmrPairStringMap explicitRemovedMethodPathCache{&arenaResource};
    PmrMethodMemoMap methodTargetMemoCache{&arenaResource};
    PmrStringVector concreteCallBaseCandidates{&arenaResource};
    PmrStringVector methodReceiverResolutionCandidates{&arenaResource};
    PmrSymbolStringMap canonicalReceiverAliasPathCache{&arenaResource};

    void resetArena() {
      this->~CallTargetResolutionScratch();
      new (this) CallTargetResolutionScratch();
    }
  };

  struct ExprReturnKindMemoKey {
    const Expr *expr = nullptr;
    const Definition *definitionOwner = nullptr;
    const Execution *executionOwner = nullptr;
    const ParameterInfo *paramsData = nullptr;
    std::size_t paramsSize = 0;
    const void *localsIdentity = nullptr;
    std::size_t localsSize = 0;

    bool operator==(const ExprReturnKindMemoKey &other) const {
      return expr == other.expr &&
             definitionOwner == other.definitionOwner &&
             executionOwner == other.executionOwner &&
             paramsData == other.paramsData &&
             paramsSize == other.paramsSize &&
             localsIdentity == other.localsIdentity &&
             localsSize == other.localsSize;
    }
  };

  struct ExprReturnKindMemoKeyHash {
    std::size_t operator()(const ExprReturnKindMemoKey &key) const {
      std::size_t hash = std::hash<const Expr *>{}(key.expr);
      hash ^= std::hash<const Definition *>{}(key.definitionOwner) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<const Execution *>{}(key.executionOwner) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<const ParameterInfo *>{}(key.paramsData) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<std::size_t>{}(key.paramsSize) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<const void *>{}(key.localsIdentity) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<std::size_t>{}(key.localsSize) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  struct ExprReturnKindMemoValue {
    ReturnKind kind = ReturnKind::Unknown;
    uint64_t localsRevision = 0;
  };

  struct ExprStructReturnMemoKey {
    const Expr *expr = nullptr;
    const Definition *definitionOwner = nullptr;
    const Execution *executionOwner = nullptr;
    const ParameterInfo *paramsData = nullptr;
    std::size_t paramsSize = 0;
    const void *localsIdentity = nullptr;
    std::size_t localsSize = 0;

    bool operator==(const ExprStructReturnMemoKey &other) const {
      return expr == other.expr &&
             definitionOwner == other.definitionOwner &&
             executionOwner == other.executionOwner &&
             paramsData == other.paramsData &&
             paramsSize == other.paramsSize &&
             localsIdentity == other.localsIdentity &&
             localsSize == other.localsSize;
    }
  };

  struct ExprStructReturnMemoKeyHash {
    std::size_t operator()(const ExprStructReturnMemoKey &key) const {
      std::size_t hash = std::hash<const Expr *>{}(key.expr);
      hash ^= std::hash<const Definition *>{}(key.definitionOwner) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<const Execution *>{}(key.executionOwner) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<const ParameterInfo *>{}(key.paramsData) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<std::size_t>{}(key.paramsSize) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<const void *>{}(key.localsIdentity) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= std::hash<std::size_t>{}(key.localsSize) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  struct ExprStructReturnMemoValue {
    std::string structPath;
    uint64_t localsRevision = 0;
  };

  struct StructFieldReturnKindMemoKey {
    const Definition *structDefinition = nullptr;
    std::string fieldName;
    bool staticReceiver = false;

    bool operator==(const StructFieldReturnKindMemoKey &other) const {
      return structDefinition == other.structDefinition &&
             fieldName == other.fieldName &&
             staticReceiver == other.staticReceiver;
    }
  };

  struct StructFieldReturnKindMemoKeyHash {
    std::size_t operator()(const StructFieldReturnKindMemoKey &key) const {
      std::size_t hash = std::hash<const Definition *>{}(key.structDefinition);
      hash ^= std::hash<std::string>{}(key.fieldName) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      hash ^= std::hash<bool>{}(key.staticReceiver) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  const Program &program_;
  const std::string &entryPath_;
  std::string &error_;
  const std::vector<std::string> &defaultEffects_;
  const std::vector<std::string> &entryDefaultEffects_;
  SemanticDiagnosticInfo *diagnosticInfo_ = nullptr;
  DiagnosticSink diagnosticSink_;
  bool collectDiagnostics_ = false;
  uint32_t benchmarkSemanticDefinitionValidationWorkerCount_ = 1;
  bool benchmarkSemanticPhaseCountersEnabled_ = false;
  bool methodTargetMemoizationEnabled_ = true;
  bool structReturnMemoizationEnabled_ = true;
  bool benchmarkGraphLocalAutoLegacyKeyShadowEnabled_ = false;
  bool benchmarkGraphLocalAutoLegacySideChannelShadowEnabled_ = false;
  bool benchmarkGraphLocalAutoDependencyScratchPmrEnabled_ = true;
  ValidationCounters validationCounters_;
  bool allowRecursiveReturnInference_ = true;
  bool deferUnknownReturnInferenceErrors_ = false;

  std::unordered_set<std::string> defaultEffectSet_;
  std::unordered_set<std::string> entryDefaultEffectSet_;
  DefinitionPrepassSnapshot definitionPrepassSnapshot_;
  std::unordered_map<std::string, const Definition *> defMap_;
  std::unordered_map<std::string, ReturnKind> returnKinds_;
  std::unordered_map<std::string, std::string> returnStructs_;
  std::unordered_map<std::string, BindingInfo> returnBindings_;
  mutable SymbolInterner graphLocalAutoScopePathInterner_;
  mutable std::unordered_set<std::string> graphLocalAutoLegacyKeyShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, BindingInfo, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyBindingShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, std::string, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyInitializerResolvedPathShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, BindingInfo, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyInitializerBindingShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, QuerySnapshotData, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyQuerySnapshotShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, LocalAutoTrySnapshotData, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyTryValueShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, std::string, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyDirectCallPathShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, ReturnKind, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyDirectCallReturnKindShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, std::string, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyMethodCallPathShadow_;
  mutable std::unordered_map<GraphLocalAutoKey, ReturnKind, GraphLocalAutoKeyHash>
      graphLocalAutoLegacyMethodCallReturnKindShadow_;
  std::unordered_map<GraphLocalAutoKey, GraphLocalAutoFacts, GraphLocalAutoKeyHash> graphLocalAutoFacts_;
  std::unordered_set<std::string> structNames_;
  std::unordered_set<std::string> publicDefinitions_;
  std::unordered_map<std::string, std::vector<ParameterInfo>> paramsByDef_;
  std::unordered_set<std::string> overloadFamilyBasePaths_;
  std::unordered_map<std::string, std::string> uniqueSpecializationPathByBase_;
  std::unordered_set<std::string> ambiguousSpecializationBasePaths_;
  ValidationState currentValidationState_;
  std::unordered_set<std::string> inferenceStack_;
  std::unordered_set<std::string> returnBindingInferenceStack_;
  std::unordered_set<std::string> queryTypeInferenceDefinitionStack_;
  std::unordered_set<const Expr *> queryTypeInferenceExprStack_;
  std::unordered_map<std::string, std::string> importAliases_;
  bool queryFactSnapshotCacheValid_ = false;
  std::vector<QueryFactSnapshotEntry> queryFactSnapshotCache_;
  bool tryValueSnapshotCacheValid_ = false;
  bool callBindingSnapshotCacheValid_ = false;
  std::vector<TryValueSnapshotEntry> tryValueSnapshotCache_;
  std::vector<CallBindingSnapshotEntry> callBindingSnapshotCache_;
  mutable bool callableAndOnErrorSnapshotFactCacheValid_ = false;
  mutable std::vector<CallableSummarySnapshotEntry> callableSummaryDefinitionSnapshotCache_;
  mutable std::vector<OnErrorSnapshotEntry> onErrorSnapshotCache_;
  std::unordered_map<std::string, EffectFreeSummary> effectFreeDefCache_;
  std::unordered_set<std::string> effectFreeDefStack_;
  std::unordered_map<std::string, bool> effectFreeStructCache_;
  std::unordered_set<std::string> effectFreeStructStack_;
  bool mathImportAll_ = false;
  std::unordered_set<std::string> mathImports_;
  std::string entryArgsName_;
  bool allowEntryArgStringUse_ = false;
  const Expr *currentExprContext_ = nullptr;
  const Definition *currentDefinitionContext_ = nullptr;
  const Execution *currentExecutionContext_ = nullptr;
  std::vector<LocalBindingScope *> activeLocalBindingScopes_;
  std::unordered_map<const void *, uint64_t> localBindingMemoRevisionByIdentity_;
  std::unordered_map<ExprReturnKindMemoKey, ExprReturnKindMemoValue, ExprReturnKindMemoKeyHash>
      inferExprReturnKindMemo_;
  std::unordered_map<ExprStructReturnMemoKey, ExprStructReturnMemoValue, ExprStructReturnMemoKeyHash>
      inferStructReturnMemo_;
  std::unordered_map<StructFieldReturnKindMemoKey, ReturnKind, StructFieldReturnKindMemoKeyHash>
      structFieldReturnKindMemo_;
  mutable CallTargetResolutionScratch callTargetResolutionScratch_;

  void observeCallVisited();
  void observeLocalMapSize(std::size_t size);
};

} // namespace primec::semantics
