  using Result = primec::ir_lowerer::CountMethodFallbackResult;

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  primec::Expr targetArg;
  targetArg.kind = primec::Expr::Kind::Name;
  targetArg.name = "items";
  countCall.args.push_back(targetArg);

  primec::Definition callee;
  callee.fullPath = "/pkg/items/count";

  std::string error;
  int resolveCalls = 0;
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++resolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++emitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(resolveCalls == 1);
  CHECK(emitCalls == 1);

  primec::Definition vectorCountCallee;
  vectorCountCallee.fullPath = "/vector/count";
  int vectorCountResolveCalls = 0;
  int vectorCountEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++vectorCountResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &vectorCountCallee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++vectorCountEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(vectorCountResolveCalls == 1);
  CHECK(vectorCountEmitCalls == 1);

  primec::Definition canonicalVectorCountCallee;
  canonicalVectorCountCallee.fullPath = "/std/collections/vector/count";
  int canonicalVectorCountResolveCalls = 0;
  int canonicalVectorCountEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalVectorCountResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &canonicalVectorCountCallee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++canonicalVectorCountEmitCalls;
              return true;
            },
            error) == Result::NoCallee);
  CHECK(error.empty());
  CHECK(canonicalVectorCountResolveCalls == 1);
  CHECK(canonicalVectorCountEmitCalls == 0);

  primec::Definition canonicalVectorCountOverloadCallee;
  canonicalVectorCountOverloadCallee.fullPath =
      "/std/collections/vector/count__ov0";
  int canonicalVectorCountOverloadResolveCalls = 0;
  int canonicalVectorCountOverloadEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalVectorCountOverloadResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &canonicalVectorCountOverloadCallee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++canonicalVectorCountOverloadEmitCalls;
              return true;
            },
            error) == Result::NoCallee);
  CHECK(error.empty());
  CHECK(canonicalVectorCountOverloadResolveCalls == 1);
  CHECK(canonicalVectorCountOverloadEmitCalls == 0);

  primec::Expr aliasCountCall = countCall;
  aliasCountCall.name = "/vector/count";
  int aliasCountResolveCalls = 0;
  int aliasCountEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            aliasCountCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++aliasCountResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++aliasCountEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(aliasCountResolveCalls == 0);
  CHECK(aliasCountEmitCalls == 0);

  primec::Expr canonicalCountCall = countCall;
  canonicalCountCall.name = "/std/collections/vector/count";
  int canonicalCountResolveCalls = 0;
  int canonicalCountEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            canonicalCountCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalCountResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++canonicalCountEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalCountResolveCalls == 1);
  CHECK(canonicalCountEmitCalls == 1);

  primec::Expr aliasMapCountCall = countCall;
  aliasMapCountCall.name = "/std/collections/map/count";
  int aliasMapCountResolveCalls = 0;
  int aliasMapCountEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            aliasMapCountCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++aliasMapCountResolveCalls;
              return &callee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++aliasMapCountEmitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(aliasMapCountResolveCalls == 0);
  CHECK(aliasMapCountEmitCalls == 0);

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args.push_back(targetArg);

  primec::Definition vectorCapacityCallee;
  vectorCapacityCallee.fullPath = "/vector/capacity";
  int capacityResolveCalls = 0;
  int capacityEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            capacityCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++capacityResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "capacity");
              return &vectorCapacityCallee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++capacityEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(capacityResolveCalls == 1);
  CHECK(capacityEmitCalls == 1);

  primec::Definition canonicalVectorCapacityOverloadCallee;
  canonicalVectorCapacityOverloadCallee.fullPath =
      "/std/collections/vector/capacity__ov0";
  int canonicalVectorCapacityOverloadResolveCalls = 0;
  int canonicalVectorCapacityOverloadEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            capacityCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalVectorCapacityOverloadResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "capacity");
              return &canonicalVectorCapacityOverloadCallee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++canonicalVectorCapacityOverloadEmitCalls;
              return true;
            },
            error) == Result::NoCallee);
  CHECK(error.empty());
  CHECK(canonicalVectorCapacityOverloadResolveCalls == 1);
  CHECK(canonicalVectorCapacityOverloadEmitCalls == 0);

  primec::Expr canonicalCapacityCall = capacityCall;
  canonicalCapacityCall.name = "/std/collections/vector/capacity";
  int canonicalCapacityResolveCalls = 0;
  int canonicalCapacityEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            canonicalCapacityCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalCapacityResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "capacity");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++canonicalCapacityEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "capacity");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalCapacityResolveCalls == 1);
  CHECK(canonicalCapacityEmitCalls == 1);

  primec::Expr indexArg;
  indexArg.kind = primec::Expr::Kind::Literal;
  indexArg.intWidth = 32;
  indexArg.literalValue = 1;

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {targetArg, indexArg};
  int atResolveCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            atCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++atResolveCalls;
              return &callee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(atResolveCalls == 0);

  primec::Expr aliasMapAtCall = atCall;
  aliasMapAtCall.name = "/std/collections/map/at";
  int aliasMapAtResolveCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            aliasMapAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++aliasMapAtResolveCalls;
              return &callee;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(aliasMapAtResolveCalls == 0);
