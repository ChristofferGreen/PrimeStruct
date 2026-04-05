



struct SemanticProgram;

using GetSetupMathBuiltinNameFn = std::function<bool(const Expr &, std::string &)>;
using GetSetupMathConstantNameFn = std::function<bool(const std::string &, std::string &)>;

struct SetupMathResolvers {
  GetSetupMathBuiltinNameFn getMathBuiltinName{};
  GetSetupMathConstantNameFn getMathConstantName{};
};
struct SetupMathAndBindingAdapters {
  SetupMathResolvers setupMathResolvers{};
  BindingTypeAdapters bindingTypeAdapters{};
};

bool isMathImportPath(const std::string &path);
bool hasProgramMathImport(const std::vector<std::string> &imports);
bool isSupportedMathBuiltinName(const std::string &name);
SetupMathResolvers makeSetupMathResolvers(bool hasMathImport);
SetupMathAndBindingAdapters makeSetupMathAndBindingAdapters(bool hasMathImport,
                                                           const SemanticProgram *semanticProgram = nullptr);
GetSetupMathBuiltinNameFn makeGetSetupMathBuiltinName(bool hasMathImport);
GetSetupMathConstantNameFn makeGetSetupMathConstantName(bool hasMathImport);
bool getSetupMathBuiltinName(const Expr &expr, bool hasMathImport, std::string &out);
bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out);
