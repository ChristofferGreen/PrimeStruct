#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.conversions");

// `convert<T>` between numeric types (docs/spec/type-system.md, Numeric conversions). An integer narrows modulo 2^N,
// like a C++ unsigned conversion; a float converts to an integer by truncating toward zero, saturating at the
// target's range and mapping NaN to 0. Every program runs on each backend and prints one converted value per row. The
// operands are `mut` locals so no pass folds a conversion away before the backend sees it.

namespace {

struct Row {
  std::string srcType;
  std::string init;
  std::string dstType;
  std::string expected;
};

void runConversionRows(const std::string &name,
                       const std::vector<Row> &rows,
                       const std::vector<std::string> &skipConfigs = {}) {
  program_matrix::ProgramCase program;
  program.name = name;
  std::string body;
  std::string expected;
  int index = 0;
  for (const Row &row : rows) {
    const std::string var = "v" + std::to_string(index++);
    body += "  [" + row.srcType + " mut] " + var + "{" + row.init + "}\n";
    body += "  print_line(convert<" + row.dstType + ">(" + var + "))\n";
    expected += row.expected + "\n";
  }
  program.source = "[effects(io_out), return<int>]\nmain() {\n" + body + "  return(0i32)\n}\n";
  program.exitCode = 0;
  program.stdoutText = expected;
  program.skipConfigs = skipConfigs;
  program_matrix::runProgramMatrix(program);
}

// x86_64 native has no byte (u8) support yet.
std::vector<std::string> u8SkipConfigs() {
#if defined(__x86_64__)
  return {"native-O0", "native-O2"};
#else
  return {};
#endif
}

} // namespace

TEST_CASE("narrowing an i64 to i32 wraps modulo 2^32") {
  runConversionRows("conv_i64_to_i32",
                    {
                        {"i64", "4294967297i64", "i32", "1"},
                        {"i64", "4294967295i64", "i32", "-1"},
                        {"i64", "2147483647i64", "i32", "2147483647"},
                        {"i64", "2147483648i64", "i32", "-2147483648"},
                        {"i64", "-2147483649i64", "i32", "2147483647"},
                        {"i64", "9223372036854775807i64", "i32", "-1"},
                        {"i64", "-9223372036854775807i64", "i32", "1"},
                        {"i64", "4294967296i64", "i32", "0"},
                    });
}

TEST_CASE("narrowing a u64 to i32 wraps modulo 2^32") {
  runConversionRows("conv_u64_to_i32",
                    {
                        {"u64", "4294967297u64", "i32", "1"},
                        {"u64", "4294967296u64", "i32", "0"},
                        {"u64", "2147483648u64", "i32", "-2147483648"},
                        {"u64", "18446744073709551615u64", "i32", "-1"},
                        {"u64", "9223372036854775808u64", "i32", "0"},
                    });
}

TEST_CASE("a narrowed i32 equals the same literal and widens to its wrapped value") {
  program_matrix::ProgramCase program;
  program.name = "conv_narrowed_i32_value";
  program.source = R"(
[effects(io_out), return<int>]
main() {
  [i64 mut] big{4294967297i64}
  [i32] x{convert<i32>(big)}
  print_line(x == 1i32)
  print_line(convert<i64>(x))
  print_line(convert<u64>(x))
  [u64 mut] huge{18446744073709551615u64}
  [i32] y{convert<i32>(huge)}
  print_line(y == -1i32)
  print_line(convert<i64>(y))
  print_line(convert<u64>(y))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n1\n1\n1\n-1\n18446744073709551615\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("narrowing an integer to u8 wraps modulo 256") {
  runConversionRows("conv_integer_to_u8",
                    {
                        {"i32", "300i32", "u8", "44"},
                        {"i32", "-1i32", "u8", "255"},
                        {"i32", "256i32", "u8", "0"},
                        {"i32", "255i32", "u8", "255"},
                        {"i32", "-129i32", "u8", "127"},
                        {"i64", "4294967551i64", "u8", "255"},
                        {"i64", "-9223372036854775807i64", "u8", "1"},
                        {"u64", "257u64", "u8", "1"},
                        {"u64", "18446744073709551615u64", "u8", "255"},
                    },
                    u8SkipConfigs());
}

TEST_SUITE_END();
