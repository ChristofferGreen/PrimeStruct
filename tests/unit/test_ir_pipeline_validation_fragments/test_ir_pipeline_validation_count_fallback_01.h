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
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++aliasMapCountResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++aliasMapCountEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(aliasMapCountResolveCalls == 1);
  CHECK(aliasMapCountEmitCalls == 1);

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
  int atEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            atCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++atResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++atEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(atResolveCalls == 1);
  CHECK(atEmitCalls == 1);

  primec::Expr canonicalAtCall = atCall;
  canonicalAtCall.name = "/std/collections/vector/at";
  int canonicalAtResolveCalls = 0;
  int canonicalAtEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            canonicalAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalAtResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++canonicalAtEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalAtResolveCalls == 1);
  CHECK(canonicalAtEmitCalls == 1);

  primec::Expr aliasAtCall = atCall;
  aliasAtCall.name = "/vector/at";
  int aliasAtResolveCalls = 0;
  int aliasAtEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            aliasAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++aliasAtResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "/vector/at");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++aliasAtEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "/vector/at");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(aliasAtResolveCalls == 1);
  CHECK(aliasAtEmitCalls == 1);

  primec::Expr aliasMapAtCall = atCall;
  aliasMapAtCall.name = "/map/at";
  int aliasMapAtResolveCalls = 0;
  int aliasMapAtEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            aliasMapAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++aliasMapAtResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++aliasMapAtEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(aliasMapAtResolveCalls == 1);
  CHECK(aliasMapAtEmitCalls == 1);

  primec::Expr reorderedAtCall = atCall;
  reorderedAtCall.args = {indexArg, targetArg};
  int reorderedAtResolveCalls = 0;
  int reorderedAtEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            reorderedAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++reorderedAtResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++reorderedAtEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
