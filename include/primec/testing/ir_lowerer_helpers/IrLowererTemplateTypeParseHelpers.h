



std::string trimTemplateTypeText(const std::string &text);
bool splitTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool parseResultTypeName(const std::string &typeName,
                         bool &hasValue,
                         LocalInfo::ValueKind &valueKind,
                         std::string &errorType);

