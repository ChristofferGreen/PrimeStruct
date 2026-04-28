    struct LoweredSumVariantSelection {
      const Definition *sumDef = nullptr;
      const SumVariant *variant = nullptr;
      const Expr *payloadExpr = nullptr;
      LocalInfo::ValueKind payloadKind = LocalInfo::ValueKind::Unknown;
      std::string payloadStructPath;
      int32_t payloadSlotCount = 1;
      bool payloadIsAggregate = false;
    };

    struct LoweredSumPayloadStorageInfo {
      LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
      std::string structPath;
      int32_t slotCount = 1;
      bool isAggregate = false;
    };

    enum class LoweredSumPickEmitResult {
      NotMatched,
      Emitted,
      Error,
    };

    auto isLowerableSumDefinition = [](const Definition &def) {
      return ir_lowerer::definitionHasTransform(def, "sum");
    };

    auto findSumVariantByName = [](const Definition &sumDef,
                                   const std::string &variantName) -> const SumVariant * {
      for (const auto &variant : sumDef.sumVariants) {
        if (variant.name == variantName) {
          return &variant;
        }
      }
      return nullptr;
    };

    auto sumPayloadTypeText = [](const SumVariant &variant) {
      if (!variant.payloadTypeText.empty()) {
        return trimTemplateTypeText(variant.payloadTypeText);
      }
      if (variant.payloadTemplateArgs.empty()) {
        return trimTemplateTypeText(variant.payloadType);
      }
      return trimTemplateTypeText(variant.payloadType) + "<" +
             joinTemplateArgsText(variant.payloadTemplateArgs) + ">";
    };

    auto sumPayloadKind = [&](const SumVariant &variant) {
      return valueKindFromTypeName(sumPayloadTypeText(variant));
    };

    auto resolveSumPayloadStorageInfo =
        [&](const Definition &sumDef,
            const SumVariant &variant,
            LoweredSumPayloadStorageInfo &infoOut) -> bool {
      infoOut = {};
      infoOut.valueKind = sumPayloadKind(variant);
      if (infoOut.valueKind != LocalInfo::ValueKind::Unknown) {
        infoOut.slotCount = 1;
        return true;
      }
      std::string payloadStructPath;
      if (!resolveStructTypeName(sumPayloadTypeText(variant),
                                 sumDef.namespacePrefix,
                                 payloadStructPath)) {
        return false;
      }
      StructSlotLayout payloadLayout;
      if (!resolveStructSlotLayout(payloadStructPath, payloadLayout)) {
        return false;
      }
      infoOut.structPath = std::move(payloadStructPath);
      infoOut.slotCount = payloadLayout.totalSlots;
      infoOut.isAggregate = true;
      return true;
    };

    auto resolveSumDefinitionByPath = [&](const std::string &path) -> const Definition * {
      if (path.empty()) {
        return nullptr;
      }
      auto defIt = defMap.find(path);
      if (defIt == defMap.end() || defIt->second == nullptr ||
          !isLowerableSumDefinition(*defIt->second)) {
        return nullptr;
      }
      return defIt->second;
    };

    auto resolveSumDefinitionForTypeText =
        [&](const std::string &typeText, const std::string &namespacePrefix) -> const Definition * {
      std::string normalized = trimTemplateTypeText(typeText);
      std::string base;
      std::string argList;
      if (splitTemplateTypeName(normalized, base, argList)) {
        normalized = trimTemplateTypeText(base);
      }
      if (normalized.empty()) {
        return nullptr;
      }

      std::vector<std::string> candidatePaths;
      auto addCandidate = [&](std::string candidate) {
        candidate = trimTemplateTypeText(candidate);
        if (!candidate.empty()) {
          candidatePaths.push_back(std::move(candidate));
        }
      };
      addCandidate(normalized);
      if (normalized.front() != '/') {
        addCandidate("/" + normalized);
        if (!namespacePrefix.empty()) {
          addCandidate(namespacePrefix + "/" + normalized);
        }
      }
      Expr syntheticTypeExpr;
      syntheticTypeExpr.kind = Expr::Kind::Call;
      syntheticTypeExpr.name = normalized;
      syntheticTypeExpr.namespacePrefix = namespacePrefix;
      addCandidate(resolveExprPath(syntheticTypeExpr));

      std::sort(candidatePaths.begin(), candidatePaths.end());
      candidatePaths.erase(std::unique(candidatePaths.begin(), candidatePaths.end()), candidatePaths.end());
      for (const std::string &candidate : candidatePaths) {
        if (const Definition *sumDef = resolveSumDefinitionByPath(candidate)) {
          return sumDef;
        }
      }

      const std::string suffix = normalized.front() == '/' ? normalized : "/" + normalized;
      std::vector<std::string> suffixMatches;
      for (const auto &[path, def] : defMap) {
        if (def == nullptr || !isLowerableSumDefinition(*def)) {
          continue;
        }
        if (path.size() >= suffix.size() &&
            path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0) {
          suffixMatches.push_back(path);
        }
      }
      std::sort(suffixMatches.begin(), suffixMatches.end());
      suffixMatches.erase(std::unique(suffixMatches.begin(), suffixMatches.end()), suffixMatches.end());
      return suffixMatches.size() == 1 ? resolveSumDefinitionByPath(suffixMatches.front()) : nullptr;
    };

    auto resolveSumDefinitionForLocalInfo = [&](const LocalInfo &info) -> const Definition * {
      return resolveSumDefinitionForTypeText(info.structTypeName, function.name);
    };

    auto loweredSumSlotCount = [&](const Definition &sumDef, int32_t &totalSlotsOut) -> bool {
      int32_t maxPayloadSlots = 1;
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSumPayloadStorageInfo(sumDef, variant, payloadInfo)) {
          totalSlotsOut = 0;
          return false;
        }
        maxPayloadSlots = std::max(maxPayloadSlots, payloadInfo.slotCount);
      }
      totalSlotsOut = 2 + maxPayloadSlots;
      return true;
    };

    auto firstUnsupportedSumPayloadVariant = [&](const Definition &sumDef) -> const SumVariant * {
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSumPayloadStorageInfo(sumDef, variant, payloadInfo)) {
          return &variant;
        }
      }
      return nullptr;
    };

    auto selectExplicitSumVariantForConstructor =
        [&](const Expr &initializer,
            const Definition &targetSum,
            LoweredSumVariantSelection &selectionOut) -> bool {
      selectionOut = {};
      if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
          initializer.isFieldAccess || initializer.args.size() != 1 ||
          initializer.argNames.size() != 1 || !initializer.argNames.front().has_value()) {
        return false;
      }
      const Definition *constructorSum =
          resolveSumDefinitionForTypeText(initializer.name, initializer.namespacePrefix);
      if (constructorSum == nullptr || constructorSum->fullPath != targetSum.fullPath) {
        return false;
      }
      const SumVariant *variant = findSumVariantByName(targetSum, *initializer.argNames.front());
      if (variant == nullptr) {
        return false;
      }
      LoweredSumPayloadStorageInfo payloadInfo;
      if (!resolveSumPayloadStorageInfo(targetSum, *variant, payloadInfo)) {
        return false;
      }
      selectionOut.sumDef = &targetSum;
      selectionOut.variant = variant;
      selectionOut.payloadExpr = &initializer.args.front();
      selectionOut.payloadKind = payloadInfo.valueKind;
      selectionOut.payloadStructPath = std::move(payloadInfo.structPath);
      selectionOut.payloadSlotCount = payloadInfo.slotCount;
      selectionOut.payloadIsAggregate = payloadInfo.isAggregate;
      return true;
    };

    auto selectSumVariantForInitializer =
        [&](const Expr &initializer,
            const Definition &targetSum,
            const LocalMap &valueLocals,
            LoweredSumVariantSelection &selectionOut) -> bool {
      if (selectExplicitSumVariantForConstructor(initializer, targetSum, selectionOut)) {
        return true;
      }
      const SumVariant *matchedVariant = nullptr;
      LoweredSumPayloadStorageInfo matchedPayloadInfo;
      const LocalInfo::ValueKind initializerKind = inferExprKind(initializer, valueLocals);
      std::string initializerStructPath = inferStructExprPath(initializer, valueLocals);
      if (initializerStructPath.empty() && initializer.kind == Expr::Kind::Call) {
        if (const Definition *initCallee = resolveDefinitionCall(initializer);
            initCallee != nullptr && ir_lowerer::isStructDefinition(*initCallee)) {
          initializerStructPath = initCallee->fullPath;
        }
      }
      for (const auto &variant : targetSum.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSumPayloadStorageInfo(targetSum, variant, payloadInfo)) {
          continue;
        }
        const bool matchesPayload =
            payloadInfo.isAggregate
                ? (!initializerStructPath.empty() &&
                   initializerStructPath == payloadInfo.structPath)
                : payloadInfo.valueKind == initializerKind;
        if (!matchesPayload) {
          continue;
        }
        if (matchedVariant != nullptr) {
          return false;
        }
        matchedVariant = &variant;
        matchedPayloadInfo = std::move(payloadInfo);
      }
      if (matchedVariant == nullptr) {
        return false;
      }
      selectionOut.sumDef = &targetSum;
      selectionOut.variant = matchedVariant;
      selectionOut.payloadExpr = &initializer;
      selectionOut.payloadKind = matchedPayloadInfo.valueKind;
      selectionOut.payloadStructPath = std::move(matchedPayloadInfo.structPath);
      selectionOut.payloadSlotCount = matchedPayloadInfo.slotCount;
      selectionOut.payloadIsAggregate = matchedPayloadInfo.isAggregate;
      return true;
    };

    auto unsupportedSumPayloadError = [&](const Definition &sumDef, const SumVariant &variant) {
      return "native backend does not support sum payload type: " +
             sumDef.fullPath + "/" + variant.name + " (" + sumPayloadTypeText(variant) + ")";
    };

    auto emitLoweredSumHeader = [&](int32_t baseLocal, int32_t totalSlots) {
      function.instructions.push_back(
          {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(totalSlots - 1))});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
    };

    auto emitLoweredSumConstructionIntoLocal =
        [&](int32_t baseLocal,
            const Definition &sumDef,
            const Expr &initializer,
            const LocalMap &valueLocals) -> bool {
      LoweredSumVariantSelection selection;
      if (!selectSumVariantForInitializer(initializer, sumDef, valueLocals, selection) ||
          selection.variant == nullptr || selection.payloadExpr == nullptr) {
        error = "native backend could not select sum variant for " + sumDef.fullPath;
        return false;
      }
      function.instructions.push_back(
          {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(selection.variant->variantIndex))});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
      if (selection.payloadIsAggregate) {
        if (!emitExpr(*selection.payloadExpr, valueLocals)) {
          return false;
        }
        const int32_t srcPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        const int32_t destPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal + 2)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
        if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, selection.payloadSlotCount)) {
          return false;
        }
        if (selection.payloadExpr->kind == Expr::Kind::Call) {
          ir_lowerer::emitDisarmTemporaryStructAfterCopy(
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              srcPtrLocal,
              selection.payloadStructPath);
        }
        return true;
      }
      if (!emitExpr(*selection.payloadExpr, valueLocals)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 2)});
      return true;
    };

    auto tryEmitLoweredSumConstructorExpr = [&](const Expr &expr, const LocalMap &valueLocals) -> bool {
      const Definition *sumDef = resolveSumDefinitionForTypeText(expr.name, expr.namespacePrefix);
      if (sumDef == nullptr) {
        return false;
      }
      int32_t totalSlots = 0;
      if (!loweredSumSlotCount(*sumDef, totalSlots)) {
        LoweredSumVariantSelection selection;
        if (selectExplicitSumVariantForConstructor(expr, *sumDef, selection) && selection.variant != nullptr) {
          error = unsupportedSumPayloadError(*sumDef, *selection.variant);
        } else if (const SumVariant *unsupportedVariant = firstUnsupportedSumPayloadVariant(*sumDef);
                   unsupportedVariant != nullptr) {
          error = unsupportedSumPayloadError(*sumDef, *unsupportedVariant);
        } else {
          error = "native backend does not support sum payload type on " + sumDef->fullPath;
        }
        return false;
      }
      const int32_t baseLocal = nextLocal;
      nextLocal += totalSlots;
      emitLoweredSumHeader(baseLocal, totalSlots);
      if (!emitLoweredSumConstructionIntoLocal(baseLocal, *sumDef, expr, valueLocals)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
      return true;
    };

    auto resolvePickTargetSumDefinition =
        [&](const Expr &targetExpr, const LocalMap &valueLocals) -> const Definition * {
      if (targetExpr.kind == Expr::Kind::Name) {
        auto localIt = valueLocals.find(targetExpr.name);
        if (localIt != valueLocals.end()) {
          return resolveSumDefinitionForLocalInfo(localIt->second);
        }
      }
      if (targetExpr.kind == Expr::Kind::Call && !targetExpr.isMethodCall) {
        return resolveSumDefinitionForTypeText(targetExpr.name, targetExpr.namespacePrefix);
      }
      return nullptr;
    };

    auto emitLoadSumSlotIndirect = [&](int32_t sumPtrLocal, int32_t slotOffset) {
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
      function.instructions.push_back(
          {IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
      function.instructions.push_back({IrOpcode::AddI64, 0});
      function.instructions.push_back({IrOpcode::LoadIndirect, 0});
    };

    auto emitSumTagComparison = [&](int32_t sumPtrLocal, int32_t tagValue) {
      emitLoadSumSlotIndirect(sumPtrLocal, 1);
      function.instructions.push_back(
          {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(tagValue))});
      function.instructions.push_back({IrOpcode::CmpEqI32, 0});
    };

    auto bindPickPayload =
        [&](const Definition &sumDef,
            const SumVariant &variant,
            const Expr &binderExpr,
            int32_t sumPtrLocal,
            LocalMap &branchLocals) -> bool {
      const LocalInfo::ValueKind payloadKind = sumPayloadKind(variant);
      LoweredSumPayloadStorageInfo payloadStorage;
      if (!resolveSumPayloadStorageInfo(sumDef, variant, payloadStorage)) {
        error = unsupportedSumPayloadError(sumDef, variant);
        return false;
      }
      LocalInfo payloadInfo;
      payloadInfo.kind = LocalInfo::Kind::Value;
      payloadInfo.valueKind = payloadStorage.isAggregate ? LocalInfo::ValueKind::Int64 : payloadKind;
      payloadInfo.structTypeName = payloadStorage.structPath;
      payloadInfo.structSlotCount = payloadStorage.slotCount;
      payloadInfo.index = nextLocal++;
      if (payloadStorage.isAggregate) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
        function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
        function.instructions.push_back({IrOpcode::AddI64, 0});
      } else {
        emitLoadSumSlotIndirect(sumPtrLocal, 2);
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadInfo.index)});
      branchLocals.emplace(binderExpr.name, payloadInfo);
      return true;
    };

    auto isPickArmEnvelope = [](const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
             !candidate.isMethodCall && !candidate.isFieldAccess &&
             candidate.hasBodyArguments && !candidate.name.empty() &&
             candidate.templateArgs.empty() && !hasNamedArguments(candidate.argNames) &&
             candidate.args.size() == 1 && candidate.args.front().kind == Expr::Kind::Name;
    };

    auto findPickArmValueExpr = [&](const Expr &arm) -> const Expr * {
      const Expr *valueExpr = nullptr;
      bool sawReturn = false;
      for (const Expr &bodyExpr : arm.bodyArguments) {
        if (bodyExpr.isBinding) {
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() != 1) {
            return nullptr;
          }
          valueExpr = &bodyExpr.args.front();
          sawReturn = true;
          continue;
        }
        if (!sawReturn) {
          valueExpr = &bodyExpr;
        }
      }
      return valueExpr;
    };

    auto emitPickArmPrefixStatements =
        [&](const Expr &arm, const Expr *valueExpr, LocalMap &branchLocals) -> bool {
      for (const Expr &bodyExpr : arm.bodyArguments) {
        if (isReturnCall(bodyExpr) && bodyExpr.args.size() == 1 && valueExpr == &bodyExpr.args.front()) {
          return true;
        }
        if (&bodyExpr == valueExpr) {
          return true;
        }
        if (!emitStatement(bodyExpr, branchLocals)) {
          return false;
        }
      }
      return true;
    };

    auto tryEmitPickExpr = [&](const Expr &expr, const LocalMap &valueLocals) -> LoweredSumPickEmitResult {
      if (!isPickCall(expr)) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (expr.args.size() != 1 || expr.bodyArguments.empty()) {
        error = "native backend requires pick(value) with variant arms";
        return LoweredSumPickEmitResult::Error;
      }
      const Definition *sumDef = resolvePickTargetSumDefinition(expr.args.front(), valueLocals);
      if (sumDef == nullptr) {
        error = "native backend pick target requires sum value";
        return LoweredSumPickEmitResult::Error;
      }
      const int32_t sumPtrLocal = allocTempLocal();
      if (!emitExpr(expr.args.front(), valueLocals)) {
        return LoweredSumPickEmitResult::Error;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sumPtrLocal)});
      const int32_t resultLocal = allocTempLocal();
      std::vector<size_t> endJumps;
      for (const Expr &arm : expr.bodyArguments) {
        if (!isPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant(payload) blocks";
          return LoweredSumPickEmitResult::Error;
        }
        const SumVariant *variant = findSumVariantByName(*sumDef, arm.name);
        if (variant == nullptr) {
          error = "native backend unknown pick variant on " + sumDef->fullPath + ": " + arm.name;
          return LoweredSumPickEmitResult::Error;
        }
        emitSumTagComparison(sumPtrLocal, variant->variantIndex);
        const size_t nextArmJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        LocalMap branchLocals = valueLocals;
        if (!bindPickPayload(*sumDef, *variant, arm.args.front(), sumPtrLocal, branchLocals)) {
          return LoweredSumPickEmitResult::Error;
        }
        const Expr *valueExpr = findPickArmValueExpr(arm);
        if (valueExpr == nullptr) {
          error = "native backend requires pick arms to produce a value";
          return LoweredSumPickEmitResult::Error;
        }
        if (!emitPickArmPrefixStatements(arm, valueExpr, branchLocals) ||
            !emitExpr(*valueExpr, branchLocals)) {
          return LoweredSumPickEmitResult::Error;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextArmJump].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
      return LoweredSumPickEmitResult::Emitted;
    };

    auto tryEmitPickStatement = [&](const Expr &stmt, LocalMap &localsIn) -> LoweredSumPickEmitResult {
      if (!isPickCall(stmt)) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (!stmt.hasBodyArguments && stmt.bodyArguments.empty()) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (stmt.args.size() != 1 || stmt.bodyArguments.empty()) {
        error = "native backend requires pick(value) with variant arms";
        return LoweredSumPickEmitResult::Error;
      }
      const Definition *sumDef = resolvePickTargetSumDefinition(stmt.args.front(), localsIn);
      if (sumDef == nullptr) {
        error = "native backend pick target requires sum value";
        return LoweredSumPickEmitResult::Error;
      }
      const int32_t sumPtrLocal = allocTempLocal();
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return LoweredSumPickEmitResult::Error;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sumPtrLocal)});
      std::vector<size_t> endJumps;
      for (const Expr &arm : stmt.bodyArguments) {
        if (!isPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant(payload) blocks";
          return LoweredSumPickEmitResult::Error;
        }
        const SumVariant *variant = findSumVariantByName(*sumDef, arm.name);
        if (variant == nullptr) {
          error = "native backend unknown pick variant on " + sumDef->fullPath + ": " + arm.name;
          return LoweredSumPickEmitResult::Error;
        }
        emitSumTagComparison(sumPtrLocal, variant->variantIndex);
        const size_t nextArmJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        LocalMap branchLocals = localsIn;
        if (!bindPickPayload(*sumDef, *variant, arm.args.front(), sumPtrLocal, branchLocals)) {
          return LoweredSumPickEmitResult::Error;
        }
        for (const Expr &bodyExpr : arm.bodyArguments) {
          if (!emitStatement(bodyExpr, branchLocals)) {
            return LoweredSumPickEmitResult::Error;
          }
        }
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextArmJump].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      return LoweredSumPickEmitResult::Emitted;
    };
