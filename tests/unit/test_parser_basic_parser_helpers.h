#pragma once

#include "test_parser_basic_helpers.h"

#include "primec/testing/ParserHelpers.h"

TEST_CASE("parser helper recognizes core builtin names") {
  CHECK(primec::parser::isBuiltinName("assign", false));
  CHECK(primec::parser::isBuiltinName("take", false));
}

TEST_CASE("parser helper validates math builtin qualification") {
  CHECK(primec::parser::isBuiltinName("sin", true));
  CHECK_FALSE(primec::parser::isBuiltinName("sin", false));
  CHECK(primec::parser::isBuiltinName("/std/math/sin", false));
  CHECK_FALSE(primec::parser::isBuiltinName("std/math/not_builtin", false));
}

TEST_CASE("parser helper validates gpu builtin qualification") {
  CHECK(primec::parser::isBuiltinName("/std/gpu/dispatch", false));
  CHECK_FALSE(primec::parser::isBuiltinName("dispatch", false));
  CHECK_FALSE(primec::parser::isBuiltinName("std/gpu/not_builtin", false));
}

TEST_CASE("parser helper validates memory intrinsic qualification") {
  CHECK(primec::parser::isBuiltinName("/std/intrinsics/memory/alloc", false));
  CHECK(primec::parser::isBuiltinName("std/intrinsics/memory/free", false));
  CHECK(primec::parser::isBuiltinName("/std/intrinsics/memory/realloc", false));
  CHECK(primec::parser::isBuiltinName("/std/intrinsics/memory/at", false));
  CHECK(primec::parser::isBuiltinName("/std/intrinsics/memory/at_unsafe", false));
  CHECK_FALSE(primec::parser::isBuiltinName("alloc", false));
  CHECK_FALSE(primec::parser::isBuiltinName("std/intrinsics/memory/not_builtin", false));
}

TEST_CASE("parser helper rejects cross-qualified builtin names") {
  CHECK_FALSE(primec::parser::isBuiltinName("/std/math/assign", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/gpu/sin", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/other/assign", false));
}

TEST_CASE("parser helper recognizes additional core builtin names") {
  CHECK(primec::parser::isBuiltinName("print_line_error", false));
  CHECK(primec::parser::isBuiltinName("remove_swap", false));
  CHECK(primec::parser::isBuiltinName("at_unsafe", false));
  CHECK(primec::parser::isBuiltinName("convert", false));
  CHECK(primec::parser::isBuiltinName("location", false));
  CHECK(primec::parser::isBuiltinName("dereference", false));
  CHECK(primec::parser::isBuiltinName("block", false));
  CHECK(primec::parser::isBuiltinName("notify", false));
  CHECK(primec::parser::isBuiltinName("insert", false));
  CHECK(primec::parser::isBuiltinName("/assign", false));
  CHECK(primec::parser::isBuiltinName("/print", false));
}

