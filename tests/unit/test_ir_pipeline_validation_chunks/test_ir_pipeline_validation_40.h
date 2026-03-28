TEST_CASE("ir lowerer setup type helper rejects wrapper string slash-method access primitive fallback") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr wrapTextCall;
  wrapTextCall.kind = primec::Expr::Kind::Call;
  wrapTextCall.name = "wrapText";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.isMethodCall = true;
  receiverCall.name = "/std/collections/vector/at";
  receiverCall.args = {wrapTextCall, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapText") {
      return primec::ir_lowerer::LocalInfo::ValueKind::String;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapText") {
      return std::string("/wrapText");
    }
    return expr.name;
  };

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");

  receiverCall.name = "/vector/at_unsafe";
  methodCall.args = {receiverCall};
  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for explicit slash-method map access receivers") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";
  receiverCall.isMethodCall = true;
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  valuesLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
            {},
            {},
            false,
            kindOut,
            nullptr)) {
      return kindOut;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for wrapper-returned explicit slash-method map access") {
  primec::Definition wrapMapDef;
  wrapMapDef.fullPath = "/wrapMap";
  primec::Transform returnMap;
  returnMap.name = "return";
  returnMap.templateArgs = {"map<i32, i32>"};
  wrapMapDef.transforms.push_back(returnMap);

  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {wrapMapDef.fullPath, &wrapMapDef},
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr wrapMapCall;
  wrapMapCall.kind = primec::Expr::Kind::Call;
  wrapMapCall.name = "wrapMap";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";
  receiverCall.isMethodCall = true;
  receiverCall.args = {wrapMapCall, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapMap") {
      return std::string("/wrapMap");
    }
    return expr.name;
  };

  auto inferExprKind = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [&](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              std::string nestedError;
              return primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
                  candidate,
                  candidateLocals,
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  {},
                  {},
                  {},
                  resolveExprPath,
                  {},
                  defMap,
                  nestedError);
            },
            {},
            {},
            false,
            kindOut,
            nullptr)) {
      return kindOut;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for explicit map tryAt receivers") {
  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  canonicalTryAtDef.namespacePrefix = "/std/collections/map";
  primec::Transform returnResult;
  returnResult.name = "return";
  returnResult.templateArgs = {"/pkg/CanonicalResult"};
  canonicalTryAtDef.transforms.push_back(returnResult);

  primec::Definition resultTagDef;
  resultTagDef.fullPath = "/pkg/CanonicalResult/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
      {resultTagDef.fullPath, &resultTagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/map/tryAt";
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps explicit map tryAt receiver alias precedence") {
  primec::Definition aliasTryAtDef;
  aliasTryAtDef.fullPath = "/map/tryAt";
  aliasTryAtDef.namespacePrefix = "/map";
  primec::Transform returnAliasResult;
  returnAliasResult.name = "return";
  returnAliasResult.templateArgs = {"/pkg/AliasResult"};
  aliasTryAtDef.transforms.push_back(returnAliasResult);

  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  canonicalTryAtDef.namespacePrefix = "/std/collections/map";
  primec::Transform returnCanonicalResult;
  returnCanonicalResult.name = "return";
  returnCanonicalResult.templateArgs = {"/pkg/CanonicalResult"};
  canonicalTryAtDef.transforms.push_back(returnCanonicalResult);

  primec::Definition aliasTagDef;
  aliasTagDef.fullPath = "/pkg/AliasResult/tag";
  primec::Definition canonicalTagDef;
  canonicalTagDef.fullPath = "/pkg/CanonicalResult/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasTryAtDef.fullPath, &aliasTryAtDef},
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
      {aliasTagDef.fullPath, &aliasTagDef},
      {canonicalTagDef.fullPath, &canonicalTagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/map/tryAt";
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == &aliasTagDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed variadic map receivers") {
  primec::Definition containsDef;
  containsDef.fullPath = "/std/collections/map/contains";
  primec::Definition atUnsafeDef;
  atUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {containsDef.fullPath, &containsDef},
      {atUnsafeDef.fullPath, &atUnsafeDef},
  };

  auto makeMethodCall = [](const std::string &packName,
                           const std::string &methodName,
                           int64_t keyValue) {
    primec::Expr packNameExpr;
    packNameExpr.kind = primec::Expr::Kind::Name;
    packNameExpr.name = packName;

    primec::Expr indexExpr;
    indexExpr.kind = primec::Expr::Kind::Literal;
    indexExpr.intWidth = 32;
    indexExpr.literalValue = 0;

    primec::Expr accessExpr;
    accessExpr.kind = primec::Expr::Kind::Call;
    accessExpr.name = "at";
    accessExpr.args = {packNameExpr, indexExpr};

    primec::Expr derefExpr;
    derefExpr.kind = primec::Expr::Kind::Call;
    derefExpr.name = "dereference";
    derefExpr.args = {accessExpr};

    primec::Expr keyExpr;
    keyExpr.kind = primec::Expr::Kind::Literal;
    keyExpr.intWidth = 32;
    keyExpr.literalValue = keyValue;

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {derefExpr, keyExpr};
    return methodCall;
  };

  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo borrowedPackInfo;
  borrowedPackInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  borrowedPackInfo.isArgsPack = true;
  borrowedPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  borrowedPackInfo.referenceToMap = true;
  borrowedPackInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  borrowedPackInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("mapRefs", borrowedPackInfo);

  primec::ir_lowerer::LocalInfo pointerPackInfo;
  pointerPackInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  pointerPackInfo.isArgsPack = true;
  pointerPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerPackInfo.pointerToMap = true;
  pointerPackInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  pointerPackInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("mapPtrs", pointerPackInfo);

  auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };
  auto isNever = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; };

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      makeMethodCall("mapRefs", "contains", 11),
      locals,
      isNever,
      isNever,
      isNever,
      {},
      {},
      {},
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == &containsDef);
  CHECK(error.empty());

  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      makeMethodCall("mapPtrs", "at_unsafe", 11),
      locals,
      isNever,
      isNever,
      isNever,
      {},
      {},
      {},
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == &atUnsafeDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup inference helper resolves wrapper-returned slash-method map access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::Expr wrapMapCall;
  wrapMapCall.kind = primec::Expr::Kind::Call;
  wrapMapCall.name = "wrapMap";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.isMethodCall = true;
  accessExpr.name = "/std/collections/map/at";
  accessExpr.args = {wrapMapCall, keyExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  auto resolveWrapMapKind = [](const primec::Expr &candidate,
                               const primec::ir_lowerer::LocalMap &,
                               primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
    if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapMap") {
      return false;
    }
    candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    return true;
  };

  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  accessExpr.name = "/std/collections/map/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer setup inference helper resolves wrapper-returned canonical map access int32 kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::Expr wrapMapCall;
  wrapMapCall.kind = primec::Expr::Kind::Call;
  wrapMapCall.name = "wrapMap";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "/std/collections/map/at";
  accessExpr.args = {wrapMapCall, keyExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  auto resolveWrapMapKind = [](const primec::Expr &candidate,
                               const primec::ir_lowerer::LocalMap &,
                               primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
    if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapMap") {
      return false;
    }
    candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    return true;
  };

  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  accessExpr.name = "/std/collections/map/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

