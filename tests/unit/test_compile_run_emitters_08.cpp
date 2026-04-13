#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter helper rejects cross-path vector access return-kind metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "count";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnKinds.emplace("/std/collections/vector/at", primec::emitter::ReturnKind::String);

  std::string resolved;
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper rejects vector alias direct-call method receiver without metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved = "/stale/path";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper rejects canonical direct-call method receiver without metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/vector/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved = "/stale/path";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper resolves canonical direct-call method receiver through same-path metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/vector/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/std/collections/vector/at", "/CanonicalMarker"},
  };

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper resolves explicit vector slash-method receivers through helper metadata") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/vector/at", "/AliasMarker"},
      {"/std/collections/vector/at", "/CanonicalMarker"},
  };

  auto expectResolved = [&](const char *receiverMethodName, const char *expectedPath) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;
    receiverCall.args = {receiverName, indexLiteral};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::string resolved;
    CHECK(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved == expectedPath);
  };

  expectResolved("/vector/at", "/AliasMarker/tag");
  expectResolved("/std/collections/vector/at", "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper prefers same-path vector slash-method access return-kind metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.isMethodCall = true;
  receiverCall.name = "/vector/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "count";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds = {
      {"/vector/at", primec::emitter::ReturnKind::String},
  };
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/string/count");
}

TEST_CASE("C++ emitter helper rejects cross-path vector slash-method access metadata fallback") {
  auto expectRejected = [&](const char *receiverMethodName, const char *availablePath) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;

    primec::Expr receiverName;
    receiverName.kind = primec::Expr::Kind::Name;
    receiverName.name = "values";

    primec::Expr indexLiteral;
    indexLiteral.kind = primec::Expr::Kind::Literal;
    indexLiteral.intWidth = 32;
    indexLiteral.literalValue = 0;

    receiverCall.args = {receiverName, indexLiteral};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "count";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "vector";
    receiverInfo.typeTemplateArg = "i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds = {
        {availablePath, primec::emitter::ReturnKind::String},
    };
    std::unordered_map<std::string, std::string> returnStructs;

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectRejected("/vector/at", "/std/collections/vector/at");
  expectRejected("/vector/at_unsafe", "/std/collections/vector/at_unsafe");
  expectRejected("/std/collections/vector/at", "/vector/at");
  expectRejected("/std/collections/vector/at_unsafe", "/vector/at_unsafe");
}

TEST_CASE("C++ emitter helper rejects explicit vector slash-method receivers without metadata") {
  auto expectRejected = [&](const char *receiverMethodName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;

    primec::Expr receiverName;
    receiverName.kind = primec::Expr::Kind::Name;
    receiverName.name = "values";

    primec::Expr indexLiteral;
    indexLiteral.kind = primec::Expr::Kind::Literal;
    indexLiteral.intWidth = 32;
    indexLiteral.literalValue = 0;

    receiverCall.args = {receiverName, indexLiteral};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "vector";
    receiverInfo.typeTemplateArg = "i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs;

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectRejected("/vector/at");
  expectRejected("/vector/at_unsafe");
  expectRejected("/std/collections/vector/at");
  expectRejected("/std/collections/vector/at_unsafe");
}

TEST_CASE("C++ emitter helper rejects explicit removed vector slash count capacity helpers") {
  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  primec::Definition aliasCountDef;
  aliasCountDef.fullPath = "/vector/count";
  primec::Definition aliasCapacityDef;
  aliasCapacityDef.fullPath = "/vector/capacity";
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  primec::Definition canonicalCapacityDef;
  canonicalCapacityDef.fullPath = "/std/collections/vector/capacity";

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasCountDef.fullPath, &aliasCountDef},
      {aliasCapacityDef.fullPath, &aliasCapacityDef},
      {canonicalCountDef.fullPath, &canonicalCountDef},
      {canonicalCapacityDef.fullPath, &canonicalCapacityDef},
  };
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  auto expectRejected = [&](const char *methodName) {
    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.isMethodCall = true;
    call.name = methodName;
    call.args = {receiver};
    call.argNames = {std::nullopt};

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectRejected("/vector/count");
  expectRejected("/vector/capacity");
  expectRejected("/std/collections/vector/count");
  expectRejected("/std/collections/vector/capacity");
}

TEST_CASE("C++ emitter helper rejects cross-path vector slash count capacity fallback") {
  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  auto expectRejected = [&](const char *methodName, const char *availablePath) {
    returnKinds.clear();
    returnKinds.emplace(availablePath, primec::emitter::ReturnKind::Int);

    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.isMethodCall = true;
    call.name = methodName;
    call.args = {receiver};
    call.argNames = {std::nullopt};

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectRejected("/vector/count", "/std/collections/vector/count");
  expectRejected("/vector/capacity", "/std/collections/vector/capacity");
  expectRejected("/std/collections/vector/count", "/vector/count");
  expectRejected("/std/collections/vector/capacity", "/vector/capacity");
}

TEST_CASE("rejects stdlib canonical vector helper method-precedence forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(40i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count(true), values.at(true)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_vector_method_helper_precedence_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_stdlib_vector_method_helper_precedence_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("rejects templated stdlib canonical vector helper method template args in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count<i32>(true), values.at<i32>(2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_vector_template_method_helper_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_vector_template_method_helper_precedence_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_stdlib_vector_template_method_helper_precedence_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("runs vector namespaced count capacity aliases through explicit alias helpers in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(90i32)
}

[return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values), /vector/capacity(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_namespaced_count_capacity_alias_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_namespaced_count_capacity_alias_same_path_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 110);
}

TEST_CASE("rejects vector namespaced count capacity aliases with only canonical helpers in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values), /vector/capacity(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_namespaced_count_capacity_alias_canonical_only_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_namespaced_count_capacity_alias_canonical_only_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("rejects vector namespaced templated canonical helper alias call without alias definition in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count<i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_namespaced_templated_canonical_alias_call.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_namespaced_templated_canonical_alias_call_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_namespaced_templated_canonical_alias_call_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/count") != std::string::npos);
}


TEST_SUITE_END();
