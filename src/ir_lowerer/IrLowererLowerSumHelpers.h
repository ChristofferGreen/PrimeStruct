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

    auto isStdlibResultSumDefinition = [](const Definition &sumDef) {
      return sumDef.fullPath == "/std/result/Result" ||
             sumDef.fullPath.rfind("/std/result/Result__", 0) == 0;
    };

    auto isLegacyResultOkCall = [](const Expr &expr) {
      return expr.kind == Expr::Kind::Call && expr.isMethodCall &&
             !expr.isFieldAccess && expr.name == "ok" &&
             !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    };

    auto isLegacyResultMapCall = [](const Expr &expr) {
      return expr.kind == Expr::Kind::Call && !expr.isFieldAccess && expr.name == "map" &&
             expr.args.size() == 3 && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    };

    auto isLegacyResultAndThenCall = [](const Expr &expr) {
      return expr.kind == Expr::Kind::Call && !expr.isFieldAccess && expr.name == "and_then" &&
             expr.args.size() == 3 && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    };

    auto isLegacyResultMap2Call = [](const Expr &expr) {
      return expr.kind == Expr::Kind::Call && !expr.isFieldAccess && expr.name == "map2" &&
             expr.args.size() == 4 && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    };

    auto sumPayloadTypeText = [](const SumVariant &variant) {
      if (!variant.hasPayload) {
        return std::string{};
      }
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
      if (!variant.hasPayload) {
        infoOut.slotCount = 0;
        return true;
      }
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

    auto resolvePublishedSumPayloadStorageInfo =
        [&](const Definition &sumDef,
            const SemanticProgramSumVariantMetadata &publishedVariant,
            LoweredSumPayloadStorageInfo &infoOut) -> bool {
      infoOut = {};
      if (!publishedVariant.hasPayload) {
        infoOut.slotCount = 0;
        return true;
      }
      const std::string payloadTypeText =
          trimTemplateTypeText(publishedVariant.payloadTypeText);
      infoOut.valueKind = valueKindFromTypeName(payloadTypeText);
      if (infoOut.valueKind != LocalInfo::ValueKind::Unknown) {
        infoOut.slotCount = 1;
        return true;
      }
      std::string payloadStructPath;
      if (!resolveStructTypeName(payloadTypeText,
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
      int32_t maxPayloadSlots = 0;
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

    auto defaultUnitVariant = [](const Definition &sumDef) -> const SumVariant * {
      if (sumDef.sumVariants.empty() || sumDef.sumVariants.front().hasPayload) {
        return nullptr;
      }
      return &sumDef.sumVariants.front();
    };

    auto unitVariantForNameExpr =
        [&](const Definition &sumDef, const Expr &expr) -> const SumVariant * {
      if (expr.kind != Expr::Kind::Name) {
        return nullptr;
      }
      const SumVariant *variant = findSumVariantByName(sumDef, expr.name);
      return variant != nullptr && !variant->hasPayload ? variant : nullptr;
    };

    auto unitVariantForConstructorArg =
        [&](const Definition &sumDef, const Expr &arg) -> const SumVariant * {
      if (const SumVariant *variant = unitVariantForNameExpr(sumDef, arg)) {
        return variant;
      }
      if (arg.kind != Expr::Kind::Call || arg.name != "block" ||
          !arg.hasBodyArguments || !arg.args.empty() ||
          arg.bodyArguments.size() != 1) {
        return nullptr;
      }
      return unitVariantForNameExpr(sumDef, arg.bodyArguments.front());
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
          initializer.isFieldAccess) {
        return false;
      }
      const Definition *constructorSum =
          resolveSumDefinitionForTypeText(initializer.name, initializer.namespacePrefix);
      if (constructorSum == nullptr || constructorSum->fullPath != targetSum.fullPath) {
        return false;
      }
      const std::vector<Expr> *constructorArgs = &initializer.args;
      const std::vector<std::optional<std::string>> *constructorArgNames =
          &initializer.argNames;
      std::vector<Expr> normalizedArgs;
      std::vector<std::optional<std::string>> normalizedArgNames;
      if (initializer.args.size() == 1 && initializer.argNames.size() == 1 &&
          !initializer.argNames.front().has_value() &&
          initializer.args.front().kind == Expr::Kind::Call &&
          initializer.args.front().name == "block" &&
          initializer.args.front().hasBodyArguments &&
          initializer.args.front().bodyArguments.empty() &&
          initializer.args.front().args.empty()) {
        constructorArgs = &normalizedArgs;
        constructorArgNames = &normalizedArgNames;
      }
      const SumVariant *variant = nullptr;
      if (constructorArgs->empty() && constructorArgNames->empty()) {
        variant = defaultUnitVariant(targetSum);
      } else if (constructorArgs->size() == 1 && constructorArgNames->size() == 1 &&
                 !constructorArgNames->front().has_value()) {
        variant = unitVariantForConstructorArg(targetSum, constructorArgs->front());
      } else if (constructorArgs->size() == 1 && constructorArgNames->size() == 1 &&
                 constructorArgNames->front().has_value()) {
        variant = findSumVariantByName(targetSum, *constructorArgNames->front());
      }
      if (variant == nullptr) {
        return false;
      }
      LoweredSumPayloadStorageInfo payloadInfo;
      if (!resolveSumPayloadStorageInfo(targetSum, *variant, payloadInfo)) {
        return false;
      }
      selectionOut.sumDef = &targetSum;
      selectionOut.variant = variant;
      selectionOut.payloadExpr = variant->hasPayload ? &constructorArgs->front() : nullptr;
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
      if (isStdlibResultSumDefinition(targetSum) && isLegacyResultOkCall(initializer)) {
        if (initializer.args.size() != 2 && initializer.args.size() != 1) {
          return false;
        }
        const SumVariant *variant = findSumVariantByName(targetSum, "ok");
        if (variant == nullptr) {
          return false;
        }
        if (variant->hasPayload != (initializer.args.size() == 2)) {
          return false;
        }
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSumPayloadStorageInfo(targetSum, *variant, payloadInfo)) {
          return false;
        }
        selectionOut.sumDef = &targetSum;
        selectionOut.variant = variant;
        selectionOut.payloadExpr = variant->hasPayload ? &initializer.args[1] : nullptr;
        selectionOut.payloadKind = payloadInfo.valueKind;
        selectionOut.payloadStructPath = std::move(payloadInfo.structPath);
        selectionOut.payloadSlotCount = payloadInfo.slotCount;
        selectionOut.payloadIsAggregate = payloadInfo.isAggregate;
        return true;
      }
      if (selectExplicitSumVariantForConstructor(initializer, targetSum, selectionOut)) {
        return true;
      }
      if (const SumVariant *variant = unitVariantForNameExpr(targetSum, initializer)) {
        selectionOut.sumDef = &targetSum;
        selectionOut.variant = variant;
        return true;
      }
      if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
          initializer.hasBodyArguments && initializer.bodyArguments.empty() &&
          initializer.args.empty()) {
        if (const SumVariant *variant = defaultUnitVariant(targetSum)) {
          selectionOut.sumDef = &targetSum;
          selectionOut.variant = variant;
          return true;
        }
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
        if (!variant.hasPayload) {
          continue;
        }
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
      auto emitLoadSumSlotIndirectForConstruction = [&](int32_t sumPtrLocal, int32_t slotOffset) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
        if (slotOffset != 0) {
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
        }
        function.instructions.push_back({IrOpcode::LoadIndirect, 0});
      };
      auto emitSumTagComparisonForConstruction = [&](int32_t sumPtrLocal, int32_t tagValue) {
        emitLoadSumSlotIndirectForConstruction(sumPtrLocal, 1);
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(tagValue))});
        function.instructions.push_back({IrOpcode::CmpEqI32, 0});
      };
      auto findResultLambdaValueExpr = [](const Expr &lambdaExpr) -> const Expr * {
        for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
          const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
          const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
          if (bodyExpr.isBinding) {
            continue;
          }
          if (isReturnCall(bodyExpr)) {
            if (bodyExpr.args.size() != 1 || !isLast) {
              return nullptr;
            }
            return &bodyExpr.args.front();
          }
          if (isLast) {
            return &bodyExpr;
          }
        }
        return nullptr;
      };
      auto emitResultLambdaPrefixStatements =
          [&](const Expr &lambdaExpr, const Expr *valueExpr, LocalMap &lambdaLocals) -> bool {
        for (const Expr &bodyExpr : lambdaExpr.bodyArguments) {
          if (isReturnCall(bodyExpr) && bodyExpr.args.size() == 1 && valueExpr == &bodyExpr.args.front()) {
            return true;
          }
          if (&bodyExpr == valueExpr) {
            return true;
          }
          if (!emitStatement(bodyExpr, lambdaLocals)) {
            return false;
          }
        }
        return true;
      };
      auto bindSumPayloadLocal =
          [&](const Definition &payloadSumDef,
              const SumVariant &payloadVariant,
              int32_t sumPtrLocal,
              const std::string &payloadName,
              LocalMap &payloadLocals) -> bool {
        LoweredSumPayloadStorageInfo payloadStorage;
        if (!resolveSumPayloadStorageInfo(payloadSumDef, payloadVariant, payloadStorage)) {
          error = unsupportedSumPayloadError(payloadSumDef, payloadVariant);
          return false;
        }
        LocalInfo payloadInfo;
        payloadInfo.kind = LocalInfo::Kind::Value;
        payloadInfo.valueKind = payloadStorage.isAggregate ? LocalInfo::ValueKind::Int64 : payloadStorage.valueKind;
        payloadInfo.structTypeName = payloadStorage.structPath;
        payloadInfo.structSlotCount = payloadStorage.slotCount;
        payloadInfo.index = nextLocal++;
        if (payloadStorage.isAggregate) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
        } else {
          emitLoadSumSlotIndirectForConstruction(sumPtrLocal, 2);
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadInfo.index)});
        if (payloadInfo.valueKind == LocalInfo::ValueKind::String && payloadInfo.structTypeName.empty()) {
          payloadInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
        }
        payloadLocals.emplace(payloadName, payloadInfo);
        return true;
      };
      auto emitCopySumPayload =
          [&](const Definition &sourceSumDef,
              const SumVariant &sourceVariant,
              const SumVariant &targetVariant,
              int32_t sourceSumPtrLocal,
              int32_t targetBaseLocal,
              const std::string &builtinName,
              const std::string &payloadDescription) -> bool {
        LoweredSumPayloadStorageInfo sourcePayload;
        LoweredSumPayloadStorageInfo targetPayload;
        if (!resolveSumPayloadStorageInfo(sourceSumDef, sourceVariant, sourcePayload)) {
          error = unsupportedSumPayloadError(sourceSumDef, sourceVariant);
          return false;
        }
        if (!resolveSumPayloadStorageInfo(sumDef, targetVariant, targetPayload)) {
          error = unsupportedSumPayloadError(sumDef, targetVariant);
          return false;
        }
        if (sourcePayload.isAggregate != targetPayload.isAggregate ||
            sourcePayload.valueKind != targetPayload.valueKind ||
            sourcePayload.structPath != targetPayload.structPath ||
            sourcePayload.slotCount != targetPayload.slotCount) {
          error = "native backend " + builtinName + " requires matching " +
                  payloadDescription + " payload storage";
          return false;
        }
        if (sourcePayload.isAggregate) {
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          const int32_t destPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          return emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, sourcePayload.slotCount);
        }
        emitLoadSumSlotIndirectForConstruction(sourceSumPtrLocal, 2);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
        return true;
      };
      auto emitSelectedSumPayloadIntoLocal =
          [&](const LoweredSumVariantSelection &selection,
              const LocalMap &selectedLocals,
              int32_t targetBaseLocal) -> bool {
        if (selection.variant == nullptr) {
          error = "native backend sum variant was not selected for " + sumDef.fullPath;
          return false;
        }
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(selection.variant->variantIndex))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 1)});
        if (!selection.variant->hasPayload) {
          return true;
        }
        if (selection.payloadExpr == nullptr) {
          error = "native backend sum payload was not selected for " +
                  sumDef.fullPath + "/" + selection.variant->name;
          return false;
        }
        if (selection.payloadIsAggregate) {
          if (!emitExpr(*selection.payloadExpr, selectedLocals)) {
            return false;
          }
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          const int32_t destPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
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
        if (!emitExpr(*selection.payloadExpr, selectedLocals)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
        return true;
      };
      auto emitMappedValueIntoSum =
          [&](const Expr &mappedExpr,
              const LocalMap &mappedLocals,
              const SumVariant &targetVariant,
              int32_t targetBaseLocal) -> bool {
        LoweredSumPayloadStorageInfo targetPayload;
        if (!resolveSumPayloadStorageInfo(sumDef, targetVariant, targetPayload)) {
          error = unsupportedSumPayloadError(sumDef, targetVariant);
          return false;
        }
        if (!targetPayload.isAggregate) {
          if (!emitExpr(mappedExpr, mappedLocals)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
          return true;
        }
        if (!emitExpr(mappedExpr, mappedLocals)) {
          return false;
        }
        const int32_t srcPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        const int32_t destPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
        if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, targetPayload.slotCount)) {
          return false;
        }
        if (mappedExpr.kind == Expr::Kind::Call) {
          ir_lowerer::emitDisarmTemporaryStructAfterCopy(
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              srcPtrLocal,
              targetPayload.structPath);
        }
        return true;
      };
      auto emitAndThenResultIntoSum =
          [&](const Expr &resultExpr,
              const LocalMap &resultLocals,
              int32_t targetBaseLocal) -> bool {
        LoweredSumVariantSelection selection;
        if (!selectSumVariantForInitializer(resultExpr, sumDef, resultLocals, selection) ||
            selection.variant == nullptr) {
          error = "IR backends require Result.and_then lambdas to produce stdlib Result sums";
          return false;
        }
        return emitSelectedSumPayloadIntoLocal(selection, resultLocals, targetBaseLocal);
      };
      struct StdlibResultSumSource {
        const Definition *sumDef = nullptr;
        int32_t sumPtrLocal = -1;
      };
      auto materializeStdlibResultSumSource =
          [&](const Expr &sourceExpr,
              const LocalMap &sourceLocals,
              const std::string &builtinName,
              const std::string &sourceDescription,
              StdlibResultSumSource &sourceOut) -> bool {
        sourceOut = {};
        const std::string sourceLabel =
            sourceDescription.empty() ? "source" : sourceDescription + " source";
        const Definition *sourceSumDef = nullptr;
        if (sourceExpr.kind == Expr::Kind::Name) {
          auto sourceIt = sourceLocals.find(sourceExpr.name);
          if (sourceIt == sourceLocals.end()) {
            error = "native backend " + builtinName + " " +
                    sourceLabel + " is unknown: " + sourceExpr.name;
            return false;
          }
          sourceSumDef = resolveSumDefinitionForLocalInfo(sourceIt->second);
          if (sourceSumDef == nullptr || !isStdlibResultSumDefinition(*sourceSumDef)) {
            error = "native backend " + builtinName + " " +
                    sourceLabel + " requires stdlib Result sum";
            return false;
          }
          sourceOut.sumDef = sourceSumDef;
          sourceOut.sumPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceIt->second.index)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sourceOut.sumPtrLocal)});
          return true;
        }
        if (sourceExpr.kind != Expr::Kind::Call) {
          error = "native backend " + builtinName + " " +
                  sourceLabel + " requires local or direct stdlib Result sum";
          return false;
        }
        const std::string sourceStructPath = inferStructExprPath(sourceExpr, sourceLocals);
        sourceSumDef = resolveSumDefinitionForTypeText(sourceStructPath, sourceExpr.namespacePrefix);
        if (sourceSumDef == nullptr || !isStdlibResultSumDefinition(*sourceSumDef)) {
          error = "native backend " + builtinName + " " +
                  sourceLabel + " requires local or direct stdlib Result sum";
          return false;
        }
        if (!emitExpr(sourceExpr, sourceLocals)) {
          return false;
        }
        sourceOut.sumDef = sourceSumDef;
        sourceOut.sumPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sourceOut.sumPtrLocal)});
        return true;
      };
      auto tryEmitLoweredResultMapIntoLocal = [&]() -> std::optional<bool> {
        if (!isStdlibResultSumDefinition(sumDef) || !isLegacyResultMapCall(initializer)) {
          return std::nullopt;
        }
        const Expr &sourceExpr = initializer.args[1];
        StdlibResultSumSource source;
        if (!materializeStdlibResultSumSource(sourceExpr, valueLocals, "Result.map", "", source)) {
          return false;
        }
        const SumVariant *sourceOkVariant = findSumVariantByName(*source.sumDef, "ok");
        const SumVariant *sourceErrorVariant = findSumVariantByName(*source.sumDef, "error");
        const SumVariant *targetOkVariant = findSumVariantByName(sumDef, "ok");
        const SumVariant *targetErrorVariant = findSumVariantByName(sumDef, "error");
        if (sourceOkVariant == nullptr || sourceErrorVariant == nullptr ||
            targetOkVariant == nullptr || targetErrorVariant == nullptr ||
            !sourceOkVariant->hasPayload || !sourceErrorVariant->hasPayload ||
            !targetOkVariant->hasPayload || !targetErrorVariant->hasPayload) {
          error = "native backend Result.map requires value-carrying stdlib Result sums";
          return false;
        }
        const Expr &lambdaExpr = initializer.args[2];
        if (!lambdaExpr.isLambda) {
          error = "Result.map requires a lambda argument";
          return false;
        }
        if (lambdaExpr.args.size() != 1 || lambdaExpr.args.front().kind != Expr::Kind::Name) {
          error = "Result.map requires a single-parameter lambda";
          return false;
        }
        const Expr *mappedValueExpr = findResultLambdaValueExpr(lambdaExpr);
        if (mappedValueExpr == nullptr) {
          error = "IR backends require Result.map lambda bodies";
          return false;
        }

        emitSumTagComparisonForConstruction(source.sumPtrLocal, sourceOkVariant->variantIndex);
        const size_t jumpErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(targetOkVariant->variantIndex))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        LocalMap lambdaLocals = valueLocals;
        if (!bindSumPayloadLocal(*source.sumDef,
                                 *sourceOkVariant,
                                 source.sumPtrLocal,
                                 lambdaExpr.args.front().name,
                                 lambdaLocals)) {
          return false;
        }
        if (!emitResultLambdaPrefixStatements(lambdaExpr, mappedValueExpr, lambdaLocals)) {
          return false;
        }
        if (!emitMappedValueIntoSum(*mappedValueExpr, lambdaLocals, *targetOkVariant, baseLocal)) {
          return false;
        }
        const size_t jumpEndIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t errorIndex = function.instructions.size();
        function.instructions[jumpErrorIndex].imm = static_cast<uint64_t>(errorIndex);
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(targetErrorVariant->variantIndex))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*source.sumDef,
                                *sourceErrorVariant,
                                *targetErrorVariant,
                                source.sumPtrLocal,
                                baseLocal,
                                "Result.map",
                                "error")) {
          return false;
        }

        const size_t endIndex = function.instructions.size();
        function.instructions[jumpEndIndex].imm = static_cast<uint64_t>(endIndex);
        return true;
      };
      if (const auto resultMapEmitResult = tryEmitLoweredResultMapIntoLocal();
          resultMapEmitResult.has_value()) {
        return *resultMapEmitResult;
      }
      auto tryEmitLoweredResultAndThenIntoLocal = [&]() -> std::optional<bool> {
        if (!isStdlibResultSumDefinition(sumDef) || !isLegacyResultAndThenCall(initializer)) {
          return std::nullopt;
        }
        const Expr &sourceExpr = initializer.args[1];
        StdlibResultSumSource source;
        if (!materializeStdlibResultSumSource(sourceExpr, valueLocals, "Result.and_then", "", source)) {
          return false;
        }
        const SumVariant *sourceOkVariant = findSumVariantByName(*source.sumDef, "ok");
        const SumVariant *sourceErrorVariant = findSumVariantByName(*source.sumDef, "error");
        const SumVariant *targetErrorVariant = findSumVariantByName(sumDef, "error");
        if (sourceOkVariant == nullptr || sourceErrorVariant == nullptr ||
            targetErrorVariant == nullptr || !sourceOkVariant->hasPayload ||
            !sourceErrorVariant->hasPayload || !targetErrorVariant->hasPayload) {
          error = "native backend Result.and_then requires value-carrying stdlib Result sums";
          return false;
        }
        const Expr &lambdaExpr = initializer.args[2];
        if (!lambdaExpr.isLambda) {
          error = "Result.and_then requires a lambda argument";
          return false;
        }
        if (lambdaExpr.args.size() != 1 || lambdaExpr.args.front().kind != Expr::Kind::Name) {
          error = "Result.and_then requires a single-parameter lambda";
          return false;
        }
        const Expr *chainedResultExpr = findResultLambdaValueExpr(lambdaExpr);
        if (chainedResultExpr == nullptr) {
          error = "IR backends require Result.and_then lambda bodies";
          return false;
        }

        emitSumTagComparisonForConstruction(source.sumPtrLocal, sourceOkVariant->variantIndex);
        const size_t jumpErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        LocalMap lambdaLocals = valueLocals;
        if (!bindSumPayloadLocal(*source.sumDef,
                                 *sourceOkVariant,
                                 source.sumPtrLocal,
                                 lambdaExpr.args.front().name,
                                 lambdaLocals)) {
          return false;
        }
        if (!emitResultLambdaPrefixStatements(lambdaExpr, chainedResultExpr, lambdaLocals)) {
          return false;
        }
        if (!emitAndThenResultIntoSum(*chainedResultExpr, lambdaLocals, baseLocal)) {
          return false;
        }
        const size_t jumpEndIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t errorIndex = function.instructions.size();
        function.instructions[jumpErrorIndex].imm = static_cast<uint64_t>(errorIndex);
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(targetErrorVariant->variantIndex))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*source.sumDef,
                                *sourceErrorVariant,
                                *targetErrorVariant,
                                source.sumPtrLocal,
                                baseLocal,
                                "Result.and_then",
                                "error")) {
          return false;
        }

        const size_t endIndex = function.instructions.size();
        function.instructions[jumpEndIndex].imm = static_cast<uint64_t>(endIndex);
        return true;
      };
      if (const auto resultAndThenEmitResult = tryEmitLoweredResultAndThenIntoLocal();
          resultAndThenEmitResult.has_value()) {
        return *resultAndThenEmitResult;
      }
      auto tryEmitLoweredResultMap2IntoLocal = [&]() -> std::optional<bool> {
        if (!isStdlibResultSumDefinition(sumDef) || !isLegacyResultMap2Call(initializer)) {
          return std::nullopt;
        }
        const Expr &leftExpr = initializer.args[1];
        const Expr &rightExpr = initializer.args[2];
        StdlibResultSumSource left;
        StdlibResultSumSource right;
        if (!materializeStdlibResultSumSource(leftExpr, valueLocals, "Result.map2", "left", left) ||
            !materializeStdlibResultSumSource(rightExpr, valueLocals, "Result.map2", "right", right)) {
          return false;
        }
        const SumVariant *leftOkVariant = findSumVariantByName(*left.sumDef, "ok");
        const SumVariant *leftErrorVariant = findSumVariantByName(*left.sumDef, "error");
        const SumVariant *rightOkVariant = findSumVariantByName(*right.sumDef, "ok");
        const SumVariant *rightErrorVariant = findSumVariantByName(*right.sumDef, "error");
        const SumVariant *targetOkVariant = findSumVariantByName(sumDef, "ok");
        const SumVariant *targetErrorVariant = findSumVariantByName(sumDef, "error");
        if (leftOkVariant == nullptr || leftErrorVariant == nullptr ||
            rightOkVariant == nullptr || rightErrorVariant == nullptr ||
            targetOkVariant == nullptr || targetErrorVariant == nullptr ||
            !leftOkVariant->hasPayload || !leftErrorVariant->hasPayload ||
            !rightOkVariant->hasPayload || !rightErrorVariant->hasPayload ||
            !targetOkVariant->hasPayload || !targetErrorVariant->hasPayload) {
          error = "native backend Result.map2 requires value-carrying stdlib Result sums";
          return false;
        }
        const Expr &lambdaExpr = initializer.args[3];
        if (!lambdaExpr.isLambda) {
          error = "Result.map2 requires a lambda argument";
          return false;
        }
        if (lambdaExpr.args.size() != 2 ||
            lambdaExpr.args[0].kind != Expr::Kind::Name ||
            lambdaExpr.args[1].kind != Expr::Kind::Name) {
          error = "Result.map2 requires a two-parameter lambda";
          return false;
        }
        const Expr *mappedValueExpr = findResultLambdaValueExpr(lambdaExpr);
        if (mappedValueExpr == nullptr) {
          error = "IR backends require Result.map2 lambda bodies";
          return false;
        }

        emitSumTagComparisonForConstruction(left.sumPtrLocal, leftOkVariant->variantIndex);
        const size_t jumpLeftErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        emitSumTagComparisonForConstruction(right.sumPtrLocal, rightOkVariant->variantIndex);
        const size_t jumpRightErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(targetOkVariant->variantIndex))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        LocalMap lambdaLocals = valueLocals;
        if (!bindSumPayloadLocal(*left.sumDef,
                                 *leftOkVariant,
                                 left.sumPtrLocal,
                                 lambdaExpr.args[0].name,
                                 lambdaLocals)) {
          return false;
        }
        if (!bindSumPayloadLocal(*right.sumDef,
                                 *rightOkVariant,
                                 right.sumPtrLocal,
                                 lambdaExpr.args[1].name,
                                 lambdaLocals)) {
          return false;
        }
        if (!emitResultLambdaPrefixStatements(lambdaExpr, mappedValueExpr, lambdaLocals)) {
          return false;
        }
        if (!emitMappedValueIntoSum(*mappedValueExpr, lambdaLocals, *targetOkVariant, baseLocal)) {
          return false;
        }
        const size_t jumpEndIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t leftErrorIndex = function.instructions.size();
        function.instructions[jumpLeftErrorIndex].imm = static_cast<uint64_t>(leftErrorIndex);
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(targetErrorVariant->variantIndex))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*left.sumDef,
                                *leftErrorVariant,
                                *targetErrorVariant,
                                left.sumPtrLocal,
                                baseLocal,
                                "Result.map2",
                                "left error")) {
          return false;
        }
        const size_t jumpAfterLeftErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t rightErrorIndex = function.instructions.size();
        function.instructions[jumpRightErrorIndex].imm = static_cast<uint64_t>(rightErrorIndex);
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(targetErrorVariant->variantIndex))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*right.sumDef,
                                *rightErrorVariant,
                                *targetErrorVariant,
                                right.sumPtrLocal,
                                baseLocal,
                                "Result.map2",
                                "right error")) {
          return false;
        }

        const size_t endIndex = function.instructions.size();
        function.instructions[jumpEndIndex].imm = static_cast<uint64_t>(endIndex);
        function.instructions[jumpAfterLeftErrorIndex].imm = static_cast<uint64_t>(endIndex);
        return true;
      };
      if (const auto resultMap2EmitResult = tryEmitLoweredResultMap2IntoLocal();
          resultMap2EmitResult.has_value()) {
        return *resultMap2EmitResult;
      }
      LoweredSumVariantSelection selection;
      if (!selectSumVariantForInitializer(initializer, sumDef, valueLocals, selection) ||
          selection.variant == nullptr) {
        error = "native backend could not select sum variant for " + sumDef.fullPath;
        return false;
      }
      return emitSelectedSumPayloadIntoLocal(selection, valueLocals, baseLocal);
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

    auto describePickTargetName = [&](const Expr &targetExpr) {
      if (targetExpr.kind == Expr::Kind::Name && !targetExpr.name.empty()) {
        return function.name + " -> " + targetExpr.name;
      }
      if (!targetExpr.name.empty()) {
        return function.name + " -> " + targetExpr.name;
      }
      return function.name + " -> <expr>";
    };

    auto resolveSemanticProductPickTargetSumDefinition =
        [&](const Expr &targetExpr,
            const LocalMap &valueLocals) -> const Definition * {
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      if (!semanticTargets.hasSemanticProduct || semanticTargets.semanticProgram == nullptr) {
        return nullptr;
      }

      auto requirePublishedSumMetadata =
          [&](const Definition &sumDef, const Expr &sourceExpr) -> const Definition * {
        if (findSemanticProductSumTypeMetadata(semanticTargets, sumDef.fullPath) == nullptr) {
          error = "missing semantic-product sum metadata for pick target: " +
                  describePickTargetName(sourceExpr);
          return nullptr;
        }
        return &sumDef;
      };

      if (targetExpr.kind == Expr::Kind::Name) {
        const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFactByScopeAndName(
                semanticTargets, function.name, targetExpr.name);
        if (bindingFact == nullptr || bindingFact->bindingTypeText.empty()) {
          if (valueLocals.find(targetExpr.name) != valueLocals.end()) {
            error = "missing semantic-product pick target binding fact: " +
                    describePickTargetName(targetExpr);
          }
          return nullptr;
        }
        const Definition *semanticSumDef =
            resolveSumDefinitionForTypeText(bindingFact->bindingTypeText, function.name);
        if (semanticSumDef == nullptr) {
          error = "semantic-product pick target binding type is not a sum: " +
                  describePickTargetName(targetExpr);
          return nullptr;
        }
        auto localIt = valueLocals.find(targetExpr.name);
        if (localIt != valueLocals.end()) {
          const Definition *localSumDef = resolveSumDefinitionForLocalInfo(localIt->second);
          if (localSumDef != nullptr && localSumDef->fullPath != semanticSumDef->fullPath) {
            error = "stale semantic-product pick target binding type: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
        }
        return requirePublishedSumMetadata(*semanticSumDef, targetExpr);
      }

      if (targetExpr.kind == Expr::Kind::Call) {
        if (!targetExpr.isMethodCall) {
          if (const Definition *constructorSum =
                  resolveSumDefinitionForTypeText(targetExpr.name, targetExpr.namespacePrefix);
              constructorSum != nullptr) {
            return requirePublishedSumMetadata(*constructorSum, targetExpr);
          }
        }
        if (targetExpr.semanticNodeId != 0) {
          const SemanticProgramQueryFact *queryFact =
              findSemanticProductQueryFact(semanticTargets, targetExpr);
          if (queryFact == nullptr) {
            error = "missing semantic-product pick target query fact: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
          std::string queryTypeText = trimTemplateTypeText(queryFact->bindingTypeText);
          if (queryTypeText.empty()) {
            queryTypeText = trimTemplateTypeText(queryFact->queryTypeText);
          }
          if (queryTypeText.empty()) {
            error = "incomplete semantic-product pick target query fact: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
          const Definition *querySumDef =
              resolveSumDefinitionForTypeText(queryTypeText, function.name);
          if (querySumDef == nullptr) {
            error = "semantic-product pick target query type is not a sum: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
          const std::string queryTargetPath =
              std::string(semanticProgramQueryFactResolvedPath(
                  *semanticTargets.semanticProgram, *queryFact));
          if (!queryTargetPath.empty()) {
            const SemanticProgramReturnFact *returnFact =
                findSemanticProductReturnFactByPath(semanticTargets, queryTargetPath);
            if (returnFact != nullptr && !returnFact->bindingTypeText.empty()) {
              const Definition *returnSumDef =
                  resolveSumDefinitionForTypeText(returnFact->bindingTypeText, function.name);
              if (returnSumDef != nullptr &&
                  returnSumDef->fullPath != querySumDef->fullPath) {
                error = "stale semantic-product pick target query type: " +
                        describePickTargetName(targetExpr);
                return nullptr;
              }
            }
          }
          return requirePublishedSumMetadata(*querySumDef, targetExpr);
        }
      }

      return nullptr;
    };

    auto resolvePickTargetSumDefinition =
        [&](const Expr &targetExpr, const LocalMap &valueLocals) -> const Definition * {
      if (const Definition *semanticSumDef =
              resolveSemanticProductPickTargetSumDefinition(targetExpr, valueLocals);
          semanticSumDef != nullptr || !error.empty()) {
        return semanticSumDef;
      }
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

    auto resolveSemanticProductSumVariantTag =
        [&](const Definition &sumDef,
            const SumVariant &variant,
            std::string_view operationLabel,
            int32_t &tagValueOut) -> bool {
      tagValueOut = static_cast<int32_t>(variant.variantIndex);
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      if (!semanticTargets.hasSemanticProduct || semanticTargets.semanticProgram == nullptr) {
        return true;
      }
      const SemanticProgramSumVariantMetadata *publishedVariant =
          findSemanticProductSumVariantMetadata(semanticTargets, sumDef.fullPath, variant.name);
      const std::string diagnosticSuffix = sumDef.fullPath + " -> " + variant.name;
      if (publishedVariant == nullptr) {
        error = "missing semantic-product sum variant metadata for " +
                std::string(operationLabel) + ": " + diagnosticSuffix;
        return false;
      }
      const std::string expectedPayloadType =
          variant.hasPayload ? sumPayloadTypeText(variant) : std::string{};
      const uint32_t expectedTagValue = static_cast<uint32_t>(variant.variantIndex);
      if (publishedVariant->variantIndex != variant.variantIndex ||
          publishedVariant->tagValue != expectedTagValue ||
          publishedVariant->hasPayload != variant.hasPayload ||
          trimTemplateTypeText(publishedVariant->payloadTypeText) != expectedPayloadType) {
        error = "stale semantic-product sum variant metadata for " +
                std::string(operationLabel) + ": " + diagnosticSuffix;
        return false;
      }
      tagValueOut = static_cast<int32_t>(publishedVariant->tagValue);
      return true;
    };

    auto findSumPayloadMoveHelper = [&](const std::string &structPath) -> const Definition * {
      auto moveIt = defMap.find(structPath + "/Move");
      if (moveIt != defMap.end()) {
        return moveIt->second;
      }
      moveIt = defMap.find(structPath + "/Copy");
      if (moveIt != defMap.end()) {
        return moveIt->second;
      }
      return nullptr;
    };

    auto findSumPayloadDestroyHelper = [&](const std::string &structPath) -> const Definition * {
      auto destroyIt = defMap.find(structPath + "/DestroyStack");
      if (destroyIt != defMap.end()) {
        return destroyIt->second;
      }
      destroyIt = defMap.find(structPath + "/Destroy");
      if (destroyIt != defMap.end()) {
        return destroyIt->second;
      }
      return nullptr;
    };

    auto emitActiveSumPayloadDestroyFromSumPtr =
        [&](const Definition &sumDef, int32_t sourceSumPtrLocal, const LocalMap &valueLocals) -> bool {
      std::vector<size_t> endJumps;
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSumPayloadStorageInfo(sumDef, variant, payloadInfo)) {
          error = unsupportedSumPayloadError(sumDef, variant);
          return false;
        }
        if (!payloadInfo.isAggregate) {
          continue;
        }
        const Definition *destroyHelper = findSumPayloadDestroyHelper(payloadInfo.structPath);
        if (destroyHelper == nullptr) {
          continue;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(
                sumDef, variant, "sum payload destroy", tagValue)) {
          return false;
        }
        emitSumTagComparison(sourceSumPtrLocal, tagValue);
        const size_t nextVariantJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        const int32_t payloadPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
        function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
        function.instructions.push_back({IrOpcode::AddI64, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadPtrLocal)});
        if (!ir_lowerer::emitDestroyHelperFromPtr(payloadPtrLocal,
                                                  payloadInfo.structPath,
                                                  destroyHelper,
                                                  valueLocals,
                                                  [&](const Expr &callExpr,
                                                      const Definition &callee,
                                                      const LocalMap &callLocals,
                                                      bool requireValue) {
                                                    return emitInlineDefinitionCall(
                                                        callExpr, callee, callLocals, requireValue);
                                                  },
                                                  error)) {
          return false;
        }
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextVariantJump].imm = static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm = static_cast<uint64_t>(function.instructions.size());
      }
      return true;
    };

    auto emitActiveSumPayloadMoveFromSumPtr =
        [&](int32_t destBaseLocal,
            const Definition &sumDef,
            int32_t sourceSumPtrLocal,
            const LocalMap &valueLocals) -> bool {
      std::vector<size_t> endJumps;
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSumPayloadStorageInfo(sumDef, variant, payloadInfo)) {
          error = unsupportedSumPayloadError(sumDef, variant);
          return false;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(
                sumDef, variant, "sum payload move", tagValue)) {
          return false;
        }
        emitSumTagComparison(sourceSumPtrLocal, tagValue);
        const size_t nextVariantJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        if (!variant.hasPayload) {
          endJumps.push_back(function.instructions.size());
          function.instructions.push_back({IrOpcode::Jump, 0});
          function.instructions[nextVariantJump].imm =
              static_cast<uint64_t>(function.instructions.size());
          continue;
        }
        if (payloadInfo.isAggregate) {
          const int32_t destPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(destBaseLocal + 2)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (const Definition *moveHelper = findSumPayloadMoveHelper(payloadInfo.structPath)) {
            if (!ir_lowerer::emitMoveHelperFromPtrs(destPtrLocal,
                                                    srcPtrLocal,
                                                    payloadInfo.structPath,
                                                    moveHelper,
                                                    valueLocals,
                                                    [&](const Expr &callExpr,
                                                        const Definition &callee,
                                                        const LocalMap &callLocals,
                                                        bool requireValue) {
                                                      return emitInlineDefinitionCall(
                                                          callExpr, callee, callLocals, requireValue);
                                                    },
                                                    error)) {
              return false;
            }
          } else if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, payloadInfo.slotCount)) {
            return false;
          }
        } else {
          emitLoadSumSlotIndirect(sourceSumPtrLocal, 2);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destBaseLocal + 2)});
        }
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextVariantJump].imm = static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm = static_cast<uint64_t>(function.instructions.size());
      }
      return true;
    };

    auto tryEmitLoweredSumMoveIntoLocal =
        [&](int32_t baseLocal,
            const Definition &sumDef,
            const Expr &initializer,
            const LocalMap &valueLocals,
            bool &emittedOut) -> bool {
      emittedOut = false;
      if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
          initializer.isFieldAccess || !isSimpleCallName(initializer, "move") ||
          initializer.args.size() != 1 || initializer.args.front().kind != Expr::Kind::Name ||
          hasNamedArguments(initializer.argNames) || !initializer.templateArgs.empty() ||
          initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
        return true;
      }
      auto sourceIt = valueLocals.find(initializer.args.front().name);
      if (sourceIt == valueLocals.end()) {
        error = "native backend sum move requires a local source: " + initializer.args.front().name;
        return false;
      }
      const Definition *sourceSumDef = resolveSumDefinitionForLocalInfo(sourceIt->second);
      if (sourceSumDef == nullptr || sourceSumDef->fullPath != sumDef.fullPath) {
        error = "native backend sum move source type mismatch on " + sumDef.fullPath;
        return false;
      }
      const int32_t sourceSumPtrLocal = allocTempLocal();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceIt->second.index)});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
      emitLoadSumSlotIndirect(sourceSumPtrLocal, 1);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
      if (!emitActiveSumPayloadMoveFromSumPtr(baseLocal, sumDef, sourceSumPtrLocal, valueLocals)) {
        return false;
      }
      emittedOut = true;
      return true;
    };

    auto makePickPayloadLocalInfo =
        [&](const Definition &sumDef,
            const SumVariant &variant,
            const SemanticProgramSumVariantMetadata *publishedVariant,
            LocalInfo &payloadInfoOut) -> bool {
      LoweredSumPayloadStorageInfo payloadStorage;
      const bool resolvedPayloadStorage =
          publishedVariant != nullptr
              ? resolvePublishedSumPayloadStorageInfo(sumDef,
                                                      *publishedVariant,
                                                      payloadStorage)
              : resolveSumPayloadStorageInfo(sumDef, variant, payloadStorage);
      if (!resolvedPayloadStorage) {
        error = unsupportedSumPayloadError(sumDef, variant);
        return false;
      }
      payloadInfoOut = {};
      payloadInfoOut.kind = LocalInfo::Kind::Value;
      payloadInfoOut.valueKind =
          payloadStorage.isAggregate ? LocalInfo::ValueKind::Int64 : payloadStorage.valueKind;
      payloadInfoOut.structTypeName = payloadStorage.structPath;
      payloadInfoOut.structSlotCount = payloadStorage.slotCount;
      return true;
    };

    auto bindPickPayload =
        [&](const Definition &sumDef,
            const SumVariant &variant,
            const SemanticProgramSumVariantMetadata *publishedVariant,
            const Expr &binderExpr,
            int32_t sumPtrLocal,
            LocalMap &branchLocals) -> bool {
      LocalInfo payloadInfo;
      if (!makePickPayloadLocalInfo(sumDef, variant, publishedVariant, payloadInfo)) {
        return false;
      }
      payloadInfo.index = nextLocal++;
      if (!payloadInfo.structTypeName.empty()) {
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

    auto isPickCall = [](const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
             !candidate.isMethodCall && !candidate.isFieldAccess &&
             isSimpleCallName(candidate, "pick") &&
             candidate.templateArgs.empty() && !hasNamedArguments(candidate.argNames);
    };

    auto isPickArmEnvelopeBase = [](const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
             !candidate.isMethodCall && !candidate.isFieldAccess &&
             candidate.hasBodyArguments && !candidate.name.empty() &&
             candidate.templateArgs.empty() && !hasNamedArguments(candidate.argNames);
    };

    auto isPayloadPickArmEnvelope = [&](const Expr &candidate) {
      return isPickArmEnvelopeBase(candidate) && candidate.args.size() == 1 &&
             candidate.args.front().kind == Expr::Kind::Name;
    };

    auto isUnitCallPickArmEnvelope = [&](const Expr &candidate) {
      return isPickArmEnvelopeBase(candidate) && candidate.args.empty();
    };

    auto isUnitBindingPickArmEnvelope = [](const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call && candidate.isBinding &&
             !candidate.isMethodCall && !candidate.isFieldAccess &&
             !candidate.name.empty() && candidate.transforms.empty() &&
             candidate.templateArgs.empty() && !candidate.hasBodyArguments &&
             candidate.bodyArguments.empty() && candidate.args.size() == 1 &&
             candidate.argNames.size() == 1 && !candidate.argNames.front().has_value();
    };

    auto isSupportedPickArmEnvelope = [&](const Expr &candidate) {
      return isPayloadPickArmEnvelope(candidate) ||
             isUnitCallPickArmEnvelope(candidate) ||
             isUnitBindingPickArmEnvelope(candidate);
    };

    auto pickArmBodyExprs = [&](const Expr &arm) {
      std::vector<const Expr *> out;
      if (isPayloadPickArmEnvelope(arm) || isUnitCallPickArmEnvelope(arm)) {
        out.reserve(arm.bodyArguments.size());
        for (const Expr &bodyExpr : arm.bodyArguments) {
          out.push_back(&bodyExpr);
        }
        return out;
      }
      if (!isUnitBindingPickArmEnvelope(arm)) {
        return out;
      }
      const Expr &initializer = arm.args.front();
      if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
          initializer.hasBodyArguments && initializer.args.empty()) {
        out.reserve(initializer.bodyArguments.size());
        for (const Expr &bodyExpr : initializer.bodyArguments) {
          out.push_back(&bodyExpr);
        }
        return out;
      }
      out.push_back(&initializer);
      return out;
    };

    auto findPickArmValueExpr = [&](const Expr &arm) -> const Expr * {
      const Expr *valueExpr = nullptr;
      bool sawReturn = false;
      for (const Expr *bodyExprPtr : pickArmBodyExprs(arm)) {
        const Expr &bodyExpr = *bodyExprPtr;
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

    auto validateSemanticProductPickArmVariant =
        [&](const Definition &sumDef,
            const SumVariant &variant) -> const SemanticProgramSumVariantMetadata * {
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      if (!semanticTargets.hasSemanticProduct || semanticTargets.semanticProgram == nullptr) {
        return nullptr;
      }
      const SemanticProgramSumVariantMetadata *publishedVariant =
          findSemanticProductSumVariantMetadata(semanticTargets, sumDef.fullPath, variant.name);
      const std::string diagnosticSuffix = sumDef.fullPath + " -> " + variant.name;
      if (publishedVariant == nullptr) {
        error = "missing semantic-product sum variant metadata for pick arm: " +
                diagnosticSuffix;
        return nullptr;
      }
      const std::string expectedPayloadType =
          variant.hasPayload ? sumPayloadTypeText(variant) : std::string{};
      const uint32_t expectedTagValue = static_cast<uint32_t>(variant.variantIndex);
      if (publishedVariant->variantIndex != variant.variantIndex ||
          publishedVariant->tagValue != expectedTagValue ||
          publishedVariant->hasPayload != variant.hasPayload ||
          trimTemplateTypeText(publishedVariant->payloadTypeText) != expectedPayloadType) {
        error = "stale semantic-product sum variant metadata for pick arm: " +
                diagnosticSuffix;
        return nullptr;
      }
      return publishedVariant;
    };

    auto resolvePickArmVariant =
        [&](const Definition &sumDef,
            const Expr &arm,
            const SemanticProgramSumVariantMetadata **publishedVariantOut) -> const SumVariant * {
      if (publishedVariantOut != nullptr) {
        *publishedVariantOut = nullptr;
      }
      const SumVariant *variant = findSumVariantByName(sumDef, arm.name);
      if (variant == nullptr) {
        error = "native backend unknown pick variant on " + sumDef.fullPath + ": " + arm.name;
        return nullptr;
      }
      const SemanticProgramSumVariantMetadata *publishedVariant =
          validateSemanticProductPickArmVariant(sumDef, *variant);
      if (!error.empty()) {
        return nullptr;
      }
      if (publishedVariantOut != nullptr) {
        *publishedVariantOut = publishedVariant;
      }
      return variant;
    };

    auto emitPickArmPrefixStatements =
        [&](const Expr &arm, const Expr *valueExpr, LocalMap &branchLocals) -> bool {
      for (const Expr *bodyExprPtr : pickArmBodyExprs(arm)) {
        const Expr &bodyExpr = *bodyExprPtr;
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

    auto inferPickAggregateResult =
        [&](const Expr &expr,
            const Definition &sumDef,
            const LocalMap &valueLocals,
            std::string &structPathOut,
            StructSlotLayout &layoutOut) -> bool {
      structPathOut.clear();
      bool sawAggregateResult = false;
      for (const Expr &arm : expr.bodyArguments) {
        const bool payloadArm = isPayloadPickArmEnvelope(arm);
        if (!isSupportedPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant blocks";
          return false;
        }
        const SemanticProgramSumVariantMetadata *publishedVariant = nullptr;
        const SumVariant *variant = resolvePickArmVariant(sumDef, arm, &publishedVariant);
        if (variant == nullptr) {
          return false;
        }
        if (variant->hasPayload != payloadArm) {
          error = variant->hasPayload
                      ? "native backend requires payload pick arm for " +
                            sumDef.fullPath + "/" + variant->name
                      : "native backend unit pick arm cannot bind payload: " +
                            sumDef.fullPath + "/" + variant->name;
          return false;
        }
        LocalMap branchLocals = valueLocals;
        if (payloadArm) {
          LocalInfo payloadInfo;
          if (!makePickPayloadLocalInfo(sumDef, *variant, publishedVariant, payloadInfo)) {
            return false;
          }
          branchLocals.emplace(arm.args.front().name, payloadInfo);
        }
        const Expr *valueExpr = findPickArmValueExpr(arm);
        if (valueExpr == nullptr) {
          error = "native backend requires pick arms to produce a value";
          return false;
        }
        const std::string branchStructPath = inferStructExprPath(*valueExpr, branchLocals);
        if (branchStructPath.empty()) {
          structPathOut.clear();
          return true;
        }
        if (!sawAggregateResult) {
          structPathOut = branchStructPath;
          sawAggregateResult = true;
          continue;
        }
        if (branchStructPath != structPathOut) {
          error = "native backend requires pick aggregate arms to produce the same struct type";
          return false;
        }
      }
      if (!sawAggregateResult) {
        structPathOut.clear();
        return true;
      }
      if (!resolveStructSlotLayout(structPathOut, layoutOut)) {
        error = "native backend could not resolve pick aggregate result layout: " + structPathOut;
        return false;
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
        if (error.empty()) {
          error = "native backend pick target requires sum value";
        }
        return LoweredSumPickEmitResult::Error;
      }
      const int32_t sumPtrLocal = allocTempLocal();
      if (!emitExpr(expr.args.front(), valueLocals)) {
        return LoweredSumPickEmitResult::Error;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sumPtrLocal)});
      std::string aggregateResultStructPath;
      StructSlotLayout aggregateResultLayout;
      if (!inferPickAggregateResult(expr,
                                    *sumDef,
                                    valueLocals,
                                    aggregateResultStructPath,
                                    aggregateResultLayout)) {
        return LoweredSumPickEmitResult::Error;
      }
      const bool emitsAggregateResult = !aggregateResultStructPath.empty();
      int32_t resultBaseLocal = -1;
      int32_t resultLocal = allocTempLocal();
      if (emitsAggregateResult) {
        resultBaseLocal = nextLocal;
        nextLocal += aggregateResultLayout.totalSlots;
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(aggregateResultLayout.totalSlots - 1))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultBaseLocal)});
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(resultBaseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
      }
      std::vector<size_t> endJumps;
      for (const Expr &arm : expr.bodyArguments) {
        const bool payloadArm = isPayloadPickArmEnvelope(arm);
        if (!isSupportedPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant blocks";
          return LoweredSumPickEmitResult::Error;
        }
        const SemanticProgramSumVariantMetadata *publishedVariant = nullptr;
        const SumVariant *variant = resolvePickArmVariant(*sumDef, arm, &publishedVariant);
        if (variant == nullptr) {
          return LoweredSumPickEmitResult::Error;
        }
        if (variant->hasPayload != payloadArm) {
          error = variant->hasPayload
                      ? "native backend requires payload pick arm for " +
                            sumDef->fullPath + "/" + variant->name
                      : "native backend unit pick arm cannot bind payload: " +
                            sumDef->fullPath + "/" + variant->name;
          return LoweredSumPickEmitResult::Error;
        }
        const int32_t tagValue = publishedVariant != nullptr
                                     ? static_cast<int32_t>(publishedVariant->tagValue)
                                     : static_cast<int32_t>(variant->variantIndex);
        emitSumTagComparison(sumPtrLocal, tagValue);
        const size_t nextArmJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        LocalMap branchLocals = valueLocals;
        if (payloadArm &&
            !bindPickPayload(
                *sumDef, *variant, publishedVariant, arm.args.front(), sumPtrLocal, branchLocals)) {
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
        if (emitsAggregateResult) {
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (!emitStructCopyFromPtrs(resultLocal, srcPtrLocal, aggregateResultLayout.totalSlots)) {
            return LoweredSumPickEmitResult::Error;
          }
          if (valueExpr->kind == Expr::Kind::Call) {
            ir_lowerer::emitDisarmTemporaryStructAfterCopy(
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                srcPtrLocal,
                aggregateResultStructPath);
          }
        } else {
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
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
        if (error.empty()) {
          error = "native backend pick target requires sum value";
        }
        return LoweredSumPickEmitResult::Error;
      }
      const int32_t sumPtrLocal = allocTempLocal();
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return LoweredSumPickEmitResult::Error;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sumPtrLocal)});
      std::vector<size_t> endJumps;
      for (const Expr &arm : stmt.bodyArguments) {
        const bool payloadArm = isPayloadPickArmEnvelope(arm);
        if (!isSupportedPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant blocks";
          return LoweredSumPickEmitResult::Error;
        }
        const SemanticProgramSumVariantMetadata *publishedVariant = nullptr;
        const SumVariant *variant = resolvePickArmVariant(*sumDef, arm, &publishedVariant);
        if (variant == nullptr) {
          return LoweredSumPickEmitResult::Error;
        }
        if (variant->hasPayload != payloadArm) {
          error = variant->hasPayload
                      ? "native backend requires payload pick arm for " +
                            sumDef->fullPath + "/" + variant->name
                      : "native backend unit pick arm cannot bind payload: " +
                            sumDef->fullPath + "/" + variant->name;
          return LoweredSumPickEmitResult::Error;
        }
        const int32_t tagValue = publishedVariant != nullptr
                                     ? static_cast<int32_t>(publishedVariant->tagValue)
                                     : static_cast<int32_t>(variant->variantIndex);
        emitSumTagComparison(sumPtrLocal, tagValue);
        const size_t nextArmJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        LocalMap branchLocals = localsIn;
        if (payloadArm &&
            !bindPickPayload(
                *sumDef, *variant, publishedVariant, arm.args.front(), sumPtrLocal, branchLocals)) {
          return LoweredSumPickEmitResult::Error;
        }
        for (const Expr *bodyExprPtr : pickArmBodyExprs(arm)) {
          if (!emitStatement(*bodyExprPtr, branchLocals)) {
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
