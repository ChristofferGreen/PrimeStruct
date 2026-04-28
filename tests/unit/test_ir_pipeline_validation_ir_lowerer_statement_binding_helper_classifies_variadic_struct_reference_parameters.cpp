#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

constexpr const char *kExperimentalSoaVectorStructTypePrefix =
    "/std/collections/experimental_soa_vector/SoaVector__";
const std::string kSpecializedExperimentalSoaVectorStructType =
    std::string(kExperimentalSoaVectorStructTypePrefix) + "specialized";

const primec::Definition *findDefinitionByPath(const primec::Program &program,
                                               std::string_view fullPath) {
  for (const auto &definition : program.definitions) {
    if (definition.fullPath == fullPath) {
      return &definition;
    }
  }
  return nullptr;
}

const primec::Expr *findBindingStatementByName(const primec::Definition &definition,
                                               std::string_view name) {
  for (const auto &statement : definition.statements) {
    if (statement.isBinding && statement.name == name) {
      return &statement;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("ir lowerer statement binding helper tolerates missing parameter callbacks") {
  primec::Expr param;
  param.name = "value";

  primec::ir_lowerer::LocalInfo info;
  info.index = 9;
  std::string error;
  CHECK(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      primec::ir_lowerer::IsBindingMutableFn{},
      primec::ir_lowerer::HasExplicitBindingTypeTransformFn{},
      primec::ir_lowerer::BindingKindFn{},
      primec::ir_lowerer::BindingValueKindFn{},
      primec::ir_lowerer::InferBindingExprKindFn{},
      primec::ir_lowerer::IsFileErrorBindingFn{},
      primec::ir_lowerer::SetReferenceArrayInfoForBindingFn{},
      primec::ir_lowerer::ApplyStructBindingInfoFn{},
      primec::ir_lowerer::ApplyStructBindingInfoFn{},
      primec::ir_lowerer::IsStringBindingFn{},
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic struct reference parameters") {
  primec::Expr param;
  param.name = "values";
  param.namespacePrefix = "/pkg";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<Pair>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo &infoOut) {
        for (const auto &transform : expr.transforms) {
          if (transform.name == "Reference" && transform.templateArgs.size() == 1 &&
              transform.templateArgs.front() == "Pair") {
            infoOut.structTypeName = "/pkg/Pair";
            return;
          }
        }
      },
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.structTypeName == "/pkg/Pair");
}

TEST_CASE("ir lowerer statement binding helper classifies variadic scalar pointer parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<i32>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic struct pointer parameters") {
  primec::Expr param;
  param.name = "values";
  param.namespacePrefix = "/pkg";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<Pair>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo &infoOut) {
        for (const auto &transform : expr.transforms) {
          if (transform.name == "Pointer" && transform.templateArgs.size() == 1 &&
              transform.templateArgs.front() == "Pair") {
            infoOut.structTypeName = "/pkg/Pair";
            return;
          }
        }
      },
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.structTypeName == "/pkg/Pair");
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed map parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference</std/collections/map<i32, i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToMap);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer map parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer</std/collections/map<i32, i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.pointerToMap);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer vector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<vector<i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.pointerToVector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer soa_vector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<soa_vector<Particle>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.pointerToVector);
  CHECK(info.isSoaVector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed imported SoaVector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<SoaVector<Particle>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToVector);
  CHECK(info.isSoaVector);
  CHECK(info.structTypeName.rfind(
            "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer imported SoaVector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<SoaVector<Particle>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.pointerToVector);
  CHECK(info.isSoaVector);
  CHECK(info.structTypeName.rfind(
            "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic soa_vector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("soa_vector<Particle>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.isSoaVector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper keeps specialized experimental soa_vector references as plain references without semantic surface metadata") {
  primec::Expr param;
  param.name = "values";
  primec::Transform referenceTransform;
  referenceTransform.name = "Reference";
  referenceTransform.templateArgs.push_back(kSpecializedExperimentalSoaVectorStructType);
  param.transforms.push_back(referenceTransform);

  auto bindingTypeAdapters = primec::ir_lowerer::makeBindingTypeAdapters();

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      bindingTypeAdapters.setReferenceArrayInfo,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK_FALSE(info.referenceToVector);
  CHECK_FALSE(info.isSoaVector);
  CHECK(info.structTypeName.empty());
}

TEST_CASE(
    "ir lowerer statement binding helper classifies parsed variadic borrowed "
    "imported soa_vector parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
score_refs([args<Reference<SoaVector<Particle>>>] values) {
  return(0i32)
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(score_refs(borrowed))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  const primec::Definition *scoreRefs = findDefinitionByPath(program, "/score_refs");
  REQUIRE(scoreRefs != nullptr);
  REQUIRE(scoreRefs->parameters.size() == 1);

  auto bindingTypeAdapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);
  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *bindingFact = primec::ir_lowerer::findSemanticProductBindingFact(
      semanticProductTargets, scoreRefs->parameters.front());
  REQUIRE(bindingFact != nullptr);
  const bool hasSoaVectorText =
      bindingFact->bindingTypeText.find("SoaVector") != std::string::npos ||
      bindingFact->bindingTypeText.find("soa_vector") != std::string::npos;
  CHECK(hasSoaVectorText);
  const bool hasRawElementType = !scoreRefs->parameters.front().transforms.empty() &&
                                 !scoreRefs->parameters.front().transforms.front().templateArgs.empty();
  CHECK(hasRawElementType);
  if (hasRawElementType) {
    const std::string &rawElementTypeText =
        scoreRefs->parameters.front().transforms.front().templateArgs.front();
    INFO("rawElementTypeText=" << rawElementTypeText);
    const bool rawHasSoaVectorText =
        rawElementTypeText.find("SoaVector") != std::string::npos ||
        rawElementTypeText.find("soa_vector") != std::string::npos;
    CHECK(rawHasSoaVectorText);
  }
  CHECK_FALSE(bindingTypeAdapters.isStringBinding(scoreRefs->parameters.front()));

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  const bool inferred = primec::ir_lowerer::inferCallParameterLocalInfo(
      scoreRefs->parameters.front(),
      {},
      [](const primec::Expr &) { return false; },
      bindingTypeAdapters.hasExplicitBindingTypeTransform,
      bindingTypeAdapters.bindingKind,
      bindingTypeAdapters.bindingValueKind,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      bindingTypeAdapters.isFileErrorBinding,
      bindingTypeAdapters.setReferenceArrayInfo,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      bindingTypeAdapters.isStringBinding,
      info,
      error,
      {},
      {},
      {},
      &semanticProductTargets);
  CHECK_MESSAGE(inferred, error);
  REQUIRE(inferred);
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToVector);
  CHECK(info.isSoaVector);
  CHECK(
      info.structTypeName.rfind(
          "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
}

TEST_CASE("ir lowerer statement binding helper uses semantic-product args-pack binding types") {
  primec::Expr param;
  param.name = "values";
  param.namespacePrefix = "/pkg";
  param.semanticNodeId = 9101;
  primec::Transform argsTransform;
  argsTransform.name = "args";
  param.transforms.push_back(argsTransform);

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/pkg/score_refs",
      .siteKind = "parameter",
      .name = "values",
      .bindingTypeText = "args<Reference<Pair>>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 7,
      .sourceColumn = 12,
      .semanticNodeId = 9101,
      .resolvedPathId = primec::InvalidSymbolId,
  });
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &expr) {
        return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
      },
      [](const primec::Expr &expr) {
        return primec::ir_lowerer::bindingKindFromTransforms(expr);
      },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo &infoOut) {
        for (const auto &transform : expr.transforms) {
          if (transform.name == "Reference" && transform.templateArgs.size() == 1 &&
              transform.templateArgs.front() == "Pair") {
            infoOut.structTypeName = "/pkg/Pair";
            return;
          }
        }
      },
      [](const primec::Expr &) { return false; },
      info,
      error,
      {},
      {},
      {},
      &semanticProgram,
      &semanticIndex));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.structTypeName == "/pkg/Pair");
}

