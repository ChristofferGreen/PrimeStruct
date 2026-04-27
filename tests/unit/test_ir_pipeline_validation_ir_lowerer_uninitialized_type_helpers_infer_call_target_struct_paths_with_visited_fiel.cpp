#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer uninitialized type helpers infer call target struct paths with visited field index callbacks") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "Ctor") {
      return std::string("/pkg/Ctor");
    }
    if (expr.name == "factory") {
      return std::string("/pkg/factory");
    }
    return std::string("/pkg/unknown");
  };
  auto inferDefinitionStructReturnPath = [](const std::string &path, std::unordered_set<std::string> &visitedDefs) {
    if (path == "/pkg/factory" && visitedDefs.insert(path).second) {
      return std::string("/pkg/FromDef");
    }
    return std::string();
  };

  std::unordered_set<std::string> visitedDefs;
  primec::Expr factoryCall;
  factoryCall.kind = primec::Expr::Kind::Call;
  factoryCall.name = "factory";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            factoryCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath, visitedDefs) == "/pkg/FromDef");
  CHECK(visitedDefs.count("/pkg/factory") == 1);
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            factoryCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath, visitedDefs)
            .empty());

  primec::Expr ctorCall;
  ctorCall.kind = primec::Expr::Kind::Call;
  ctorCall.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            ctorCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath, visitedDefs) == "/pkg/Ctor");
}

TEST_CASE("ir lowerer uninitialized type helpers infer definition return paths with visited map lookups") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "inner";
  factoryDef.returnExpr = returnExpr;
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
  };

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto inferStructReturnExprPath = [](const primec::Expr &expr, std::unordered_set<std::string> &visitedDefs) {
    if (expr.name == "inner" && visitedDefs.count("/pkg/factory") > 0) {
      return std::string("/pkg/FromExpr");
    }
    return std::string();
  };

  std::unordered_set<std::string> visitedDefs;
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapWithVisited(
            "/pkg/factory", defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs) ==
        "/pkg/FromExpr");
  CHECK(visitedDefs.count("/pkg/factory") == 1);

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapWithVisited(
            "/pkg/missing", defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs)
            .empty());
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapWithVisited(
            "/pkg/factory", defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs)
            .empty());
}

TEST_CASE("ir lowerer uninitialized type helpers infer definition return paths from definition map") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "inner";
  factoryDef.returnExpr = returnExpr;
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
  };

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto inferStructReturnExprPath = [](const primec::Expr &expr, std::unordered_set<std::string> &visitedDefs) {
    if (expr.name == "inner" && visitedDefs.count("/pkg/factory") > 0) {
      return std::string("/pkg/FromExpr");
    }
    return std::string();
  };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMap(
            "/pkg/factory", defMap, resolveStructTypeName, inferStructReturnExprPath) == "/pkg/FromExpr");
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMap(
            "/pkg/missing", defMap, resolveStructTypeName, inferStructReturnExprPath)
            .empty());
}

TEST_CASE("ir lowerer uninitialized type helpers infer wrapped experimental soa vector return paths") {
  primec::Definition helperDef;
  helperDef.fullPath = "/pkg/makeSoa";
  helperDef.namespacePrefix = "/pkg";

  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"Reference<soa_vector</pkg/Particle>>"};
  helperDef.transforms.push_back(returnTransform);

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto inferStructExprPath = [](const primec::Expr &) { return std::string(); };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(
            helperDef, resolveStructTypeName, inferStructExprPath) ==
        "/std/collections/experimental_soa_vector/SoaVector__pkg/Particle");
}

