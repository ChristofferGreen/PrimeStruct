#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer struct return helpers reject canonical fallback from alias defs without returns") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/Entry",
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

  primec::Definition aliasAt;
  aliasAt.fullPath = "/vector/at";
  aliasAt.namespacePrefix = "/vector";

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/vector/at";
  canonicalAt.namespacePrefix = "/std/collections/vector";
  primec::Transform returnEntry;
  returnEntry.name = "return";
  returnEntry.templateArgs = {"Entry"};
  canonicalAt.transforms.push_back(returnEntry);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAt.fullPath, &aliasAt},
      {canonicalAt.fullPath, &canonicalAt},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  primec::Expr aliasAtCall;
  aliasAtCall.kind = primec::Expr::Kind::Call;
  aliasAtCall.name = "/vector/at";
  aliasAtCall.args = {valuesName, indexLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(aliasAtCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct return helpers keep empty result when alias candidates have no struct returns") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/Entry",
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

  primec::Definition aliasAt;
  aliasAt.fullPath = "/vector/at";
  aliasAt.namespacePrefix = "/vector";

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/vector/at";
  canonicalAt.namespacePrefix = "/std/collections/vector";
  primec::Transform returnInt;
  returnInt.name = "return";
  returnInt.templateArgs = {"i32"};
  canonicalAt.transforms.push_back(returnInt);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAt.fullPath, &aliasAt},
      {canonicalAt.fullPath, &canonicalAt},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  primec::Expr aliasAtCall;
  aliasAtCall.kind = primec::Expr::Kind::Call;
  aliasAtCall.name = "/vector/at";
  aliasAtCall.args = {valuesName, indexLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(aliasAtCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct return helpers reject canonical vector access call forwarding") {
  const std::unordered_set<std::string> structNames = {
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

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/vector/at";
  canonicalAt.namespacePrefix = "/std/collections/vector";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAt.transforms.push_back(returnMarker);

  primec::Definition canonicalAtUnsafe;
  canonicalAtUnsafe.fullPath = "/std/collections/vector/at_unsafe";
  canonicalAtUnsafe.namespacePrefix = "/std/collections/vector";
  canonicalAtUnsafe.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAt.fullPath, &canonicalAt},
      {canonicalAtUnsafe.fullPath, &canonicalAtUnsafe},
  };
  std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;
  knownFields["values"].typeName = "vector";
  knownFields["values"].typeTemplateArg = "i32";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 2;

  primec::Expr canonicalAtCall;
  canonicalAtCall.kind = primec::Expr::Kind::Call;
  canonicalAtCall.name = "/std/collections/vector/at";
  canonicalAtCall.args = {valuesName, indexLiteral};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(canonicalAtCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());

  primec::Expr canonicalAtUnsafeCall;
  canonicalAtUnsafeCall.kind = primec::Expr::Kind::Call;
  canonicalAtUnsafeCall.name = "/std/collections/vector/at_unsafe";
  canonicalAtUnsafeCall.args = {valuesName, indexLiteral};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(canonicalAtUnsafeCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct return helpers keep vector method alias precedence") {
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

  primec::Definition aliasAt;
  aliasAt.fullPath = "/vector/at";
  aliasAt.namespacePrefix = "/vector";
  primec::Transform returnAliasMarker;
  returnAliasMarker.name = "return";
  returnAliasMarker.templateArgs = {"AliasMarker"};
  aliasAt.transforms.push_back(returnAliasMarker);

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/vector/at";
  canonicalAt.namespacePrefix = "/std/collections/vector";
  primec::Transform returnCanonicalMarker;
  returnCanonicalMarker.name = "return";
  returnCanonicalMarker.templateArgs = {"CanonicalMarker"};
  canonicalAt.transforms.push_back(returnCanonicalMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAt.fullPath, &aliasAt},
      {canonicalAt.fullPath, &canonicalAt},
  };
  std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;
  knownFields["values"].typeName = "vector";
  knownFields["values"].typeTemplateArg = "i32";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 2;

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "at";
  methodCall.args = {valuesName, indexLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/AliasMarker");
}

TEST_CASE("ir lowerer struct return helpers reject canonical vector method forwarding") {
  const std::unordered_set<std::string> structNames = {
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

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/vector/at";
  canonicalAt.namespacePrefix = "/std/collections/vector";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAt.transforms.push_back(returnMarker);

  primec::Definition canonicalAtUnsafe;
  canonicalAtUnsafe.fullPath = "/std/collections/vector/at_unsafe";
  canonicalAtUnsafe.namespacePrefix = "/std/collections/vector";
  canonicalAtUnsafe.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAt.fullPath, &canonicalAt},
      {canonicalAtUnsafe.fullPath, &canonicalAtUnsafe},
  };
  std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;
  knownFields["values"].typeName = "vector";
  knownFields["values"].typeTemplateArg = "i32";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 2;

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "at";
  methodCall.args = {valuesName, indexLiteral};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());

  methodCall.name = "at_unsafe";
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct return helpers keep bare map access canonical forwarding") {
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

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/map/at";
  canonicalAt.namespacePrefix = "/std/collections/map";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAt.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAt.fullPath, &canonicalAt},
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
  methodCall.name = "at";
  methodCall.args = {valuesName, keyLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/Marker");
}

TEST_CASE("ir lowerer struct return helpers keep canonical slash-path map access forwarding") {
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

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/map/at";
  canonicalAt.namespacePrefix = "/std/collections/map";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAt.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAt.fullPath, &canonicalAt},
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
  methodCall.name = "/std/collections/map/at";
  methodCall.args = {valuesName, keyLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/Marker");
}

TEST_CASE("ir lowerer struct return helpers keep canonical map access call forwarding") {
  const std::unordered_set<std::string> structNames = {
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

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/map/at";
  canonicalAt.namespacePrefix = "/std/collections/map";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAt.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAt.fullPath, &canonicalAt},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 2;

  primec::Expr canonicalAtCall;
  canonicalAtCall.kind = primec::Expr::Kind::Call;
  canonicalAtCall.name = "/std/collections/map/at";
  canonicalAtCall.args = {valuesName, keyLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(canonicalAtCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/Marker");
}

TEST_CASE("ir lowerer struct return helpers reject map access compatibility call forwarding") {
  const std::unordered_set<std::string> structNames = {
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

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/map/at";
  canonicalAt.namespacePrefix = "/std/collections/map";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAt.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAt.fullPath, &canonicalAt},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 2;

  primec::Expr aliasAtCall;
  aliasAtCall.kind = primec::Expr::Kind::Call;
  aliasAtCall.name = "/map/at";
  aliasAtCall.args = {valuesName, keyLiteral};

  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(aliasAtCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct return helpers reject map tryAt compatibility call forwarding") {
  const std::unordered_set<std::string> structNames = {
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
                                                          defMap).empty());
}


TEST_SUITE_END();
