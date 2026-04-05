        if (!expr.isMethodCall &&
            (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
            expr.args.size() == 1) {
          const bool isBorrowCall = isSimpleCallName(expr, "borrow");
          const Expr &storage = expr.args.front();
          UninitializedStorageAccess access;
          bool resolved = false;
          if (!resolveUninitializedStorage(storage, localsIn, access, resolved)) {
            return false;
          }
          if (resolved) {
            auto emitFieldPointer = [&](const Expr &receiver,
                                        const LocalInfo *receiverLocal,
                                        const StructSlotFieldInfo &field,
                                        int32_t ptrLocal) -> bool {
              if (receiverLocal != nullptr) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(receiverLocal->index)});
              } else {
                if (!emitExpr(receiver, localsIn)) {
                  return false;
                }
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              const uint64_t offsetBytes = static_cast<uint64_t>(field.slotOffset) * IrSlotBytes;
              if (offsetBytes != 0) {
                function.instructions.push_back({IrOpcode::PushI64, offsetBytes});
                function.instructions.push_back({IrOpcode::AddI64, 0});
              }
              return true;
            };
            auto emitIndirectPointer = [&](const Expr &pointerExpr, int32_t ptrLocal) -> bool {
              if (pointerExpr.kind == Expr::Kind::Name) {
                auto it = localsIn.find(pointerExpr.name);
                if (it != localsIn.end() &&
                    (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
                  function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
                  function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
                  return true;
                }
              }
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              return true;
            };
            if (access.location == UninitializedStorageAccess::Location::Local) {
              const LocalInfo &storageInfo = *access.local;
              if (!storageInfo.structTypeName.empty()) {
                if (isBorrowCall) {
                  function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
                  return true;
                }
                StructSlotLayout layout;
                if (!resolveStructSlotLayout(storageInfo.structTypeName, layout)) {
                  return false;
                }
                const int32_t baseLocal = nextLocal;
                nextLocal += layout.totalSlots;
                function.instructions.push_back(
                    {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                const int32_t destPtrLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
                const int32_t srcPtrLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
                if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, layout.totalSlots)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
                return true;
              }
              if (isBorrowCall) {
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(storageInfo.index)});
              } else {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
              }
              return true;
            }
            if (access.location == UninitializedStorageAccess::Location::Field) {
              const Expr &receiver = storage.args.front();
              const StructSlotFieldInfo &field = access.fieldSlot;
              const int32_t ptrLocal = allocTempLocal();
              if (!emitFieldPointer(receiver, access.receiver, field, ptrLocal)) {
                return false;
              }
              if (isBorrowCall) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
                return true;
              }
              if (!field.structPath.empty()) {
                const int32_t baseLocal = nextLocal;
                nextLocal += field.slotCount;
                function.instructions.push_back(
                    {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(field.slotCount - 1))});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                const int32_t destPtrLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
                if (!emitStructCopyFromPtrs(destPtrLocal, ptrLocal, field.slotCount)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
                return true;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              return true;
            }
            if (access.location == UninitializedStorageAccess::Location::Indirect) {
              const int32_t ptrLocal = allocTempLocal();
              if (access.pointerExpr == nullptr || !emitIndirectPointer(*access.pointerExpr, ptrLocal)) {
                return false;
              }
              if (isBorrowCall) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
                return true;
              }
              if (!access.typeInfo.structPath.empty()) {
                StructSlotLayout layout;
                if (!resolveStructSlotLayout(access.typeInfo.structPath, layout)) {
                  return false;
                }
                const int32_t baseLocal = nextLocal;
                nextLocal += layout.totalSlots;
                function.instructions.push_back(
                    {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                const int32_t destPtrLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
                if (!emitStructCopyFromPtrs(destPtrLocal, ptrLocal, layout.totalSlots)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
                return true;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              return true;
            }
          }
        }
        if ((expr.hasBodyArguments || !expr.bodyArguments.empty()) && !isBlockCall(expr)) {
          Expr callExpr = expr;
          callExpr.bodyArguments.clear();
          callExpr.hasBodyArguments = false;
          if (!emitExpr(callExpr, localsIn)) {
            return false;
          }
          const int32_t resultLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
          OnErrorScope onErrorScope(currentOnError, std::nullopt);
          pushFileScope();
          LocalMap blockLocals = localsIn;
          for (const auto &bodyExpr : expr.bodyArguments) {
            if (!emitStatement(bodyExpr, blockLocals)) {
              return false;
            }
          }
          emitFileScopeCleanup(fileScopeStack.back());
          popFileScope();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          return true;
        }
