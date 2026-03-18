#include "SemanticsValidateReflectionGeneratedHelpers.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateReflectionGeneratedHelpersCompare.h"
#include "SemanticsValidateReflectionGeneratedHelpersSerialization.h"
#include "SemanticsValidateReflectionGeneratedHelpersState.h"
#include "SemanticsValidateReflectionGeneratedHelpersValidate.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" || name == "enum" || name == "compute" || name == "workgroup_size" ||
         name == "unsafe" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" ||
         name == "buffer" || name == "reflect" || name == "generate" || name == "Additive" ||
         name == "Multiplicative" || name == "Comparable" || name == "Indexable";
}

std::string formatTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

std::string formatTransformType(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + formatTemplateArgs(transform.templateArgs) + ">";
}

} // namespace

bool rewriteReflectionGeneratedHelpers(Program &program, std::string &error) {
  auto isStructDefinition = [](const Definition &def, bool &isExplicitOut) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
    }
    if (hasStructTransform) {
      isExplicitOut = true;
      return true;
    }
    if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      isExplicitOut = false;
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        isExplicitOut = false;
        return false;
      }
    }
    isExplicitOut = false;
    return true;
  };
  auto hasTransformNamed = [](const std::vector<Transform> &transforms, const std::string &name) {
    return std::any_of(transforms.begin(), transforms.end(), [&](const Transform &transform) {
      return transform.name == name;
    });
  };
  auto transformHasArgument = [](const Transform &transform, const std::string &value) {
    return std::any_of(transform.arguments.begin(), transform.arguments.end(), [&](const std::string &arg) {
      return arg == value;
    });
  };

  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> definitionPaths;
  structNames.reserve(program.definitions.size());
  definitionPaths.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool isExplicitStruct = false;
    if (isStructDefinition(def, isExplicitStruct)) {
      structNames.insert(def.fullPath);
    }
    definitionPaths.insert(def.fullPath);
  }

  auto makeTypeBinding = [](const std::string &bindingName,
                            const std::string &typeName,
                            const std::string &namespacePrefix,
                            bool isMutable = false) {
    Expr binding;
    binding.isBinding = true;
    binding.name = bindingName;
    binding.namespacePrefix = namespacePrefix;
    Transform typeTransform;
    std::string typeBase;
    std::string typeArgText;
    if (semantics::splitTemplateTypeName(typeName, typeBase, typeArgText) && !typeBase.empty() &&
        !typeArgText.empty()) {
      typeTransform.name = typeBase;
      std::vector<std::string> typeArgs;
      if (semantics::splitTopLevelTemplateArgs(typeArgText, typeArgs)) {
        typeTransform.templateArgs = std::move(typeArgs);
      } else {
        typeTransform.templateArgs.push_back(typeArgText);
      }
    } else {
      typeTransform.name = typeName;
    }
    binding.transforms.push_back(std::move(typeTransform));
    if (isMutable) {
      Transform mutTransform;
      mutTransform.name = "mut";
      binding.transforms.push_back(std::move(mutTransform));
    }
    return binding;
  };
  auto makeNameExpr = [](const std::string &name) {
    Expr expr;
    expr.kind = Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeStringLiteralExpr = [](const std::string &value) {
    Expr expr;
    expr.kind = Expr::Kind::StringLiteral;
    std::string encoded;
    encoded.reserve(value.size() + 8);
    encoded.push_back('"');
    for (const char ch : value) {
      if (ch == '"' || ch == '\\') {
        encoded.push_back('\\');
      }
      encoded.push_back(ch);
    }
    encoded += "\"utf8";
    expr.stringValue = std::move(encoded);
    return expr;
  };
  auto makeCallExpr = [](const std::string &name, Expr arg) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = name;
    call.args.push_back(std::move(arg));
    call.argNames.push_back(std::nullopt);
    return call;
  };
  auto appendPublicVisibility = [](Definition &helper) {
    Transform visibilityTransform;
    visibilityTransform.name = "public";
    helper.transforms.push_back(std::move(visibilityTransform));
  };
  auto makeFieldAccessExpr = [&](const std::string &receiverName, const std::string &fieldName) {
    Expr fieldAccess;
    fieldAccess.kind = Expr::Kind::Call;
    fieldAccess.isFieldAccess = true;
    fieldAccess.name = fieldName;
    fieldAccess.args.push_back(makeNameExpr(receiverName));
    fieldAccess.argNames.push_back(std::nullopt);
    return fieldAccess;
  };
  auto bindingTypeFromField = [](const Expr &fieldBindingExpr, bool &ambiguousOut) -> std::string {
    ambiguousOut = false;
    std::optional<std::string> typeName;
    for (const auto &transform : fieldBindingExpr.transforms) {
      if (isNonTypeTransformName(transform.name)) {
        continue;
      }
      const std::string candidateType = formatTransformType(transform);
      if (typeName.has_value()) {
        ambiguousOut = true;
        return {};
      }
      typeName = candidateType;
    }
    if (!typeName.has_value()) {
      return "int";
    }
    return *typeName;
  };
  auto isCompareEligibleFieldType = [](const std::string &fieldTypeName) {
    const std::string normalized = semantics::normalizeBindingTypeName(fieldTypeName);
    return normalized == "int" || normalized == "i32" || normalized == "i64" || normalized == "u64" ||
           normalized == "bool" || normalized == "float" || normalized == "f32" || normalized == "f64" ||
           normalized == "string" || normalized == "integer" || normalized == "decimal";
  };
  auto isHash64EligibleFieldType = [](const std::string &fieldTypeName) {
    const std::string normalized = semantics::normalizeBindingTypeName(fieldTypeName);
    return normalized == "int" || normalized == "i32" || normalized == "i64" || normalized == "u64" ||
           normalized == "bool" || normalized == "float" || normalized == "f32" || normalized == "f64";
  };
  std::vector<Definition> rewrittenDefinitions;
  rewrittenDefinitions.reserve(program.definitions.size());
  for (auto &def : program.definitions) {
    const bool isStruct = structNames.count(def.fullPath) > 0;
    bool shouldGenerateEqual = false;
    bool shouldGenerateNotEqual = false;
    bool shouldGenerateDefault = false;
    bool shouldGenerateIsDefault = false;
    bool shouldGenerateClone = false;
    bool shouldGenerateDebugPrint = false;
    bool shouldGenerateCompare = false;
    bool shouldGenerateHash64 = false;
    bool shouldGenerateClear = false;
    bool shouldGenerateCopyFrom = false;
    bool shouldGenerateValidate = false;
    bool shouldGenerateSerialize = false;
    bool shouldGenerateDeserialize = false;
    if (isStruct && hasTransformNamed(def.transforms, "reflect")) {
      for (const auto &transform : def.transforms) {
        if (transform.name != "generate") {
          continue;
        }
        shouldGenerateEqual = shouldGenerateEqual || transformHasArgument(transform, "Equal");
        shouldGenerateNotEqual = shouldGenerateNotEqual || transformHasArgument(transform, "NotEqual");
        shouldGenerateDefault = shouldGenerateDefault || transformHasArgument(transform, "Default");
        shouldGenerateIsDefault = shouldGenerateIsDefault || transformHasArgument(transform, "IsDefault");
        shouldGenerateClone = shouldGenerateClone || transformHasArgument(transform, "Clone");
        shouldGenerateDebugPrint = shouldGenerateDebugPrint || transformHasArgument(transform, "DebugPrint");
        shouldGenerateCompare = shouldGenerateCompare || transformHasArgument(transform, "Compare");
        shouldGenerateHash64 = shouldGenerateHash64 || transformHasArgument(transform, "Hash64");
        shouldGenerateClear = shouldGenerateClear || transformHasArgument(transform, "Clear");
        shouldGenerateCopyFrom = shouldGenerateCopyFrom || transformHasArgument(transform, "CopyFrom");
        shouldGenerateValidate = shouldGenerateValidate || transformHasArgument(transform, "Validate");
        shouldGenerateSerialize = shouldGenerateSerialize || transformHasArgument(transform, "Serialize");
        shouldGenerateDeserialize = shouldGenerateDeserialize || transformHasArgument(transform, "Deserialize");
      }
    }

    std::vector<std::string> fieldNames;
    std::unordered_map<std::string, std::string> fieldTypeNames;
    if (shouldGenerateEqual || shouldGenerateNotEqual || shouldGenerateIsDefault || shouldGenerateClone ||
        shouldGenerateDebugPrint || shouldGenerateCompare || shouldGenerateHash64 || shouldGenerateClear ||
        shouldGenerateCopyFrom || shouldGenerateValidate || shouldGenerateSerialize || shouldGenerateDeserialize) {
      fieldNames.reserve(def.statements.size());
      fieldTypeNames.reserve(def.statements.size());
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          continue;
        }
        const bool isStaticField =
            std::any_of(stmt.transforms.begin(), stmt.transforms.end(), [](const Transform &transform) {
              return transform.name == "static";
            });
        if (!isStaticField) {
          fieldNames.push_back(stmt.name);
          bool ambiguousFieldType = false;
          const std::string fieldTypeName = bindingTypeFromField(stmt, ambiguousFieldType);
          if (!ambiguousFieldType && !fieldTypeName.empty()) {
            fieldTypeNames.emplace(stmt.name, fieldTypeName);
          }
        }
      }
    }
    auto ensureGeneratedHelperFieldEligibility = [&](const std::string &helperName,
                                                     const std::function<bool(const std::string &)> &isEligible) -> bool {
      const std::string helperPath = def.fullPath + "/" + helperName;
      for (const auto &fieldName : fieldNames) {
        const auto typeIt = fieldTypeNames.find(fieldName);
        if (typeIt == fieldTypeNames.end()) {
          continue;
        }
        if (isEligible(typeIt->second)) {
          continue;
        }
        error = "generated reflection helper " + helperPath + " does not support field envelope: " +
                fieldName + " (" + typeIt->second + ")";
        return false;
      }
      return true;
    };
    if (shouldGenerateCompare && !ensureGeneratedHelperFieldEligibility("Compare", isCompareEligibleFieldType)) {
      return false;
    }
    if (shouldGenerateHash64 && !ensureGeneratedHelperFieldEligibility("Hash64", isHash64EligibleFieldType)) {
      return false;
    }
    if (shouldGenerateSerialize && !ensureGeneratedHelperFieldEligibility("Serialize", isHash64EligibleFieldType)) {
      return false;
    }
    if (shouldGenerateDeserialize && !ensureGeneratedHelperFieldEligibility("Deserialize", isHash64EligibleFieldType)) {
      return false;
    }

    auto emitDebugPrintHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/DebugPrint";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "DebugPrint";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("void");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      if (fieldNames.empty()) {
        helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr(def.fullPath + " {}")));
      } else {
        helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr(def.fullPath + " {")));
        for (const auto &fieldName : fieldNames) {
          helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr("  " + fieldName)));
        }
        helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr("}")));
      }

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitCloneHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Clone";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Clone";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back(def.fullPath);
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      Expr cloneCall;
      cloneCall.kind = Expr::Kind::Call;
      cloneCall.name = def.fullPath;
      for (const auto &fieldName : fieldNames) {
        cloneCall.args.push_back(makeFieldAccessExpr("value", fieldName));
        cloneCall.argNames.push_back(std::nullopt);
      }
      helper.returnExpr = std::move(cloneCall);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    if (shouldGenerateEqual) {
      ReflectionGeneratedHelperContext compareContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionComparisonHelper(compareContext, "Equal", "equal", "and", true)) {
        return false;
      }
    }
    if (shouldGenerateNotEqual) {
      ReflectionGeneratedHelperContext compareContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionComparisonHelper(compareContext, "NotEqual", "not_equal", "or", false)) {
        return false;
      }
    }
    if (shouldGenerateCompare) {
      ReflectionGeneratedHelperContext compareContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionCompareHelper(compareContext)) {
        return false;
      }
    }
    if (shouldGenerateHash64) {
      ReflectionGeneratedHelperContext compareContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionHash64Helper(compareContext)) {
        return false;
      }
    }
    if (shouldGenerateClear) {
      ReflectionGeneratedHelperContext stateContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionClearHelper(stateContext)) {
        return false;
      }
    }
    if (shouldGenerateCopyFrom) {
      ReflectionGeneratedHelperContext stateContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionCopyFromHelper(stateContext)) {
        return false;
      }
    }
    if (shouldGenerateValidate) {
      ReflectionGeneratedHelperContext validationContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionValidateHelper(validationContext)) {
        return false;
      }
    }
    if (shouldGenerateSerialize) {
      ReflectionGeneratedHelperContext serializationContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionSerializeHelper(serializationContext)) {
        return false;
      }
    }
    if (shouldGenerateDeserialize) {
      ReflectionGeneratedHelperContext serializationContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionDeserializeHelper(serializationContext)) {
        return false;
      }
    }
    if (shouldGenerateDefault) {
      ReflectionGeneratedHelperContext stateContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionDefaultHelper(stateContext)) {
        return false;
      }
    }
    if (shouldGenerateIsDefault) {
      ReflectionGeneratedHelperContext stateContext{
          def, fieldNames, fieldTypeNames, definitionPaths, rewrittenDefinitions, error};
      if (!emitReflectionIsDefaultHelper(stateContext)) {
        return false;
      }
    }
    if (shouldGenerateClone) {
      if (!emitCloneHelper()) {
        return false;
      }
    }
    if (shouldGenerateDebugPrint) {
      if (!emitDebugPrintHelper()) {
        return false;
      }
    }

    rewrittenDefinitions.push_back(std::move(def));
  }
  program.definitions = std::move(rewrittenDefinitions);
  return true;
}


} // namespace primec::semantics
