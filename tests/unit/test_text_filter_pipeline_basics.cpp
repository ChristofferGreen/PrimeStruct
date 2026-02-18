#include "primec/TextFilterPipeline.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.text_filters.pipeline.basics");

TEST_CASE("pipeline passes through text") {
  const std::string source = "include</std>\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("pipeline preserves quoted include paths") {
  const std::string source = "include<\"/std/io\">\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("pipeline preserves include with version attribute") {
  const std::string source =
      "include<\"/std/io\", version=\"1.2\">\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("pipeline preserves include with comments") {
  const std::string source =
      "include /* comment */ <\"/std/io\">\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("pipeline preserves include with tight comment") {
  const std::string source =
      "include/* comment */<\"/std/io\">\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("pipeline preserves include with payload comments") {
  const std::string source =
      "include</std/io /* > should be ignored */>\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("implicit-utf8 runs in include-like paths") {
  const std::string source = "main(){ /path/include<\"hello\"> }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"implicit-utf8"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("\"hello\"utf8") != std::string::npos);
}

TEST_CASE("pipeline preserves single-quoted include paths") {
  const std::string source = "include<'/std\\\\'>\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("pipeline preserves line comments") {
  const std::string source = "main(){ value(1i32)// a+b should stay\n return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("// a+b should stay") != std::string::npos);
  CHECK(output.find("plus(a, b)") == std::string::npos);
}

TEST_CASE("filters ignore line comments") {
  const std::string source =
      "main(){\n"
      "  value(1i32)// a+b and array<i32>{1i32,2i32} should stay\n"
      "  return(1i32)\n"
      "}\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators", "implicit-utf8", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("// a+b and array<i32>{1i32,2i32} should stay") != std::string::npos);
  CHECK(output.find("plus(a, b)") == std::string::npos);
  CHECK(output.find("array<i32>(1i32,2i32)") == std::string::npos);
}

TEST_CASE("pipeline preserves block comments") {
  const std::string source = "main(){ /* a*b should stay */ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("/* a*b should stay */") != std::string::npos);
  CHECK(output.find("multiply(a, b)") == std::string::npos);
}

TEST_CASE("filters ignore block comments") {
  const std::string source =
      "main(){\n"
      "  /* a*b and \"raw\" should stay */\n"
      "  return(1i32)\n"
      "}\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators", "implicit-utf8", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("/* a*b and \"raw\" should stay */") != std::string::npos);
  CHECK(output.find("multiply(a, b)") == std::string::npos);
  CHECK(output.find("\"raw\"utf8") == std::string::npos);
}

TEST_CASE("append_operators only modifies opted-in lists") {
  const std::string source = R"(
[return<int>]
main(){ return(1i32+2i32) }

[append_operators return<int>]
helper(){ return(1i32+2i32) }
)";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"append_operators"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("[return<int> operators]") == std::string::npos);
  CHECK(output.find("[append_operators return<int> operators]") != std::string::npos);
}

TEST_CASE("append_operators does not duplicate operators") {
  const std::string source = R"(
[append_operators operators return<int>]
main(){ return(1i32+2i32) }
)";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"append_operators"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("[append_operators operators return<int> operators]") == std::string::npos);
  CHECK(output.find("[append_operators operators return<int>]") != std::string::npos);
}

TEST_CASE("pipeline isolates lambda bodies from transforms") {
  const std::string source = R"(
[operators]
main() {
  return(1i32+2i32)
  return([]([i32] value){ return(value+3i32) })
}
)";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters.clear();
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("plus(1i32, 2i32)") != std::string::npos);
  CHECK(output.find("value+3i32") != std::string::npos);
}

TEST_SUITE_END();
