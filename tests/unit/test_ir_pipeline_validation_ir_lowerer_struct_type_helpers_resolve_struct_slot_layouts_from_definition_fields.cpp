#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer struct type helpers resolve struct slot layouts from definition fields") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto collectStructLayoutFields = [](const std::string &structPath,
                                      std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Inner") {
      out.push_back({"x", "i32", "", false});
      return true;
    }
    if (structPath == "/pkg/Outer") {
      out.push_back({"id", "i64", "", false});
      out.push_back({"payload", "uninitialized", "Inner", false});
      out.push_back({"skipStatic", "i32", "", true});
      return true;
    }
    return false;
  };
  auto resolveDefinitionNamespacePrefix = [](const std::string &structPath, std::string &namespacePrefixOut) {
    if (structPath == "/pkg/Inner" || structPath == "/pkg/Outer") {
      namespacePrefixOut = "/pkg";
      return true;
    }
    return false;
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  primec::ir_lowerer::StructSlotLayoutInfo layout;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/Outer",
                                                                           collectStructLayoutFields,
                                                                           resolveDefinitionNamespacePrefix,
                                                                           resolveStructTypeName,
                                                                           valueKindFromTypeName,
                                                                           layoutCache,
                                                                           layoutStack,
                                                                           layout,
                                                                           error));
  CHECK(error.empty());
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[0].name == "id");
  CHECK(layout.fields[0].slotOffset == 1);
  CHECK(layout.fields[0].slotCount == 1);
  CHECK(layout.fields[0].valueKind == ValueKind::Int64);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].slotOffset == 2);
  CHECK(layout.fields[1].slotCount == 2);
  CHECK(layout.fields[1].structPath == "/pkg/Inner");
  CHECK(layoutCache.count("/pkg/Outer") == 1);
  CHECK(layoutCache.count("/pkg/Inner") == 1);
  CHECK(layoutStack.empty());

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(primec::ir_lowerer::resolveStructFieldSlotFromDefinitionFields("/pkg/Outer",
                                                                          "payload",
                                                                          collectStructLayoutFields,
                                                                          resolveDefinitionNamespacePrefix,
                                                                          resolveStructTypeName,
                                                                          valueKindFromTypeName,
                                                                          layoutCache,
                                                                          layoutStack,
                                                                          slot,
                                                                          error));
  CHECK(slot.name == "payload");
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");
}

TEST_CASE("ir lowerer struct type helpers resolve struct slots from definition field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
      {"/pkg/Null", nullptr},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  primec::ir_lowerer::StructSlotLayoutInfo layout;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFieldIndex("/pkg/Outer",
                                                                               fieldIndex,
                                                                               defMap,
                                                                               resolveStructTypeName,
                                                                               valueKindFromTypeName,
                                                                               layoutCache,
                                                                               layoutStack,
                                                                               layout,
                                                                               error));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(primec::ir_lowerer::resolveStructFieldSlotFromDefinitionFieldIndex("/pkg/Outer",
                                                                              "payload",
                                                                              fieldIndex,
                                                                              defMap,
                                                                              resolveStructTypeName,
                                                                              valueKindFromTypeName,
                                                                              layoutCache,
                                                                              layoutStack,
                                                                              slot,
                                                                              error));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFieldIndex("/pkg/Null",
                                                                                   fieldIndex,
                                                                                   defMap,
                                                                                   resolveStructTypeName,
                                                                                   valueKindFromTypeName,
                                                                                   layoutCache,
                                                                                   layoutStack,
                                                                                   layout,
                                                                                   error));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Null");
}

