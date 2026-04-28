#pragma once

#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

std::string trimTemplateTypeText(const std::string &text);
bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg);
bool splitTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool isResultTemplateTypeBaseName(const std::string &base);
bool parseResultTypeName(const std::string &typeName,
                         bool &hasValue,
                         LocalInfo::ValueKind &valueKind,
                         std::string &errorType);

} // namespace primec::ir_lowerer