TEST_CASE("ir lowerer uninitialized type helpers infer definition return paths by call target field index") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr stepExpr;
  stepExpr.kind = primec::Expr::Kind::Call;
  stepExpr.name = "step";
  factoryDef.returnExpr = stepExpr;

  primec::Definition stepDef;
  stepDef.fullPath = "/pkg/step";
  stepDef.namespacePrefix = "/pkg";
  primec::Expr ctorExpr;
  ctorExpr.kind = primec::Expr::Kind::Call;
  ctorExpr.name = "Ctor";
  stepDef.returnExpr = ctorExpr;

  primec::Definition cycleADef;
  cycleADef.fullPath = "/pkg/cycleA";
  cycleADef.namespacePrefix = "/pkg";
  primec::Expr cycleBExpr;
  cycleBExpr.kind = primec::Expr::Kind::Call;
  cycleBExpr.name = "cycleB";
  cycleADef.returnExpr = cycleBExpr;

  primec::Definition cycleBDef;
  cycleBDef.fullPath = "/pkg/cycleB";
  cycleBDef.namespacePrefix = "/pkg";
  primec::Expr cycleAExpr;
  cycleAExpr.kind = primec::Expr::Kind::Call;
  cycleAExpr.name = "cycleA";
  cycleBDef.returnExpr = cycleAExpr;

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
      {"/pkg/step", &stepDef},
      {"/pkg/cycleA", &cycleADef},
      {"/pkg/cycleB", &cycleBDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
            "/pkg/factory", defMap, resolveStructTypeName, resolveExprPath, fieldIndex) == "/pkg/Ctor");
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
            "/pkg/cycleA", defMap, resolveStructTypeName, resolveExprPath, fieldIndex)
            .empty());

  std::unordered_set<std::string> visitedDefs = {"/pkg/factory"};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
            "/pkg/factory", defMap, resolveStructTypeName, resolveExprPath, fieldIndex, visitedDefs)
            .empty());
}

TEST_CASE("ir lowerer uninitialized type helpers infer expression struct paths by definition call target field index") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr stepExpr;
  stepExpr.kind = primec::Expr::Kind::Call;
  stepExpr.name = "step";
  factoryDef.returnExpr = stepExpr;

  primec::Definition stepDef;
  stepDef.fullPath = "/pkg/step";
  stepDef.namespacePrefix = "/pkg";
  primec::Expr ctorExpr;
  ctorExpr.kind = primec::Expr::Kind::Call;
  ctorExpr.name = "Ctor";
  stepDef.returnExpr = ctorExpr;

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
      {"/pkg/step", &stepDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };
  auto resolveStructFieldSlot = [](const std::string &structPath,
                                   const std::string &fieldName,
                                   primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Local" && fieldName == "slot") {
      out = {"slot", 1, 1, primec::ir_lowerer::LocalInfo::ValueKind::Unknown, "/pkg/Nested"};
      return true;
    }
    return false;
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localInfo;
  localInfo.structTypeName = "/pkg/Local";
  locals.emplace("self", localInfo);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "self";
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            nameExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Local");

  primec::Expr fieldReceiverExpr;
  fieldReceiverExpr.kind = primec::Expr::Kind::Name;
  fieldReceiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(fieldReceiverExpr);
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            fieldExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Nested");

  primec::Expr unresolvedFieldExpr;
  unresolvedFieldExpr.kind = primec::Expr::Kind::Call;
  unresolvedFieldExpr.isFieldAccess = true;
  unresolvedFieldExpr.name = "factory";
  unresolvedFieldExpr.args.push_back(fieldReceiverExpr);
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(unresolvedFieldExpr,
                                                                                             locals,
                                                                                             defMap,
                                                                                             resolveStructTypeName,
                                                                                             resolveExprPath,
                                                                                             fieldIndex,
                                                                                             resolveStructFieldSlot)
            .empty());

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "factory";
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            callExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Ctor");
}

TEST_CASE("ir lowerer struct return path helpers infer implicit if expression returns") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";

  primec::Expr condExpr;
  condExpr.kind = primec::Expr::Kind::Name;
  condExpr.name = "usePrimary";

  primec::Expr ctorExpr;
  ctorExpr.kind = primec::Expr::Kind::Call;
  ctorExpr.name = "Ctor";

  primec::Expr stepExpr;
  stepExpr.kind = primec::Expr::Kind::Call;
  stepExpr.name = "step";

  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments.push_back(ctorExpr);

  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments.push_back(stepExpr);

  primec::Expr ifExpr;
  ifExpr.kind = primec::Expr::Kind::Call;
  ifExpr.name = "if";
  ifExpr.args = {condExpr, thenBlock, elseBlock};
  ifExpr.argNames = {std::nullopt, std::nullopt, std::nullopt};
  factoryDef.statements.push_back(ifExpr);

  primec::Expr condParam;
  condParam.isBinding = true;
  condParam.name = "usePrimary";
  primec::Transform boolTransform;
  boolTransform.name = "bool";
  condParam.transforms.push_back(boolTransform);
  factoryDef.parameters.push_back(condParam);

  primec::Definition stepDef;
  stepDef.fullPath = "/pkg/step";
  stepDef.namespacePrefix = "/pkg";
  stepDef.returnExpr = ctorExpr;

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
      {"/pkg/step", &stepDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };
  auto resolveStructFieldSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
            "/pkg/factory", defMap, resolveStructTypeName, resolveExprPath, fieldIndex) == "/pkg/Ctor");

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "factory";
  primec::ir_lowerer::LocalMap locals;
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            callExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Ctor");
}

