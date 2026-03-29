  CHECK(error.empty());
  CHECK(reorderedAtResolveCalls == 2);
  CHECK(reorderedAtEmitCalls == 1);

  primec::Expr reorderedAtStringCall = atCall;
  primec::Expr stringKeyArg;
  stringKeyArg.kind = primec::Expr::Kind::StringLiteral;
  stringKeyArg.stringValue = "\"only\"raw_utf8";
  reorderedAtStringCall.args = {stringKeyArg, targetArg};
  int reorderedAtStringResolveCalls = 0;
  int reorderedAtStringEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            reorderedAtStringCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++reorderedAtStringResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++reorderedAtStringEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(reorderedAtStringResolveCalls == 2);
  CHECK(reorderedAtStringEmitCalls == 1);

  primec::Expr reorderedAtNameCall = atCall;
  primec::Expr indexNameArg;
  indexNameArg.kind = primec::Expr::Kind::Name;
  indexNameArg.name = "indexName";
  reorderedAtNameCall.args = {indexNameArg, targetArg};
  int reorderedAtNameResolveCalls = 0;
  int reorderedAtNameEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            reorderedAtNameCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++reorderedAtNameResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++reorderedAtNameEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error,
            [&](const primec::Expr &receiverExpr) {
              return receiverExpr.kind == primec::Expr::Kind::Name && receiverExpr.name == "items";
            }) == Result::Emitted);
  CHECK(error.empty());
  CHECK(reorderedAtNameResolveCalls == 1);
  CHECK(reorderedAtNameEmitCalls == 1);

  primec::Expr noReorderAtNameCall = atCall;
  noReorderAtNameCall.args = {targetArg, indexNameArg};
  int noReorderAtNameResolveCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            noReorderAtNameCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++noReorderAtNameResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "indexName") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error,
            [&](const primec::Expr &receiverExpr) {
              return receiverExpr.kind == primec::Expr::Kind::Name && receiverExpr.name == "items";
            }) == Result::NoCallee);
  CHECK(error.empty());
  CHECK(noReorderAtNameResolveCalls == 1);

  primec::Expr namedAtCall = reorderedAtNameCall;
  namedAtCall.argNames = {std::string("index"), std::string("values")};
  int namedAtResolveCalls = 0;
  int namedAtEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            namedAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++namedAtResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++namedAtEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error,
            [&](const primec::Expr &receiverExpr) {
              return receiverExpr.kind == primec::Expr::Kind::Name && receiverExpr.name == "items";
            }) == Result::Emitted);
  CHECK(error.empty());
  CHECK(namedAtResolveCalls == 1);
  CHECK(namedAtEmitCalls == 1);

  primec::Expr atUnsafeCall = atCall;
  atUnsafeCall.name = "at_unsafe";
  int atUnsafeResolveCalls = 0;
  int atUnsafeEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            atUnsafeCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++atUnsafeResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++atUnsafeEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(atUnsafeResolveCalls == 1);
  CHECK(atUnsafeEmitCalls == 1);

  primec::Expr canonicalAtUnsafeCall = atUnsafeCall;
  canonicalAtUnsafeCall.name = "/std/collections/vector/at_unsafe";
  int canonicalAtUnsafeResolveCalls = 0;
  int canonicalAtUnsafeEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            canonicalAtUnsafeCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalAtUnsafeResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++canonicalAtUnsafeEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalAtUnsafeResolveCalls == 1);
  CHECK(canonicalAtUnsafeEmitCalls == 1);

  primec::Expr reorderedAtUnsafeStringCall = atUnsafeCall;
  reorderedAtUnsafeStringCall.args = {stringKeyArg, targetArg};
  int reorderedAtUnsafeStringResolveCalls = 0;
  int reorderedAtUnsafeStringEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            reorderedAtUnsafeStringCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++reorderedAtUnsafeStringResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++reorderedAtUnsafeStringEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(reorderedAtUnsafeStringResolveCalls == 2);
  CHECK(reorderedAtUnsafeStringEmitCalls == 1);

  primec::Expr canonicalMapAtUnsafeCall = reorderedAtUnsafeStringCall;
  canonicalMapAtUnsafeCall.name = "/std/collections/map/at_unsafe";
  int canonicalMapAtUnsafeResolveCalls = 0;
  int canonicalMapAtUnsafeEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            canonicalMapAtUnsafeCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalMapAtUnsafeResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++canonicalMapAtUnsafeEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at_unsafe");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalMapAtUnsafeResolveCalls == 2);
  CHECK(canonicalMapAtUnsafeEmitCalls == 1);

  primec::Expr tempReceiver;
  tempReceiver.kind = primec::Expr::Kind::Call;
  tempReceiver.name = "wrapItems";
  primec::Expr tempLeadingAtCall = reorderedAtCall;
  tempLeadingAtCall.args = {tempReceiver, targetArg};
  int tempLeadingAtResolveCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            tempLeadingAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++tempLeadingAtResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "at");
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NoCallee);
  CHECK(error.empty());
  CHECK(tempLeadingAtResolveCalls == 1);

  primec::Expr reorderedPushCall;
  reorderedPushCall.kind = primec::Expr::Kind::Call;
  reorderedPushCall.name = "push";
  reorderedPushCall.args = {indexArg, targetArg};
  int reorderedPushResolveCalls = 0;
  int reorderedPushEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            reorderedPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++reorderedPushResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++reorderedPushEmitCalls;
              CHECK(methodExpr.isMethodCall);
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(reorderedPushResolveCalls == 2);
  CHECK(reorderedPushEmitCalls == 1);

  primec::Expr aliasPushCall = reorderedPushCall;
  aliasPushCall.name = "/vector/push";
  int aliasPushResolveCalls = 0;
  int aliasPushEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            aliasPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++aliasPushResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++aliasPushEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(aliasPushResolveCalls == 0);
  CHECK(aliasPushEmitCalls == 0);

  primec::Expr canonicalPushCall = reorderedPushCall;
  canonicalPushCall.name = "/std/collections/vector/push";
  int canonicalPushResolveCalls = 0;
  int canonicalPushEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            canonicalPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalPushResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++canonicalPushEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalPushResolveCalls == 2);
  CHECK(canonicalPushEmitCalls == 1);

  primec::Expr canonicalClearCall;
  canonicalClearCall.kind = primec::Expr::Kind::Call;
  canonicalClearCall.name = "/std/collections/vector/clear";
  canonicalClearCall.args = {targetArg};
  int canonicalClearResolveCalls = 0;
  int canonicalClearEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            canonicalClearCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++canonicalClearResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "clear");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++canonicalClearEmitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "clear");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalClearResolveCalls == 1);
  CHECK(canonicalClearEmitCalls == 1);

  primec::Expr boolArg;
  boolArg.kind = primec::Expr::Kind::BoolLiteral;
  boolArg.boolValue = true;
  primec::Expr reorderedBoolPushCall = reorderedPushCall;
  reorderedBoolPushCall.args = {boolArg, targetArg};
  int reorderedBoolPushResolveCalls = 0;
  int reorderedBoolPushEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            reorderedBoolPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++reorderedBoolPushResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++reorderedBoolPushEmitCalls;
              CHECK(methodExpr.isMethodCall);
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(reorderedBoolPushResolveCalls == 2);
  CHECK(reorderedBoolPushEmitCalls == 1);

  primec::Expr namedPushCall = reorderedPushCall;
  primec::Expr namedPushValueArg;
  namedPushValueArg.kind = primec::Expr::Kind::StringLiteral;
  namedPushValueArg.stringValue = "\"payload\"raw_utf8";
  namedPushCall.args = {namedPushValueArg, targetArg};
  namedPushCall.argNames = {std::string("value"), std::string("values")};
  int namedPushResolveCalls = 0;
  int namedPushEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            namedPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++namedPushResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
                  methodExpr.args.front().name == "items") {
                return &callee;
              }
              return nullptr;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++namedPushEmitCalls;
              CHECK(methodExpr.isMethodCall);
              REQUIRE_FALSE(methodExpr.args.empty());
              CHECK(methodExpr.args.front().kind == primec::Expr::Kind::Name);
              CHECK(methodExpr.args.front().name == "items");
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(namedPushResolveCalls == 1);
  CHECK(namedPushEmitCalls == 1);

  primec::Expr tempFirstPushCall = reorderedPushCall;
  tempFirstPushCall.args = {tempReceiver, targetArg};
  int tempFirstPushResolveCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            tempFirstPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++tempFirstPushResolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "push");
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NoCallee);
  CHECK(error.empty());
  CHECK(tempFirstPushResolveCalls == 1);

  int builtinLikeResolveCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++builtinLikeResolveCalls;
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NoCallee);
  CHECK(builtinLikeResolveCalls == 1);

  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NoCallee);

  error = "preserve";
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            capacityCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NoCallee);
  CHECK(error == "preserve");

  primec::Expr bodyArgExpr;
  bodyArgExpr.kind = primec::Expr::Kind::Literal;
  bodyArgExpr.intWidth = 32;
  bodyArgExpr.literalValue = 1;
  primec::Expr countWithBodyArgs = countCall;
  countWithBodyArgs.hasBodyArguments = true;
  countWithBodyArgs.bodyArguments.push_back(bodyArgExpr);
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countWithBodyArgs,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);
  CHECK(error == "native backend does not support block arguments on calls");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              error = "emit failed";
              return false;
            },
            error) == Result::Error);
  CHECK(error == "emit failed");
