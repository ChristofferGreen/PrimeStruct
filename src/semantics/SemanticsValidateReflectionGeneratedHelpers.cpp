#include "SemanticsValidateReflectionGeneratedHelpers.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateReflectionGeneratedHelpersSerialization.h"

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
  auto makeI32LiteralExpr = [](uint64_t value) {
    Expr expr;
    expr.kind = Expr::Kind::Literal;
    expr.literalValue = value;
    expr.intWidth = 32;
    expr.isUnsigned = false;
    return expr;
  };
  auto makeU64LiteralExpr = [](uint64_t value) {
    Expr expr;
    expr.kind = Expr::Kind::Literal;
    expr.literalValue = value;
    expr.intWidth = 64;
    expr.isUnsigned = true;
    return expr;
  };
  auto makeBoolLiteralExpr = [](bool value) {
    Expr expr;
    expr.kind = Expr::Kind::BoolLiteral;
    expr.boolValue = value;
    return expr;
  };
  auto makeBinaryCallExpr = [](const std::string &name, Expr left, Expr right) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = name;
    call.args.push_back(std::move(left));
    call.argNames.push_back(std::nullopt);
    call.args.push_back(std::move(right));
    call.argNames.push_back(std::nullopt);
    return call;
  };
  auto makeReturnStatementExpr = [](Expr valueExpr) {
    Expr returnCall;
    returnCall.kind = Expr::Kind::Call;
    returnCall.name = "return";
    returnCall.args.push_back(std::move(valueExpr));
    returnCall.argNames.push_back(std::nullopt);
    return returnCall;
  };
  auto makeEnvelopeExpr = [](const std::string &name, std::vector<Expr> bodyArguments) {
    Expr envelope;
    envelope.kind = Expr::Kind::Call;
    envelope.name = name;
    envelope.hasBodyArguments = true;
    envelope.bodyArguments = std::move(bodyArguments);
    return envelope;
  };
  auto makeIfStatementExpr = [](Expr conditionExpr, Expr thenEnvelopeExpr, Expr elseEnvelopeExpr) {
    Expr ifCall;
    ifCall.kind = Expr::Kind::Call;
    ifCall.name = "if";
    ifCall.args.push_back(std::move(conditionExpr));
    ifCall.argNames.push_back(std::nullopt);
    ifCall.args.push_back(std::move(thenEnvelopeExpr));
    ifCall.argNames.push_back(std::nullopt);
    ifCall.args.push_back(std::move(elseEnvelopeExpr));
    ifCall.argNames.push_back(std::nullopt);
    return ifCall;
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
  auto makeFieldComparisonExpr = [&](const std::string &comparisonName,
                                     const std::string &leftReceiverName,
                                     const std::string &rightReceiverName,
                                     const std::string &fieldName) {
    Expr compare;
    compare.kind = Expr::Kind::Call;
    compare.name = comparisonName;
    compare.args.push_back(makeFieldAccessExpr(leftReceiverName, fieldName));
    compare.argNames.push_back(std::nullopt);
    compare.args.push_back(makeFieldAccessExpr(rightReceiverName, fieldName));
    compare.argNames.push_back(std::nullopt);
    return compare;
  };
  auto makeBinaryBoolExpr = [](const std::string &operatorName, Expr left, Expr right) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = operatorName;
    call.args.push_back(std::move(left));
    call.argNames.push_back(std::nullopt);
    call.args.push_back(std::move(right));
    call.argNames.push_back(std::nullopt);
    return call;
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

    auto emitComparisonHelper = [&](const std::string &helperName,
                                    const std::string &comparisonName,
                                    const std::string &foldOperatorName,
                                    bool emptyValue) -> bool {
      const std::string helperPath = def.fullPath + "/" + helperName;
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = helperName;
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("bool");
      helper.transforms.push_back(std::move(returnTransform));

      helper.parameters.push_back(makeTypeBinding("left", def.fullPath, helper.namespacePrefix));
      helper.parameters.push_back(makeTypeBinding("right", def.fullPath, helper.namespacePrefix));

      if (fieldNames.empty()) {
        Expr emptyLiteral;
        emptyLiteral.kind = Expr::Kind::BoolLiteral;
        emptyLiteral.boolValue = emptyValue;
        helper.returnExpr = std::move(emptyLiteral);
      } else {
        Expr combined = makeFieldComparisonExpr(comparisonName, "left", "right", fieldNames.front());
        for (size_t index = 1; index < fieldNames.size(); ++index) {
          combined = makeBinaryBoolExpr(foldOperatorName,
                                        std::move(combined),
                                        makeFieldComparisonExpr(comparisonName, "left", "right", fieldNames[index]));
        }
        helper.returnExpr = std::move(combined);
      }
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
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
    auto emitCompareHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Compare";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Compare";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("i32");
      helper.transforms.push_back(std::move(returnTransform));

      helper.parameters.push_back(makeTypeBinding("left", def.fullPath, helper.namespacePrefix));
      helper.parameters.push_back(makeTypeBinding("right", def.fullPath, helper.namespacePrefix));

      for (const auto &fieldName : fieldNames) {
        Expr lessThanExpr = makeFieldComparisonExpr("less_than", "left", "right", fieldName);
        Expr lessThanResultExpr =
            makeBinaryCallExpr("minus", makeI32LiteralExpr(0), makeI32LiteralExpr(1));
        std::vector<Expr> lessThenBody;
        lessThenBody.push_back(makeReturnStatementExpr(std::move(lessThanResultExpr)));
        helper.statements.push_back(makeIfStatementExpr(
            std::move(lessThanExpr),
            makeEnvelopeExpr("then", std::move(lessThenBody)),
            makeEnvelopeExpr("else", {})));

        Expr greaterThanExpr = makeFieldComparisonExpr("greater_than", "left", "right", fieldName);
        std::vector<Expr> greaterThenBody;
        greaterThenBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(1)));
        helper.statements.push_back(makeIfStatementExpr(
            std::move(greaterThanExpr),
            makeEnvelopeExpr("then", std::move(greaterThenBody)),
            makeEnvelopeExpr("else", {})));
      }

      helper.returnExpr = makeI32LiteralExpr(0);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitHash64Helper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Hash64";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Hash64";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("u64");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      Expr combined = makeU64LiteralExpr(1469598103934665603ULL);
      for (const auto &fieldName : fieldNames) {
        Expr convertFieldExpr;
        convertFieldExpr.kind = Expr::Kind::Call;
        convertFieldExpr.name = "convert";
        convertFieldExpr.templateArgs.push_back("u64");
        convertFieldExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
        convertFieldExpr.argNames.push_back(std::nullopt);

        combined = makeBinaryCallExpr(
            "plus",
            makeBinaryCallExpr("multiply", std::move(combined), makeU64LiteralExpr(1099511628211ULL)),
            std::move(convertFieldExpr));
      }
      helper.returnExpr = std::move(combined);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitClearHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Clear";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Clear";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("void");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix, true));

      if (!fieldNames.empty()) {
        Expr defaultCall;
        defaultCall.kind = Expr::Kind::Call;
        defaultCall.name = def.fullPath;

        Expr defaultValueBinding = makeTypeBinding("defaultValue", def.fullPath, helper.namespacePrefix);
        defaultValueBinding.args.push_back(std::move(defaultCall));
        defaultValueBinding.argNames.push_back(std::nullopt);
        helper.statements.push_back(std::move(defaultValueBinding));

        for (const auto &fieldName : fieldNames) {
          Expr assignExpr;
          assignExpr.kind = Expr::Kind::Call;
          assignExpr.name = "assign";
          assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
          assignExpr.argNames.push_back(std::nullopt);
          assignExpr.args.push_back(makeFieldAccessExpr("defaultValue", fieldName));
          assignExpr.argNames.push_back(std::nullopt);
          helper.statements.push_back(std::move(assignExpr));
        }
      }

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitCopyFromHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/CopyFrom";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "CopyFrom";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("void");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix, true));
      helper.parameters.push_back(makeTypeBinding("other", def.fullPath, helper.namespacePrefix));

      for (const auto &fieldName : fieldNames) {
        Expr assignExpr;
        assignExpr.kind = Expr::Kind::Call;
        assignExpr.name = "assign";
        assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
        assignExpr.argNames.push_back(std::nullopt);
        assignExpr.args.push_back(makeFieldAccessExpr("other", fieldName));
        assignExpr.argNames.push_back(std::nullopt);
        helper.statements.push_back(std::move(assignExpr));
      }

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitValidateHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Validate";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      std::vector<std::string> fieldHookPaths;
      fieldHookPaths.reserve(fieldNames.size());
      for (const auto &fieldName : fieldNames) {
        const std::string hookPath = def.fullPath + "/ValidateField_" + fieldName;
        if (definitionPaths.count(hookPath) > 0) {
          error = "generated reflection helper already exists: " + hookPath;
          return false;
        }
        fieldHookPaths.push_back(hookPath);
      }

      for (size_t fieldIndex = 0; fieldIndex < fieldNames.size(); ++fieldIndex) {
        Definition hook;
        hook.name = "ValidateField_" + fieldNames[fieldIndex];
        hook.fullPath = fieldHookPaths[fieldIndex];
        hook.namespacePrefix = def.fullPath;
        hook.sourceLine = def.sourceLine;
        hook.sourceColumn = def.sourceColumn;

        appendPublicVisibility(hook);
        Transform returnTransform;
        returnTransform.name = "return";
        returnTransform.templateArgs.push_back("bool");
        hook.transforms.push_back(std::move(returnTransform));
        hook.parameters.push_back(makeTypeBinding("value", def.fullPath, hook.namespacePrefix));
        hook.returnExpr = makeBoolLiteralExpr(true);
        hook.hasReturnStatement = true;

        rewrittenDefinitions.push_back(std::move(hook));
        definitionPaths.insert(fieldHookPaths[fieldIndex]);
      }

      Definition helper;
      helper.name = "Validate";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("Result<FileError>");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      for (size_t fieldIndex = 0; fieldIndex < fieldHookPaths.size(); ++fieldIndex) {
        Expr checkCall;
        checkCall.kind = Expr::Kind::Call;
        checkCall.name = fieldHookPaths[fieldIndex];
        checkCall.args.push_back(makeNameExpr("value"));
        checkCall.argNames.push_back(std::nullopt);

        Expr conditionExpr;
        conditionExpr.kind = Expr::Kind::Call;
        conditionExpr.name = "not";
        conditionExpr.args.push_back(std::move(checkCall));
        conditionExpr.argNames.push_back(std::nullopt);

        std::vector<Expr> thenBody;
        thenBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(fieldIndex + 1)));
        helper.statements.push_back(makeIfStatementExpr(
            std::move(conditionExpr),
            makeEnvelopeExpr("then", std::move(thenBody)),
            makeEnvelopeExpr("else", {})));
      }

      Expr okResultTypeExpr;
      okResultTypeExpr.kind = Expr::Kind::Name;
      okResultTypeExpr.name = "Result";

      Expr okResultCall;
      okResultCall.kind = Expr::Kind::Call;
      okResultCall.isMethodCall = true;
      okResultCall.name = "ok";
      okResultCall.args.push_back(std::move(okResultTypeExpr));
      okResultCall.argNames.push_back(std::nullopt);
      helper.returnExpr = std::move(okResultCall);
      helper.hasReturnStatement = true;

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
    auto emitDefaultHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Default";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Default";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back(def.fullPath);
      helper.transforms.push_back(std::move(returnTransform));

      Expr defaultCall;
      defaultCall.kind = Expr::Kind::Call;
      defaultCall.name = def.fullPath;
      helper.returnExpr = std::move(defaultCall);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitIsDefaultHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/IsDefault";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "IsDefault";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("bool");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      if (fieldNames.empty()) {
        Expr alwaysTrue;
        alwaysTrue.kind = Expr::Kind::BoolLiteral;
        alwaysTrue.boolValue = true;
        helper.returnExpr = std::move(alwaysTrue);
      } else {
        Expr defaultCall;
        defaultCall.kind = Expr::Kind::Call;
        defaultCall.name = def.fullPath;

        Expr defaultValueBinding = makeTypeBinding("defaultValue", def.fullPath, helper.namespacePrefix);
        defaultValueBinding.args.push_back(std::move(defaultCall));
        defaultValueBinding.argNames.push_back(std::nullopt);
        helper.statements.push_back(std::move(defaultValueBinding));

        Expr combined = makeFieldComparisonExpr("equal", "value", "defaultValue", fieldNames.front());
        for (size_t index = 1; index < fieldNames.size(); ++index) {
          Expr compare = makeFieldComparisonExpr("equal", "value", "defaultValue", fieldNames[index]);
          combined = makeBinaryBoolExpr("and", std::move(combined), std::move(compare));
        }
        helper.returnExpr = std::move(combined);
      }
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };

    if (shouldGenerateEqual) {
      if (!emitComparisonHelper("Equal", "equal", "and", true)) {
        return false;
      }
    }
    if (shouldGenerateNotEqual) {
      if (!emitComparisonHelper("NotEqual", "not_equal", "or", false)) {
        return false;
      }
    }
    if (shouldGenerateCompare) {
      if (!emitCompareHelper()) {
        return false;
      }
    }
    if (shouldGenerateHash64) {
      if (!emitHash64Helper()) {
        return false;
      }
    }
    if (shouldGenerateClear) {
      if (!emitClearHelper()) {
        return false;
      }
    }
    if (shouldGenerateCopyFrom) {
      if (!emitCopyFromHelper()) {
        return false;
      }
    }
    if (shouldGenerateValidate) {
      if (!emitValidateHelper()) {
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
      if (!emitDefaultHelper()) {
        return false;
      }
    }
    if (shouldGenerateIsDefault) {
      if (!emitIsDefaultHelper()) {
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
