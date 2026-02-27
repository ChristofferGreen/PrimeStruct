
TEST_CASE("ir serialize roundtrip and vm execution") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
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
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].instructions.size() == 4);
  CHECK(module.functions[0].instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(module.functions[0].instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(module.functions[0].instructions[2].op == primec::IrOpcode::AddI32);
  CHECK(module.functions[0].instructions[3].op == primec::IrOpcode::ReturnI32);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  CHECK(decoded.functions[0].instructions.size() == 4);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(decoded, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers semicolon-separated imports") {
  const std::string source = R"(
import /util; /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(2i32), 4i32))
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
}

TEST_CASE("ir lowers struct constructor call") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  thing([count] 3i32)
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
}

TEST_CASE("ir lowers struct return values") {
  const std::string source = R"(
[struct]
Point() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Point>]
make_point([i32] x, [i32] y) {
  return(Point([x] x, [y] y))
}

[return<int>]
main() {
  [Point] value{make_point(3i32, 4i32)}
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
}

TEST_CASE("ir lowers assign from imported struct-return function call") {
  const std::string source = R"(
import /math3/*

namespace math3 {
  [struct]
  Vec3() {
    [i32] x{0i32}
    [i32] y{0i32}
    [i32] z{0i32}
  }

  [return<Vec3>]
  makeVec3([i32] x, [i32] y, [i32] z) {
    return(Vec3([x] x, [y] y, [z] z))
  }
}

[return<int>]
main() {
  [Vec3 mut] sample{Vec3()}
  assign(sample, makeVec3(4i32, 5i32, 6i32))
  return(sample.z)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(result == 6);
}

TEST_CASE("semantics reports assign diagnostic for immutable struct local") {
  const std::string source = R"(
import /math3/*

namespace math3 {
  [struct]
  Vec3() {
    [i32] x{0i32}
    [i32] y{0i32}
    [i32] z{0i32}
  }

  [return<Vec3>]
  makeVec3([i32] x, [i32] y, [i32] z) {
    return(Vec3([x] x, [y] y, [z] z))
  }
}

[return<int>]
main() {
  [Vec3] sample{Vec3()}
  assign(sample, makeVec3(4i32, 5i32, 6i32))
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "assign target must be a mutable binding: sample");
}

TEST_CASE("ir emits struct layout metadata") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
  [i64] count{2i64}
}

[return<void>]
main() {
  Thing()
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
  auto layoutIt = std::find_if(module.structLayouts.begin(),
                               module.structLayouts.end(),
                               [](const primec::IrStructLayout &layout) { return layout.name == "/Thing"; });
  REQUIRE(layoutIt != module.structLayouts.end());
  CHECK(layoutIt->alignmentBytes == 8u);
  CHECK(layoutIt->totalSizeBytes == 16u);
  REQUIRE(layoutIt->fields.size() == 2u);
  CHECK(layoutIt->fields[0].name == "value");
  CHECK(layoutIt->fields[0].offsetBytes == 0u);
  CHECK(layoutIt->fields[0].sizeBytes == 4u);
  CHECK(layoutIt->fields[1].name == "count");
  CHECK(layoutIt->fields[1].offsetBytes == 8u);
  CHECK(layoutIt->fields[1].sizeBytes == 8u);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());
  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  auto decodedIt = std::find_if(decoded.structLayouts.begin(),
                                decoded.structLayouts.end(),
                                [](const primec::IrStructLayout &layout) { return layout.name == "/Thing"; });
  REQUIRE(decodedIt != decoded.structLayouts.end());
  CHECK(decodedIt->totalSizeBytes == 16u);
  CHECK(decodedIt->fields.size() == 2u);
}

TEST_CASE("ir infers omitted struct field envelopes before layout emission") {
  const std::string source = R"(
[struct]
Vec3() {
  [i32] x{1i32}
  [i32] y{2i32}
  [i32] z{3i32}
}

[return<Vec3>]
makeCenter() {
  return(Vec3())
}

[struct]
Sphere() {
  center{makeCenter()}
  [i32] radius{4i32}
}

[return<void>]
main() {
  Sphere()
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

  auto layoutIt = std::find_if(module.structLayouts.begin(),
                               module.structLayouts.end(),
                               [](const primec::IrStructLayout &layout) { return layout.name == "/Sphere"; });
  REQUIRE(layoutIt != module.structLayouts.end());
  REQUIRE(layoutIt->fields.size() == 2u);
  CHECK(layoutIt->fields[0].name == "center");
  CHECK(layoutIt->fields[0].envelope == "/Vec3");
  CHECK(layoutIt->fields[0].offsetBytes == 0u);
  CHECK(layoutIt->fields[0].sizeBytes == 12u);
  CHECK(layoutIt->fields[1].name == "radius");
  CHECK(layoutIt->fields[1].offsetBytes == 12u);
  CHECK(layoutIt->totalSizeBytes == 16u);
}

TEST_CASE("ir emits struct field visibility and static metadata") {
  const std::string source = R"(
[struct]
Widget() {
  [i32] size{1i32}
  [private static i32] counter{2i32}
  [public i32] weight{3i32}
}

[return<void>]
main() {
  Widget()
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
  auto layoutIt = std::find_if(module.structLayouts.begin(),
                               module.structLayouts.end(),
                               [](const primec::IrStructLayout &layout) { return layout.name == "/Widget"; });
  REQUIRE(layoutIt != module.structLayouts.end());
  CHECK(layoutIt->totalSizeBytes == 8u);
  REQUIRE(layoutIt->fields.size() == 3u);
  auto fieldByName = [&](const std::string &name) -> const primec::IrStructField * {
    auto it = std::find_if(layoutIt->fields.begin(),
                           layoutIt->fields.end(),
                           [&](const primec::IrStructField &field) { return field.name == name; });
    if (it == layoutIt->fields.end()) {
      return nullptr;
    }
    return &(*it);
  };
  const primec::IrStructField *sizeField = fieldByName("size");
  const primec::IrStructField *counterField = fieldByName("counter");
  const primec::IrStructField *weightField = fieldByName("weight");
  REQUIRE(sizeField != nullptr);
  REQUIRE(counterField != nullptr);
  REQUIRE(weightField != nullptr);
  CHECK(sizeField->visibility == primec::IrStructVisibility::Public);
  CHECK(sizeField->isStatic == false);
  CHECK(counterField->visibility == primec::IrStructVisibility::Private);
  CHECK(counterField->isStatic == true);
  CHECK(weightField->visibility == primec::IrStructVisibility::Public);
  CHECK(weightField->isStatic == false);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());
  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  auto decodedIt = std::find_if(decoded.structLayouts.begin(),
                                decoded.structLayouts.end(),
                                [](const primec::IrStructLayout &layout) { return layout.name == "/Widget"; });
  REQUIRE(decodedIt != decoded.structLayouts.end());
  REQUIRE(decodedIt->fields.size() == 3u);
  auto decodedCounter = std::find_if(decodedIt->fields.begin(),
                                     decodedIt->fields.end(),
                                     [](const primec::IrStructField &field) { return field.name == "counter"; });
  REQUIRE(decodedCounter != decodedIt->fields.end());
  CHECK(decodedCounter->visibility == primec::IrStructVisibility::Private);
  CHECK(decodedCounter->isStatic == true);
}

TEST_CASE("ir emits struct field categories") {
  const std::string source = R"(
[struct]
Payload() {
  [pod i32] count{1i32}
  [handle i64] file{2i64}
  [gpu_lane i32] lane{3i32}
  [i32] plain{4i32}
}

[return<void>]
main() {
  Payload()
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
  auto layoutIt = std::find_if(module.structLayouts.begin(),
                               module.structLayouts.end(),
                               [](const primec::IrStructLayout &layout) { return layout.name == "/Payload"; });
  REQUIRE(layoutIt != module.structLayouts.end());
  REQUIRE(layoutIt->fields.size() == 4u);
  auto fieldByName = [&](const std::string &name) -> const primec::IrStructField * {
    auto it = std::find_if(layoutIt->fields.begin(),
                           layoutIt->fields.end(),
                           [&](const primec::IrStructField &field) { return field.name == name; });
    if (it == layoutIt->fields.end()) {
      return nullptr;
    }
    return &(*it);
  };
  const primec::IrStructField *countField = fieldByName("count");
  const primec::IrStructField *fileField = fieldByName("file");
  const primec::IrStructField *laneField = fieldByName("lane");
  const primec::IrStructField *plainField = fieldByName("plain");
  REQUIRE(countField != nullptr);
  REQUIRE(fileField != nullptr);
  REQUIRE(laneField != nullptr);
  REQUIRE(plainField != nullptr);
  CHECK(countField->category == primec::IrStructFieldCategory::Pod);
  CHECK(fileField->category == primec::IrStructFieldCategory::Handle);
  CHECK(laneField->category == primec::IrStructFieldCategory::GpuLane);
  CHECK(plainField->category == primec::IrStructFieldCategory::Default);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());
  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  auto decodedIt = std::find_if(decoded.structLayouts.begin(),
                                decoded.structLayouts.end(),
                                [](const primec::IrStructLayout &layout) { return layout.name == "/Payload"; });
  REQUIRE(decodedIt != decoded.structLayouts.end());
  auto decodedLane = std::find_if(decodedIt->fields.begin(),
                                  decodedIt->fields.end(),
                                  [](const primec::IrStructField &field) { return field.name == "lane"; });
  REQUIRE(decodedLane != decodedIt->fields.end());
  CHECK(decodedLane->category == primec::IrStructFieldCategory::GpuLane);
}

TEST_CASE("ir serialize roundtrip with implicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{2i32}
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
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions[0].instructions.size() == 3);
  CHECK(module.functions[0].instructions[2].op == primec::IrOpcode::ReturnVoid);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  REQUIRE(decoded.functions[0].instructions.size() == 3);
  CHECK(decoded.functions[0].instructions[2].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 7;
  REQUIRE(vm.execute(decoded, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers literal statement with pop") {
  const std::string source = R"(
[return<int>]
main() {
  1i32
  return(2i32)
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
  REQUIRE(module.functions.size() == 1);
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 4);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::Pop);
  CHECK(inst[2].op == primec::IrOpcode::PushI32);
  CHECK(inst[3].op == primec::IrOpcode::ReturnI32);
}

TEST_CASE("ir lowers i64 arithmetic") {
  const std::string source = R"(
[return<i64>]
main() {
  return(plus(1i64, 2i64))
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
  REQUIRE(module.functions.size() == 1);
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 4);
  CHECK(inst[0].op == primec::IrOpcode::PushI64);
  CHECK(inst[1].op == primec::IrOpcode::PushI64);
  CHECK(inst[2].op == primec::IrOpcode::AddI64);
  CHECK(inst[3].op == primec::IrOpcode::ReturnI64);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers u64 comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(0xFFFFFFFFFFFFFFFFu64, 1u64))
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

  bool sawCmp = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtU64) {
      sawCmp = true;
      break;
    }
  }
  CHECK(sawCmp);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers locals and assign") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  assign(value, plus(value, 3i32))
  return(value)
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
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 10);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[3].op == primec::IrOpcode::PushI32);
  CHECK(inst[4].op == primec::IrOpcode::AddI32);
  CHECK(inst[5].op == primec::IrOpcode::Dup);
  CHECK(inst[6].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[7].op == primec::IrOpcode::Pop);
  CHECK(inst[8].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[9].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowers assign in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(assign(value, 4i32))
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
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 6);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::PushI32);
  CHECK(inst[3].op == primec::IrOpcode::Dup);
  CHECK(inst[4].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[5].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowers increment in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(increment(value))
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
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 8);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[3].op == primec::IrOpcode::PushI32);
  CHECK(inst[4].op == primec::IrOpcode::AddI32);
  CHECK(inst[5].op == primec::IrOpcode::Dup);
  CHECK(inst[6].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[7].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 2);
}

TEST_CASE("ir lowers decrement in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  return(decrement(value))
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
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 8);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[3].op == primec::IrOpcode::PushI32);
  CHECK(inst[4].op == primec::IrOpcode::SubI32);
  CHECK(inst[5].op == primec::IrOpcode::Dup);
  CHECK(inst[6].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[7].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 2);
}

TEST_CASE("ir lowers enum definitions to struct layouts") {
  const std::string source = R"(
[enum]
Colors() {
  assign(Blue, 5i32)
  Red
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
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());
  auto layoutIt = std::find_if(module.structLayouts.begin(),
                               module.structLayouts.end(),
                               [](const primec::IrStructLayout &layout) { return layout.name == "/Colors"; });
  REQUIRE(layoutIt != module.structLayouts.end());
  REQUIRE(layoutIt->fields.size() == 3u);
  auto fieldByName = [&](const std::string &name) -> const primec::IrStructField * {
    auto it = std::find_if(layoutIt->fields.begin(),
                           layoutIt->fields.end(),
                           [&](const primec::IrStructField &field) { return field.name == name; });
    if (it == layoutIt->fields.end()) {
      return nullptr;
    }
    return &(*it);
  };
  const primec::IrStructField *valueField = fieldByName("value");
  const primec::IrStructField *blueField = fieldByName("Blue");
  const primec::IrStructField *redField = fieldByName("Red");
  REQUIRE(valueField != nullptr);
  REQUIRE(blueField != nullptr);
  REQUIRE(redField != nullptr);
  CHECK(valueField->isStatic == false);
  CHECK(blueField->isStatic == true);
  CHECK(redField->isStatic == true);
}
