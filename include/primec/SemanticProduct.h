#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace primec {

struct SemanticProgramDefinition {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct SemanticProgramExecution {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct SemanticProgramDirectCallTarget {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct SemanticProgramMethodCallTarget {
  std::string scopePath;
  std::string methodName;
  std::string receiverTypeText;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct SemanticProgramBridgePathChoice {
  std::string scopePath;
  std::string collectionFamily;
  std::string helperName;
  std::string chosenPath;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct SemanticProgramCallableSummary {
  std::string fullPath;
  bool isExecution = false;
  std::string returnKind;
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
  std::size_t onErrorBoundArgCount = 0;
};

struct SemanticProgramTypeMetadata {
  std::string fullPath;
  std::string category;
  bool isPublic = false;
  bool hasNoPadding = false;
  bool hasPlatformIndependentPadding = false;
  bool hasExplicitAlignment = false;
  uint32_t explicitAlignmentBytes = 0;
  std::size_t fieldCount = 0;
  std::size_t enumValueCount = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct SemanticProgram {
  std::string entryPath;
  std::vector<std::string> sourceImports;
  std::vector<std::string> imports;
  std::vector<SemanticProgramDefinition> definitions;
  std::vector<SemanticProgramExecution> executions;
  std::vector<SemanticProgramDirectCallTarget> directCallTargets;
  std::vector<SemanticProgramMethodCallTarget> methodCallTargets;
  std::vector<SemanticProgramBridgePathChoice> bridgePathChoices;
  std::vector<SemanticProgramCallableSummary> callableSummaries;
  std::vector<SemanticProgramTypeMetadata> typeMetadata;
};

} // namespace primec
