#pragma once

#include <functional>
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
  };

  struct CallBindingSnapshotEntry {
    std::string scopePath;
    std::string callName;
    std::string resolvedPath;
    int sourceLine = 0;
    int sourceColumn = 0;
    BindingInfo binding;
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
    bool returnResultHasValue = false;
    std::string returnResultValueType;
    std::string returnResultErrorType;
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
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.activeEffects)) {
      validator.currentValidationContext_.activeEffects = std::move(nextIn);
    }
    ~EffectScope() {
      validator.currentValidationContext_.activeEffects = std::move(previous);
    }
  };
  struct ValidationContextScope {
    SemanticsValidator &validator;
    ValidationContext previous;
    ValidationContextScope(SemanticsValidator &validatorIn, ValidationContext context)
        : validator(validatorIn), previous(validatorIn.currentValidationContext_) {
      validator.currentValidationContext_ = std::move(context);
    }
    ~ValidationContextScope() {
      validator.currentValidationContext_ = std::move(previous);
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
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.onError)) {
      validator.currentValidationContext_.onError = std::move(nextIn);
    }
    ~OnErrorScope() {
      validator.currentValidationContext_.onError = std::move(previous);
    }
  };
  struct MovedScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    MovedScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.movedBindings)) {
      validator.currentValidationContext_.movedBindings = std::move(nextIn);
    }
    ~MovedScope() {
      validator.currentValidationContext_.movedBindings = std::move(previous);
    }
  };
  struct BorrowEndScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    BorrowEndScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.endedReferenceBorrows)) {
      validator.currentValidationContext_.endedReferenceBorrows = std::move(nextIn);
    }
    ~BorrowEndScope() {
      validator.currentValidationContext_.endedReferenceBorrows = std::move(previous);
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
  std::unordered_set<std::string> structNames_;
  std::unordered_set<std::string> publicDefinitions_;
  std::unordered_map<std::string, std::vector<ParameterInfo>> paramsByDef_;
  std::unordered_map<std::string, ValidationContext> definitionValidationContexts_;
  std::unordered_map<std::string, ValidationContext> executionValidationContexts_;
  ValidationContext currentValidationContext_;
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