TEST_CASE("ir lowerer struct type helpers build struct-slot resolvers from definition field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::string error;

  auto resolveStructSlotLayout = primec::ir_lowerer::makeResolveStructSlotLayoutFromDefinitionFieldIndex(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);
  auto resolveStructFieldSlot = primec::ir_lowerer::makeResolveStructFieldSlotFromDefinitionFieldIndex(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(resolveStructFieldSlot("/pkg/Outer", "payload", slot));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct-slot resolution adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::string error;
  auto adapters = primec::ir_lowerer::makeStructSlotResolutionAdapters(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(adapters.resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(adapters.resolveStructFieldSlot("/pkg/Outer", "payload", slot));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(adapters.resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct-slot adapters with owned state") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  std::string error;
  auto adapters = primec::ir_lowerer::makeStructSlotResolutionAdaptersWithOwnedState(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, error);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(adapters.resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(adapters.resolveStructFieldSlot("/pkg/Outer", "payload", slot));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(adapters.resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct layout adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    if (namespacePrefix == "/pkg" && typeName == "Outer") {
      resolvedOut = "/pkg/Outer";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  std::string error;
  const auto adapters = primec::ir_lowerer::makeStructLayoutResolutionAdaptersWithOwnedSlotState(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, error);

  primec::ir_lowerer::StructArrayTypeInfo arrayInfo;
  REQUIRE(adapters.structArrayInfo.resolveStructArrayTypeInfoFromPath("/pkg/Inner", arrayInfo));
  CHECK(arrayInfo.structPath == "/pkg/Inner");
  CHECK(arrayInfo.elementKind == ValueKind::Int32);
  CHECK(arrayInfo.fieldCount == 1);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(adapters.structSlotResolution.resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  CHECK_FALSE(adapters.structSlotResolution.resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers report definition slot layout diagnostics") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto resolveDefinitionNamespacePrefix = [](const std::string &structPath, std::string &namespacePrefixOut) {
    if (structPath.rfind("/pkg/", 0) == 0) {
      namespacePrefixOut = "/pkg";
      return true;
    }
    return false;
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Cycle") {
      resolvedOut = "/pkg/Cycle";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };

  {
    auto collectStructLayoutFields = [](const std::string &structPath,
                                        std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
      out.clear();
      if (structPath == "/pkg/BadUninit") {
        out.push_back({"slot", "uninitialized", "", false});
        return true;
      }
      return false;
    };
    primec::ir_lowerer::StructSlotLayoutCache layoutCache;
    std::unordered_set<std::string> layoutStack;
    primec::ir_lowerer::StructSlotLayoutInfo layout;
    std::string error;
    CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/BadUninit",
                                                                                 collectStructLayoutFields,
                                                                                 resolveDefinitionNamespacePrefix,
                                                                                 resolveStructTypeName,
                                                                                 valueKindFromTypeName,
                                                                                 layoutCache,
                                                                                 layoutStack,
                                                                                 layout,
                                                                                 error));
    CHECK(error == "uninitialized requires a template argument on /pkg/BadUninit");
  }

  {
    auto collectStructLayoutFields = [](const std::string &structPath,
                                        std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
      out.clear();
      if (structPath == "/pkg/SoaHolder") {
        out.push_back({"storage", "soa_vector", "Particle", false});
        return true;
      }
      return false;
    };
    primec::ir_lowerer::StructSlotLayoutCache layoutCache;
    std::unordered_set<std::string> layoutStack;
    primec::ir_lowerer::StructSlotLayoutInfo layout;
    std::string error;
    REQUIRE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/SoaHolder",
                                                                             collectStructLayoutFields,
                                                                             resolveDefinitionNamespacePrefix,
                                                                             resolveStructTypeName,
                                                                             valueKindFromTypeName,
                                                                             layoutCache,
                                                                             layoutStack,
                                                                             layout,
                                                                             error));
    CHECK(error.empty());
    CHECK(layout.totalSlots == 4);
    REQUIRE(layout.fields.size() == 1);
    CHECK(layout.fields[0].name == "storage");
    CHECK(layout.fields[0].slotOffset == 1);
    CHECK(layout.fields[0].slotCount == 3);
    CHECK(layout.fields[0].structPath == "/soa_vector");
  }

  {
    auto collectStructLayoutFields = [](const std::string &structPath,
                                        std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
      out.clear();
      if (structPath == "/pkg/BadTemplate") {
        out.push_back({"slot", "array", "i32", false});
        return true;
      }
      return false;
    };
    primec::ir_lowerer::StructSlotLayoutCache layoutCache;
    std::unordered_set<std::string> layoutStack;
    primec::ir_lowerer::StructSlotLayoutInfo layout;
    std::string error;
    CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/BadTemplate",
                                                                                 collectStructLayoutFields,
                                                                                 resolveDefinitionNamespacePrefix,
                                                                                 resolveStructTypeName,
                                                                                 valueKindFromTypeName,
                                                                                 layoutCache,
                                                                                 layoutStack,
                                                                                 layout,
                                                                                 error));
    CHECK(error == "native backend does not support templated struct fields on /pkg/BadTemplate");
  }

  {
    auto collectStructLayoutFields = [](const std::string &structPath,
                                        std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
      out.clear();
      if (structPath == "/pkg/Cycle") {
        out.push_back({"next", "Cycle", "", false});
        return true;
      }
      return false;
    };
    primec::ir_lowerer::StructSlotLayoutCache layoutCache;
    std::unordered_set<std::string> layoutStack;
    primec::ir_lowerer::StructSlotLayoutInfo layout;
    std::string error;
    CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/Cycle",
                                                                                 collectStructLayoutFields,
                                                                                 resolveDefinitionNamespacePrefix,
                                                                                 resolveStructTypeName,
                                                                                 valueKindFromTypeName,
                                                                                 layoutCache,
                                                                                 layoutStack,
                                                                                 layout,
                                                                                 error));
    CHECK(error == "recursive struct layout not supported: /pkg/Cycle");
  }
}

