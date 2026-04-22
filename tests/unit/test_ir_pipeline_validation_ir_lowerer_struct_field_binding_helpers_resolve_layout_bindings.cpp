#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer struct field binding helpers resolve layout bindings") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
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

  primec::Definition holderDef;
  holderDef.fullPath = "/pkg/Holder";
  holderDef.namespacePrefix = "/pkg";

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeA.fullPath, &makeA},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields = {
      {"existing", {"i64", ""}},
  };

  primec::Expr literalValue;
  literalValue.kind = primec::Expr::Kind::Literal;
  literalValue.intWidth = 64;

  primec::Expr explicitField;
  explicitField.kind = primec::Expr::Kind::Call;
  explicitField.isBinding = true;
  explicitField.name = "typed";
  primec::Transform explicitType;
  explicitType.name = "i32";
  explicitField.transforms = {explicitType};
  primec::ir_lowerer::LayoutFieldBinding binding;
  std::string error;
  CHECK(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                      explicitField,
                                                      knownFields,
                                                      structNames,
                                                      resolveStructTypePath,
                                                      resolveStructLayoutExprPath,
                                                      defMap,
                                                      binding,
                                                      error));
  CHECK(binding.typeName == "i32");
  CHECK(error.empty());

  primec::Expr primitiveField;
  primitiveField.kind = primec::Expr::Kind::Call;
  primitiveField.isBinding = true;
  primitiveField.name = "count";
  primitiveField.args = {literalValue};
  binding = {};
  error.clear();
  CHECK(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                      primitiveField,
                                                      knownFields,
                                                      structNames,
                                                      resolveStructTypePath,
                                                      resolveStructLayoutExprPath,
                                                      defMap,
                                                      binding,
                                                      error));
  CHECK(binding.typeName == "i64");
  CHECK(error.empty());

  primec::Expr makeCall;
  makeCall.kind = primec::Expr::Kind::Call;
  makeCall.name = "makeA";

  primec::Expr structField;
  structField.kind = primec::Expr::Kind::Call;
  structField.isBinding = true;
  structField.name = "child";
  structField.args = {makeCall};
  binding = {};
  error.clear();
  CHECK(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                      structField,
                                                      knownFields,
                                                      structNames,
                                                      resolveStructTypePath,
                                                      resolveStructLayoutExprPath,
                                                      defMap,
                                                      binding,
                                                      error));
  CHECK(binding.typeName == "/pkg/A");
  CHECK(error.empty());

  primec::Expr missingArgField;
  missingArgField.kind = primec::Expr::Kind::Call;
  missingArgField.isBinding = true;
  missingArgField.name = "badCount";
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                            missingArgField,
                                                            knownFields,
                                                            structNames,
                                                            resolveStructTypePath,
                                                            resolveStructLayoutExprPath,
                                                            defMap,
                                                            binding,
                                                            error));
  CHECK(error == "omitted struct field envelope requires exactly one initializer: /pkg/Holder/badCount");

  primec::Expr unresolvedName;
  unresolvedName.kind = primec::Expr::Kind::Name;
  unresolvedName.name = "missing";

  primec::Expr unresolvedField;
  unresolvedField.kind = primec::Expr::Kind::Call;
  unresolvedField.isBinding = true;
  unresolvedField.name = "badType";
  unresolvedField.args = {unresolvedName};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                            unresolvedField,
                                                            knownFields,
                                                            structNames,
                                                            resolveStructTypePath,
                                                            resolveStructLayoutExprPath,
                                                            defMap,
                                                            binding,
                                                            error));
  CHECK(error == "unresolved or ambiguous omitted struct field envelope: /pkg/Holder/badType");
}

