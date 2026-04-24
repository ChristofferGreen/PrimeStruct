#include "primec/AstMemory.h"

#include <optional>
#include <string>
#include <vector>

namespace primec {
namespace {

template <typename T>
void releaseVectorStorage(std::vector<T> &values) {
  std::vector<T>().swap(values);
}

void releaseExprBodies(Expr &expr) {
  for (Expr &arg : expr.args) {
    releaseExprBodies(arg);
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    releaseExprBodies(bodyArg);
  }
  releaseVectorStorage(expr.args);
  releaseVectorStorage(expr.argNames);
  releaseVectorStorage(expr.bodyArguments);
  expr.hasBodyArguments = false;
}

void releaseDefinitionBody(Definition &definition) {
  for (Expr &parameter : definition.parameters) {
    releaseExprBodies(parameter);
  }
  for (Expr &statement : definition.statements) {
    releaseExprBodies(statement);
  }
  if (definition.returnExpr.has_value()) {
    releaseExprBodies(*definition.returnExpr);
    definition.returnExpr.reset();
  }
  releaseVectorStorage(definition.parameters);
  releaseVectorStorage(definition.statements);
}

void releaseExecutionBody(Execution &execution) {
  for (Expr &argument : execution.arguments) {
    releaseExprBodies(argument);
  }
  for (Expr &bodyArgument : execution.bodyArguments) {
    releaseExprBodies(bodyArgument);
  }
  releaseVectorStorage(execution.arguments);
  releaseVectorStorage(execution.argumentNames);
  releaseVectorStorage(execution.bodyArguments);
  execution.hasBodyArguments = false;
}

void addStringEstimate(const std::string &value, ProgramHeapEstimateStats &stats) {
  stats.strings += 1;
  stats.dynamicBytes += static_cast<uint64_t>(value.capacity()) + 1u;
}

template <typename T>
void addVectorStorageEstimate(const std::vector<T> &values, ProgramHeapEstimateStats &stats) {
  stats.dynamicBytes += static_cast<uint64_t>(values.capacity()) * static_cast<uint64_t>(sizeof(T));
}

void accumulateTransformEstimate(const Transform &transform, ProgramHeapEstimateStats &stats) {
  stats.transforms += 1;
  addStringEstimate(transform.name, stats);
  addVectorStorageEstimate(transform.templateArgs, stats);
  for (const std::string &arg : transform.templateArgs) {
    addStringEstimate(arg, stats);
  }
  addVectorStorageEstimate(transform.arguments, stats);
  for (const std::string &arg : transform.arguments) {
    addStringEstimate(arg, stats);
  }
}

void accumulateExprEstimate(const Expr &expr, ProgramHeapEstimateStats &stats);

void accumulateExprVectorEstimate(const std::vector<Expr> &exprs, ProgramHeapEstimateStats &stats) {
  addVectorStorageEstimate(exprs, stats);
  for (const Expr &expr : exprs) {
    accumulateExprEstimate(expr, stats);
  }
}

void accumulateExprEstimate(const Expr &expr, ProgramHeapEstimateStats &stats) {
  stats.exprs += 1;
  addStringEstimate(expr.floatValue, stats);
  addStringEstimate(expr.stringValue, stats);
  addStringEstimate(expr.name, stats);
  accumulateExprVectorEstimate(expr.args, stats);
  addVectorStorageEstimate(expr.argNames, stats);
  for (const std::optional<std::string> &argName : expr.argNames) {
    if (argName.has_value()) {
      addStringEstimate(*argName, stats);
    }
  }
  accumulateExprVectorEstimate(expr.bodyArguments, stats);
  addVectorStorageEstimate(expr.templateArgs, stats);
  for (const std::string &templateArg : expr.templateArgs) {
    addStringEstimate(templateArg, stats);
  }
  addStringEstimate(expr.namespacePrefix, stats);
  addVectorStorageEstimate(expr.transforms, stats);
  for (const Transform &transform : expr.transforms) {
    accumulateTransformEstimate(transform, stats);
  }
  addVectorStorageEstimate(expr.lambdaCaptures, stats);
  for (const std::string &capture : expr.lambdaCaptures) {
    addStringEstimate(capture, stats);
  }
}

} // namespace

ProgramHeapEstimateStats estimateProgramHeap(const Program &program) {
  ProgramHeapEstimateStats stats;
  addVectorStorageEstimate(program.sourceImports, stats);
  for (const std::string &importPath : program.sourceImports) {
    addStringEstimate(importPath, stats);
  }
  addVectorStorageEstimate(program.imports, stats);
  for (const std::string &importPath : program.imports) {
    addStringEstimate(importPath, stats);
  }
  addVectorStorageEstimate(program.definitions, stats);
  for (const Definition &definition : program.definitions) {
    stats.definitions += 1;
    addStringEstimate(definition.name, stats);
    addStringEstimate(definition.fullPath, stats);
    addStringEstimate(definition.namespacePrefix, stats);
    addVectorStorageEstimate(definition.transforms, stats);
    for (const Transform &transform : definition.transforms) {
      accumulateTransformEstimate(transform, stats);
    }
    addVectorStorageEstimate(definition.templateArgs, stats);
    for (const std::string &templateArg : definition.templateArgs) {
      addStringEstimate(templateArg, stats);
    }
    accumulateExprVectorEstimate(definition.parameters, stats);
    accumulateExprVectorEstimate(definition.statements, stats);
    if (definition.returnExpr.has_value()) {
      stats.dynamicBytes += sizeof(Expr);
      accumulateExprEstimate(*definition.returnExpr, stats);
    }
  }
  addVectorStorageEstimate(program.executions, stats);
  for (const Execution &execution : program.executions) {
    stats.executions += 1;
    addStringEstimate(execution.name, stats);
    addStringEstimate(execution.fullPath, stats);
    addStringEstimate(execution.namespacePrefix, stats);
    addVectorStorageEstimate(execution.transforms, stats);
    for (const Transform &transform : execution.transforms) {
      accumulateTransformEstimate(transform, stats);
    }
    addVectorStorageEstimate(execution.templateArgs, stats);
    for (const std::string &templateArg : execution.templateArgs) {
      addStringEstimate(templateArg, stats);
    }
    accumulateExprVectorEstimate(execution.arguments, stats);
    addVectorStorageEstimate(execution.argumentNames, stats);
    for (const std::optional<std::string> &argumentName : execution.argumentNames) {
      if (argumentName.has_value()) {
        addStringEstimate(*argumentName, stats);
      }
    }
    accumulateExprVectorEstimate(execution.bodyArguments, stats);
  }
  return stats;
}

void releaseLoweredAstBodies(Program &program) {
  for (Definition &definition : program.definitions) {
    releaseDefinitionBody(definition);
  }
  for (Execution &execution : program.executions) {
    releaseExecutionBody(execution);
  }
}

} // namespace primec