TEST_CASE("ir lowerer struct type helpers apply struct value info") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform qualifier;
  qualifier.name = "mut";
  typedBinding.transforms.push_back(qualifier);
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, info);
  CHECK(info.structTypeName == "/pkg/Foo");

  primec::Expr pointerBinding;
  pointerBinding.namespacePrefix = "/pkg";
  primec::Transform pointerType;
  pointerType.name = "Pointer";
  pointerType.templateArgs = {"Foo"};
  pointerBinding.transforms.push_back(pointerType);

  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(pointerBinding, resolveStruct, pointerInfo);
  CHECK(pointerInfo.structTypeName == "/pkg/Foo");

  primec::ir_lowerer::LocalInfo alreadyTyped;
  alreadyTyped.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  alreadyTyped.structTypeName = "/already/Set";
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, alreadyTyped);
  CHECK(alreadyTyped.structTypeName == "/already/Set");
}

TEST_CASE("ir lowerer struct type helpers build struct value info applier") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };
  auto applyStructValueInfo = primec::ir_lowerer::makeApplyStructValueInfoFromBinding(resolveStruct);

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  applyStructValueInfo(typedBinding, info);
  CHECK(info.structTypeName == "/pkg/Foo");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct-type resolution adapters") {
  const std::unordered_set<std::string> structNames = {"/pkg/Foo"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Foo", "/pkg/Foo"}};
  auto adapters = primec::ir_lowerer::makeStructTypeResolutionAdapters(structNames, importAliases);

  std::string resolved;
  REQUIRE(adapters.resolveStructTypeName("Foo", "/pkg", resolved));
  CHECK(resolved == "/pkg/Foo");

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  adapters.applyStructValueInfo(typedBinding, info);
  CHECK(info.structTypeName == "/pkg/Foo");
}

TEST_CASE("ir lowerer struct type helpers build bundled setup-type and struct-type adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::unordered_set<std::string> structNames = {"/pkg/Foo"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Foo", "/pkg/Foo"}};
  const auto adapters = primec::ir_lowerer::makeSetupTypeAndStructTypeAdapters(structNames, importAliases);

  CHECK(adapters.valueKindFromTypeName("i64") == ValueKind::Int64);
  CHECK(adapters.combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);

  std::string resolved;
  REQUIRE(adapters.structTypeResolutionAdapters.resolveStructTypeName("Foo", "/pkg", resolved));
  CHECK(resolved == "/pkg/Foo");
  CHECK_FALSE(adapters.structTypeResolutionAdapters.resolveStructTypeName("Missing", "/pkg", resolved));

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  adapters.structTypeResolutionAdapters.applyStructValueInfo(typedBinding, info);
  CHECK(info.structTypeName == "/pkg/Foo");
}

TEST_SUITE_END();
