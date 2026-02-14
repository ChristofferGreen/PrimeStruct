#include "primec/IncludeResolver.h"

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {
std::string writeTemp(const std::string &name, const std::string &contents) {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests";
  std::filesystem::create_directories(dir);
  auto path = dir / name;
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}
} // namespace

TEST_SUITE_BEGIN("primestruct.includes.multiple");

TEST_CASE("expands multiple include paths") {
  const std::string libA = writeTemp("lib_multi_a.prime", "// LIB_A\n[return<int>]\nhelper_a(){ return(1i32) }\n");
  const std::string libB = writeTemp("lib_multi_b.prime", "// LIB_B\n[return<int>]\nhelper_b(){ return(2i32) }\n");
  const std::string srcPath = writeTemp("main_multi.prime",
                                        "include<\"" + libA + "\"; \"" + libB + "\",>\n"
                                        "[return<int>]\nmain(){ return(helper_a()) }\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("LIB_A") != std::string::npos);
  CHECK(source.find("LIB_B") != std::string::npos);
}

TEST_CASE("expands whitespace-separated include paths") {
  const std::string libA = writeTemp("lib_ws_a.prime", "// LIB_WS_A\n[return<int>]\nhelper_a(){ return(1i32) }\n");
  const std::string libB = writeTemp("lib_ws_b.prime", "// LIB_WS_B\n[return<int>]\nhelper_b(){ return(2i32) }\n");
  const std::string srcPath =
      writeTemp("main_ws.prime", "include<\"" + libA + "\" \"" + libB + "\">\n[return<int>]\nmain(){ return(helper_a()) }\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("LIB_WS_A") != std::string::npos);
  CHECK(source.find("LIB_WS_B") != std::string::npos);
}

TEST_CASE("ignores nested duplicate includes") {
  const std::string marker = "LIB_D_MARKER";
  const std::string libD = writeTemp("lib_nested_d.prime",
                                     "// " + marker + "\n"
                                     "[return<int>]\nhelper_d(){ return(4i32) }\n");
  const std::string libB =
      writeTemp("lib_nested_b.prime", "include<\"" + libD + "\">\n// LIB_B\n");
  const std::string libC =
      writeTemp("lib_nested_c.prime", "include<\"" + libD + "\">\n// LIB_C\n");
  const std::string srcPath = writeTemp("main_nested.prime",
                                        "include<\"" + libB + "\">\n"
                                        "include<\"" + libC + "\">\n"
                                        "[return<int>]\nmain(){ return(helper_d()) }\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find(marker);
  const auto second = source.find(marker, first == std::string::npos ? 0 : first + 1);
  CHECK(first != std::string::npos);
  CHECK(second == std::string::npos);
}

TEST_SUITE_END();
