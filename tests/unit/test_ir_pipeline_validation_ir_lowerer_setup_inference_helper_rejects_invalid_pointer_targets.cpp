#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup inference helper rejects invalid pointer targets") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("ptr", pointerInfo);

  primec::ir_lowerer::LocalInfo arrayRefInfo;
  arrayRefInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  arrayRefInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  arrayRefInfo.referenceToArray = true;
  locals.emplace("arrayRef", arrayRefInfo);

  primec::Expr arrayRefName;
  arrayRefName.kind = primec::Expr::Kind::Name;
  arrayRefName.name = "arrayRef";
  primec::Expr locationArrayRef;
  locationArrayRef.kind = primec::Expr::Kind::Call;
  locationArrayRef.name = "location";
  locationArrayRef.args = {arrayRefName};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            locationArrayRef,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr ptrName;
  ptrName.kind = primec::Expr::Kind::Name;
  ptrName.name = "ptr";
  primec::Expr one;
  one.kind = primec::Expr::Kind::Literal;
  one.literalValue = 1;
  primec::Expr ptrOffset;
  ptrOffset.kind = primec::Expr::Kind::Call;
  ptrOffset.name = "plus";
  ptrOffset.args = {ptrName, one};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            ptrOffset,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  primec::Expr nestedInvalid;
  nestedInvalid.kind = primec::Expr::Kind::Call;
  nestedInvalid.name = "plus";
  nestedInvalid.args = {ptrOffset, ptrName};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            nestedInvalid,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::ir_lowerer::LocalInfo argsPackArrayRefInfo;
  argsPackArrayRefInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  argsPackArrayRefInfo.isArgsPack = true;
  argsPackArrayRefInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  argsPackArrayRefInfo.referenceToArray = true;
  argsPackArrayRefInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("arrayValues", argsPackArrayRefInfo);

  primec::Expr arrayValuesName;
  arrayValuesName.kind = primec::Expr::Kind::Name;
  arrayValuesName.name = "arrayValues";
  primec::Expr atArrayCall;
  atArrayCall.kind = primec::Expr::Kind::Call;
  atArrayCall.name = "at";
  atArrayCall.args = {arrayValuesName, one};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            atArrayCall,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer setup inference helper infers buffer element kinds") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo bufferInfo;
  bufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  locals.emplace("values", bufferInfo);

  primec::Expr bufferName;
  bufferName.kind = primec::Expr::Kind::Name;
  bufferName.name = "values";
  CHECK(primec::ir_lowerer::inferBufferElementValueKind(
            bufferName,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr bufferCtor;
  bufferCtor.kind = primec::Expr::Kind::Call;
  bufferCtor.name = "buffer";
  bufferCtor.templateArgs = {"i32"};
  CHECK(primec::ir_lowerer::inferBufferElementValueKind(
            bufferCtor,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr arrayExpr;
  arrayExpr.kind = primec::Expr::Kind::Name;
  arrayExpr.name = "arr";
  primec::Expr uploadCall;
  uploadCall.kind = primec::Expr::Kind::Call;
  uploadCall.name = "upload";
  uploadCall.args = {arrayExpr};
  CHECK(primec::ir_lowerer::inferBufferElementValueKind(
            uploadCall,
            locals,
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "arr") {
                return primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::ir_lowerer::LocalInfo packedBufferInfo;
  packedBufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  packedBufferInfo.isArgsPack = true;
  packedBufferInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  packedBufferInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("buffers", packedBufferInfo);

  primec::Expr packedBuffersName;
  packedBuffersName.kind = primec::Expr::Kind::Name;
  packedBuffersName.name = "buffers";
  primec::Expr packedBufferIndex;
  packedBufferIndex.kind = primec::Expr::Kind::Literal;
  packedBufferIndex.intWidth = 32;
  packedBufferIndex.literalValue = 0;
  primec::Expr packedBufferAccess;
  packedBufferAccess.kind = primec::Expr::Kind::Call;
  packedBufferAccess.name = "at";
  packedBufferAccess.args = {packedBuffersName, packedBufferIndex};
  CHECK(primec::ir_lowerer::inferBufferElementValueKind(
            packedBufferAccess,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::ir_lowerer::LocalInfo borrowedBufferInfo;
  borrowedBufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  borrowedBufferInfo.isArgsPack = true;
  borrowedBufferInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  borrowedBufferInfo.referenceToBuffer = true;
  borrowedBufferInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
  locals.emplace("borrowedBuffers", borrowedBufferInfo);

  primec::Expr borrowedBuffersName;
  borrowedBuffersName.kind = primec::Expr::Kind::Name;
  borrowedBuffersName.name = "borrowedBuffers";
  primec::Expr borrowedBufferIndex;
  borrowedBufferIndex.kind = primec::Expr::Kind::Literal;
  borrowedBufferIndex.intWidth = 32;
  borrowedBufferIndex.literalValue = 0;
  primec::Expr borrowedBufferAccess;
  borrowedBufferAccess.kind = primec::Expr::Kind::Call;
  borrowedBufferAccess.name = "at";
  borrowedBufferAccess.args = {borrowedBuffersName, borrowedBufferIndex};
  primec::Expr borrowedBufferDeref;
  borrowedBufferDeref.kind = primec::Expr::Kind::Call;
  borrowedBufferDeref.name = "dereference";
  borrowedBufferDeref.args = {borrowedBufferAccess};
  CHECK(primec::ir_lowerer::inferBufferElementValueKind(
            borrowedBufferDeref,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::ir_lowerer::LocalInfo pointerBufferInfo;
  pointerBufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  pointerBufferInfo.isArgsPack = true;
  pointerBufferInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerBufferInfo.pointerToBuffer = true;
  pointerBufferInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
  locals.emplace("pointerBuffers", pointerBufferInfo);

  primec::Expr pointerBuffersName;
  pointerBuffersName.kind = primec::Expr::Kind::Name;
  pointerBuffersName.name = "pointerBuffers";
  primec::Expr pointerBufferIndex;
  pointerBufferIndex.kind = primec::Expr::Kind::Literal;
  pointerBufferIndex.intWidth = 32;
  pointerBufferIndex.literalValue = 0;
  primec::Expr pointerBufferAccess;
  pointerBufferAccess.kind = primec::Expr::Kind::Call;
  pointerBufferAccess.name = "at";
  pointerBufferAccess.args = {pointerBuffersName, pointerBufferIndex};
  primec::Expr pointerBufferDeref;
  pointerBufferDeref.kind = primec::Expr::Kind::Call;
  pointerBufferDeref.name = "dereference";
  pointerBufferDeref.args = {pointerBufferAccess};
  CHECK(primec::ir_lowerer::inferBufferElementValueKind(
            pointerBufferDeref,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
}

TEST_CASE("ir lowerer setup inference helper infers and validates array element kinds") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo arrayInfo;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  arrayInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("items", arrayInfo);

  primec::Expr itemsName;
  itemsName.kind = primec::Expr::Kind::Name;
  itemsName.name = "items";
  CHECK(primec::ir_lowerer::inferArrayElementValueKind(
            itemsName,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string{}; },
            [](const std::string &, primec::ir_lowerer::LocalInfo::ValueKind &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr readbackCall;
  readbackCall.kind = primec::Expr::Kind::Call;
  readbackCall.name = "readback";
  readbackCall.args = {itemsName};
  CHECK(primec::ir_lowerer::inferArrayElementValueKind(
            readbackCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
            },
            [](const primec::Expr &) { return std::string{}; },
            [](const std::string &, primec::ir_lowerer::LocalInfo::ValueKind &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Expr structCall;
  structCall.kind = primec::Expr::Kind::Call;
  structCall.name = "/pkg/MyStructArray";
  CHECK(primec::ir_lowerer::inferArrayElementValueKind(
            structCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &expr) { return expr.name; },
            [](const std::string &path, primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
              if (path == "/pkg/MyStructArray") {
                kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
                return true;
              }
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "/pkg/load";
  CHECK(primec::ir_lowerer::inferArrayElementValueKind(
            directCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string{}; },
            [](const std::string &, primec::ir_lowerer::LocalInfo::ValueKind &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
              kindOut = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::Expr canonicalMapCtorCall;
  canonicalMapCtorCall.kind = primec::Expr::Kind::Call;
  canonicalMapCtorCall.name = "/std/collections/map/map";
  canonicalMapCtorCall.templateArgs = {"i32", "bool"};
  CHECK(primec::ir_lowerer::inferArrayElementValueKind(
            canonicalMapCtorCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string{}; },
            [](const std::string &, primec::ir_lowerer::LocalInfo::ValueKind &) { return false; },
            [](const primec::Expr &expr,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
              if (expr.name == "/std/collections/map/map") {
                kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
                return true;
              }
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "load";
  CHECK(primec::ir_lowerer::inferArrayElementValueKind(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string{}; },
            [](const std::string &, primec::ir_lowerer::LocalInfo::ValueKind &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
              kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
              return true;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  CHECK(primec::ir_lowerer::inferArrayElementValueKind(
            readbackCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::String;
            },
            [](const primec::Expr &) { return std::string{}; },
            [](const std::string &, primec::ir_lowerer::LocalInfo::ValueKind &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
              return false;
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper resolves call expression return kinds") {
  using Resolution = primec::ir_lowerer::CallExpressionReturnKindResolution;

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "/pkg/load";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "load";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveCallExpressionReturnKind(
            directCall,
            {},
            [](const primec::Expr &expr,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &kindOut,
               bool &matchedOut) {
              matchedOut = (expr.name == "/pkg/load");
              if (!matchedOut) {
                return false;
              }
              kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveCallExpressionReturnKind(
            countCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &expr,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &kindOut,
               bool &matchedOut) {
              matchedOut = (expr.name == "count");
              if (!matchedOut) {
                return false;
              }
              kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveCallExpressionReturnKind(
            methodCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &expr,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &kindOut,
               bool &matchedOut) {
              matchedOut = expr.isMethodCall && expr.name == "load";
              if (!matchedOut) {
                return false;
              }
              kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
              return true;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
}

TEST_CASE("ir lowerer setup inference helper handles unresolved call return kinds") {
  using Resolution = primec::ir_lowerer::CallExpressionReturnKindResolution;

  primec::Expr unsupportedDirectCall;
  unsupportedDirectCall.kind = primec::Expr::Kind::Call;
  unsupportedDirectCall.name = "/pkg/unsupported";

  primec::Expr unknownCall;
  unknownCall.kind = primec::Expr::Kind::Call;
  unknownCall.name = "noop";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::resolveCallExpressionReturnKind(
            unsupportedDirectCall,
            {},
            [](const primec::Expr &expr,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &,
               bool &matchedOut) {
              matchedOut = (expr.name == "/pkg/unsupported");
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            kindOut) == Resolution::MatchedButUnsupported);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK(primec::ir_lowerer::resolveCallExpressionReturnKind(
            unknownCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            kindOut) == Resolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr dereferenceCall;
  dereferenceCall.kind = primec::Expr::Kind::Call;
  dereferenceCall.name = "dereference";
  primec::Expr pointerName;
  pointerName.kind = primec::Expr::Kind::Name;
  pointerName.name = "ptr";
  dereferenceCall.args = {pointerName};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK(primec::ir_lowerer::resolveCallExpressionReturnKind(
            dereferenceCall,
            {},
            [](const primec::Expr &expr,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &,
               bool &matchedOut) {
              matchedOut = (expr.name == "dereference");
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &, bool &matchedOut) {
              matchedOut = false;
              return false;
            },
            kindOut) == Resolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper resolves array and map access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
  locals.emplace("scores", mapInfo);

  primec::ir_lowerer::LocalInfo arrayInfo;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  arrayInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
  locals.emplace("values", arrayInfo);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr stringTarget;
  stringTarget.kind = primec::Expr::Kind::StringLiteral;
  stringTarget.stringValue = "text";
  primec::Expr stringAccess;
  stringAccess.kind = primec::Expr::Kind::Call;
  stringAccess.name = "at";
  stringAccess.args = {stringTarget, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            stringAccess,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr mapTarget;
  mapTarget.kind = primec::Expr::Kind::Name;
  mapTarget.name = "scores";
  primec::Expr mapAccess;
  mapAccess.kind = primec::Expr::Kind::Call;
  mapAccess.name = "at";
  mapAccess.args = {mapTarget, indexExpr};
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            mapAccess,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::Expr arrayTarget;
  arrayTarget.kind = primec::Expr::Kind::Name;
  arrayTarget.name = "values";
  primec::Expr arrayAccess;
  arrayAccess.kind = primec::Expr::Kind::Call;
  arrayAccess.name = "at_unsafe";
  arrayAccess.args = {arrayTarget, indexExpr};
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            arrayAccess,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
}

TEST_CASE("ir lowerer setup inference helper resolves reordered positional access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
  locals.emplace("values", mapInfo);

  primec::ir_lowerer::LocalInfo keyInfo;
  keyInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  keyInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("indexName", keyInfo);

  primec::Expr keyNameExpr;
  keyNameExpr.kind = primec::Expr::Kind::Name;
  keyNameExpr.name = "indexName";
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at_unsafe";
  accessExpr.args = {keyNameExpr, valuesExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
}

TEST_CASE("ir lowerer setup inference helper resolves string map reference access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapRefInfo;
  mapRefInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  mapRefInfo.referenceToMap = true;
  mapRefInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("values", mapRefInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 1;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}


TEST_SUITE_END();
