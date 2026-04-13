#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer struct return helpers keep map tryAt alias precedence") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/AliasMarker",
      "/pkg/CanonicalMarker",
  };
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };
  const auto resolveStructLayoutExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return std::string("/pkg/") + expr.name;
  };

  primec::Definition aliasTryAt;
  aliasTryAt.fullPath = "/map/tryAt";
  aliasTryAt.namespacePrefix = "/map";
  primec::Transform returnAliasMarker;
  returnAliasMarker.name = "return";
  returnAliasMarker.templateArgs = {"AliasMarker"};
  aliasTryAt.transforms.push_back(returnAliasMarker);

  primec::Definition canonicalTryAt;
  canonicalTryAt.fullPath = "/std/collections/map/tryAt";
  canonicalTryAt.namespacePrefix = "/std/collections/map";
  primec::Transform returnCanonicalMarker;
  returnCanonicalMarker.name = "return";
  returnCanonicalMarker.templateArgs = {"CanonicalMarker"};
  canonicalTryAt.transforms.push_back(returnCanonicalMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasTryAt.fullPath, &aliasTryAt},
      {canonicalTryAt.fullPath, &canonicalTryAt},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 2;

  primec::Expr aliasTryAtCall;
  aliasTryAtCall.kind = primec::Expr::Kind::Call;
  aliasTryAtCall.name = "/map/tryAt";
  aliasTryAtCall.args = {valuesName, keyLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(aliasTryAtCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/AliasMarker");
}

TEST_CASE("ir lowerer struct return helpers reject explicit map tryAt method canonical forwarding") {
  const std::unordered_set<std::string> structNames = {
      "/map",
      "/pkg/Marker",
  };
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };
  const auto resolveStructLayoutExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return std::string("/pkg/") + expr.name;
  };

  primec::Definition canonicalTryAt;
  canonicalTryAt.fullPath = "/std/collections/map/tryAt";
  canonicalTryAt.namespacePrefix = "/std/collections/map";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalTryAt.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalTryAt.fullPath, &canonicalTryAt},
  };
  std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;
  knownFields["values"].typeName = "map";
  knownFields["values"].typeTemplateArg = "i32, i32";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 2;

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "/map/tryAt";
  methodCall.args = {valuesName, keyLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct return helpers keep explicit map tryAt method alias precedence") {
  const std::unordered_set<std::string> structNames = {
      "/map",
      "/pkg/AliasMarker",
      "/pkg/CanonicalMarker",
  };
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };
  const auto resolveStructLayoutExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return std::string("/pkg/") + expr.name;
  };

  primec::Definition aliasTryAt;
  aliasTryAt.fullPath = "/map/tryAt";
  aliasTryAt.namespacePrefix = "/map";
  primec::Transform returnAliasMarker;
  returnAliasMarker.name = "return";
  returnAliasMarker.templateArgs = {"AliasMarker"};
  aliasTryAt.transforms.push_back(returnAliasMarker);

  primec::Definition canonicalTryAt;
  canonicalTryAt.fullPath = "/std/collections/map/tryAt";
  canonicalTryAt.namespacePrefix = "/std/collections/map";
  primec::Transform returnCanonicalMarker;
  returnCanonicalMarker.name = "return";
  returnCanonicalMarker.templateArgs = {"CanonicalMarker"};
  canonicalTryAt.transforms.push_back(returnCanonicalMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasTryAt.fullPath, &aliasTryAt},
      {canonicalTryAt.fullPath, &canonicalTryAt},
  };
  std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;
  knownFields["values"].typeName = "map";
  knownFields["values"].typeTemplateArg = "i32, i32";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 2;

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "/map/tryAt";
  methodCall.args = {valuesName, keyLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/AliasMarker");
}