TEST_CASE("ir lowerer uninitialized type helpers build expression struct path inferer from definition call target field index") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr ctorExpr;
  ctorExpr.kind = primec::Expr::Kind::Call;
  ctorExpr.name = "Ctor";
  factoryDef.returnExpr = ctorExpr;

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };
  auto resolveStructFieldSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  auto inferStructExprPath =
      primec::ir_lowerer::makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
          defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "factory";
  primec::ir_lowerer::LocalMap locals;
  CHECK(inferStructExprPath(callExpr, locals) == "/pkg/Ctor");

  primec::ir_lowerer::LocalInfo valuesInfo;
  valuesInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  valuesInfo.isArgsPack = true;
  valuesInfo.structTypeName = "/pkg/Ctor";
  valuesInfo.structSlotCount = 2;
  locals.emplace("values", valuesInfo);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 0;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.isMethodCall = true;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  accessExpr.args = {valuesExpr, indexExpr};
  CHECK(inferStructExprPath(accessExpr, locals) == "/pkg/Ctor");

  primec::Expr namespacedAccessExpr;
  namespacedAccessExpr.kind = primec::Expr::Kind::Call;
  namespacedAccessExpr.name = "at";
  namespacedAccessExpr.namespacePrefix = "/std/collections/map";
  namespacedAccessExpr.isMethodCall = false;
  namespacedAccessExpr.args = {valuesExpr, indexExpr};
  CHECK(inferStructExprPath(namespacedAccessExpr, locals) == "/pkg/Ctor");
}

TEST_CASE("ir lowerer uninitialized type helpers specialize explicit vector return paths from definition call targets") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"vector<Particle>"};
  factoryDef.transforms.push_back(returnTransform);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };
  auto resolveStructFieldSlot = [](const std::string &, const std::string &,
                                   primec::ir_lowerer::StructSlotFieldInfo &) { return false; };

  auto inferStructExprPath =
      primec::ir_lowerer::makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
          defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "factory";
  primec::ir_lowerer::LocalMap locals;

  const std::string inferredStructPath = inferStructExprPath(callExpr, locals);
  CHECK(inferredStructPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0);
}

TEST_CASE("ir lowerer uninitialized type helpers infer dereference expression struct paths") {
  const std::unordered_map<std::string, const primec::Definition *> defMap;
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &) { return std::string(); };
  auto resolveStructFieldSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.structTypeName = "/pkg/Ctor";
  locals.emplace("ptr", pointerInfo);

  primec::Expr ptrExpr;
  ptrExpr.kind = primec::Expr::Kind::Name;
  ptrExpr.name = "ptr";

  primec::Expr dereferenceExpr;
  dereferenceExpr.kind = primec::Expr::Kind::Call;
  dereferenceExpr.name = "dereference";
  dereferenceExpr.args = {ptrExpr};

  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            dereferenceExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Ctor");
}

