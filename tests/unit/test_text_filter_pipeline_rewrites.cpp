#include "primec/TextFilterPipeline.h"

#include "third_party/doctest.h"

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
