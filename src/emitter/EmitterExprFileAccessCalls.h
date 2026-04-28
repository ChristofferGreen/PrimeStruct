    if (!expr.isMethodCall && isSimpleCallName(expr, "File") && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      std::ostringstream out;
      const std::string mode = expr.templateArgs.front();
      std::string pathText =
          emitExpr(expr.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                   resultInfos, returnStructs, allowMathBare);
      if (mode == "Read") {
        out << "ps_file_open_read(" << pathText << ")";
        return out.str();
      }
      if (mode == "Write") {
        out << "ps_file_open_write(" << pathText << ")";
        return out.str();
      }
      if (mode == "Append") {
        out << "ps_file_open_append(" << pathText << ")";
        return out.str();
      }
    }
    if (full.rfind("/file/", 0) == 0) {
      std::ostringstream out;
      const std::string receiver =
          emitExpr(expr.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                   resultInfos, returnStructs, allowMathBare);
      if (expr.name == "write" || expr.name == "write_line") {
        out << "ps_result_status_from_error(ps_file_" << expr.name << "(" << receiver;
        for (size_t i = 1; i < expr.args.size(); ++i) {
          out << ", "
              << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                          resultInfos, returnStructs, allowMathBare);
        }
        out << "))";
        return out.str();
      }
      if (expr.name == "write_byte" && expr.args.size() == 2) {
        out << "ps_result_status_from_error(ps_file_write_byte(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << "))";
        return out.str();
      }
      if (expr.name == "read_byte" && expr.args.size() == 2) {
        out << "ps_result_status_from_error(ps_file_read_byte(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << "))";
        return out.str();
      }
      if (expr.name == "write_bytes" && expr.args.size() == 2) {
        out << "ps_result_status_from_error(ps_file_write_bytes(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << "))";
        return out.str();
      }
      if (expr.name == "flush") {
        out << "ps_result_status_from_error(ps_file_flush(" << receiver << "))";
        return out.str();
      }
      if (expr.name == "close") {
        out << "ps_result_status_from_error(ps_file_close(" << receiver << "))";
        return out.str();
      }
    }
    if (full == "/file_error/why" && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_file_error_why("
          << emitExpr(expr.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                      resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
