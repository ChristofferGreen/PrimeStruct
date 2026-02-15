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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.text_filters.pipeline.rewrites");

TEST_CASE("rewrites divide operator without spaces") {
  const std::string source = "main(){ return(a/b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("divide(a, b)") != std::string::npos);
}

TEST_CASE("does not rewrite slash paths") {
  const std::string source = "main(){ return(/math/sin(0.0f)) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("/math/sin(0.0f)") != std::string::npos);
  CHECK(output.find("divide(/math") == std::string::npos);
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

TEST_CASE("rewrites array literal brackets to call") {
  const std::string source = "main(){ return(array<i32>[1i32,2i32]) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("array<i32>(1i32,2i32)") != std::string::npos);
}

TEST_CASE("array bracket literal rewrite does not consume following indexing") {
  const std::string source = "main(){ return(array<i32>[1i32,2i32][0i32]) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("array<i32>(1i32,2i32)[0i32]") != std::string::npos);
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

TEST_CASE("rewrites map literal brackets to call") {
  const std::string source = "main(){ return(map<i32,i32>[1i32,2i32]) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32,2i32)") != std::string::npos);
}

TEST_CASE("rewrites vector literal braces to call") {
  const std::string source = "main(){ return(vector<i32>{1i32,2i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("vector<i32>(1i32,2i32)") != std::string::npos);
}

TEST_CASE("rewrites vector literal brackets to call") {
  const std::string source = "main(){ return(vector<i32>[1i32,2i32]) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("vector<i32>(1i32,2i32)") != std::string::npos);
}

TEST_CASE("rewrites map literal brackets with equals pairs") {
  const std::string source = "main(){ return(map<i32, i32>[1i32=2i32, 3i32=4i32]) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32, i32>(1i32, 2i32, 3i32, 4i32)") != std::string::npos);
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

TEST_CASE("rewrites map literal with semicolon pairs") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=2i32; 3i32=4i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, 2i32, 3i32, 4i32)") != std::string::npos);
}

TEST_CASE("rewrites map literal with call key equals pairs") {
  const std::string source = "main(){ return(map<i32,i32>{hash(1i32)=2i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(hash(1i32), 2i32)") != std::string::npos);
}

TEST_CASE("rewrites map literal with whitespace pairs") {
  const std::string source = "main(){ return(map<i32,i32>{1i32 2i32 3i32 4i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, 2i32, 3i32, 4i32)") != std::string::npos);
}

TEST_CASE("map literal rewrite ignores line comments") {
  const std::string source =
      "main(){\n"
      "  return(map<i32,i32>{1i32=2i32 // map-comment a=b\n"
      "  3i32=4i32})\n"
      "}\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("// map-comment a=b") != std::string::npos);
}

TEST_CASE("map literal rewrite ignores block comments") {
  const std::string source =
      "main(){ return(map<i32,i32>{1i32=2i32 /* map-comment a=b */ 3i32=4i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("/* map-comment a=b */") != std::string::npos);
}

TEST_CASE("filter order affects map literal rewrites") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=2i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;

  primec::TextFilterOptions operatorsFirst;
  operatorsFirst.enabledFilters = {"operators", "collections"};
  CHECK(pipeline.apply(source, output, error, operatorsFirst));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(assign(1i32, 2i32))") != std::string::npos);

  primec::TextFilterOptions collectionsFirst;
  collectionsFirst.enabledFilters = {"collections", "operators"};
  std::string output2;
  CHECK(pipeline.apply(source, output2, error, collectionsFirst));
  CHECK(error.empty());
  CHECK(output2.find("map<i32,i32>(1i32, 2i32)") != std::string::npos);
}

TEST_CASE("default filters keep map literal pairs") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=2i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, 2i32)") != std::string::npos);
  CHECK(output.find("assign(") == std::string::npos);
}

TEST_CASE("rewrites map literal equals pairs without commas") {
  const std::string source = "main(){ return(map<i32,i32>{1i32=2i32 3i32=4i32}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, 2i32 3i32, 4i32)") != std::string::npos);
}

TEST_CASE("rewrites map literal brackets with string keys") {
  const std::string source = "main(){ return(map<string, i32>[\"a\"=1i32, \"b\"=2i32]) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<string, i32>(\"a\"utf8, 1i32, \"b\"utf8, 2i32)") != std::string::npos);
}

TEST_CASE("does not rewrite bracket collections without template list") {
  const std::string source = "main(){ return(array[1i32]) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
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
      "main(){ return(map<i32,i32>{1i32=make_color([hue] 1i32, [value] 2i32)}) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output.find("map<i32,i32>(1i32, make_color([hue] 1i32, [value] 2i32))") != std::string::npos);
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

TEST_SUITE_END();

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

TEST_CASE("does not rewrite spaced slash") {
  const std::string source = "main(){ return(a / b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite spaced plus") {
  const std::string source = "main(){ return(a + b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite spaced minus") {
  const std::string source = "main(){ return(a - b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite spaced multiply") {
  const std::string source = "main(){ return(a * b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite spaced less_than") {
  const std::string source = "main(){ return(a < b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_CASE("does not rewrite spaced comparisons and boolean ops") {
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
  CHECK(output == source);
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

TEST_CASE("does not rewrite operators around block comments") {
  const std::string source = "main(){ return(a/*x*/b) }\n";
  primec::TextFilterPipeline pipeline;
  std::string output;
  std::string error;
  CHECK(pipeline.apply(source, output, error));
  CHECK(error.empty());
  CHECK(output == source);
}

TEST_SUITE_END();

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
