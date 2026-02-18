#include "primec/TextFilterPipeline.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.text_filters.pipeline.collections");

TEST_CASE("rewrites array literal braces") {
  const std::string source = "main(){ array<i32>{1i32, 2i32} }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("array<i32>(1i32, 2i32)") != std::string::npos);
}

TEST_CASE("rewrites map literal equals pairs") {
  const std::string source = "main(){ map<i32, i32>{1i32=2i32, 3i32=4i32} }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32, i32>(1i32, 2i32, 3i32, 4i32)") != std::string::npos);
}

TEST_CASE("map literal preserves equality operators") {
  const std::string source = "main(){ map<i32, i32>{1i32=2i32, 3i32==4i32} }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("equal(3i32, 4i32)") != std::string::npos);
}

TEST_CASE("map literal preserves nested assignment operators in values") {
  const std::string source = "main(){ map<i32, i32>{1i32=value=2i32} }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32, i32>(1i32, assign(value, 2i32))") != std::string::npos);
}

TEST_CASE("rewrites nested expressions inside array literal") {
  const std::string source = "main(){ array<i32>{1i32+2i32} }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("array<i32>(plus(1i32, 2i32))") != std::string::npos);
}

TEST_CASE("rewrites map literal with string keys") {
  const std::string source = "main(){ map<string, i32>{\"a\"=1i32, \"b\"=2i32} }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<string, i32>(\"a\"utf8, 1i32, \"b\"utf8, 2i32)") != std::string::npos);
}

TEST_CASE("map literal handles nested braces") {
  const std::string source = "main(){ map<i32, i32>{1i32=plus(2i32, 3i32), 4i32=5i32} }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32, i32>(1i32, plus(2i32, 3i32), 4i32, 5i32)") != std::string::npos);
}

TEST_CASE("fails on unterminated collection literal") {
  const std::string source = "array<i32>{1i32";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  (void)pipeline.apply(source, output, error);
  CHECK(error.find("unterminated collection literal") != std::string::npos);
}

TEST_CASE("fails on unterminated block comment in map literal") {
  const std::string source = "map<i32,i32>{1i32=2i32 /* comment }";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK_FALSE(pipeline.apply(source, output, error));
  CHECK(error.find("unterminated block comment") != std::string::npos);
}

TEST_CASE("map literal whitespace pairs handle line comments") {
  const std::string source =
      "main(){ return(map<i32,i32>{1i32 2i32 // pair comment\n"
      " 3i32 4i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("// pair comment") != std::string::npos);
  CHECK(output.find("map<i32,i32>(1i32, 2i32") != std::string::npos);
  CHECK(output.find(", 3i32, 4i32") != std::string::npos);
}

TEST_CASE("fails on unterminated template list in collection literal") {
  const std::string source = "array<i32{1i32}";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  (void)pipeline.apply(source, output, error);
  CHECK(error.find("unterminated template list in collection literal") != std::string::npos);
}

TEST_CASE("rewrites plus operator with exponent float") {
  const std::string source = "main(){ return(1e-3+2i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(1e-3, 2i32)") != std::string::npos);
}

TEST_CASE("does not add suffix without implicit filter") {
  const std::string source = "main(){ return(42) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_SUITE_END();