TEST_CASE("ir lowerer struct field binding helpers collect struct layout bindings") {
  primec::Program program;

  primec::Definition structA;
  structA.fullPath = "/pkg/A";
  structA.namespacePrefix = "/pkg";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structA.transforms = {structTransform};

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  primec::Definition structS;
  structS.fullPath = "/pkg/S";
  structS.namespacePrefix = "/pkg";
  structS.transforms = {structTransform};

  primec::Expr typedField;
  typedField.kind = primec::Expr::Kind::Call;
  typedField.isBinding = true;
  typedField.name = "typed";
  primec::Transform typeI32;
  typeI32.name = "i32";
  typedField.transforms = {typeI32};

  primec::Expr typedRef;
  typedRef.kind = primec::Expr::Kind::Name;
  typedRef.name = "typed";

  primec::Expr copiedField;
  copiedField.kind = primec::Expr::Kind::Call;
  copiedField.isBinding = true;
  copiedField.name = "copied";
  copiedField.args = {typedRef};

  primec::Expr makeCall;
  makeCall.kind = primec::Expr::Kind::Call;
  makeCall.name = "makeA";

  primec::Expr childField;
  childField.kind = primec::Expr::Kind::Call;
  childField.isBinding = true;
  childField.name = "child";
  childField.args = {makeCall};

  structS.statements = {typedField, copiedField, childField};

  program.definitions = {structA, makeA, structS};

  std::unordered_map<std::string, const primec::Definition *> defMap;
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
      "/pkg/S",
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

  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> fieldsByStruct;
  std::string error;
  CHECK(primec::ir_lowerer::collectStructLayoutFieldBindings(program,
                                                             structNames,
                                                             resolveStructTypePath,
                                                             resolveStructLayoutExprPath,
                                                             defMap,
                                                             fieldsByStruct,
                                                             error));
  CHECK(error.empty());
  REQUIRE(fieldsByStruct.count("/pkg/S") == 1u);
  REQUIRE(fieldsByStruct.count("/pkg/A") == 1u);
  REQUIRE(fieldsByStruct["/pkg/S"].size() == 3u);
  CHECK(fieldsByStruct["/pkg/S"][0].typeName == "i32");
  CHECK(fieldsByStruct["/pkg/S"][1].typeName == "i32");
  CHECK(fieldsByStruct["/pkg/S"][2].typeName == "/pkg/A");
  CHECK(fieldsByStruct["/pkg/A"].empty());

  primec::Program badProgram;
  primec::Definition badStruct;
  badStruct.fullPath = "/pkg/Bad";
  badStruct.namespacePrefix = "/pkg";
  badStruct.transforms = {structTransform};
  primec::Expr badField;
  badField.kind = primec::Expr::Kind::Call;
  badField.isBinding = true;
  badField.name = "bad";
  badStruct.statements = {badField};
  badProgram.definitions = {badStruct};

  std::unordered_map<std::string, const primec::Definition *> badDefMap = {
      {badStruct.fullPath, &badProgram.definitions[0]},
  };
  const std::unordered_set<std::string> badStructNames = {
      "/pkg/Bad",
  };
  const auto badResolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, badStructNames, importAliases);
  };
  fieldsByStruct.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::collectStructLayoutFieldBindings(badProgram,
                                                                   badStructNames,
                                                                   badResolveStructTypePath,
                                                                   resolveStructLayoutExprPath,
                                                                   badDefMap,
                                                                   fieldsByStruct,
                                                                   error));
  CHECK(error == "omitted struct field envelope requires exactly one initializer: /pkg/Bad/bad");
}

TEST_CASE("ir lowerer struct field binding helpers collect layout bindings from program context") {
  primec::Program program;

  primec::Definition structA;
  structA.fullPath = "/pkg/A";
  structA.namespacePrefix = "/pkg";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structA.transforms = {structTransform};

  primec::Definition structS;
  structS.fullPath = "/pkg/S";
  structS.namespacePrefix = "/pkg";
  structS.transforms = {structTransform};

  primec::Expr typedField;
  typedField.kind = primec::Expr::Kind::Call;
  typedField.isBinding = true;
  typedField.name = "typed";
  primec::Transform typeI32;
  typeI32.name = "i32";
  typedField.transforms = {typeI32};

  primec::Expr childCtor;
  childCtor.kind = primec::Expr::Kind::Call;
  childCtor.name = "/pkg/A";
  primec::Expr childField;
  childField.kind = primec::Expr::Kind::Call;
  childField.isBinding = true;
  childField.name = "child";
  childField.args = {childCtor};

  structS.statements = {typedField, childField};
  program.definitions = {structA, structS};

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  primec::ir_lowerer::buildDefinitionMapAndStructNames(program.definitions, defMap, structNames);
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };

  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> fieldsByStruct;
  std::string error;
  CHECK(primec::ir_lowerer::collectStructLayoutFieldBindingsFromProgramContext(
      program, structNames, resolveStructTypePath, defMap, importAliases, fieldsByStruct, error));
  CHECK(error.empty());
  REQUIRE(fieldsByStruct.count("/pkg/S") == 1u);
  REQUIRE(fieldsByStruct["/pkg/S"].size() == 2u);
  CHECK(fieldsByStruct["/pkg/S"][0].typeName == "i32");
  CHECK(fieldsByStruct["/pkg/S"][1].typeName == "/pkg/A");

  primec::Program badProgram;
  primec::Definition badStruct;
  badStruct.fullPath = "/pkg/Bad";
  badStruct.namespacePrefix = "/pkg";
  badStruct.transforms = {structTransform};
  primec::Expr badField;
  badField.kind = primec::Expr::Kind::Call;
  badField.isBinding = true;
  badField.name = "bad";
  badStruct.statements = {badField};
  badProgram.definitions = {badStruct};

  std::unordered_map<std::string, const primec::Definition *> badDefMap;
  std::unordered_set<std::string> badStructNames;
  primec::ir_lowerer::buildDefinitionMapAndStructNames(badProgram.definitions, badDefMap, badStructNames);
  const auto badResolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, badStructNames, importAliases);
  };
  fieldsByStruct.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::collectStructLayoutFieldBindingsFromProgramContext(
      badProgram, badStructNames, badResolveStructTypePath, badDefMap, importAliases, fieldsByStruct, error));
  CHECK(error == "omitted struct field envelope requires exactly one initializer: /pkg/Bad/bad");
}

