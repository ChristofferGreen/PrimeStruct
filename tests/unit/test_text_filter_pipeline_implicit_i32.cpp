#include "primec/TextFilterPipeline.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.text_filters.pipeline.implicit_i32");

TEST_CASE("adds i32 suffix to bare integer literal") {
  const std::string source = "main(){ return(42) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters.push_back("implicit-i32");
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("42i32") != std::string::npos);
}

TEST_CASE("adds i32 suffix to negative integer literal") {
  const std::string source = "main(){ return(-7) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters.push_back("implicit-i32");
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("-7i32") != std::string::npos);
}

TEST_CASE("adds i32 suffix to hex integer literal") {
  const std::string source = "main(){ return(0x2A) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters.push_back("implicit-i32");
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("0x2Ai32") != std::string::npos);
}

TEST_CASE("adds i32 suffix to negative hex integer literal") {
  const std::string source = "main(){ return(-0x2A) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters.push_back("implicit-i32");
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("-0x2Ai32") != std::string::npos);
}

TEST_CASE("rewrites plus with hex literals") {
  const std::string source = "main(){ return(0x1i32+0x2i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(0x1i32, 0x2i32)") != std::string::npos);
}

TEST_CASE("rewrites plus with comma-separated literal") {
  const std::string source = "main(){ return(1,000+2) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("plus(1,000i32, 2i32)") != std::string::npos);
}

TEST_CASE("does not change suffixed integer literals") {
  const std::string source = "main(){ return(42i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not add suffix to i64 or u64 literals with implicit filter") {
  const std::string source = "main(){ return(9i64); return(10u64) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("implicit i32 filter skips float with trailing dot") {
  const std::string source = "main(){ return(1.) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("implicit i32 filter skips float with suffix after dot") {
  const std::string source = "main(){ return(1.f32) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("implicit i32 filter skips float with exponent after dot") {
  const std::string source = "main(){ return(1.e2) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite numbers inside strings") {
  const std::string source = "main(){ log(\"42\") }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == "main(){ log(\"42\"utf8) }\n");
}

TEST_CASE("does not rewrite numbers inside raw strings") {
  const std::string source = "main(){ log(\"42+7\"raw_utf8) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == "main(){ log(\"42+7\"raw_utf8) }\n");
}

TEST_CASE("does not add suffix to float literal") {
  const std::string source = "main(){ return(1.5f) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not add suffix to float literal without suffix") {
  const std::string source = "main(){ return(2.5) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not add suffix to float literal with exponent") {
  const std::string source = "main(){ return(1e3) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not add suffix before identifier characters") {
  const std::string source = "main(){ return(1foo) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not add suffix before dot access") {
  const std::string source = "main(){ return(1.value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite collection literal name prefixes") {
  const std::string source = "main(){ return(arrayish{1i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_SUITE_END();
