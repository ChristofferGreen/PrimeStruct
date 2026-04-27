TEST_CASE("ir emits struct layout metadata") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
  [i64] count{2i64}
}

[return<void>]
main() {
  Thing{}
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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

TEST_CASE("ir emits std math matrix layout metadata") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] value{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  return(convert<int>(value.m10))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  auto layoutIt = std::find_if(module.structLayouts.begin(),
                               module.structLayouts.end(),
                               [](const primec::IrStructLayout &layout) { return layout.name == "/std/math/Mat2"; });
  REQUIRE(layoutIt != module.structLayouts.end());
  REQUIRE(layoutIt->fields.size() == 4u);
  CHECK(layoutIt->fields[0].name == "m00");
  CHECK(layoutIt->fields[0].envelope == "f32");
  CHECK(layoutIt->fields[1].name == "m01");
  CHECK(layoutIt->fields[1].envelope == "f32");
  CHECK(layoutIt->fields[2].name == "m10");
  CHECK(layoutIt->fields[2].envelope == "f32");
  CHECK(layoutIt->fields[3].name == "m11");
  CHECK(layoutIt->fields[3].envelope == "f32");
  CHECK(layoutIt->totalSizeBytes == 16u);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());
  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  auto decodedIt = std::find_if(decoded.structLayouts.begin(),
                                decoded.structLayouts.end(),
                                [](const primec::IrStructLayout &layout) { return layout.name == "/std/math/Mat2"; });
  REQUIRE(decodedIt != decoded.structLayouts.end());
  CHECK(decodedIt->fields.size() == 4u);
  CHECK(decodedIt->totalSizeBytes == 16u);
}

TEST_CASE("ir emits std math quaternion layout metadata") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] value{Quat{0.0f32, 0.0f32, 0.0f32, 2.0f32}}
  [Quat] normalized{value.toNormalized()}
  return(convert<int>(normalized.w))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  auto layoutIt = std::find_if(module.structLayouts.begin(),
                               module.structLayouts.end(),
                               [](const primec::IrStructLayout &layout) { return layout.name == "/std/math/Quat"; });
  REQUIRE(layoutIt != module.structLayouts.end());
  REQUIRE(layoutIt->fields.size() == 4u);
  CHECK(layoutIt->fields[0].name == "x");
  CHECK(layoutIt->fields[0].envelope == "f32");
  CHECK(layoutIt->fields[1].name == "y");
  CHECK(layoutIt->fields[1].envelope == "f32");
  CHECK(layoutIt->fields[2].name == "z");
  CHECK(layoutIt->fields[2].envelope == "f32");
  CHECK(layoutIt->fields[3].name == "w");
  CHECK(layoutIt->fields[3].envelope == "f32");
  CHECK(layoutIt->totalSizeBytes == 16u);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());
  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  auto decodedIt = std::find_if(decoded.structLayouts.begin(),
                                decoded.structLayouts.end(),
                                [](const primec::IrStructLayout &layout) { return layout.name == "/std/math/Quat"; });
  REQUIRE(decodedIt != decoded.structLayouts.end());
  CHECK(decodedIt->fields.size() == 4u);
  CHECK(decodedIt->totalSizeBytes == 16u);
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
  return(Vec3{})
}

[struct]
Sphere() {
  center{makeCenter()}
  [i32] radius{4i32}
}

[return<void>]
main() {
  Sphere{}
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  Widget{}
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  Payload{}
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