TEST_CASE("parser helper rejects non-builtin slash paths") {
  CHECK_FALSE(primec::parser::isBuiltinName("foo/bar", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/foo/bar", false));
  CHECK_FALSE(primec::parser::isBuiltinName("totally_unknown_name", false));
}

TEST_CASE("parser helper validates identifier edge cases") {
  std::string error;
  CHECK_FALSE(primec::parser::validateIdentifierText("", error));
  CHECK(error.find("invalid identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("/", error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("1bad", error));
  CHECK(error.find("invalid identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("ab-c", error));
  CHECK(error.find("invalid identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("/root/bad-segment", error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("root/path", error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("loop", error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);

  error.clear();
  CHECK(primec::parser::validateIdentifierText("_localName", error));
  CHECK(error.empty());

  error.clear();
  CHECK(primec::parser::validateIdentifierText("/demo/_privateName", error));
  CHECK(error.empty());

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("/demo/", error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("parser helper validates transform-name special cases") {
  std::string error;
  CHECK(primec::parser::validateTransformName("return", error));
  CHECK(primec::parser::validateTransformName("mut", error));
  CHECK(primec::parser::validateTransformName("/demo/name", error));

  error.clear();
  CHECK_FALSE(primec::parser::validateTransformName("1bad", error));
  CHECK(error.find("invalid identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateTransformName("/demo/loop", error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_CASE("parser helper validates float literal edge cases") {
  CHECK_FALSE(primec::parser::isValidFloatLiteral(""));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("-"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("."));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("e10"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("1e"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("1e+"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("1.2x"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("1e-"));
  CHECK(primec::parser::isValidFloatLiteral("1e2"));
  CHECK(primec::parser::isValidFloatLiteral("1e+2"));
  CHECK(primec::parser::isValidFloatLiteral("1E-2"));
  CHECK(primec::parser::isValidFloatLiteral("12"));
  CHECK(primec::parser::isValidFloatLiteral("1."));
  CHECK(primec::parser::isValidFloatLiteral(".5"));
  CHECK(primec::parser::isValidFloatLiteral("-.5"));
}

TEST_CASE("parser helper validates additional float literal malformed forms") {
  CHECK_FALSE(primec::parser::isValidFloatLiteral("+1"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("--1"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("1..2"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("1e2e3"));
  CHECK_FALSE(primec::parser::isValidFloatLiteral("1e-2x"));
  CHECK(primec::parser::isValidFloatLiteral("0"));
  CHECK(primec::parser::isValidFloatLiteral("-0"));
  CHECK(primec::parser::isValidFloatLiteral("0.0e-0"));
}

TEST_CASE("parser helper describes invalid token edge cases") {
  primec::Token emptyToken;
  emptyToken.kind = primec::TokenKind::Invalid;
  emptyToken.text = "";
  CHECK(primec::parser::describeInvalidToken(emptyToken) == "<unknown>");

  primec::Token asciiToken;
  asciiToken.kind = primec::TokenKind::Invalid;
  asciiToken.text = "$";
  CHECK(primec::parser::describeInvalidToken(asciiToken) == "'$'");

  primec::Token textToken;
  textToken.kind = primec::TokenKind::Invalid;
  textToken.text = "oops";
  CHECK(primec::parser::describeInvalidToken(textToken) == "oops");

  primec::Token controlToken;
  controlToken.kind = primec::TokenKind::Invalid;
  controlToken.text = std::string(1, static_cast<char>(0x01));
  CHECK(primec::parser::describeInvalidToken(controlToken) == "0x01");
}

TEST_CASE("parser helper describes invalid token high-byte and newline") {
  primec::Token highByteToken;
  highByteToken.kind = primec::TokenKind::Invalid;
  highByteToken.text = std::string(1, static_cast<char>(0x80));
  CHECK(primec::parser::describeInvalidToken(highByteToken) == "0x80");

  primec::Token newlineToken;
  newlineToken.kind = primec::TokenKind::Invalid;
  newlineToken.text = "\n";
  CHECK(primec::parser::describeInvalidToken(newlineToken) == "0x0a");
}

TEST_CASE("parser helper hex digit utility handles edge chars") {
  CHECK(primec::parser::isHexDigitChar('0'));
  CHECK(primec::parser::isHexDigitChar('a'));
  CHECK(primec::parser::isHexDigitChar('F'));
  CHECK_FALSE(primec::parser::isHexDigitChar('g'));
}

TEST_CASE("parser helper rejects reserved keyword slash segments") {
  std::string error;
  CHECK_FALSE(primec::parser::validateIdentifierText("/demo/loop", error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_CASE("parser helper rejects additional reserved keyword forms") {
  std::string error;
  CHECK_FALSE(primec::parser::validateIdentifierText("match", error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("/demo/if", error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::parser::validateIdentifierText("/demo/1segment", error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("parser helper rejects all reserved keywords as identifiers") {
  const std::vector<std::string> keywords = {"mut",       "return", "include", "import", "namespace",
                                             "class",     "true",   "false",   "if",     "else",
                                             "loop",      "while",  "for",     "match"};
  for (const auto &keyword : keywords) {
    std::string error;
    CHECK_FALSE(primec::parser::validateIdentifierText(keyword, error));
    CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);

    error.clear();
    CHECK_FALSE(primec::parser::validateIdentifierText("/scope/" + keyword, error));
    CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
  }
}

TEST_CASE("parser helper classifies struct transform names") {
  CHECK(primec::parser::isStructTransformName("struct"));
  CHECK(primec::parser::isStructTransformName("enum"));
  CHECK(primec::parser::isStructTransformName("gpu_lane"));
  CHECK_FALSE(primec::parser::isStructTransformName("effects"));
}

TEST_CASE("parser helper classifies extended struct transform names") {
  CHECK(primec::parser::isStructTransformName("pod"));
  CHECK(primec::parser::isStructTransformName("handle"));
  CHECK(primec::parser::isStructTransformName("no_padding"));
  CHECK(primec::parser::isStructTransformName("platform_independent_padding"));
  CHECK_FALSE(primec::parser::isStructTransformName("platformIndependentPadding"));
}

TEST_CASE("parser helper classifies binding transform lists") {
  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out"};
  CHECK_FALSE(primec::parser::hasExplicitBindingTypeTransform({effects}));
  CHECK_FALSE(primec::parser::isBindingTransformList({effects}));

  primec::Transform mut;
  mut.name = "mut";
  CHECK(primec::parser::isBindingTransformList({effects, mut}));

  primec::Transform explicitType;
  explicitType.name = "i32";
  CHECK(primec::parser::hasExplicitBindingTypeTransform({explicitType}));
  CHECK(primec::parser::isBindingTransformList({explicitType}));
}

TEST_CASE("parser helper handles empty and argument-only binding transform lists") {
  std::vector<primec::Transform> emptyTransforms;
  CHECK_FALSE(primec::parser::isBindingTransformList(emptyTransforms));
  CHECK_FALSE(primec::parser::hasExplicitBindingTypeTransform(emptyTransforms));

  primec::Transform argumentOnly;
  argumentOnly.name = "custom";
  argumentOnly.arguments = {"value"};
  CHECK_FALSE(primec::parser::hasExplicitBindingTypeTransform({argumentOnly}));
  CHECK_FALSE(primec::parser::isBindingTransformList({argumentOnly}));
}

TEST_CASE("parser helper classifies binding auxiliary transforms") {
  CHECK(primec::parser::isBindingAuxTransformName("mut"));
  CHECK(primec::parser::isBindingAuxTransformName("copy"));
  CHECK(primec::parser::isBindingAuxTransformName("restrict"));
  CHECK(primec::parser::isBindingAuxTransformName("align_bytes"));
  CHECK(primec::parser::isBindingAuxTransformName("align_kbytes"));
  CHECK(primec::parser::isBindingAuxTransformName("pod"));
  CHECK(primec::parser::isBindingAuxTransformName("handle"));
  CHECK(primec::parser::isBindingAuxTransformName("gpu_lane"));
  CHECK(primec::parser::isBindingAuxTransformName("public"));
  CHECK(primec::parser::isBindingAuxTransformName("private"));
  CHECK(primec::parser::isBindingAuxTransformName("static"));
  CHECK_FALSE(primec::parser::isBindingAuxTransformName("effects"));
}

TEST_CASE("parser helper explicit type detection ignores effects capabilities") {
  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out"};

  primec::Transform capabilities;
  capabilities.name = "capabilities";
  capabilities.arguments = {"io_out"};

  CHECK_FALSE(primec::parser::hasExplicitBindingTypeTransform({effects, capabilities}));
  CHECK_FALSE(primec::parser::isBindingTransformList({effects, capabilities}));
}

TEST_CASE("parser helper detects explicit type transform without arguments") {
  primec::Transform customType;
  customType.name = "MyEnvelope";
  CHECK(primec::parser::hasExplicitBindingTypeTransform({customType}));
  CHECK(primec::parser::isBindingTransformList({customType}));
}

TEST_CASE("parser helper validates qualified gpu helper names") {
  CHECK(primec::parser::isBuiltinName("/std/gpu/buffer_load", false));
  CHECK(primec::parser::isBuiltinName("/std/gpu/readback", false));
  CHECK(primec::parser::isBuiltinName("/std/gpu/global_id_x", false));
  CHECK_FALSE(primec::parser::isBuiltinName("buffer_load", false));
  CHECK_FALSE(primec::parser::isBuiltinName("readback", false));
  CHECK_FALSE(primec::parser::isBuiltinName("global_id_x", false));
}

TEST_CASE("parser helper recognizes additional builtin name groups") {
  CHECK(primec::parser::isBuiltinName("plus", false));
  CHECK(primec::parser::isBuiltinName("minus", false));
  CHECK(primec::parser::isBuiltinName("multiply", false));
  CHECK(primec::parser::isBuiltinName("divide", false));
  CHECK(primec::parser::isBuiltinName("print", false));
  CHECK(primec::parser::isBuiltinName("print_line", false));
  CHECK(primec::parser::isBuiltinName("print_error", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/totally_unknown_name", false));

  CHECK(primec::parser::isBuiltinName("sqrt", true));
  CHECK_FALSE(primec::parser::isBuiltinName("sqrt", false));
  CHECK(primec::parser::isBuiltinName("/std/math/sqrt", false));

  CHECK(primec::parser::isBuiltinName("/std/gpu/global_id_y", false));
  CHECK(primec::parser::isBuiltinName("/std/gpu/global_id_z", false));
  CHECK(primec::parser::isBuiltinName("/std/gpu/buffer_store", false));
  CHECK(primec::parser::isBuiltinName("/std/gpu/buffer", false));
  CHECK(primec::parser::isBuiltinName("/std/gpu/upload", false));
  CHECK_FALSE(primec::parser::isBuiltinName("global_id_y", false));
  CHECK_FALSE(primec::parser::isBuiltinName("global_id_z", false));
  CHECK_FALSE(primec::parser::isBuiltinName("buffer_store", false));
  CHECK_FALSE(primec::parser::isBuiltinName("buffer", false));
  CHECK_FALSE(primec::parser::isBuiltinName("upload", false));
}

TEST_CASE("parser helper recognizes control and container builtin names") {
  CHECK(primec::parser::isBuiltinName("if", false));
  CHECK(primec::parser::isBuiltinName("then", false));
  CHECK(primec::parser::isBuiltinName("else", false));
  CHECK(primec::parser::isBuiltinName("repeat", false));
  CHECK(primec::parser::isBuiltinName("return", false));
  CHECK(primec::parser::isBuiltinName("try", false));
  CHECK(primec::parser::isBuiltinName("increment", false));
  CHECK(primec::parser::isBuiltinName("decrement", false));

  CHECK(primec::parser::isBuiltinName("array", false));
  CHECK(primec::parser::isBuiltinName("vector", false));
  CHECK(primec::parser::isBuiltinName("map", false));
  CHECK(primec::parser::isBuiltinName("File", false));
  CHECK(primec::parser::isBuiltinName("count", false));
  CHECK(primec::parser::isBuiltinName("capacity", false));
  CHECK(primec::parser::isBuiltinName("push", false));
  CHECK(primec::parser::isBuiltinName("pop", false));
  CHECK(primec::parser::isBuiltinName("reserve", false));
  CHECK(primec::parser::isBuiltinName("clear", false));
  CHECK(primec::parser::isBuiltinName("remove_at", false));
  CHECK(primec::parser::isBuiltinName("at", false));

  CHECK_FALSE(primec::parser::isBuiltinName("/std/gpu/assign", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/math/readback", false));
}

TEST_CASE("parser helper recognizes operator builtin names") {
  CHECK(primec::parser::isBuiltinName("negate", false));
  CHECK(primec::parser::isBuiltinName("greater_than", false));
  CHECK(primec::parser::isBuiltinName("less_than", false));
  CHECK(primec::parser::isBuiltinName("greater_equal", false));
  CHECK(primec::parser::isBuiltinName("less_equal", false));
  CHECK(primec::parser::isBuiltinName("equal", false));
  CHECK(primec::parser::isBuiltinName("not_equal", false));
  CHECK(primec::parser::isBuiltinName("and", false));
  CHECK(primec::parser::isBuiltinName("or", false));
  CHECK(primec::parser::isBuiltinName("not", false));
  CHECK_FALSE(primec::parser::isBuiltinName("logical_and", false));
}

TEST_CASE("parser helper validates late math builtin names") {
  CHECK(primec::parser::isBuiltinName("is_nan", true));
  CHECK(primec::parser::isBuiltinName("is_inf", true));
  CHECK(primec::parser::isBuiltinName("is_finite", true));
  CHECK_FALSE(primec::parser::isBuiltinName("is_nan", false));
  CHECK_FALSE(primec::parser::isBuiltinName("is_inf", false));
  CHECK_FALSE(primec::parser::isBuiltinName("is_finite", false));
  CHECK(primec::parser::isBuiltinName("/std/math/is_nan", false));
  CHECK(primec::parser::isBuiltinName("/std/math/is_inf", false));
  CHECK(primec::parser::isBuiltinName("/std/math/is_finite", false));
}

TEST_CASE("parser helper validates advanced math builtin names") {
  CHECK(primec::parser::isBuiltinName("atan2", true));
  CHECK(primec::parser::isBuiltinName("copysign", true));
  CHECK(primec::parser::isBuiltinName("fma", true));
  CHECK(primec::parser::isBuiltinName("hypot", true));
  CHECK(primec::parser::isBuiltinName("asinh", true));
  CHECK(primec::parser::isBuiltinName("acosh", true));
  CHECK(primec::parser::isBuiltinName("atanh", true));

  CHECK_FALSE(primec::parser::isBuiltinName("atan2", false));
  CHECK_FALSE(primec::parser::isBuiltinName("copysign", false));
  CHECK_FALSE(primec::parser::isBuiltinName("fma", false));
  CHECK_FALSE(primec::parser::isBuiltinName("hypot", false));

  CHECK(primec::parser::isBuiltinName("/std/math/atan2", false));
  CHECK(primec::parser::isBuiltinName("/std/math/copysign", false));
  CHECK(primec::parser::isBuiltinName("/std/math/fma", false));
  CHECK(primec::parser::isBuiltinName("/std/math/hypot", false));
  CHECK(primec::parser::isBuiltinName("/std/math/asinh", false));
  CHECK(primec::parser::isBuiltinName("/std/math/acosh", false));
  CHECK(primec::parser::isBuiltinName("/std/math/atanh", false));
}

TEST_CASE("parser helper validates interpolation and trig math builtins") {
  CHECK(primec::parser::isBuiltinName("clamp", true));
  CHECK(primec::parser::isBuiltinName("lerp", true));
  CHECK(primec::parser::isBuiltinName("saturate", true));
  CHECK(primec::parser::isBuiltinName("radians", true));
  CHECK(primec::parser::isBuiltinName("degrees", true));
  CHECK(primec::parser::isBuiltinName("sinh", true));
  CHECK(primec::parser::isBuiltinName("cosh", true));
  CHECK(primec::parser::isBuiltinName("tanh", true));
  CHECK(primec::parser::isBuiltinName("log10", true));
  CHECK(primec::parser::isBuiltinName("pow", true));

  CHECK_FALSE(primec::parser::isBuiltinName("clamp", false));
  CHECK_FALSE(primec::parser::isBuiltinName("log10", false));

  CHECK(primec::parser::isBuiltinName("/std/math/clamp", false));
  CHECK(primec::parser::isBuiltinName("/std/math/lerp", false));
  CHECK(primec::parser::isBuiltinName("/std/math/saturate", false));
  CHECK(primec::parser::isBuiltinName("/std/math/radians", false));
  CHECK(primec::parser::isBuiltinName("/std/math/degrees", false));
  CHECK(primec::parser::isBuiltinName("/std/math/log10", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/math/saturatee", false));
}

TEST_CASE("parser helper validates foundational math builtin names") {
  CHECK(primec::parser::isBuiltinName("abs", true));
  CHECK(primec::parser::isBuiltinName("sign", true));
  CHECK(primec::parser::isBuiltinName("min", true));
  CHECK(primec::parser::isBuiltinName("max", true));
  CHECK(primec::parser::isBuiltinName("floor", true));
  CHECK(primec::parser::isBuiltinName("ceil", true));
  CHECK(primec::parser::isBuiltinName("round", true));
  CHECK(primec::parser::isBuiltinName("trunc", true));
  CHECK(primec::parser::isBuiltinName("fract", true));
  CHECK(primec::parser::isBuiltinName("cbrt", true));
  CHECK(primec::parser::isBuiltinName("exp", true));
  CHECK(primec::parser::isBuiltinName("exp2", true));
  CHECK(primec::parser::isBuiltinName("log", true));
  CHECK(primec::parser::isBuiltinName("log2", true));
  CHECK(primec::parser::isBuiltinName("sin", true));
  CHECK(primec::parser::isBuiltinName("cos", true));
  CHECK(primec::parser::isBuiltinName("tan", true));
  CHECK(primec::parser::isBuiltinName("asin", true));
  CHECK(primec::parser::isBuiltinName("acos", true));
  CHECK(primec::parser::isBuiltinName("atan", true));

  CHECK_FALSE(primec::parser::isBuiltinName("abs", false));
  CHECK_FALSE(primec::parser::isBuiltinName("log2", false));

  CHECK(primec::parser::isBuiltinName("/std/math/abs", false));
  CHECK(primec::parser::isBuiltinName("/std/math/log2", false));
  CHECK(primec::parser::isBuiltinName("/std/math/atan", false));
}

TEST_CASE("parser helper handles malformed and unslashed builtin paths") {
  CHECK_FALSE(primec::parser::isBuiltinName("", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/math/", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/gpu/", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/math//sin", false));
  CHECK_FALSE(primec::parser::isBuiltinName("/std/gpu//dispatch", false));

  CHECK(primec::parser::isBuiltinName("std/math/sin", false));
  CHECK(primec::parser::isBuiltinName("std/gpu/dispatch", false));
}

TEST_CASE("parser helper detects explicit type after skipped transforms") {
  primec::Transform capabilities;
  capabilities.name = "capabilities";
  capabilities.arguments = {"io_out"};

  primec::Transform auxiliary;
  auxiliary.name = "mut";

  primec::Transform argumentOnly;
  argumentOnly.name = "wrapper";
  argumentOnly.arguments = {"value"};

  primec::Transform explicitType;
  explicitType.name = "Payload";

  std::vector<primec::Transform> transforms = {capabilities, auxiliary, argumentOnly, explicitType};
  CHECK(primec::parser::hasExplicitBindingTypeTransform(transforms));
  CHECK(primec::parser::isBindingTransformList(transforms));
}

TEST_CASE("parser helper handles mixed transform list without explicit type") {
  primec::Transform capabilities;
  capabilities.name = "capabilities";
  capabilities.arguments = {"io_out"};

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_in"};

  primec::Transform auxiliary;
  auxiliary.name = "restrict";

  primec::Transform argumentOnly;
  argumentOnly.name = "wrapper";
  argumentOnly.arguments = {"payload"};

  std::vector<primec::Transform> transforms = {capabilities, effects, auxiliary, argumentOnly};
  CHECK_FALSE(primec::parser::hasExplicitBindingTypeTransform(transforms));
  CHECK(primec::parser::isBindingTransformList(transforms));
}
