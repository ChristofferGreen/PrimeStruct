TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir validator accepts lowered canonical module") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Glsl, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::WasmBrowser, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer rejects non-eliminated reflection query paths") {
  const std::string source = R"(
[return<int>]
main() {
  /meta/type_name<i32>()
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  REQUIRE(parser.parse(program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error == "native backend requires compile-time reflection query elimination before IR emission: /meta/type_name");
}

TEST_CASE("ir lowerer reflection queries leave no runtime call state") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] typeName{meta.type_name<Item>()}
  [string] fieldName{meta.field_name<Item>(0i32)}
  [bool] hasReflect{meta.has_transform<Item>(reflect)}
  [bool] hasComparable{meta.has_trait<i32>(Comparable)}
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  REQUIRE(module.entryIndex >= 0);
  REQUIRE(static_cast<size_t>(module.entryIndex) < module.functions.size());
  const auto &entryFunction = module.functions[static_cast<size_t>(module.entryIndex)];
  for (const auto &instruction : entryFunction.instructions) {
    CHECK(instruction.op != primec::IrOpcode::Call);
    CHECK(instruction.op != primec::IrOpcode::CallVoid);
  }
  for (const auto &function : module.functions) {
    CHECK(function.name.rfind("/meta/", 0) != 0);
  }
  for (const auto &entry : module.stringTable) {
    CHECK(entry.rfind("/meta/", 0) != 0);
  }
}

TEST_CASE("ir lowerer map contains avoids missing-key runtime helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  if (/std/collections/map/contains(values, 1i32)) {
    return(1i32)
  }
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(std::find(module.stringTable.begin(), module.stringTable.end(), "map key not found") == module.stringTable.end());
}

TEST_CASE("ir lowerer guarded map Result lookup avoids missing-key runtime helpers") {
  const std::string source = R"(
import /std/collections/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<Result<i32, MyError>>]
probe([map<i32, i32>] values, [i32] key) {
  if(/std/collections/map/contains(values, key),
     then(){ return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else(){ return(multiply(convert<i64>(1i32), 4294967296i64)) })
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 7i32)}
  probe(values, 1i32)
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(std::find(module.stringTable.begin(), module.stringTable.end(), "map key not found") == module.stringTable.end());
}

TEST_CASE("ir lowerer effects unit resolves entry and non-entry defaults") {
  const std::vector<primec::Transform> transforms;
  const std::vector<std::string> defaultEffects = {"io_out"};
  const std::vector<std::string> entryDefaultEffects = {"io_err"};

  const auto entryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, true, defaultEffects, entryDefaultEffects);
  CHECK(entryActive.size() == 1);
  CHECK(entryActive.count("io_err") == 1);
  CHECK(entryActive.count("io_out") == 0);

  const auto nonEntryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, false, defaultEffects, entryDefaultEffects);
  CHECK(nonEntryActive.size() == 1);
  CHECK(nonEntryActive.count("io_out") == 1);
  CHECK(nonEntryActive.count("io_err") == 0);
}

TEST_CASE("ir lowerer effects unit rejects software numeric envelopes") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("decimal");
  mainDef.transforms.push_back(returnTransform);
  program.definitions.push_back(std::move(mainDef));

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateNoSoftwareNumericTypes(program, error));
  CHECK(error == "native backend does not support software numeric types: decimal");
}

TEST_CASE("ir lowerer helper classifies soa_vector as collection builtin") {
  primec::Expr soaVectorCall;
  soaVectorCall.kind = primec::Expr::Kind::Call;
  soaVectorCall.name = "soa_vector";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(soaVectorCall, builtin));
  CHECK(builtin == "soa_vector");
}

TEST_CASE("ir lowerer helper rejects array namespaced vector constructor alias builtin") {
  primec::Expr arrayVectorCall;
  arrayVectorCall.kind = primec::Expr::Kind::Call;
  arrayVectorCall.name = "/array/vector";

  std::string builtin;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinCollectionName(arrayVectorCall, builtin));
}

TEST_CASE("shared simple-call helpers reject removed array count alias") {
  primec::Expr bareCountCall;
  bareCountCall.kind = primec::Expr::Kind::Call;
  bareCountCall.name = "count";
  CHECK(primec::semantics::isSimpleCallName(bareCountCall, "count"));
  CHECK(primec::ir_lowerer::isSimpleCallName(bareCountCall, "count"));
  CHECK(primec::emitter::isSimpleCallName(bareCountCall, "count"));

  primec::Expr canonicalCountCall = bareCountCall;
  canonicalCountCall.name = "/std/collections/vector/count";
  CHECK(primec::semantics::isSimpleCallName(canonicalCountCall, "count"));
  CHECK(primec::ir_lowerer::isSimpleCallName(canonicalCountCall, "count"));
  CHECK(primec::emitter::isSimpleCallName(canonicalCountCall, "count"));

  primec::Expr removedAliasCall = bareCountCall;
  removedAliasCall.name = "/array/count";
  CHECK_FALSE(primec::semantics::isSimpleCallName(removedAliasCall, "count"));
  CHECK_FALSE(primec::ir_lowerer::isSimpleCallName(removedAliasCall, "count"));
  CHECK_FALSE(primec::emitter::isSimpleCallName(removedAliasCall, "count"));
}

TEST_CASE("emitter cpp keeps canonical vector count builtin fallback") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(/std/collections/vector/count(values))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::Emitter emitter;
  const std::string cpp = emitter.emitCpp(program, "/main");
  CHECK(cpp.find("ps_array_count(") != std::string::npos);
}

