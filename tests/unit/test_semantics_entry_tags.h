TEST_SUITE_BEGIN("primestruct.semantics.tags");

TEST_CASE("pod and handle tags conflict on definitions") {
  const std::string source = R"(
[pod handle]
Thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot be tagged as handle or gpu_lane") != std::string::npos);
}

TEST_CASE("pod definitions reject handle fields") {
  const std::string source = R"(
[pod]
Thing() {
  [handle i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot contain handle or gpu_lane fields") != std::string::npos);
}

TEST_CASE("pod and gpu_lane tags conflict on definitions") {
  const std::string source = R"(
[pod gpu_lane]
Thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot be tagged as handle or gpu_lane") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on definitions") {
  const std::string source = R"(
[handle gpu_lane]
Thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("handle definitions cannot be tagged as gpu_lane") != std::string::npos);
}

TEST_CASE("fields reject handle and gpu_lane together") {
  const std::string source = R"(
[struct]
Thing() {
  [handle gpu_lane i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as handle and gpu_lane") != std::string::npos);
}

TEST_CASE("fields reject pod and handle together") {
  const std::string source = R"(
[struct]
Thing() {
  [pod handle i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as pod and handle") != std::string::npos);
}

TEST_CASE("fields reject pod and gpu_lane together") {
  const std::string source = R"(
[struct]
Thing() {
  [pod gpu_lane i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as pod and gpu_lane") != std::string::npos);
}

TEST_CASE("handle tag does not replace binding type") {
  const std::string source = R"(
[return<int>]
main() {
  [handle i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("handle tag accepts template arg without changing type") {
  const std::string source = R"(
[return<int>]
main() {
  [handle<PathNode> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pod tag rejects template args on bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [pod<i32> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding transforms do not take template arguments") != std::string::npos);
}

TEST_CASE("duplicate static tags reject on bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [static static i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate static transform on binding") != std::string::npos);
}

TEST_CASE("duplicate visibility tags reject on bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [public private i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility transforms are mutually exclusive") != std::string::npos);
}

TEST_CASE("visibility tags reject on definitions") {
  const std::string source = R"(
[public return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
}

TEST_CASE("static tag rejects on definitions") {
  const std::string source = R"(
[static return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
}

TEST_CASE("copy tag rejects on definitions") {
  const std::string source = R"(
[copy return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("copy transform is only supported on bindings and parameters") != std::string::npos);
}

TEST_CASE("restrict tag rejects on definitions") {
  const std::string source = R"(
[restrict<i32> return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict transform is only supported on bindings and parameters") != std::string::npos);
}

TEST_SUITE_END();
