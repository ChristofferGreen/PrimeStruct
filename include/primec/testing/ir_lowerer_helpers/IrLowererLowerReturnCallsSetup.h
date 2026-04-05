



using LowerReturnCallsEmitFileErrorWhyFn = std::function<bool(int32_t)>;

struct LowerReturnCallsSetupInput {
  IrFunction *function = nullptr;
  InternRuntimeErrorStringFn internString;
};

bool runLowerReturnCallsSetup(const LowerReturnCallsSetupInput &input,
                              LowerReturnCallsEmitFileErrorWhyFn &emitFileErrorWhyOut,
                              std::string &errorOut);