TEST_CASE("ir lowerer statement binding helper rejects missing semantic args-pack binding type") {
  primec::Expr param;
  param.name = "values";
  param.semanticNodeId = 9102;
  primec::Transform argsTransform;
  argsTransform.name = "args";
  param.transforms.push_back(argsTransform);

  primec::SemanticProgram semanticProgram;
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo info;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &expr) {
        return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
      },
      [](const primec::Expr &expr) {
        return primec::ir_lowerer::bindingKindFromTransforms(expr);
      },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error,
      {},
      {},
      {},
      &semanticProgram,
      &semanticIndex));
  CHECK(error == "missing semantic-product args-pack binding type: values");
}

TEST_CASE("ir lowerer statement binding helper rejects incomplete semantic args-pack binding type") {
  primec::Expr param;
  param.name = "values";
  param.semanticNodeId = 9103;
  primec::Transform argsTransform;
  argsTransform.name = "args";
  param.transforms.push_back(argsTransform);

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/pkg/score_refs",
      .siteKind = "parameter",
      .name = "values",
      .bindingTypeText = "args",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 7,
      .sourceColumn = 12,
      .semanticNodeId = 9103,
      .resolvedPathId = primec::InvalidSymbolId,
  });
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo info;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &expr) {
        return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
      },
      [](const primec::Expr &expr) {
        return primec::ir_lowerer::bindingKindFromTransforms(expr);
      },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error,
      {},
      {},
      {},
      &semanticProgram,
      &semanticIndex));
  CHECK(error == "incomplete semantic-product args-pack binding type: values");
}

