#include "primec/TextFilterPipeline.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.text_filters.pipeline.implicit_utf8");

TEST_CASE("implicit utf8 appends to single-quoted strings") {
  const std::string source = "main(){ return('a') }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == "main(){ return('a'utf8) }\n");
}

TEST_CASE("implicit utf8 preserves explicit suffix") {
  const std::string source = "main(){ return(\"a\"ascii) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("implicit utf8 preserves raw_ascii suffix") {
  const std::string source = "main(){ return(\"a\"raw_ascii) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("implicit utf8 can be disabled") {
  const std::string source = "main(){ return(\"a\") }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"collections", "operators"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("rewrites plus operator with raw string literals") {
  const std::string source = "main(){ return(\"a\"raw_utf8+\"b\"raw_utf8) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(\"a\"raw_utf8, \"b\"raw_utf8)") != std::string::npos);
}

TEST_CASE("rewrites multiply operator with unary minus operand") {
  const std::string source = "main(){ return(a*-b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("multiply(a, negate(b))") != std::string::npos);
}

TEST_CASE("rewrites assign with unary minus operand") {
  const std::string source = "main(){ value=-b }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("assign(value, negate(b))") != std::string::npos);
}

TEST_CASE("rewrites multiply with negative numeric literal") {
  const std::string source = "main(){ return(1i32*-2i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("multiply(1i32, -2i32)") != std::string::npos);
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

TEST_CASE("rewrites not operator with numeric literal") {
  const std::string source = "main(){ return(!1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("not(1i32)") != std::string::npos);
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

TEST_CASE("rewrites not operator before templated call") {
  const std::string source = "main(){ return(!convert<i32>(1i64)) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("not(convert<i32>(1i64))") != std::string::npos);
}

TEST_CASE("rewrites chained not operators") {
  const std::string source = "main(){ return(!!flag) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("not(not(flag))") != std::string::npos);
}

TEST_CASE("rewrites chained not operators with implicit i32") {
  const std::string source = "main(){ return(!!!1) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"operators", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("not(not(not(1i32)))") != std::string::npos);
}

TEST_CASE("rewrites not operator before negative literal") {
  const std::string source = "main(){ return(!-1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("not(-1i32)") != std::string::npos);
}

TEST_CASE("rewrites not operator before negative literal with implicit i32") {
  const std::string source = "main(){ return(!-1) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"operators", "implicit-i32"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output.find("not(-1i32)") != std::string::npos);
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

TEST_CASE("rewrites prefix increment operator") {
  const std::string source = "main(){ return(++value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("increment(value)") != std::string::npos);
}

TEST_CASE("rewrites postfix increment operator") {
  const std::string source = "main(){ return(value++) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("increment(value)") != std::string::npos);
}

TEST_CASE("rewrites prefix decrement operator") {
  const std::string source = "main(){ return(--value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("decrement(value)") != std::string::npos);
}

TEST_CASE("rewrites postfix decrement operator") {
  const std::string source = "main(){ return(value--) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("decrement(value)") != std::string::npos);
}

TEST_CASE("rewrites plus with templated call on left") {
  const std::string source = "main(){ return(convert<i32>(1i64)+2i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(convert<i32>(1i64), 2i32)") != std::string::npos);
}

TEST_CASE("rewrites plus with templated call on right") {
  const std::string source = "main(){ return(1i32+convert<i32>(2i64)) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(1i32, convert<i32>(2i64))") != std::string::npos);
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

TEST_CASE("rewrites location before name") {
  const std::string source = "main(){ return(&value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("location(value)") != std::string::npos);
}

TEST_CASE("rewrites dereference before name") {
  const std::string source = "main(){ return(*value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("dereference(value)") != std::string::npos);
}

TEST_CASE("does not rewrite template list syntax") {
  const std::string source = "if<bool>(cond, then(){ }, else(){ })\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite nested template lists") {
  const std::string source = "main(){ return(convert<map<i32, array<i32>>>(1i32)) }\n";
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"operators"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("rewrites spaced slash") {
  const std::string source = "main(){ return(a / b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("divide(a, b)") != std::string::npos);
}

TEST_CASE("rewrites spaced plus") {
  const std::string source = "main(){ return(a + b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(a, b)") != std::string::npos);
}

TEST_CASE("rewrites spaced minus") {
  const std::string source = "main(){ return(a - b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("minus(a, b)") != std::string::npos);
}

TEST_CASE("rewrites spaced multiply") {
  const std::string source = "main(){ return(a * b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("multiply(a, b)") != std::string::npos);
}

TEST_CASE("rewrites spaced less_than") {
  const std::string source = "main(){ return(a < b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("less_than(a, b)") != std::string::npos);
}

TEST_CASE("rewrites spaced comparisons and boolean ops") {
  const std::string source =
      "main(){\n"
      "  return(a > b)\n"
      "  return(a <= b)\n"
      "  return(a >= b)\n"
      "  return(a == b)\n"
      "  return(a != b)\n"
      "  return(a && b)\n"
      "  return(a || b)\n"
      "  return(a = b)\n"
      "}\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("greater_than(a, b)") != std::string::npos);
  CHECK(output.find("less_equal(a, b)") != std::string::npos);
  CHECK(output.find("greater_equal(a, b)") != std::string::npos);
  CHECK(output.find("equal(a, b)") != std::string::npos);
  CHECK(output.find("not_equal(a, b)") != std::string::npos);
  CHECK(output.find("and(a, b)") != std::string::npos);
  CHECK(output.find("or(a, b)") != std::string::npos);
  CHECK(output.find("assign(a, b)") != std::string::npos);
}

TEST_CASE("does not rewrite spaced not operator") {
  const std::string source = "main(){ return(! a) }\n";
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

TEST_CASE("does not rewrite block comments") {
  const std::string source = "/* a/b */\nmain(){ return(1i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("rewrites unary minus after block comments") {
  const std::string source = "main(){ return(/* x */-value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == "main(){ return(/* x */negate(value)) }\n");
}

TEST_CASE("rewrites address-of after block comments") {
  const std::string source = "main(){ return(/* x */&value) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == "main(){ return(/* x */location(value)) }\n");
}

TEST_CASE("does not rewrite operators around block comments") {
  const std::string source = "main(){ return(a/*x*/b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite operators after block comments") {
  const std::string source = "main(){ return(a/*x*/+b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite operators before block comments") {
  const std::string source = "main(){ return(a+/*x*/b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_SUITE_END();
