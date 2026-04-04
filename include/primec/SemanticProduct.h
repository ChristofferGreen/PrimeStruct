#pragma once

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

struct SemanticProgram {
  std::string entryPath;
  std::vector<std::string> sourceImports;
  std::vector<std::string> imports;
  std::vector<SemanticProgramDefinition> definitions;
  std::vector<SemanticProgramExecution> executions;
  std::vector<SemanticProgramDirectCallTarget> directCallTargets;
  std::vector<SemanticProgramMethodCallTarget> methodCallTargets;
};

} // namespace primec
