#include "primec/TextFilterPipeline.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.text_filters");

TEST_CASE("pipeline passes through text") {
  const std::string source = "include</std>\n[return<int>]\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("rewrites divide operator without spaces") {
  const std::string source = "main(){ return(a/b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("divide(a, b)") != std::string::npos);
}

TEST_CASE("rewrites plus operator without spaces") {
  const std::string source = "main(){ return(a+b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(a, b)") != std::string::npos);
}

TEST_CASE("rewrites minus operator without spaces") {
  const std::string source = "main(){ return(a-b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("minus(a, b)") != std::string::npos);
}

TEST_CASE("rewrites multiply operator without spaces") {
  const std::string source = "main(){ return(a*b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("multiply(a, b)") != std::string::npos);
}

TEST_CASE("rewrites greater_than operator without spaces") {
  const std::string source = "main(){ return(a>b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("greater_than(a, b)") != std::string::npos);
}

TEST_CASE("rewrites equal operator without spaces") {
  const std::string source = "main(){ return(a==b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("equal(a, b)") != std::string::npos);
}

TEST_CASE("rewrites not_equal operator without spaces") {
  const std::string source = "main(){ return(a!=b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("not_equal(a, b)") != std::string::npos);
}

TEST_CASE("rewrites less_than operator without spaces") {
  const std::string source = "main(){ return(a<b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("less_than(a, b)") != std::string::npos);
}

TEST_CASE("rewrites greater_equal operator without spaces") {
  const std::string source = "main(){ return(a>=b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("greater_equal(a, b)") != std::string::npos);
}

TEST_CASE("rewrites less_equal operator without spaces") {
  const std::string source = "main(){ return(a<=b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("less_equal(a, b)") != std::string::npos);
}

TEST_CASE("rewrites and operator without spaces") {
  const std::string source = "main(){ return(a&&b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("and(a, b)") != std::string::npos);
}

TEST_CASE("rewrites or operator without spaces") {
  const std::string source = "main(){ return(a||b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("or(a, b)") != std::string::npos);
}

TEST_CASE("does not rewrite template list syntax") {
  const std::string source = "if<bool>(cond, then{ }, else{ })\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite spaced slash") {
  const std::string source = "main(){ return(a / b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite unary minus") {
  const std::string source = "main(){ return(-1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite path tokens") {
  const std::string source = "use(/foo/bar)\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite line comments") {
  const std::string source = "// a/b\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("adds i32 suffix to bare integer literal") {
  const std::string source = "main(){ return(42) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("42i32") != std::string::npos);
}

TEST_CASE("adds i32 suffix to negative integer literal") {
  const std::string source = "main(){ return(-7) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("-7i32") != std::string::npos);
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

TEST_CASE("does not rewrite numbers inside strings") {
  const std::string source = "main(){ log(\"42\") }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not add suffix when disabled") {
  const std::string source = "main(){ return(42) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.implicitI32Suffix = false;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_SUITE_END();
