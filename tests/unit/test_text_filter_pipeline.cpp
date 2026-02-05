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

TEST_CASE("rewrites array literal braces to call") {
  const std::string source = "main(){ return(array<i32>{1i32,2i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("array<i32>(1i32,2i32)") != std::string::npos);
}

TEST_CASE("rewrites map literal braces to call") {
  const std::string source = "main(){ return(map<i32,i32>{1i32,2i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32,2i32)") != std::string::npos);
}

TEST_CASE("rewrites map literal with equals pairs") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=2i32,3i32=4i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, 2i32,3i32, 4i32)") != std::string::npos);
}

TEST_CASE("map literal preserves assignment in values") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=assign(a,2i32)}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, assign(a,2i32))") != std::string::npos);
}

TEST_CASE("rewrites map literal with string keys") {
  const std::string source = "main(){ return(map<i32,i32>{\"a\"=1i32,\"b\"=2i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(\"a\"utf8, 1i32,\"b\"utf8, 2i32)") != std::string::npos);
}

TEST_CASE("map literal preserves equality operators") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=2i32==3i32,4i32=5i32>=6i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, equal(2i32, 3i32),4i32, greater_equal(5i32, 6i32))") !=
        std::string::npos);
}

TEST_CASE("map literal preserves named args in values") {
  const std::string source =
      "main(){ return(map<i32,i32>{1i32=make_color(hue = 1i32, value = 2i32)}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, make_color(hue = 1i32, value = 2i32))") != std::string::npos);
}

TEST_CASE("map literal rewrites nested map literals") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=map<i32,i32>{2i32=3i32}}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, map<i32,i32>(2i32, 3i32))") != std::string::npos);
}

TEST_CASE("map literal rewrites whitespace-heavy pairs") {
  const std::string source =
      "main(){ return(map<i32,i32>{\n  1i32 = 2i32,\n  3i32 = 4i32\n}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(\n  1i32 ,  2i32,\n  3i32 ,  4i32\n)") != std::string::npos);
}

TEST_CASE("rewrites plus operator with call operands") {
  const std::string source = "main(){ return(foo()+bar()) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(foo(), bar())") != std::string::npos);
}

TEST_CASE("rewrites multiply with parenthesized operand") {
  const std::string source = "main(){ return((1i32+2i32)*3i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("multiply(plus(1i32, 2i32), 3i32)") != std::string::npos);
}

TEST_CASE("rewrites plus operator with negative literal") {
  const std::string source = "main(){ return(-1i32+2i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(-1i32, 2i32)") != std::string::npos);
}

TEST_CASE("rewrites plus operator with unary minus operand") {
  const std::string source = "main(){ return(a+-b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(a, negate(b))") != std::string::npos);
}

TEST_CASE("rewrites plus operator with leading unary minus name") {
  const std::string source = "main(){ return(-value+2i32) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(negate(value), 2i32)") != std::string::npos);
}

TEST_CASE("rewrites plus operator with string literals") {
  const std::string source = "main(){ return(\"a\"+\"b\") }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(\"a\"utf8, \"b\"utf8)") != std::string::npos);
}

TEST_CASE("rewrites plus operator with raw string literals") {
  const std::string source = "main(){ return(R\"(a)\"+R\"(b)\") }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("plus(R\"(a)\"utf8, R\"(b)\"utf8)") != std::string::npos);
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
  options.enabledFilters = {"operators", "collections", "implicit-i32"};
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
  const std::string source = "main(){ log(R\"(42+7)\") }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == "main(){ log(R\"(42+7)\"utf8) }\n");
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
  options.enabledFilters = {"operators", "collections"};
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error, options));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_SUITE_END();