TEST_CASE("ir lowerer statement binding helper preserves inferred borrowed soa_vector return metadata") {
  primec::Definition callee;
  callee.fullPath = "/pkg/slice_ref";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("Reference<SoaVector<Particle>>");
  callee.transforms.push_back(returnTransform);

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "borrowed";
  stmt.namespacePrefix = "/pkg";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "slice_ref";
  init.namespacePrefix = "/pkg";

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          stmt,
          init,
          {},
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
          },
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::bindingKindFromTransforms(expr);
          },
          [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
            return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [&](const primec::Expr &expr) -> const primec::Definition * {
            return expr.name == "slice_ref" ? &callee : nullptr;
          });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToVector);
  CHECK(info.isSoaVector);
  CHECK(info.structTypeName.rfind(
            "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper preserves inferred borrowed map return metadata") {
  primec::Definition callee;
  callee.fullPath = "/pkg/pick_map";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("Reference<map<i32, bool>>");
  callee.transforms.push_back(returnTransform);

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "borrowed";
  stmt.namespacePrefix = "/pkg";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "pick_map";
  init.namespacePrefix = "/pkg";

  std::string expectedStructPath;
  REQUIRE(primec::ir_lowerer::resolveSpecializedExperimentalMapStructPathForBindingType(
      "map<i32, bool>", expectedStructPath));

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          stmt,
          init,
          {},
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
          },
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::bindingKindFromTransforms(expr);
          },
          [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
            return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [&](const primec::Expr &expr) -> const primec::Definition * {
            return expr.name == "pick_map" ? &callee : nullptr;
          });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToMap);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(info.structTypeName == expectedStructPath);
}

