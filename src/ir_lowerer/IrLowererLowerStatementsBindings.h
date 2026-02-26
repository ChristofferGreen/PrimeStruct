  emitStatement = [&](const Expr &stmt, LocalMap &localsIn) -> bool {
    if (!stmt.isBinding && stmt.kind == Expr::Kind::StringLiteral) {
      error = "native backend does not support string literal statements";
      return false;
    }
    if (stmt.isBinding) {
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (localsIn.count(stmt.name) > 0) {
        error = "binding redefines existing name: " + stmt.name;
        return false;
      }
      const Expr &init = stmt.args.front();
      std::string uninitializedType;
      if (extractUninitializedTemplateArg(stmt, uninitializedType)) {
        UninitializedTypeInfo uninitInfo;
        if (!resolveUninitializedTypeInfo(uninitializedType, stmt.namespacePrefix, uninitInfo)) {
          if (error.empty()) {
            error = "native backend does not support uninitialized storage for type: " + uninitializedType;
          }
          return false;
        }
        LocalInfo info;
        info.isMutable = isBindingMutable(stmt);
        info.isUninitializedStorage = true;
        info.kind = uninitInfo.kind;
        info.valueKind = uninitInfo.valueKind;
        info.mapKeyKind = uninitInfo.mapKeyKind;
        info.mapValueKind = uninitInfo.mapValueKind;
        info.structTypeName = uninitInfo.structPath;
        if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
          StructSlotLayout layout;
          if (!resolveStructSlotLayout(info.structTypeName, layout)) {
            return false;
          }
          const int32_t baseLocal = nextLocal;
          nextLocal += layout.totalSlots;
          info.index = nextLocal++;
          function.instructions.push_back(
              {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
          localsIn.emplace(stmt.name, info);
          return true;
        }
        info.index = nextLocal++;
        IrOpcode zeroOp = IrOpcode::PushI32;
        uint64_t zeroImm = 0;
        if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector ||
            info.kind == LocalInfo::Kind::Map || info.kind == LocalInfo::Kind::Buffer) {
          zeroOp = IrOpcode::PushI64;
        } else {
          switch (info.valueKind) {
            case LocalInfo::ValueKind::Int64:
            case LocalInfo::ValueKind::UInt64:
              zeroOp = IrOpcode::PushI64;
              break;
            case LocalInfo::ValueKind::Float32:
              zeroOp = IrOpcode::PushF32;
              break;
            case LocalInfo::ValueKind::Float64:
              zeroOp = IrOpcode::PushF64;
              break;
            case LocalInfo::ValueKind::Int32:
            case LocalInfo::ValueKind::Bool:
              zeroOp = IrOpcode::PushI32;
              break;
            case LocalInfo::ValueKind::String:
              zeroOp = IrOpcode::PushI64;
              break;
            default:
              error = "native backend requires a concrete uninitialized storage type on " + stmt.name;
              return false;
          }
        }
        function.instructions.push_back({zeroOp, zeroImm});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        localsIn.emplace(stmt.name, info);
        return true;
      }
      LocalInfo::Kind kind = bindingKind(stmt);
      if (!hasExplicitBindingTypeTransform(stmt) && kind == LocalInfo::Kind::Value) {
        if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it != localsIn.end()) {
            kind = it->second.kind;
          }
        } else if (init.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(init, collection)) {
            if (collection == "array") {
              kind = LocalInfo::Kind::Array;
            } else if (collection == "vector") {
              kind = LocalInfo::Kind::Vector;
            } else if (collection == "map") {
              kind = LocalInfo::Kind::Map;
            }
          }
        }
      }
      LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
      LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
      LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
      if (kind == LocalInfo::Kind::Map) {
        if (hasExplicitBindingTypeTransform(stmt)) {
          for (const auto &transform : stmt.transforms) {
            if (transform.name == "map" && transform.templateArgs.size() == 2) {
              mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
              mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
              break;
            }
          }
        } else if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
            mapKeyKind = it->second.mapKeyKind;
            mapValueKind = it->second.mapValueKind;
          }
        } else if (init.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(init, collection) && collection == "map" && init.templateArgs.size() == 2) {
            mapKeyKind = valueKindFromTypeName(init.templateArgs[0]);
            mapValueKind = valueKindFromTypeName(init.templateArgs[1]);
          }
        }
        valueKind = mapValueKind;
      } else if (hasExplicitBindingTypeTransform(stmt)) {
        valueKind = bindingValueKind(stmt, kind);
      } else if (kind == LocalInfo::Kind::Value) {
        valueKind = inferExprKind(init, localsIn);
        if (valueKind == LocalInfo::ValueKind::Unknown) {
          valueKind = LocalInfo::ValueKind::Int32;
        }
      } else if (kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector) {
        if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it != localsIn.end() &&
              (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
            valueKind = it->second.valueKind;
          }
        } else if (init.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(init, collection) && (collection == "array" || collection == "vector") &&
              init.templateArgs.size() == 1) {
            valueKind = valueKindFromTypeName(init.templateArgs.front());
          }
        }
      }
      LocalInfo info;
      info.isMutable = isBindingMutable(stmt);
      info.kind = kind;
      info.valueKind = valueKind;
      info.mapKeyKind = mapKeyKind;
      info.mapValueKind = mapValueKind;
      setReferenceArrayInfo(stmt, info);
      applyStructArrayInfo(stmt, info);
      applyStructValueInfo(stmt, info);
      if (info.kind == LocalInfo::Kind::Value && info.valueKind == LocalInfo::ValueKind::Unknown &&
          info.structTypeName.empty()) {
        std::string inferredStruct = inferStructExprPath(init, localsIn);
        if (!inferredStruct.empty()) {
          info.structTypeName = inferredStruct;
        } else {
          info.valueKind = LocalInfo::ValueKind::Int32;
        }
      }
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "File") {
          info.isFileHandle = true;
          info.valueKind = LocalInfo::ValueKind::Int64;
        } else if (transform.name == "Result") {
          info.isResult = true;
          info.resultHasValue = (transform.templateArgs.size() == 2);
          info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          if (!transform.templateArgs.empty()) {
            info.resultErrorType = transform.templateArgs.back();
          }
        }
      }
      info.isFileError = isFileErrorBinding(stmt);
      valueKind = info.valueKind;

      if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
        std::string initStruct = inferStructExprPath(init, localsIn);
        if (initStruct.empty() || initStruct != info.structTypeName) {
          error = "struct binding initializer type mismatch on " + stmt.name;
          return false;
        }
        StructSlotLayout layout;
        if (!resolveStructSlotLayout(info.structTypeName, layout)) {
          return false;
        }
        const int32_t baseLocal = nextLocal;
        nextLocal += layout.totalSlots;
        info.index = nextLocal++;
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        const int32_t srcPtrLocal = allocTempLocal();
        if (!emitExpr(init, localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
          return false;
        }
        localsIn.emplace(stmt.name, info);
        return true;
      }

      if (valueKind == LocalInfo::ValueKind::String) {
        if (kind != LocalInfo::Kind::Value) {
          error = "native backend does not support string pointers or references";
          return false;
        }
        int32_t index = -1;
        LocalInfo::StringSource source = LocalInfo::StringSource::None;
        bool argvChecked = true;
        bool emittedValue = false;
        if (init.kind == Expr::Kind::StringLiteral) {
          std::string decoded;
          if (!parseStringLiteral(init.stringValue, decoded)) {
            return false;
          }
          index = internString(decoded);
          source = LocalInfo::StringSource::TableIndex;
          emittedValue = true;
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
        } else if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it == localsIn.end()) {
            error = "native backend does not know identifier: " + init.name;
            return false;
          }
          if (it->second.valueKind != LocalInfo::ValueKind::String ||
              it->second.stringSource == LocalInfo::StringSource::None) {
            error = "native backend requires string bindings to use string literals, bindings, or entry args";
            return false;
          }
          source = it->second.stringSource;
          index = it->second.stringIndex;
          if (source == LocalInfo::StringSource::ArgvIndex) {
            argvChecked = it->second.argvChecked;
          }
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          emittedValue = true;
        } else if (init.kind == Expr::Kind::Call) {
          std::string accessName;
          if (getBuiltinArrayAccessName(init, accessName)) {
            if (init.args.size() != 2) {
              error = accessName + " requires exactly two arguments";
              return false;
            }
            if (!isEntryArgsName(init.args.front(), localsIn)) {
              error = "native backend only supports entry argument indexing";
              return false;
            }
            LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(init.args[1], localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              error = "native backend requires integer indices for " + accessName;
              return false;
            }

            const int32_t argvIndexLocal = allocTempLocal();
            if (!emitExpr(init.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal)});

            if (accessName == "at") {
              if (indexKind != LocalInfo::ValueKind::UInt64) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
                function.instructions.push_back({pushZeroForIndex(indexKind), 0});
                function.instructions.push_back({cmpLtForIndex(indexKind), 0});
                size_t jumpNonNegative = function.instructions.size();
                function.instructions.push_back({IrOpcode::JumpIfZero, 0});
                emitArrayIndexOutOfBounds();
                size_t nonNegativeIndex = function.instructions.size();
                function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
              }

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
              function.instructions.push_back({IrOpcode::PushArgc, 0});
              function.instructions.push_back({cmpGeForIndex(indexKind), 0});
              size_t jumpInRange = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitArrayIndexOutOfBounds();
              size_t inRangeIndex = function.instructions.size();
              function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
            source = LocalInfo::StringSource::ArgvIndex;
            argvChecked = (accessName == "at");
            emittedValue = true;
          } else {
            LocalInfo::ValueKind initKind = inferExprKind(init, localsIn);
            if (initKind != LocalInfo::ValueKind::String) {
              error = "native backend requires string bindings to use string literals, bindings, or entry args";
              return false;
            }
            if (!emitExpr(init, localsIn)) {
              return false;
            }
            source = LocalInfo::StringSource::RuntimeIndex;
            emittedValue = true;
          }
        } else {
          error = "native backend requires string bindings to use string literals, bindings, or entry args";
          return false;
        }
        LocalInfo info;
        info.index = nextLocal++;
        info.isMutable = isBindingMutable(stmt);
        info.kind = LocalInfo::Kind::Value;
        info.valueKind = LocalInfo::ValueKind::String;
        info.stringSource = source;
        info.stringIndex = index;
        info.argvChecked = argvChecked;
        if (!emittedValue) {
          error = "native backend requires string bindings to use string literals, bindings, or entry args";
          return false;
        }
        localsIn.emplace(stmt.name, info);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        return true;
      }
      if (valueKind == LocalInfo::ValueKind::Unknown && kind != LocalInfo::Kind::Map) {
        error = "native backend requires typed bindings";
        return false;
      }
      if (!emitExpr(init, localsIn)) {
        return false;
      }
      info.index = nextLocal++;
      if (info.kind == LocalInfo::Kind::Reference) {
        if (init.kind != Expr::Kind::Call || !isSimpleCallName(init, "location") || init.args.size() != 1) {
          error = "reference binding requires location(...) initializer";
          return false;
        }
      }
      localsIn.emplace(stmt.name, info);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
      if (info.isFileHandle && !fileScopeStack.empty()) {
        fileScopeStack.back().push_back(info.index);
      }
      return true;
    }
    if (stmt.kind == Expr::Kind::Call && !stmt.isMethodCall &&
        (isSimpleCallName(stmt, "init") || isSimpleCallName(stmt, "drop"))) {
      const bool isInit = isSimpleCallName(stmt, "init");
      const size_t expectedArgs = isInit ? 2 : 1;
      if (stmt.args.size() != expectedArgs) {
        error = std::string(isInit ? "init" : "drop") + " requires exactly " +
                std::to_string(expectedArgs) + " argument" + (expectedArgs == 1 ? "" : "s");
        return false;
      }
      UninitializedStorageAccess access;
      bool resolved = false;
      if (!resolveUninitializedStorage(stmt.args.front(), localsIn, access, resolved)) {
        return false;
      }
      if (!resolved) {
        error = std::string(isInit ? "init" : "drop") + " requires uninitialized storage";
        return false;
      }
      auto emitFieldPointer = [&](const Expr &receiver, const StructSlotFieldInfo &field, int32_t ptrLocal) -> bool {
        if (!emitExpr(receiver, localsIn)) {
          return false;
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
      if (isInit) {
        const Expr &valueExpr = stmt.args.back();
        if (access.location == UninitializedStorageAccess::Location::Local) {
          const LocalInfo &storageInfo = *access.local;
          if (!storageInfo.structTypeName.empty()) {
            StructSlotLayout layout;
            if (!resolveStructSlotLayout(storageInfo.structTypeName, layout)) {
              return false;
            }
            if (!emitExpr(valueExpr, localsIn)) {
              return false;
            }
            const int32_t srcPtrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
            const int32_t destPtrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
            if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, layout.totalSlots)) {
              return false;
            }
            return true;
          }
          if (!emitExpr(valueExpr, localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(storageInfo.index)});
          return true;
        }
        if (access.location == UninitializedStorageAccess::Location::Field) {
          const Expr &receiver = stmt.args.front().args.front();
          const StructSlotFieldInfo &field = access.fieldSlot;
          const int32_t ptrLocal = allocTempLocal();
          if (!emitFieldPointer(receiver, field, ptrLocal)) {
            return false;
          }
          if (!field.structPath.empty()) {
            if (!emitExpr(valueExpr, localsIn)) {
              return false;
            }
            const int32_t srcPtrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
            if (!emitStructCopyFromPtrs(ptrLocal, srcPtrLocal, field.slotCount)) {
              return false;
            }
            return true;
          }
          if (!emitExpr(valueExpr, localsIn)) {
            return false;
          }
          const int32_t valueLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});
          return true;
        }
      }
      return true;
    }
    if (stmt.kind == Expr::Kind::Call && !stmt.isMethodCall && isSimpleCallName(stmt, "take") &&
        stmt.args.size() == 1) {
      UninitializedStorageAccess access;
      bool resolved = false;
      if (!resolveUninitializedStorage(stmt.args.front(), localsIn, access, resolved)) {
        return false;
      }
      if (resolved) {
        if (!emitExpr(stmt, localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::Pop, 0});
        return true;
      }
    }
    PrintBuiltin printBuiltin;
    if (stmt.kind == Expr::Kind::Call && getPrintBuiltin(stmt, printBuiltin)) {
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = printBuiltin.name + " does not support body arguments";
        return false;
      }
      if (stmt.args.size() != 1) {
        error = printBuiltin.name + " requires exactly one argument";
        return false;
      }
      return emitPrintArg(stmt.args.front(), localsIn, printBuiltin);
    }
    PathSpaceBuiltin pathSpaceBuiltin;
    if (stmt.kind == Expr::Kind::Call && getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && !resolveDefinitionCall(stmt)) {
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = pathSpaceBuiltin.name + " does not support body arguments";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error = pathSpaceBuiltin.name + " does not support template arguments";
        return false;
      }
      if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
        error = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
                " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
        return false;
      }
      for (const auto &arg : stmt.args) {
        if (arg.kind == Expr::Kind::StringLiteral) {
          continue;
        }
        if (!emitExpr(arg, localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::Pop, 0});
      }
      return true;
    }
    if (isReturnCall(stmt)) {
      if (activeInlineContext) {
        InlineContext &context = *activeInlineContext;
        if (stmt.args.empty()) {
          if (!context.returnsVoid) {
            error = "return requires exactly one argument";
            return false;
          }
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
        }
        if (stmt.args.size() != 1) {
          error = "return requires exactly one argument";
          return false;
        }
        if (context.returnsVoid) {
          error = "return value not allowed for void definition";
          return false;
        }
        if (context.returnLocal < 0) {
          error = "native backend missing inline return local";
          return false;
        }
        if (!emitExpr(stmt.args.front(), localsIn)) {
          return false;
        }
        if (context.returnsArray) {
          LocalInfo::ValueKind arrayKind = inferArrayElementKind(stmt.args.front(), localsIn);
          if (arrayKind == LocalInfo::ValueKind::Unknown) {
            error = "native backend only supports returning array values";
            return false;
          }
          if (arrayKind == LocalInfo::ValueKind::String) {
            error = "native backend does not support string array return types";
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
        }
        LocalInfo::ValueKind returnKind = inferExprKind(stmt.args.front(), localsIn);
        if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64 ||
            returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool ||
            returnKind == LocalInfo::ValueKind::Float32 || returnKind == LocalInfo::ValueKind::Float64 ||
            returnKind == LocalInfo::ValueKind::String) {
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
        }
        error = "native backend only supports returning numeric, bool, or string values";
        return false;
      }

      if (stmt.args.empty()) {
        if (!returnsVoid) {
          error = "return requires exactly one argument";
          return false;
        }
        emitFileScopeCleanupAll();
        function.instructions.push_back({IrOpcode::ReturnVoid, 0});
        sawReturn = true;
        return true;
      }
      if (stmt.args.size() != 1) {
        error = "return requires exactly one argument";
        return false;
      }
      if (returnsVoid) {
        error = "return value not allowed for void definition";
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      emitFileScopeCleanupAll();
      LocalInfo::ValueKind arrayKind = inferArrayElementKind(stmt.args.front(), localsIn);
      if (arrayKind != LocalInfo::ValueKind::Unknown) {
        if (arrayKind == LocalInfo::ValueKind::String) {
          error = "native backend does not support string array return types";
          return false;
        }
        function.instructions.push_back({IrOpcode::ReturnI64, 0});
      } else {
        LocalInfo::ValueKind returnKind = inferExprKind(stmt.args.front(), localsIn);
        if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64) {
          function.instructions.push_back({IrOpcode::ReturnI64, 0});
        } else if (returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
          function.instructions.push_back({IrOpcode::ReturnI32, 0});
        } else if (returnKind == LocalInfo::ValueKind::String) {
          function.instructions.push_back({IrOpcode::ReturnI64, 0});
        } else if (returnKind == LocalInfo::ValueKind::Float32) {
          function.instructions.push_back({IrOpcode::ReturnF32, 0});
        } else if (returnKind == LocalInfo::ValueKind::Float64) {
          function.instructions.push_back({IrOpcode::ReturnF64, 0});
        } else {
          error = "native backend only supports returning numeric, bool, or string values";
          return false;
        }
      }
      sawReturn = true;
      return true;
    }
    if (isMatchCall(stmt)) {
      Expr expanded;
      if (!lowerMatchToIf(stmt, expanded, error)) {
        return false;
      }
      return emitStatement(expanded, localsIn);
    }
    if (isIfCall(stmt)) {
      if (stmt.args.size() != 3) {
        error = "if requires condition, then, else";
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = "if does not accept trailing block arguments";
        return false;
      }
      if (!emitExpr(stmt.args[0], localsIn)) {
        return false;
      }
      LocalInfo::ValueKind condKind = inferExprKind(stmt.args[0], localsIn);
      if (condKind != LocalInfo::ValueKind::Bool) {
        error = "if condition requires bool";
        return false;
      }
      const Expr &thenArg = stmt.args[1];
      const Expr &elseArg = stmt.args[2];
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
      auto emitBranch = [&](const Expr &branch, LocalMap &branchLocals) -> bool {
        return emitBlock(branch, branchLocals);
      };
      size_t jumpIfZeroIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});
      LocalMap thenLocals = localsIn;
      if (!emitBranch(thenArg, thenLocals)) {
        return false;
      }
      size_t jumpIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::Jump, 0});
      size_t elseIndex = function.instructions.size();
      function.instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
      LocalMap elseLocals = localsIn;
      if (!emitBranch(elseArg, elseLocals)) {
        return false;
      }
      size_t endIndex = function.instructions.size();
      function.instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
