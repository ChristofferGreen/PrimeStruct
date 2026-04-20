



using LowerReturnCallsEmitFileErrorWhyFn = std::function<bool(int32_t)>;

struct LowerReturnCallsSetupInput {
  ::primec::IrFunction *function = nullptr;
  InternRuntimeErrorStringFn internString;
};

bool runLowerReturnCallsSetup(const LowerReturnCallsSetupInput &input,
                              LowerReturnCallsEmitFileErrorWhyFn &emitFileErrorWhyOut,
                              std::string &errorOut);
