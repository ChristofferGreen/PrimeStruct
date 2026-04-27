#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IrLowererStructFieldBindingHelpers.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec {
struct SemanticProgramTypeMetadata;
struct SemanticProgram;
}

namespace primec::ir_lowerer {

struct BindingTypeLayout {
  uint32_t sizeBytes = 0;
  uint32_t alignmentBytes = 1;
};

uint32_t alignTo(uint32_t value, uint32_t alignment);
bool parsePositiveInt(const std::string &text, int &valueOut);
bool extractAlignment(const std::vector<Transform> &transforms,
                      const std::string &context,
                      uint32_t &alignmentOut,
                      bool &hasAlignment,
                      std::string &error);
bool classifyBindingTypeLayout(const LayoutFieldBinding &binding,
                               BindingTypeLayout &layoutOut,
                               std::string &structTypeNameOut,
                               std::string &errorOut);
bool resolveBindingTypeLayout(
    const LayoutFieldBinding &binding,
    const std::string &namespacePrefix,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const Definition &, IrStructLayout &, std::string &)> &computeStructLayout,
    BindingTypeLayout &layoutOut,
    std::string &errorOut);
bool computeStructLayoutWithCache(
    const std::string &structPath,
    std::unordered_map<std::string, IrStructLayout> &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    const std::function<bool(IrStructLayout &, std::string &)> &computeUncachedLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut);
bool validateSemanticProductStructLayoutCoverage(const Program &program,
                                                 const SemanticProgram *semanticProgram,
                                                 std::string &errorOut);
bool computeStructLayoutUncached(
    const Definition &def,
    const std::vector<LayoutFieldBinding> &fieldBindings,
    const std::function<bool(const LayoutFieldBinding &, BindingTypeLayout &, std::string &)> &resolveFieldTypeLayout,
    const SemanticProgramTypeMetadata *typeMetadata,
    IrStructLayout &layoutOut,
    std::string &errorOut);
bool computeStructLayoutFromFieldInfo(
    const Definition &def,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const Definition &, IrStructLayout &)> &computeStructLayout,
    const SemanticProgram *semanticProgram,
    IrStructLayout &layoutOut,
    std::string &errorOut);
bool appendProgramStructLayouts(
    const Program &program,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const SemanticProgram *semanticProgram,
    const std::function<bool(const Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut);
bool appendStructLayoutField(const std::string &structPath,
                             const Expr &fieldExpr,
                             const LayoutFieldBinding &binding,
                             const std::function<bool(const LayoutFieldBinding &,
                                                      BindingTypeLayout &,
                                                      std::string &)> &resolveTypeLayout,
                             uint32_t &offsetInOut,
                             uint32_t &structAlignmentInOut,
                             IrStructLayout &layoutOut,
                             std::string &errorOut);
bool isLayoutQualifierName(const std::string &name);
IrStructFieldCategory fieldCategory(const Expr &expr);
IrStructVisibility fieldVisibility(const Expr &expr);
bool isStaticField(const Expr &expr);

} // namespace primec::ir_lowerer