TEST_CASE("ir lowerer statement binding helper classifies explicit soa_vector locals with specialized struct paths") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "values";

  primec::Transform typeTransform;
  typeTransform.name = "SoaVector";
  typeTransform.templateArgs.push_back("Particle");
  stmt.transforms.push_back(typeTransform);

  primec::Transform mutTransform;
  mutTransform.name = "mut";
  stmt.transforms.push_back(mutTransform);

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "soaVectorNew";
  init.templateArgs.push_back("Particle");

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          stmt,
          init,
          {},
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
          },
          [](const primec::Expr &) {
            return primec::ir_lowerer::LocalInfo::Kind::Value;
          },
          [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
            return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.structTypeName.rfind(
            "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE(
    "ir lowerer statement binding helper preserves indexed borrowed soa_vector "
    "args-pack dereference struct paths") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesInfo;
  valuesInfo.kind = LocalInfo::Kind::Array;
  valuesInfo.isArgsPack = true;
  valuesInfo.argsPackElementKind = LocalInfo::Kind::Reference;
  valuesInfo.referenceToVector = true;
  valuesInfo.isSoaVector = true;
  valuesInfo.structTypeName = kSpecializedExperimentalSoaVectorStructType;
  locals.emplace("values", valuesInfo);

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "head";

  primec::Transform typeTransform;
  typeTransform.name = "SoaVector";
  typeTransform.templateArgs.push_back("Particle");
  stmt.transforms.push_back(typeTransform);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr atExpr;
  atExpr.kind = primec::Expr::Kind::Call;
  atExpr.name = "at";
  atExpr.args = {valuesName, indexExpr};

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "dereference";
  init.args = {atExpr};

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          stmt,
          init,
          locals,
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
          },
          [](const primec::Expr &) {
            return primec::ir_lowerer::LocalInfo::Kind::Value;
          },
          [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
            return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.isSoaVector);
  CHECK(info.structTypeName.rfind(
            "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
}

TEST_CASE(
    "ir lowerer statement binding helper preserves indexed pointer soa_vector "
    "args-pack dereference struct paths") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesInfo;
  valuesInfo.kind = LocalInfo::Kind::Array;
  valuesInfo.isArgsPack = true;
  valuesInfo.argsPackElementKind = LocalInfo::Kind::Pointer;
  valuesInfo.pointerToVector = true;
  valuesInfo.isSoaVector = true;
  valuesInfo.structTypeName = kSpecializedExperimentalSoaVectorStructType;
  locals.emplace("values", valuesInfo);

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "head";

  primec::Transform typeTransform;
  typeTransform.name = "SoaVector";
  typeTransform.templateArgs.push_back("Particle");
  stmt.transforms.push_back(typeTransform);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr atExpr;
  atExpr.kind = primec::Expr::Kind::Call;
  atExpr.name = "at";
  atExpr.args = {valuesName, indexExpr};

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "dereference";
  init.args = {atExpr};

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          stmt,
          init,
          locals,
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
          },
          [](const primec::Expr &) {
            return primec::ir_lowerer::LocalInfo::Kind::Value;
          },
          [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
            return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.isSoaVector);
  CHECK(info.structTypeName.rfind(
            "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
}

TEST_CASE(
    "ir lowerer statement binding helper classifies parsed borrowed imported "
    "soa_vector locals") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] a0{soaVectorNew<Particle>()}
  [Reference<SoaVector<Particle>>] r0{location(a0)}
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  const primec::Definition *mainDef = findDefinitionByPath(program, "/main");
  REQUIRE(mainDef != nullptr);
  const primec::Expr *binding = findBindingStatementByName(*mainDef, "r0");
  REQUIRE(binding != nullptr);
  REQUIRE(binding->args.size() == 1);

  auto bindingTypeAdapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);
  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          *binding,
          binding->args.front(),
          {},
          bindingTypeAdapters.hasExplicitBindingTypeTransform,
          bindingTypeAdapters.bindingKind,
          bindingTypeAdapters.bindingValueKind,
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          {},
          &semanticProductTargets);

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToVector);
  CHECK(info.isSoaVector);
  CHECK(info.structTypeName.rfind(
            "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0);
}

TEST_CASE(
    "ir lowerer statement binding helper canonicalizes imported bare struct "
    "parameter semantic types") {
  const std::string source = R"(
import /std/ui/*

[return<int>]
use_layout([LayoutTree mut] layout) {
  return(0i32)
}

[return<int>]
main([array<string>] args) {
  [LayoutTree mut] layout{LayoutTree{}}
  return(use_layout(layout))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  const primec::Definition *useLayoutDef = findDefinitionByPath(program, "/use_layout");
  REQUIRE(useLayoutDef != nullptr);
  REQUIRE(useLayoutDef->parameters.size() == 1);
  const primec::Definition *layoutTreeDef = findDefinitionByPath(program, "/std/ui/LayoutTree");
  REQUIRE(layoutTreeDef != nullptr);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  primec::ir_lowerer::buildDefinitionMapAndStructNames(
      program.definitions, defMap, structNames, &semanticProgram);
  const auto importAliases = primec::ir_lowerer::buildImportAliasesFromProgram(
      program.imports, program.definitions, defMap);
  const auto callResolutionAdapters =
      primec::ir_lowerer::makeCallResolutionAdapters(defMap, importAliases, &semanticProgram);
  const auto resolveDefinitionCall =
      primec::ir_lowerer::makeResolveDefinitionCall(
          defMap, callResolutionAdapters.resolveExprPath, &semanticProgram);
  auto bindingTypeAdapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);
  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::ir_lowerer::LocalInfo info;
  info.index = 17;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      useLayoutDef->parameters.front(),
      {},
      [](const primec::Expr &) { return false; },
      bindingTypeAdapters.hasExplicitBindingTypeTransform,
      bindingTypeAdapters.bindingKind,
      bindingTypeAdapters.bindingValueKind,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      bindingTypeAdapters.isFileErrorBinding,
      bindingTypeAdapters.setReferenceArrayInfo,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      bindingTypeAdapters.isStringBinding,
      info,
      error,
      {},
      resolveDefinitionCall,
      {},
      &semanticProductTargets));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.structTypeName == "/std/ui/LayoutTree");
}

TEST_CASE(
    "ir lowerer statement binding helper classifies specialized vector "
    "semantic parameter types as vectors") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
dump_words([vector<i32>] words) {}

[return<int>]
main([array<string>] args) {
  [vector<i32> mut] words{vector<i32>()}
  dump_words(words)
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  const primec::Definition *dumpWordsDef = findDefinitionByPath(program, "/dump_words");
  REQUIRE(dumpWordsDef != nullptr);
  REQUIRE(dumpWordsDef->parameters.size() == 1);

  auto bindingTypeAdapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);
  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::ir_lowerer::LocalInfo info;
  info.index = 18;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      dumpWordsDef->parameters.front(),
      {},
      [](const primec::Expr &) { return false; },
      bindingTypeAdapters.hasExplicitBindingTypeTransform,
      bindingTypeAdapters.bindingKind,
      bindingTypeAdapters.bindingValueKind,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      bindingTypeAdapters.isFileErrorBinding,
      bindingTypeAdapters.setReferenceArrayInfo,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      bindingTypeAdapters.isStringBinding,
      info,
      error,
      {},
      [&](const primec::Expr &expr) -> const primec::Definition * {
        if (!expr.name.empty() &&
            expr.name.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
          return findDefinitionByPath(program, expr.name);
        }
        return nullptr;
      },
      {},
      &semanticProductTargets));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.structTypeName.empty());
}

TEST_CASE("ir lowerer statement binding helper leaves explicit bare struct locals unresolved") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "holder";

  primec::Transform typeTransform;
  typeTransform.name = "Holder";
  stmt.transforms.push_back(typeTransform);

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "Holder";

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          stmt,
          init,
          {},
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
          },
          [](const primec::Expr &) {
            return primec::ir_lowerer::LocalInfo::Kind::Value;
          },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(info.structTypeName.empty());
}

TEST_CASE(
    "ir lowerer statement binding helper defers parsed explicit bare struct "
    "semantic surface names") {
  const std::string source = R"(
[struct]
Holder() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Holder mut] holder{Holder{}}
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  const primec::Definition *mainDef = findDefinitionByPath(program, "/main");
  REQUIRE(mainDef != nullptr);
  const primec::Expr *binding = findBindingStatementByName(*mainDef, "holder");
  REQUIRE(binding != nullptr);
  REQUIRE(binding->args.size() == 1);

  auto bindingTypeAdapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);
  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          *binding,
          binding->args.front(),
          {},
          bindingTypeAdapters.hasExplicitBindingTypeTransform,
          bindingTypeAdapters.bindingKind,
          bindingTypeAdapters.bindingValueKind,
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          {},
          &semanticProductTargets);

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(info.structTypeName.empty());
}

TEST_CASE(
    "ir lowerer statement binding helper classifies explicit file locals "
    "without struct layout names") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "file";

  primec::Transform typeTransform;
  typeTransform.name = "File";
  typeTransform.templateArgs.push_back("Read");
  stmt.transforms.push_back(typeTransform);

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "File";
  init.templateArgs.push_back("Read");

  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          stmt,
          init,
          {},
          [](const primec::Expr &expr) {
            return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
          },
          [](const primec::Expr &) {
            return primec::ir_lowerer::LocalInfo::Kind::Value;
          },
          [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
            return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(info.structTypeName.empty());
}

TEST_CASE("ir lowerer statement binding helper classifies variadic map parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("map<i32, bool>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 11;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer statement binding helper rejects string reference parameters") {
  primec::Expr param;
  param.name = "label";

  primec::ir_lowerer::LocalInfo info;
  info.index = 4;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return true; },
      info,
      error));
  CHECK(error == "native backend does not support string pointers or references");
}

TEST_CASE("ir lowerer statement binding helper rejects variadic string reference parameters with arg-pack diagnostic") {
  primec::Expr param;
  param.name = "labels";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<string>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 4;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return true; },
      info,
      error));
  CHECK(error == "variadic args<T> does not support string pointers or references");
}

TEST_CASE("ir lowerer statement binding helper selects uninitialized zero opcodes") {
  primec::IrOpcode mapZeroOp = primec::IrOpcode::PushI32;
  uint64_t mapZeroImm = 123;
  std::string error;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Map,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "mapStorage",
      mapZeroOp,
      mapZeroImm,
      error));
  CHECK(mapZeroOp == primec::IrOpcode::PushI64);
  CHECK(mapZeroImm == 0);
  CHECK(error.empty());

  primec::IrOpcode floatZeroOp = primec::IrOpcode::PushI32;
  uint64_t floatZeroImm = 99;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Float32,
      "floatStorage",
      floatZeroOp,
      floatZeroImm,
      error));
  CHECK(floatZeroOp == primec::IrOpcode::PushF32);
  CHECK(floatZeroImm == 0);
}

TEST_CASE("ir lowerer statement binding helper rejects unknown uninitialized value kind") {
  primec::IrOpcode zeroOp = primec::IrOpcode::PushI32;
  uint64_t zeroImm = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "storageSlot",
      zeroOp,
      zeroImm,
      error));
  CHECK(error == "native backend requires a concrete uninitialized storage type on storageSlot");
}

TEST_CASE("ir lowerer statement binding helper emits literal string bindings") {
  primec::Expr stmt;
  stmt.name = "label";
  stmt.isBinding = true;

  primec::Expr init;
  init.kind = primec::Expr::Kind::StringLiteral;
  init.stringValue = "\"hello\"utf8";

  primec::ir_lowerer::LocalMap locals;
  int32_t nextLocal = 5;
  std::vector<primec::IrInstruction> instructions;
  int emitExprCalls = 0;
  int boundsCalls = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::emitStringStatementBindingInitializer(
      stmt,
      init,
      locals,
      nextLocal,
      instructions,
      [](const primec::Expr &) { return false; },
      [](const std::string &decoded) {
        CHECK(decoded == "hello");
        return 17;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      []() { return 200; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { ++boundsCalls; },
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(boundsCalls == 0);
  CHECK(nextLocal == 6);
  auto it = locals.find("label");
  REQUIRE(it != locals.end());
  CHECK(it->second.index == 5);
  CHECK(it->second.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(it->second.stringSource == primec::ir_lowerer::LocalInfo::StringSource::TableIndex);
  CHECK(it->second.stringIndex == 17);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 17);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 5);
}

TEST_CASE("ir lowerer statement binding helper emits checked argv string bindings") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr stmt;
  stmt.name = "argText";
  stmt.isBinding = true;

  primec::Expr argvName;
  argvName.kind = primec::Expr::Kind::Name;
  argvName.name = "argv";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 2;
  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "at";
  init.args = {argvName, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  int32_t nextLocal = 9;
  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 40;
  int boundsCalls = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::emitStringStatementBindingInitializer(
      stmt,
      init,
      locals,
      nextLocal,
      instructions,
      [](const primec::Expr &) { return true; },
      [](const std::string &) { return 0; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, expr.literalValue});
        return true;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [&]() { return nextTempLocal++; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.kind == primec::Expr::Kind::Name && expr.name == "argv";
      },
      [&]() {
        ++boundsCalls;
        instructions.push_back({primec::IrOpcode::PushI32, 999});
      },
      error));
  CHECK(error.empty());
  CHECK(boundsCalls == 2);
  CHECK(nextLocal == 10);
  auto it = locals.find("argText");
  REQUIRE(it != locals.end());
  CHECK(it->second.index == 9);
  CHECK(it->second.isMutable);
  CHECK(it->second.valueKind == ValueKind::String);
  CHECK(it->second.stringSource == primec::ir_lowerer::LocalInfo::StringSource::ArgvIndex);
  CHECK(it->second.argvChecked);

  bool sawPushArgc = false;
  for (const auto &instruction : instructions) {
    if (instruction.op == primec::IrOpcode::PushArgc) {
      sawPushArgc = true;
      break;
    }
  }
  CHECK(sawPushArgc);
}


TEST_SUITE_END();