TEST_CASE("ir lowerer struct layout helpers parse and extract alignment transforms") {
  CHECK(primec::ir_lowerer::alignTo(7u, 4u) == 8u);
  CHECK(primec::ir_lowerer::alignTo(16u, 8u) == 16u);
  CHECK(primec::ir_lowerer::alignTo(13u, 0u) == 13u);

  int parsed = 0;
  CHECK(primec::ir_lowerer::parsePositiveInt("32i32", parsed));
  CHECK(parsed == 32);
  CHECK_FALSE(primec::ir_lowerer::parsePositiveInt("-1", parsed));
  CHECK_FALSE(primec::ir_lowerer::parsePositiveInt("0", parsed));

  std::vector<primec::Transform> transforms;
  primec::Transform alignKbytes;
  alignKbytes.name = "align_kbytes";
  alignKbytes.arguments = {"2i32"};
  transforms.push_back(alignKbytes);

  uint32_t alignment = 0;
  bool hasAlignment = false;
  std::string error;
  CHECK(primec::ir_lowerer::extractAlignment(
      transforms, "struct /pkg/S", alignment, hasAlignment, error));
  CHECK(error.empty());
  CHECK(hasAlignment);
  CHECK(alignment == 2048u);

  std::vector<primec::Transform> duplicateTransforms;
  primec::Transform alignBytes;
  alignBytes.name = "align_bytes";
  alignBytes.arguments = {"4"};
  duplicateTransforms.push_back(alignBytes);
  duplicateTransforms.push_back(alignKbytes);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::extractAlignment(
      duplicateTransforms, "field /pkg/S/value", alignment, hasAlignment, error));
  CHECK(error == "duplicate align_kbytes transform on field /pkg/S/value");

  std::vector<primec::Transform> invalidTransforms;
  primec::Transform invalidAlign;
  invalidAlign.name = "align_bytes";
  invalidAlign.arguments = {"0"};
  invalidTransforms.push_back(invalidAlign);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::extractAlignment(
      invalidTransforms, "field /pkg/S/value", alignment, hasAlignment, error));
  CHECK(error == "align_bytes requires a positive integer argument");
}

