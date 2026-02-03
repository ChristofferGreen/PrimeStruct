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

TEST_CASE("rewrites plus operator with float literals") {
  const std::string source = "main(){ return(1.5f+2.5f) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(1.5f, 2.5f)") != std::string::npos);
}

TEST_CASE("rewrites plus operator with negative literal") {
  const std::string source = "main(){ return(-1+2) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(-1i32, 2i32)") != std::string::npos);
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

TEST_CASE("rewrites assign operator without spaces") {
  const std::string source = "main(){ value=2i32 }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("assign(value, 2i32)") != std::string::npos);
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

TEST_CASE("rewrites not operator without spaces") {
  const std::string source = "main(){ return(!a) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("not(a)") != std::string::npos);
}

TEST_CASE("rewrites not operator before parentheses") {
  const std::string source = "main(){ return(!(a)) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("not(a)") != std::string::npos);
}

TEST_CASE("rewrites unary minus before name") {
  const std::string source = "main(){ return(-value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("negate(value)") != std::string::npos);
}

TEST_CASE("rewrites unary minus before parentheses") {
  const std::string source = "main(){ return(-(value)) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("negate(value)") != std::string::npos);
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

TEST_CASE("does not rewrite negative numeric literal") {
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

TEST_CASE("adds i32 suffix to hex integer literal") {
  const std::string source = "main(){ return(0x2A) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("0x2Ai32") != std::string::npos);
}

TEST_CASE("adds i32 suffix to negative hex integer literal") {
  const std::string source = "main(){ return(-0x2A) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("-0x2Ai32") != std::string::npos);
}

TEST_CASE("rewrites plus with hex literals") {
  const std::string source = "main(){ return(0x1+0x2) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(0x1i32, 0x2i32)") != std::string::npos);
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

TEST_CASE("does not rewrite numbers inside raw strings") {
  const std::string source = "main(){ log(R\"(42+7)\") }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
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

TEST_CASE("rewrites plus operator with exponent float") {
  const std::string source = "main(){ return(1e-3+2) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(1e-3, 2i32)") != std::string::npos);
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
