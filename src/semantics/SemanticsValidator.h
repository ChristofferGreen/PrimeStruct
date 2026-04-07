#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <vector>

#include "SemanticsHelpers.h"
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

  struct QueryCallTypeSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    std::string typeText;
  };

  struct QueryBindingSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo binding;
  };

  struct QueryResultTypeSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    bool hasValue = false;
    std::string valueType;
    std::string errorType;
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

  struct QueryReceiverBindingSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo receiverBinding;
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

  struct ValidationContextSnapshotEntry {
    std::string definitionPath;
    ReturnKind returnKind = ReturnKind::Unknown;
    bool definitionIsCompute = false;
    bool definitionIsUnsafe = false;
    std::vector<std::string> activeEffects;
    bool hasResultType = false;
    bool resultTypeHasValue = false;
    std::string resultValueType;
    std::string resultErrorType;
    bool hasOnError = false;
    std::string onErrorHandlerPath;
    std::string onErrorErrorType;
    size_t onErrorBoundArgCount = 0;
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

  SemanticsValidator(const Program &program,
                     const std::string &entryPath,
                     std::string &error,
                     const std::vector<std::string> &defaultEffects,
                     const std::vector<std::string> &entryDefaultEffects,
                     SemanticDiagnosticInfo *diagnosticInfo,
                     bool collectDiagnostics);

  bool run();
  std::vector<ReturnResolutionSnapshotEntry> returnResolutionSnapshotForTesting() const;
  std::vector<LocalAutoBindingSnapshotEntry> localAutoBindingSnapshotForTesting() const;
  std::vector<QueryCallTypeSnapshotEntry> queryCallTypeSnapshotForTesting();
  std::vector<QueryBindingSnapshotEntry> queryBindingSnapshotForTesting();
  std::vector<QueryResultTypeSnapshotEntry> queryResultTypeSnapshotForTesting();
  std::vector<TryValueSnapshotEntry> tryValueSnapshotForTesting();
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
  std::vector<QueryReceiverBindingSnapshotEntry> queryReceiverBindingSnapshotForTesting();
  std::vector<OnErrorSnapshotEntry> onErrorSnapshotForTesting();
  std::vector<ValidationContextSnapshotEntry> validationContextSnapshotForTesting() const;

private:
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

  const Program &program_;
  const std::string &entryPath_;
  std::string &error_;
  const std::vector<std::string> &defaultEffects_;
  const std::vector<std::string> &entryDefaultEffects_;
  SemanticDiagnosticInfo *diagnosticInfo_ = nullptr;
  DiagnosticSink diagnosticSink_;
  bool collectDiagnostics_ = false;
  bool allowRecursiveReturnInference_ = true;
  bool deferUnknownReturnInferenceErrors_ = false;

  std::unordered_set<std::string> defaultEffectSet_;
  std::unordered_set<std::string> entryDefaultEffectSet_;
  std::unordered_map<std::string, const Definition *> defMap_;
  std::unordered_map<std::string, ReturnKind> returnKinds_;
  std::unordered_map<std::string, std::string> returnStructs_;
  std::unordered_map<std::string, BindingInfo> returnBindings_;
  std::unordered_map<std::string, BindingInfo> graphLocalAutoBindings_;
  std::unordered_map<std::string, std::string> graphLocalAutoResolvedPaths_;
  std::unordered_map<std::string, BindingInfo> graphLocalAutoInitializerBindings_;
  std::unordered_map<std::string, BindingInfo> graphLocalAutoReceiverBindings_;
  std::unordered_map<std::string, std::string> graphLocalAutoQueryTypeTexts_;
  std::unordered_map<std::string, ResultTypeInfo> graphLocalAutoResultTypes_;
  std::unordered_map<std::string, LocalAutoTrySnapshotData> graphLocalAutoTryValues_;
  std::unordered_map<std::string, std::string> graphLocalAutoDirectCallResolvedPaths_;
  std::unordered_map<std::string, ReturnKind> graphLocalAutoDirectCallReturnKinds_;
  std::unordered_map<std::string, std::string> graphLocalAutoMethodCallResolvedPaths_;
  std::unordered_map<std::string, ReturnKind> graphLocalAutoMethodCallReturnKinds_;
  std::unordered_set<std::string> structNames_;
  std::unordered_set<std::string> publicDefinitions_;
  std::unordered_map<std::string, std::vector<ParameterInfo>> paramsByDef_;
  ValidationState currentValidationState_;
  std::unordered_set<std::string> inferenceStack_;
  std::unordered_set<std::string> returnBindingInferenceStack_;
  std::unordered_set<std::string> queryTypeInferenceDefinitionStack_;
  std::unordered_set<const Expr *> queryTypeInferenceExprStack_;
  std::unordered_map<std::string, std::string> importAliases_;
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
};

} // namespace primec::semantics