TEST_CASE("ir lowerer struct layout helpers classify binding type layout") {
  primec::ir_lowerer::BindingTypeLayout layout;
  std::string structTypeName;
  std::string error;

  primec::ir_lowerer::LayoutFieldBinding intAlias;
  intAlias.typeName = "int";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(intAlias, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 4u);
  CHECK(layout.alignmentBytes == 4u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding pointerType;
  pointerType.typeName = "Pointer";
  pointerType.typeTemplateArg = "i32";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(pointerType, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding uninitPrimitive;
  uninitPrimitive.typeName = "uninitialized";
  uninitPrimitive.typeTemplateArg = "i64";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(uninitPrimitive, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding uninitCollection;
  uninitCollection.typeName = "uninitialized";
  uninitCollection.typeTemplateArg = "array<f32>";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(uninitCollection, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding uninitStruct;
  uninitStruct.typeName = "uninitialized";
  uninitStruct.typeTemplateArg = "Thing";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(uninitStruct, layout, structTypeName, error));
  CHECK(structTypeName == "Thing");
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding canonicalMap;
  canonicalMap.typeName = "/std/collections/map";
  canonicalMap.typeTemplateArg = "i32, bool";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(canonicalMap, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding uninitCanonicalMap;
  uninitCanonicalMap.typeName = "uninitialized";
  uninitCanonicalMap.typeTemplateArg = "/std/collections/map<i32, bool>";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(uninitCanonicalMap, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding soaVectorType;
  soaVectorType.typeName = "soa_vector";
  soaVectorType.typeTemplateArg = "Particle";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(soaVectorType, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding missingTemplate;
  missingTemplate.typeName = "uninitialized";
  CHECK_FALSE(primec::ir_lowerer::classifyBindingTypeLayout(missingTemplate, layout, structTypeName, error));
  CHECK(error == "uninitialized requires a template argument for layout");

  primec::ir_lowerer::LayoutFieldBinding structType;
  structType.typeName = "MyStruct";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(structType, layout, structTypeName, error));
  CHECK(structTypeName == "MyStruct");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer struct layout helpers append struct fields") {
  primec::IrStructLayout layout;
  layout.name = "/pkg/S";

  uint32_t offset = 0;
  uint32_t structAlign = 1;
  int resolveCalls = 0;
  std::string error;

  const auto resolveTypeLayout = [&](const primec::ir_lowerer::LayoutFieldBinding &binding,
                                     primec::ir_lowerer::BindingTypeLayout &layoutOut,
                                     std::string &errorOut) {
    (void)errorOut;
    ++resolveCalls;
    if (binding.typeName == "i64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "i32") {
      layoutOut = {4u, 4u};
      return true;
    }
    return false;
  };

  primec::Expr staticField;
  staticField.kind = primec::Expr::Kind::Call;
  staticField.isBinding = true;
  staticField.name = "global";
  primec::Transform staticTransform;
  staticTransform.name = "static";
  primec::Transform privateTransform;
  privateTransform.name = "private";
  staticField.transforms = {privateTransform, staticTransform};

  const primec::ir_lowerer::LayoutFieldBinding staticBinding = {"i64", ""};
  CHECK(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", staticField, staticBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(resolveCalls == 0);
  REQUIRE(layout.fields.size() == 1u);
  CHECK(layout.fields[0].name == "global");
  CHECK(layout.fields[0].isStatic);
  CHECK(layout.fields[0].sizeBytes == 0u);
  CHECK(layout.fields[0].visibility == primec::IrStructVisibility::Private);

  primec::Expr valueField;
  valueField.kind = primec::Expr::Kind::Call;
  valueField.isBinding = true;
  valueField.name = "value";
  primec::Transform alignEight;
  alignEight.name = "align_bytes";
  alignEight.arguments = {"8"};
  valueField.transforms = {alignEight};

  const primec::ir_lowerer::LayoutFieldBinding valueBinding = {"i32", ""};
  CHECK(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", valueField, valueBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(resolveCalls == 1);
  REQUIRE(layout.fields.size() == 2u);
  CHECK(layout.fields[1].name == "value");
  CHECK_FALSE(layout.fields[1].isStatic);
  CHECK(layout.fields[1].offsetBytes == 0u);
  CHECK(layout.fields[1].sizeBytes == 4u);
  CHECK(layout.fields[1].alignmentBytes == 8u);
  CHECK(layout.fields[1].paddingKind == primec::IrStructPaddingKind::None);
  CHECK(offset == 4u);
  CHECK(structAlign == 8u);

  primec::Expr tailField;
  tailField.kind = primec::Expr::Kind::Call;
  tailField.isBinding = true;
  tailField.name = "tail";

  const primec::ir_lowerer::LayoutFieldBinding tailBinding = {"i64", ""};
  CHECK(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", tailField, tailBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(resolveCalls == 2);
  REQUIRE(layout.fields.size() == 3u);
  CHECK(layout.fields[2].offsetBytes == 8u);
  CHECK(layout.fields[2].paddingKind == primec::IrStructPaddingKind::Align);
  CHECK(offset == 16u);
  CHECK(structAlign == 8u);

  primec::Expr badField;
  badField.kind = primec::Expr::Kind::Call;
  badField.isBinding = true;
  badField.name = "bad";
  primec::Transform alignTwo;
  alignTwo.name = "align_bytes";
  alignTwo.arguments = {"2"};
  badField.transforms = {alignTwo};

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", badField, tailBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(error == "alignment requirement on field /pkg/S/bad is smaller than required alignment of 8");
}

TEST_CASE("ir lowerer struct layout helpers resolve binding type layout") {
  primec::Definition nested;
  nested.fullPath = "/pkg/Nested";
  nested.namespacePrefix = "/pkg";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {nested.fullPath, &nested},
  };
  const auto resolveStructTypePath = [](const std::string &typeName, const std::string &namespacePrefix) {
    return namespacePrefix + "/" + typeName;
  };

  int computeCalls = 0;
  const auto computeStructLayout = [&](const primec::Definition &def, primec::IrStructLayout &layout, std::string &error) {
    ++computeCalls;
    if (def.fullPath == "/pkg/Fail") {
      error = "layout failure";
      return false;
    }
    layout.name = def.fullPath;
    layout.totalSizeBytes = 24u;
    layout.alignmentBytes = 8u;
    return true;
  };

  primec::ir_lowerer::BindingTypeLayout layout;
  std::string error;

  const primec::ir_lowerer::LayoutFieldBinding primitive = {"i32", ""};
  CHECK(primec::ir_lowerer::resolveBindingTypeLayout(primitive,
                                                     "/pkg",
                                                     resolveStructTypePath,
                                                     defMap,
                                                     computeStructLayout,
                                                     layout,
                                                     error));
  CHECK(layout.sizeBytes == 4u);
  CHECK(layout.alignmentBytes == 4u);
  CHECK(computeCalls == 0);
  CHECK(error.empty());

  const primec::ir_lowerer::LayoutFieldBinding structBinding = {"Nested", ""};
  CHECK(primec::ir_lowerer::resolveBindingTypeLayout(structBinding,
                                                     "/pkg",
                                                     resolveStructTypePath,
                                                     defMap,
                                                     computeStructLayout,
                                                     layout,
                                                     error));
  CHECK(layout.sizeBytes == 24u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(computeCalls == 1);
  CHECK(error.empty());

  const primec::ir_lowerer::LayoutFieldBinding missingStruct = {"Missing", ""};
  CHECK_FALSE(primec::ir_lowerer::resolveBindingTypeLayout(missingStruct,
                                                           "/pkg",
                                                           resolveStructTypePath,
                                                           defMap,
                                                           computeStructLayout,
                                                           layout,
                                                           error));
  CHECK(error == "unknown struct type for layout: Missing");

  primec::Definition failing;
  failing.fullPath = "/pkg/Fail";
  failing.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> failingMap = {
      {failing.fullPath, &failing},
  };
  const primec::ir_lowerer::LayoutFieldBinding failingStruct = {"Fail", ""};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBindingTypeLayout(failingStruct,
                                                           "/pkg",
                                                           resolveStructTypePath,
                                                           failingMap,
                                                           computeStructLayout,
                                                           layout,
                                                           error));
  CHECK(error == "layout failure");
}

TEST_CASE("ir lowerer struct layout helpers compute with cache") {
  std::unordered_map<std::string, primec::IrStructLayout> layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::string error;
  primec::IrStructLayout out;

  int computeCalls = 0;
  const auto computeLayout = [&](primec::IrStructLayout &layout, std::string &computeError) {
    (void)computeError;
    ++computeCalls;
    layout.name = "/pkg/S";
    layout.totalSizeBytes = 16u;
    layout.alignmentBytes = 8u;
    return true;
  };

  CHECK(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/S", layoutCache, layoutStack, computeLayout, out, error));
  CHECK(computeCalls == 1);
  CHECK(layoutCache.count("/pkg/S") == 1u);
  CHECK(layoutStack.empty());
  CHECK(out.totalSizeBytes == 16u);
  CHECK(out.alignmentBytes == 8u);

  int cacheMissCalls = 0;
  const auto shouldNotRun = [&](primec::IrStructLayout &, std::string &) {
    ++cacheMissCalls;
    return false;
  };
  CHECK(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/S", layoutCache, layoutStack, shouldNotRun, out, error));
  CHECK(cacheMissCalls == 0);

  layoutStack.insert("/pkg/Recursive");
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/Recursive", layoutCache, layoutStack, computeLayout, out, error));
  CHECK(error == "recursive struct layout not supported: /pkg/Recursive");
  layoutStack.erase("/pkg/Recursive");

  const auto failingCompute = [&](primec::IrStructLayout &, std::string &computeError) {
    computeError = "layout failure";
    return false;
  };
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/Fail", layoutCache, layoutStack, failingCompute, out, error));
  CHECK(error == "layout failure");
  CHECK(layoutStack.count("/pkg/Fail") == 0u);

  const auto retryCompute = [&](primec::IrStructLayout &layout, std::string &) {
    layout.name = "/pkg/Fail";
    layout.totalSizeBytes = 4u;
    layout.alignmentBytes = 4u;
    return true;
  };
  CHECK(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/Fail", layoutCache, layoutStack, retryCompute, out, error));
  CHECK(out.totalSizeBytes == 4u);
  CHECK(out.alignmentBytes == 4u);
}

TEST_CASE("ir lowerer struct layout helpers compute uncached layout") {
  primec::Definition def;
  def.fullPath = "/pkg/S";
  def.namespacePrefix = "/pkg";
  primec::Transform structAlign;
  structAlign.name = "align_bytes";
  structAlign.arguments = {"16"};
  def.transforms = {structAlign};

  primec::Expr firstField;
  firstField.kind = primec::Expr::Kind::Name;
  firstField.isBinding = true;
  firstField.name = "value";

  primec::Expr staticField = firstField;
  staticField.name = "cached";
  primec::Transform staticTransform;
  staticTransform.name = "static";
  staticField.transforms = {staticTransform};
  def.statements = {firstField, staticField};

  const std::vector<primec::ir_lowerer::LayoutFieldBinding> fieldBindings = {
      {"i32", ""},
      {"UnknownStaticType", ""},
  };

  int resolveCalls = 0;
  const auto resolveFieldTypeLayout = [&](const primec::ir_lowerer::LayoutFieldBinding &binding,
                                          primec::ir_lowerer::BindingTypeLayout &layout,
                                          std::string &resolveError) {
    ++resolveCalls;
    if (binding.typeName == "i32") {
      layout = {4u, 4u};
      return true;
    }
    resolveError = "unexpected type layout request";
    return false;
  };

  primec::IrStructLayout layout;
  std::string error;
  CHECK(primec::ir_lowerer::computeStructLayoutUncached(
      def, fieldBindings, resolveFieldTypeLayout, layout, error));
  CHECK(error.empty());
  CHECK(resolveCalls == 1);
  CHECK(layout.name == "/pkg/S");
  CHECK(layout.alignmentBytes == 16u);
  CHECK(layout.totalSizeBytes == 16u);
  REQUIRE(layout.fields.size() == 2u);
  CHECK(layout.fields[0].name == "value");
  CHECK(layout.fields[0].offsetBytes == 0u);
  CHECK(layout.fields[0].sizeBytes == 4u);
  CHECK(layout.fields[0].alignmentBytes == 4u);
  CHECK_FALSE(layout.fields[0].isStatic);
  CHECK(layout.fields[1].name == "cached");
  CHECK(layout.fields[1].offsetBytes == 0u);
  CHECK(layout.fields[1].sizeBytes == 0u);
  CHECK(layout.fields[1].alignmentBytes == 1u);
  CHECK(layout.fields[1].isStatic);
}

TEST_SUITE_END();