TEST_CASE("ir lowerer uninitialized type helpers only infer namespaced internal SoA helper struct paths for location-style probes") {
  primec::Definition soaVectorDef;
  soaVectorDef.fullPath = "/std/collections/experimental_soa_vector/SoaVector__tc123";
  soaVectorDef.namespacePrefix = "/std/collections/experimental_soa_vector";
  primec::Expr storageBinding;
  storageBinding.kind = primec::Expr::Kind::Name;
  storageBinding.isBinding = true;
  storageBinding.name = "storage";
  primec::Transform storageType;
  storageType.name = "/std/collections/internal_soa_storage/SoaColumn__tc999";
  storageBinding.transforms = {storageType};
  soaVectorDef.statements = {storageBinding};

  primec::Definition soaColumnDef;
  soaColumnDef.fullPath = "/std/collections/internal_soa_storage/SoaColumn__tc999";
  soaColumnDef.namespacePrefix = "/std/collections/internal_soa_storage";
  primec::Expr dataBinding;
  dataBinding.kind = primec::Expr::Kind::Name;
  dataBinding.isBinding = true;
  dataBinding.name = "data";
  primec::Transform dataType;
  dataType.name = "Pointer";
  dataType.templateArgs = {"/pkg/Ctor"};
  dataBinding.transforms = {dataType};
  soaColumnDef.statements = {dataBinding};

  primec::Definition ctorDef;
  ctorDef.fullPath = "/pkg/Ctor";
  ctorDef.namespacePrefix = "/pkg";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {soaVectorDef.fullPath, &soaVectorDef},
      {soaColumnDef.fullPath, &soaColumnDef},
      {ctorDef.fullPath, &ctorDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &) { return std::string(); };
  auto resolveStructFieldSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.structTypeName = "/pkg/Ctor";
  locals.emplace("ptr", pointerInfo);
  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.structTypeName = soaVectorDef.fullPath;
  locals.emplace("columns", soaInfo);

  primec::Expr ptrExpr;
  ptrExpr.kind = primec::Expr::Kind::Name;
  ptrExpr.name = "ptr";
  primec::Expr locationExpr;
  locationExpr.kind = primec::Expr::Kind::Call;
  locationExpr.name = "location";
  locationExpr.namespacePrefix = "/std/collections/internal_soa_storage";
  locationExpr.args = {ptrExpr};
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            locationExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Ctor");

  primec::Expr columnsExpr;
  columnsExpr.kind = primec::Expr::Kind::Name;
  columnsExpr.name = "columns";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr refExpr;
  refExpr.kind = primec::Expr::Kind::Call;
  refExpr.name = "ref";
  refExpr.namespacePrefix = "/std/collections/internal_soa_storage";
  refExpr.args = {columnsExpr, indexExpr};
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            refExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot)
        .empty());
}

TEST_CASE(
    "ir lowerer uninitialized type helpers infer helper return experimental soa_vector struct paths") {
  primec::Definition cloneValuesDef;
  cloneValuesDef.fullPath = "/cloneValues";
  primec::Transform cloneReturn;
  cloneReturn.name = "return";
  cloneReturn.templateArgs = {"SoaVector<Particle>"};
  cloneValuesDef.transforms = {cloneReturn};

  primec::Definition pickBorrowedDef;
  pickBorrowedDef.fullPath = "/pickBorrowed";
  primec::Transform pickBorrowedReturn;
  pickBorrowedReturn.name = "return";
  pickBorrowedReturn.templateArgs = {"Reference<SoaVector<Particle>>"};
  pickBorrowedDef.transforms = {pickBorrowedReturn};

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {cloneValuesDef.fullPath, &cloneValuesDef},
      {pickBorrowedDef.fullPath, &pickBorrowedDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) {
    return false;
  };
  auto resolveExprPath = [](const primec::Expr &expr) {
    return expr.name.empty() ? std::string() : "/" + expr.name;
  };
  auto resolveExprPathWithoutSlash = [](const primec::Expr &expr) {
    return expr.name;
  };
  auto resolveStructFieldSlot = [](
                                    const std::string &,
                                    const std::string &,
                                    primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  const primec::ir_lowerer::LocalMap locals;

  primec::Expr cloneValuesExpr;
  cloneValuesExpr.kind = primec::Expr::Kind::Call;
  cloneValuesExpr.name = "cloneValues";
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            cloneValuesExpr,
            locals,
            defMap,
            resolveStructTypeName,
            resolveExprPath,
            fieldIndex,
            resolveStructFieldSlot) ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");

  primec::Expr pickBorrowedExpr;
  pickBorrowedExpr.kind = primec::Expr::Kind::Call;
  pickBorrowedExpr.name = "pickBorrowed";
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            pickBorrowedExpr,
            locals,
            defMap,
            resolveStructTypeName,
            resolveExprPath,
            fieldIndex,
            resolveStructFieldSlot) ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");

  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            cloneValuesExpr,
            locals,
            defMap,
            resolveStructTypeName,
            resolveExprPathWithoutSlash,
            fieldIndex,
            resolveStructFieldSlot) ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");

  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            pickBorrowedExpr,
            locals,
            defMap,
            resolveStructTypeName,
            resolveExprPathWithoutSlash,
            fieldIndex,
            resolveStructFieldSlot) ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");
}