TEST_CASE("emitter cpp keeps array count builtin fallback") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  bool rewroteAlias = false;
  for (auto &def : program.definitions) {
    if (def.fullPath != "/main" || !def.returnExpr.has_value()) {
      continue;
    }
    def.returnExpr->name = "/array/count";
    rewroteAlias = true;
    break;
  }
  REQUIRE(rewroteAlias);

  primec::Emitter emitter;
  const std::string cpp = emitter.emitCpp(program, "/main");
  CHECK(cpp.find("ps_array_count(") != std::string::npos);
}

TEST_CASE("semantics accepts and lowerer emits empty soa_vector literals") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<i32>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
  CHECK_FALSE(module.functions[0].instructions.empty());
}

TEST_CASE("semantics accepts soa_vector count in non-entry helpers") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/use([soa_vector<Particle>] values) {
  return(count(values))
}

[return<int>]
main() {
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/use", {}, {}, module, error));
  CHECK(error == "native backend entry parameter must be array<string>");
}

TEST_CASE("ir lowerer rejects non-empty soa_vector literals with deterministic diagnostic") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int> effects(heap_alloc)]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>(Particle(1i32))}
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error == "native backend does not support non-empty soa_vector literals");
}

TEST_CASE("semantics accepts soa_vector get before lowerer rejection") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  get(values, 0i32)
}

[return<int>]
main() {
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/use", {}, {}, module, error));
  CHECK(error == "native backend entry parameter must be array<string>");
}

TEST_CASE("semantics accepts soa_vector ref before lowerer rejection") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  ref(values, 0i32)
}

[return<int>]
main() {
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/use", {}, {}, module, error));
  CHECK(error == "native backend entry parameter must be array<string>");
}

TEST_CASE("semantics accepts to_soa before lowerer rejection") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_soa(values)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error == "native backend requires typed bindings");
}

TEST_CASE("semantics accepts to_aos before lowerer rejection") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_aos(to_soa(values))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error == "native backend requires typed bindings");
}

TEST_CASE("semantics rejects soa_vector field-view before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.x()
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "soa_vector field views are not implemented yet: x");
}

TEST_CASE("semantics rejects soa_vector field-view call-form before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  x(values)
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "soa_vector field views are not implemented yet: x");
}

TEST_CASE("semantics rejects soa_vector field-view direct-call index shape before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.x(0i32)
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "soa_vector field views require value.<field>()[index] syntax: x");
}

TEST_CASE("semantics rejects soa_vector field-view call-form index shape before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  x(values, 0i32)
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "soa_vector field views require value.<field>()[index] syntax: x");
}

TEST_CASE("semantics rejects soa_vector get method named args before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.get([index] 0i32)
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "named arguments not supported for builtin calls");
}

TEST_CASE("semantics rejects soa_vector ref method named args before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.ref([index] 0i32)
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "named arguments not supported for builtin calls");
}

TEST_CASE("ir lowerer effects unit validates program effect traversal") {
  auto makeEffectsTransform = [](const std::vector<std::string> &effects) {
    primec::Transform transform;
    transform.name = "effects";
    transform.arguments = effects;
    return transform;
  };

  primec::Program program;

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.transforms.push_back(makeEffectsTransform({"io_out"}));

  primec::Expr parameterExpr;
  parameterExpr.transforms.push_back(makeEffectsTransform({"io_err"}));
  primec::Expr nestedParameterArg;
  nestedParameterArg.transforms.push_back(makeEffectsTransform({"heap_alloc"}));
  parameterExpr.args.push_back(nestedParameterArg);
  entryDef.parameters.push_back(parameterExpr);

  primec::Expr statementExpr;
  primec::Expr nestedBodyArg;
  nestedBodyArg.transforms.push_back(makeEffectsTransform({"file_write"}));
  statementExpr.bodyArguments.push_back(nestedBodyArg);
  entryDef.statements.push_back(statementExpr);

  primec::Expr returnExpr;
  returnExpr.transforms.push_back(makeEffectsTransform({"gpu_dispatch"}));
  entryDef.returnExpr = returnExpr;
  program.definitions.push_back(entryDef);

  primec::Execution execution;
  execution.fullPath = "/run";
  execution.transforms.push_back(makeEffectsTransform({"pathspace_notify"}));
  primec::Expr execArg;
  execArg.transforms.push_back(makeEffectsTransform({"pathspace_insert"}));
  execution.arguments.push_back(execArg);
  primec::Expr execBodyArg;
  execBodyArg.transforms.push_back(makeEffectsTransform({"pathspace_take"}));
  execution.bodyArguments.push_back(execBodyArg);
  program.executions.push_back(execution);

  std::string error;
  CHECK(primec::ir_lowerer::validateProgramEffects(program, "/main", {}, {}, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer effects unit rejects unsupported nested expression effects") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr statementExpr;
  primec::Expr nestedArg;
  primec::Transform badEffects;
  badEffects.name = "effects";
  badEffects.arguments = {"unsupported_effect"};
  nestedArg.transforms.push_back(badEffects);
  statementExpr.args.push_back(nestedArg);
  entryDef.statements.push_back(statementExpr);
  program.definitions.push_back(entryDef);

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateProgramEffects(program, "/main", {}, {}, error));
  CHECK(error == "native backend does not support effect: unsupported_effect on /main");
}

TEST_CASE("ir lowerer effects unit resolves entry metadata masks") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out", "heap_alloc"};
  entryDef.transforms.push_back(effects);

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK(primec::ir_lowerer::resolveEntryMetadataMasks(
      entryDef, "/main", {"io_err"}, {"io_out"}, entryEffectMask, entryCapabilityMask, error));
  CHECK(error.empty());
  CHECK(entryEffectMask == (primec::EffectIoOut | primec::EffectHeapAlloc));
  CHECK(entryCapabilityMask == (primec::EffectIoOut | primec::EffectHeapAlloc));
}

