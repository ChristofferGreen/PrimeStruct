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
  // Wasm runs the rows of these programs, the u8 rows included (`ZextU8` cuts the byte).
  program.extraConfigs = {"wasm-wasi"};
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

// The float rows share the non-finite operands.
const std::string Nan64 = "0.0f64 / 0.0f64";
const std::string Inf64 = "1.0f64 / 0.0f64";
const std::string NegInf64 = "-1.0f64 / 0.0f64";
const std::string Nan32 = "0.0f32 / 0.0f32";
const std::string Inf32 = "1.0f32 / 0.0f32";
const std::string NegInf32 = "-1.0f32 / 0.0f32";

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
  program.extraConfigs = {"wasm-wasi"};
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

TEST_CASE("a float converts to i32 by truncating and saturating") {
  runConversionRows("conv_float_to_i32",
                    {
                        {"f64", "2.9f64", "i32", "2"},
                        {"f64", "-2.9f64", "i32", "-2"},
                        {"f64", "-0.5f64", "i32", "0"},
                        {"f64", "2147483647.0f64", "i32", "2147483647"},
                        {"f64", "2147483648.0f64", "i32", "2147483647"},
                        {"f64", "-2147483648.0f64", "i32", "-2147483648"},
                        {"f64", "-2147483649.0f64", "i32", "-2147483648"},
                        {"f64", "3000000000.0f64", "i32", "2147483647"},
                        {"f64", "-1.0e30f64", "i32", "-2147483648"},
                        {"f64", Inf64, "i32", "2147483647"},
                        {"f64", NegInf64, "i32", "-2147483648"},
                        {"f64", Nan64, "i32", "0"},
                        {"f32", "2.9f32", "i32", "2"},
                        {"f32", "2147483520.0f32", "i32", "2147483520"},
                        {"f32", "2147483648.0f32", "i32", "2147483647"},
                        {"f32", "-2147483648.0f32", "i32", "-2147483648"},
                        {"f32", "3.0e9f32", "i32", "2147483647"},
                        {"f32", "-3.0e9f32", "i32", "-2147483648"},
                        {"f32", Inf32, "i32", "2147483647"},
                        {"f32", NegInf32, "i32", "-2147483648"},
                        {"f32", Nan32, "i32", "0"},
                    });
}

TEST_CASE("a float converts to i64 by truncating and saturating") {
  runConversionRows("conv_float_to_i64",
                    {
                        {"f64", "2.9f64", "i64", "2"},
                        {"f64", "-2.9f64", "i64", "-2"},
                        {"f64", "4.5e18f64", "i64", "4500000000000000000"},
                        {"f64", "9223372036854775808.0f64", "i64", "9223372036854775807"},
                        {"f64", "-9223372036854775808.0f64", "i64", "-9223372036854775808"},
                        {"f64", "-9.3e18f64", "i64", "-9223372036854775808"},
                        {"f64", "1.0e30f64", "i64", "9223372036854775807"},
                        {"f64", Inf64, "i64", "9223372036854775807"},
                        {"f64", NegInf64, "i64", "-9223372036854775808"},
                        {"f64", Nan64, "i64", "0"},
                        {"f32", "3.0e9f32", "i64", "3000000000"},
                        {"f32", "-3.0e9f32", "i64", "-3000000000"},
                        {"f32", "9223372036854775808.0f32", "i64", "9223372036854775807"},
                        {"f32", "-9223372036854775808.0f32", "i64", "-9223372036854775808"},
                        {"f32", "1.0e30f32", "i64", "9223372036854775807"},
                        {"f32", Inf32, "i64", "9223372036854775807"},
                        {"f32", NegInf32, "i64", "-9223372036854775808"},
                        {"f32", Nan32, "i64", "0"},
                    });
}

TEST_CASE("a float converts to u64 by truncating, saturating and clamping negatives to 0") {
  runConversionRows("conv_float_to_u64",
                    {
                        {"f64", "2.9f64", "u64", "2"},
                        {"f64", "-0.5f64", "u64", "0"},
                        {"f64", "-1.0f64", "u64", "0"},
                        {"f64", "1.0e19f64", "u64", "10000000000000000000"},
                        {"f64", "9223372036854775808.0f64", "u64", "9223372036854775808"},
                        {"f64", "18446744073709551616.0f64", "u64", "18446744073709551615"},
                        {"f64", "1.0e30f64", "u64", "18446744073709551615"},
                        {"f64", Inf64, "u64", "18446744073709551615"},
                        {"f64", NegInf64, "u64", "0"},
                        {"f64", Nan64, "u64", "0"},
                        {"f32", "3.0e9f32", "u64", "3000000000"},
                        {"f32", "-1.0f32", "u64", "0"},
                        {"f32", "9223372036854775808.0f32", "u64", "9223372036854775808"},
                        {"f32", "18446744073709551616.0f32", "u64", "18446744073709551615"},
                        {"f32", "1.0e30f32", "u64", "18446744073709551615"},
                        {"f32", Inf32, "u64", "18446744073709551615"},
                        {"f32", NegInf32, "u64", "0"},
                        {"f32", Nan32, "u64", "0"},
                    });
}

TEST_CASE("a float converts to u8 by saturating at 0 and 255") {
  runConversionRows("conv_float_to_u8",
                    {
                        {"f64", "65.7f64", "u8", "65"},
                        {"f64", "254.9f64", "u8", "254"},
                        {"f64", "255.0f64", "u8", "255"},
                        {"f64", "256.0f64", "u8", "255"},
                        {"f64", "300.0f64", "u8", "255"},
                        {"f64", "-5.0f64", "u8", "0"},
                        {"f64", "-0.5f64", "u8", "0"},
                        {"f64", "1.0e30f64", "u8", "255"},
                        {"f64", Inf64, "u8", "255"},
                        {"f64", NegInf64, "u8", "0"},
                        {"f64", Nan64, "u8", "0"},
                        {"f32", "65.7f32", "u8", "65"},
                        {"f32", "300.0f32", "u8", "255"},
                        {"f32", "-5.0f32", "u8", "0"},
                        {"f32", Inf32, "u8", "255"},
                        {"f32", NegInf32, "u8", "0"},
                        {"f32", Nan32, "u8", "0"},
                    },
                    u8SkipConfigs());
}

TEST_SUITE_END();