TEST_CASE("ir lowerer uninitialized type helpers find field template args") {
  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fields;
  fields.push_back({"skip_static", "uninitialized", "i64", true});
  fields.push_back({"wrong_type", "i64", "", false});
  fields.push_back({"slot", "uninitialized", "map<i32, f64>", false});

  std::string typeTemplateArg;
  REQUIRE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "slot", typeTemplateArg));
  CHECK(typeTemplateArg == "map<i32, f64>");

  CHECK_FALSE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "missing", typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "wrong_type", typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "skip_static", typeTemplateArg));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve field storage candidates") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.isFieldAccess = true;
  storageExpr.name = "slot";
  storageExpr.args.push_back(receiverExpr);

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };

  const primec::ir_lowerer::LocalInfo *receiverOut = nullptr;
  std::string structPathOut;
  std::string typeTemplateArgOut;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageCandidate(
      storageExpr, locals, findField, receiverOut, structPathOut, typeTemplateArgOut));
  CHECK(receiverOut == &receiverIt->second);
  CHECK(structPathOut == "/pkg/Container");
  CHECK(typeTemplateArgOut == "i64");

  storageExpr.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageCandidate(
      storageExpr, locals, findField, receiverOut, structPathOut, typeTemplateArgOut));

  storageExpr.name = "slot";
  storageExpr.isFieldAccess = false;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageCandidate(
      storageExpr, locals, findField, receiverOut, structPathOut, typeTemplateArgOut));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve field storage type info") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.isFieldAccess = true;
  storageExpr.name = "slot";
  storageExpr.args.push_back(receiverExpr);

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };

  primec::ir_lowerer::UninitializedFieldStorageTypeInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageTypeInfo(
      storageExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, out, resolved, error));
  CHECK(resolved);
  CHECK(error.empty());
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.structPath == "/pkg/Container");
  CHECK(out.typeInfo.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageTypeInfo(
      storageExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, out, resolved, error));
  CHECK_FALSE(resolved);

  storageExpr.name = "slot";
  auto findUnsupportedField = [](const std::string &structPath,
                                 const std::string &fieldName,
                                 std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "Thing<i32>";
      return true;
    }
    return false;
  };
  auto resolveTypeInfoFail = [](const std::string &, const std::string &, primec::ir_lowerer::UninitializedTypeInfo &) {
    return false;
  };
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageTypeInfo(
      storageExpr, locals, findUnsupportedField, resolveNamespacePrefix, resolveTypeInfoFail, out, resolved, error));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");
}

TEST_CASE("ir lowerer uninitialized type helpers infer concrete stdlib map constructor structs") {
  primec::Definition mapPairDef;
  mapPairDef.fullPath = "/std/collections/mapPair__tgeneric";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("map<K, V>");
  mapPairDef.transforms.push_back(returnTransform);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {mapPairDef.fullPath, &mapPairDef},
  };
  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) {
    return false;
  };
  auto resolveExprPath = [](const primec::Expr &) {
    return std::string("/std/collections/mapPair__tgeneric");
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  auto resolveStructFieldSlot = [](const std::string &,
                                   const std::string &,
                                   primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  primec::Expr intMapPair;
  intMapPair.kind = primec::Expr::Kind::Call;
  intMapPair.name = "/std/collections/mapPair";
  intMapPair.templateArgs = {"string", "i32"};

  primec::Expr boolMapPair = intMapPair;
  boolMapPair.templateArgs = {"string", "bool"};

  const std::string intMapStruct =
      primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
          intMapPair, {}, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot);
  const std::string boolMapStruct =
      primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
          boolMapPair, {}, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot);

  CHECK(intMapStruct.rfind("/std/collections/experimental_map/Map__", 0) == 0);
  CHECK(boolMapStruct.rfind("/std/collections/experimental_map/Map__", 0) == 0);
  CHECK(intMapStruct != boolMapStruct);
}

TEST_SUITE_END();