TEST_CASE("ir lowerer struct return path helpers infer from definitions") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
      "/pkg/B",
      "/std/collections/experimental_soa_vector/SoaVector__Particle",
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

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  primec::Definition makeB;
  makeB.fullPath = "/pkg/makeB";
  makeB.namespacePrefix = "/pkg";
  primec::Transform returnB;
  returnB.name = "return";
  returnB.templateArgs = {"B"};
  makeB.transforms.push_back(returnB);

  primec::Definition mixed;
  mixed.fullPath = "/pkg/mixed";
  mixed.namespacePrefix = "/pkg";

  primec::Expr callA;
  callA.kind = primec::Expr::Kind::Call;
  callA.name = "makeA";

  primec::Expr callB = callA;
  callB.name = "makeB";

  primec::Expr retA;
  retA.kind = primec::Expr::Kind::Call;
  retA.name = "return";
  retA.args = {callA};

  primec::Expr retB = retA;
  retB.args = {callB};
  mixed.statements = {retA, retB};

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeA.fullPath, &makeA},
      {makeB.fullPath, &makeB},
      {mixed.fullPath, &mixed},
  };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(
            "/pkg/makeA", structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap) == "/pkg/A");
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(
            "/pkg/mixed", structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap).empty());

  primec::Definition makeSoa;
  makeSoa.fullPath = "/pkg/makeSoa";
  makeSoa.namespacePrefix = "/pkg";
  primec::Transform soaReturn;
  soaReturn.name = "return";
  soaReturn.templateArgs = {"Reference<SoaVector<Particle>>"};
  makeSoa.transforms = {soaReturn};
  defMap.emplace(makeSoa.fullPath, &makeSoa);

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(
            "/pkg/makeSoa", structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap) ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");
}

TEST_CASE("ir lowerer struct return path helpers infer from expressions") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
      "/pkg/B",
      "/pkg/Receiver",
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

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  primec::Definition makeB;
  makeB.fullPath = "/pkg/makeB";
  makeB.namespacePrefix = "/pkg";
  primec::Transform returnB;
  returnB.name = "return";
  returnB.templateArgs = {"B"};
  makeB.transforms.push_back(returnB);

  primec::Definition receiverMethod;
  receiverMethod.fullPath = "/pkg/Receiver/makeA";
  receiverMethod.namespacePrefix = "/pkg/Receiver";
  receiverMethod.transforms.push_back(returnA);

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeA.fullPath, &makeA},
      {makeB.fullPath, &makeB},
      {receiverMethod.fullPath, &receiverMethod},
  };

  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields = {
      {"recv", {"/pkg/Receiver", ""}},
  };

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "recv";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "makeA";
  methodCall.args = {receiverName};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/A");

  primec::Expr callA;
  callA.kind = primec::Expr::Kind::Call;
  callA.name = "makeA";
  primec::Expr callB = callA;
  callB.name = "makeB";

  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr ifExpr;
  ifExpr.kind = primec::Expr::Kind::Call;
  ifExpr.name = "if";
  ifExpr.args = {cond, callA, callA};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(ifExpr,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/A");

  ifExpr.args = {cond, callA, callB};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(ifExpr,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct return helpers reject vector alias canonical forwarding") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/Entry",
      "/pkg/Tagged",
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

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/vector/at";
  canonicalAt.namespacePrefix = "/std/collections/vector";
  primec::Transform returnEntry;
  returnEntry.name = "return";
  returnEntry.templateArgs = {"Entry"};
  canonicalAt.transforms.push_back(returnEntry);

  primec::Definition entryTag;
  entryTag.fullPath = "/pkg/Entry/tag";
  entryTag.namespacePrefix = "/pkg/Entry";
  primec::Transform returnTagged;
  returnTagged.name = "return";
  returnTagged.templateArgs = {"Tagged"};
  entryTag.transforms.push_back(returnTagged);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAt.fullPath, &canonicalAt},
      {entryTag.fullPath, &entryTag},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  primec::Expr aliasAt;
  aliasAt.kind = primec::Expr::Kind::Call;
  aliasAt.name = "/vector/at";
  aliasAt.args = {valuesName, indexLiteral};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(aliasAt,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "/vector/tag";
  methodCall.args = {aliasAt};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());

  methodCall.name = "/std/collections/vector/missing";
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}


TEST_SUITE_END();
