        bool handledConversionsAndCalls = false;
        if (!ir_lowerer::emitConversionsAndCallsOperatorExpr(
                expr,
                localsIn,
                nextLocal,
                emitExpr,
                inferExprKind,
                emitCompareToZero,
                allocTempLocal,
                emitFloatToIntNonFinite,
                resolveStringTableTarget,
                valueKindFromTypeName,
                getMathConstantName,
                inferStructExprPath,
                [&](const std::string &structTypeName, int32_t &slotCount) {
                  StructSlotLayout layout;
                  if (!resolveStructSlotLayout(structTypeName, layout)) {
                    return false;
                  }
                  slotCount = layout.totalSlots;
                  return true;
                },
                emitStructCopyFromPtrs,
                function.instructions,
                handledConversionsAndCalls,
                error)) {
          return false;
        }
        if (handledConversionsAndCalls) {
          return true;
        }
        if (isBlockCall(expr) && expr.hasBodyArguments) {
          if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
            error = "block expression does not accept arguments";
            return false;
          }
          if (resolveDefinitionCall(expr) != nullptr) {
            error = "block arguments require a definition target: " + resolveExprPath(expr);
            return false;
          }
          if (expr.bodyArguments.empty()) {
            error = "block expression requires a value";
            return false;
          }
          LocalMap blockLocals = localsIn;
          for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
            const Expr &bodyStmt = expr.bodyArguments[i];
            const bool isLast = (i + 1 == expr.bodyArguments.size());
            if (bodyStmt.isBinding) {
              if (isLast) {
                error = "block expression must end with an expression";
                return false;
              }
              if (!emitStatement(bodyStmt, blockLocals)) {
                return false;
              }
              continue;
            }
            if (isReturnCall(bodyStmt)) {
              if (bodyStmt.args.size() != 1) {
                error = "return requires a value in block expression";
                return false;
              }
              return emitExpr(bodyStmt.args.front(), blockLocals);
            }
            if (isLast) {
              return emitExpr(bodyStmt, blockLocals);
            }
            if (!emitStatement(bodyStmt, blockLocals)) {
              return false;
            }
          }
          error = "block expression requires a value";
          return false;
        }
        if (isMatchCall(expr)) {
          Expr expanded;
          if (!lowerMatchToIf(expr, expanded, error)) {
            return false;
          }
          return emitExpr(expanded, localsIn);
        }
        if (isIfCall(expr)) {
          if (expr.args.size() != 3) {
            error = "if requires condition, then, else";
            return false;
          }
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error = "if does not accept trailing block arguments";
            return false;
          }
          const Expr &cond = expr.args[0];
          const Expr &thenArg = expr.args[1];
          const Expr &elseArg = expr.args[2];
          auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
            if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
              return false;
            }
            if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
              return false;
            }
            if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
              return false;
            }
            return true;
          };
          if (!isIfBlockEnvelope(thenArg) || !isIfBlockEnvelope(elseArg)) {
            error = "if branches require block envelopes";
            return false;
          }
          auto inferBranchValueKind = [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
            LocalMap branchLocals = localsBase;
            bool sawValue = false;
            LocalInfo::ValueKind lastKind = LocalInfo::ValueKind::Unknown;
            for (const auto &bodyExpr : candidate.bodyArguments) {
              if (bodyExpr.isBinding) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                LocalInfo info;
                info.index = 0;
                info.isMutable = isBindingMutable(bodyExpr);
                info.kind = bindingKind(bodyExpr);
